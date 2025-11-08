/*
Test: TEST-SYNC-HEURISTIC-NEG-001
Phase: 05-implementation
Traceability:
    Requirements: REQ-F-003, REQ-NF-Reliability-001
    Design: DES-C-021, DES-I-022
    SFMEA: FM-008
Purpose: Ensure UNCALIBRATED->SLAVE transition is blocked when a validation failure occurs within the sample window (FM-008 mitigation verification).
Notes: Introduces an ordering + path delay validation failure between good samples; verifies heuristic gating (>=3 successful offsets AND zero validation failures) prevents transition.
*/

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Timestamp fake_now{};

static PTPError noop_send_announce(const AnnounceMessage&) { return PTPError::Success; }
static PTPError noop_send_sync(const SyncMessage&) { return PTPError::Success; }
static PTPError noop_send_follow_up(const FollowUpMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_req(const DelayReqMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_resp(const DelayRespMessage&) { return PTPError::Success; }
static Timestamp get_timestamp_now() { return fake_now; }
static PTPError get_tx_timestamp(std::uint16_t, Timestamp* ts) { if (ts) { *ts = fake_now; } return PTPError::Success; }
static PTPError adjust_clock(std::int64_t) { return PTPError::Success; }
static PTPError adjust_frequency(double) { return PTPError::Success; }
static void on_state_change(PortState, PortState) {}
static void on_fault(const char*) {}

static Timestamp make_ns(uint64_t ns_total) {
    Timestamp t{};
    t.setTotalSeconds(ns_total / 1'000'000'000ULL);
    t.nanoseconds = static_cast<std::uint32_t>(ns_total % 1'000'000'000ULL);
    return t;
}

static bool provide_good_sample(PtpPort& port, uint64_t base_ns) {
    SyncMessage sync{}; sync.header.setMessageType(MessageType::Sync);
    FollowUpMessage fu{}; fu.header.setMessageType(MessageType::Follow_Up);
    fu.body.preciseOriginTimestamp = make_ns(base_ns + 0);
    if (!port.process_sync(sync, make_ns(base_ns + 1'000)).is_success()) return false; // T2
    if (!port.process_delay_req(DelayReqMessage{}, make_ns(base_ns + 1'500)).is_success()) return false; // T3
    DelayRespMessage dr{}; dr.body.requestingPortIdentity = port.get_identity();
    dr.body.receiveTimestamp = make_ns(base_ns + 2'000); // T4
    if (!port.process_delay_resp(dr).is_success()) return false; // T4 set
    if (!port.process_follow_up(fu).is_success()) return false; // T1 set + offset calc
    return true;
}

static bool provide_bad_sample(PtpPort& port, uint64_t base_ns) {
    // Construct timestamps that produce non-positive mean path delay and ordering violations:
    // T1 = base+2'000, T2 = base+1'000 (T2<T1), T3 = base+3'000, T4 = base+2'900 (T4<T3)
    SyncMessage sync{}; sync.header.setMessageType(MessageType::Sync);
    FollowUpMessage fu{}; fu.header.setMessageType(MessageType::Follow_Up);
    fu.body.preciseOriginTimestamp = make_ns(base_ns + 2'000); // T1
    if (!port.process_sync(sync, make_ns(base_ns + 1'000)).is_success()) return false; // T2 < T1
    if (!port.process_delay_req(DelayReqMessage{}, make_ns(base_ns + 3'000)).is_success()) return false; // T3
    DelayRespMessage dr{}; dr.body.requestingPortIdentity = port.get_identity();
    dr.body.receiveTimestamp = make_ns(base_ns + 2'900); // T4 < T3
    if (!port.process_delay_resp(dr).is_success()) return false;
    if (!port.process_follow_up(fu).is_success()) return false; // triggers validation failure
    return true;
}

int main() {
    PortConfiguration cfg{};
    cfg.port_number = 1;
    cfg.domain_number = 0;
    cfg.announce_interval = 0;
    cfg.sync_interval = 0;
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
    if (!port.initialize().is_success()) return 1;
    if (!port.start().is_success()) return 2;

    // Move to Uncalibrated state
    if (!port.process_event(StateEvent::RS_SLAVE).is_success()) return 3;
    if (port.get_state() != PortState::Uncalibrated) return 4;

    // Provide two good samples
    if (!provide_good_sample(port, 0)) return 5;
    if (!provide_good_sample(port, 10'000)) return 6;
    if (port.get_state() != PortState::Uncalibrated) return 7; // still below 3 samples

    // Inject one bad sample (increments ValidationsFailed)
    if (!provide_bad_sample(port, 20'000)) return 8;

    // Provide additional good samples; transition should remain blocked due to validation failure gating
    if (!provide_good_sample(port, 30'000)) return 9;
    if (!provide_good_sample(port, 40'000)) return 10;

    if (port.get_state() != PortState::Uncalibrated) {
        std::fprintf(stderr, "Expected to remain UNCALIBRATED after validation failure gating\n");
        return 11;
    }

    std::puts("ptp_state_machine_heuristic_negative: PASS");
    return 0;
}
