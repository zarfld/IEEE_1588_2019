// TEST: TEST-BMCA-TRANSITION-001, TEST-BMCA-TIMEOUT-001, TEST-SYNC-001
// Related DES: DES-C-021, DES-I-022, DES-D-023, DES-I-024
// Purpose: Minimal smoke test for state machine transitions per IEEE 1588-2019 Section 9.2

#include <cstdio>
#include <cstdint>
#include <string>
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Timestamp fake_now{}; // default zero initialization (avoids GCC missing-field-initializers warning)

static PTPError noop_send_announce(const AnnounceMessage&) { return PTPError::Success; }
static PTPError noop_send_sync(const SyncMessage&) { return PTPError::Success; }
static PTPError noop_send_follow_up(const FollowUpMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_req(const DelayReqMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_resp(const DelayRespMessage&) { return PTPError::Success; }
static Timestamp get_timestamp_now() { return fake_now; }
static PTPError get_tx_timestamp(std::uint16_t, Timestamp* ts) { if(ts){*ts = fake_now;} return PTPError::Success; }
static PTPError adjust_clock(std::int64_t) { return PTPError::Success; }
static PTPError adjust_frequency(double) { return PTPError::Success; }
static void on_state_change(PortState old_s, PortState new_s) {
    (void)old_s; (void)new_s;
}
static void on_fault(const char*) {}

// Forward declarations to dispatch into other test translation units
int messages_main();
int timestamp_main();

static Timestamp make_ns(uint64_t ns_total) {
    Timestamp t{};
    t.setTotalSeconds(ns_total / 1'000'000'000ULL);
    t.nanoseconds = static_cast<std::uint32_t>(ns_total % 1'000'000'000ULL);
    return t;
}

int main(int argc, char** argv) {
    // Dispatch to focused tests when requested by CTest
    if (argc > 2 && std::string(argv[1]) == "--case") {
        std::string which = argv[2];
        if (which == "messages") return messages_main();
        if (which == "timestamp") return timestamp_main();
    }
    // Arrange: minimal port configuration and callbacks
    PortConfiguration cfg{};
    cfg.port_number = 1;
    cfg.domain_number = 0;
    cfg.announce_interval = 0; // 1s
    cfg.sync_interval = 0;     // 1s
    cfg.announce_receipt_timeout = 3;

    StateCallbacks cbs{};
    cbs.send_announce = &noop_send_announce;
    cbs.send_sync = &noop_send_sync;
    cbs.send_follow_up = &noop_send_follow_up;
    cbs.send_delay_req = &noop_send_delay_req;
    cbs.send_delay_resp = &noop_send_delay_resp;
    cbs.get_timestamp = &get_timestamp_now;
    cbs.get_tx_timestamp = &get_tx_timestamp;
    cbs.adjust_clock = &adjust_clock;
    cbs.adjust_frequency = &adjust_frequency;
    cbs.on_state_change = &on_state_change;
    cbs.on_fault = &on_fault;

    PtpPort port(cfg, cbs);

    // Act: initialize and start
    if (!port.initialize().is_success()) return 1;
    if (port.get_state() != PortState::Initializing) return 2;
    if (!port.start().is_success()) return 3;
    if (port.get_state() != PortState::Listening) return 4;

    // Simulate BMCA recommending master
    if (!port.process_event(StateEvent::RS_MASTER).is_success()) return 5;
    if (port.get_state() != PortState::PreMaster) return 6;

    // Simulate qualification timeout → Master
    if (!port.process_event(StateEvent::QUALIFICATION_TIMEOUT).is_success()) return 7;
    if (port.get_state() != PortState::Master) return 8;

    // Master tick should attempt to send announce/sync without error
    fake_now = Timestamp{}; // reset to zero timestamp
    if (!port.tick(fake_now).is_success()) return 9;

    // Simulate Announce reception list update and BMCA to slave path
    AnnounceMessage ann{};
    ann.initialize(MessageType::Announce, 0, port.get_identity());
    if (!port.process_announce(ann).is_success()) return 10;

    // Force RS_SLAVE event to transition out of master
    if (!port.process_event(StateEvent::RS_SLAVE).is_success()) return 11;
    if (port.get_state() != PortState::Uncalibrated) return 12;

    // New heuristic requires 3 full successful offset samples; provide them
    SyncMessage sync{}; sync.header.setMessageType(MessageType::Sync);
    FollowUpMessage fu{}; fu.header.setMessageType(MessageType::Follow_Up);
    fu.body.preciseOriginTimestamp = make_ns(0);
    for (int i = 0; i < 3; ++i) {
        // Provide T2, T3, T4 in order with positive path delay
        if (!port.process_sync(sync, make_ns(1'000 + i)).is_success()) return 13;
        if (!port.process_delay_req(DelayReqMessage{}, Timestamp{}).is_success()) return 13;
        DelayRespMessage dr{}; dr.body.requestingPortIdentity = port.get_identity();
        dr.body.receiveTimestamp = make_ns(2'000 + i);
        if (!port.process_delay_resp(dr).is_success()) return 13;
        if (!port.process_follow_up(fu).is_success()) return 13;
        // Should remain UNCALIBRATED until after the third sample
        if (i < 2 && port.get_state() != PortState::Uncalibrated) return 14;
    }
    if (port.get_state() != PortState::Slave) return 14;

    // Timeout path: advance time to trigger announce timeout back to Listening
    // announce timeout = (1s << log2 interval) * timeout multiplier = 1s * 3
    // advance > 3s
    // Removed unused lambda to avoid -Wunused-but-set-variable warning
    fake_now.setTotalSeconds(0);
    fake_now.nanoseconds = 0;
    Timestamp future = fake_now;
    // Simulate last_announce_time_ at t=0 by calling tick after becoming slave
    if (!port.tick(fake_now).is_success()) return 15;
    future.setTotalSeconds(4); // 4 seconds later
    if (!port.tick(future).is_success()) return 16;
    // After timeout, state should be Listening
    if (port.get_state() != PortState::Listening) return 17;

    // All checks passed
    std::puts("ptp_state_machine_basic: PASS");
    return 0;
}
