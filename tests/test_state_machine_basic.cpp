// TEST: TEST-BMCA-TRANSITION-001, TEST-BMCA-TIMEOUT-001, TEST-SYNC-001
// Related DES: DES-C-021, DES-I-022, DES-D-023, DES-I-024
// Purpose: Minimal smoke test for state machine transitions per IEEE 1588-2019 Section 9.2

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Timestamp fake_now{0};

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

int main() {
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
    fake_now = Timestamp{0};
    if (!port.tick(fake_now).is_success()) return 9;

    // Simulate Announce reception list update and BMCA to slave path
    AnnounceMessage ann{};
    ann.initialize(MessageType::Announce, 0, port.get_identity());
    if (!port.process_announce(ann).is_success()) return 10;

    // Force RS_SLAVE event to transition out of master
    if (!port.process_event(StateEvent::RS_SLAVE).is_success()) return 11;
    if (port.get_state() != PortState::Uncalibrated) return 12;

    // Follow_Up reception triggers simple sync completion path
    FollowUpMessage fu{};
    fu.initialize(MessageType::Follow_Up, 0, port.get_identity());
    if (!port.process_follow_up(fu).is_success()) return 13;
    // In simplified logic, Uncalibrated transitions to Slave after follow_up
    if (port.get_state() != PortState::Slave) return 14;

    // Timeout path: advance time to trigger announce timeout back to Listening
    // announce timeout = (1s << log2 interval) * timeout multiplier = 1s * 3
    // advance > 3s
    auto seconds_to_ns = [](std::uint64_t s){ return s * 1000000000ULL; };
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
