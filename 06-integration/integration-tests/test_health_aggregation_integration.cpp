// TEST: TEST-INTEG-HEALTH-AGG-001
// Purpose: Verify health snapshot reflects computed offsets and validations during BoundaryClock message flow
// Traceability: Phase 06 Integration; Health aggregation from metrics and recent offsets
// Standards Context: Exercises E2E timestamps processing (see IEEE 1588-2019 Section 11 overview)

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static PTPError noop_send_announce(const AnnounceMessage&) { return PTPError::Success; }
static PTPError noop_send_sync(const SyncMessage&) { return PTPError::Success; }
static PTPError noop_send_follow_up(const FollowUpMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_req(const DelayReqMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_resp(const DelayRespMessage&) { return PTPError::Success; }
static Timestamp get_timestamp_now() { return Timestamp{}; }
static PTPError get_tx_timestamp(std::uint16_t, Timestamp* ts) { if (ts) *ts = Timestamp{}; return PTPError::Success; }
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

int main() {
    // Arrange: 1-port BoundaryClock
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> cfgs{};
    cfgs[0].port_number = 1; cfgs[0].domain_number = 0; cfgs[0].announce_interval = 0; cfgs[0].sync_interval = 0;

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

    BoundaryClock bc(cfgs, 1, cbs);
    if (!bc.initialize().is_success()) return 1;
    if (!bc.start().is_success()) return 2;

    // Trigger BMCA path via Announce to move Listening -> Uncalibrated
    AnnounceMessage ann{};
    // initialize Announce with same source identity as port to satisfy minimal checks
    const PtpPort* p = bc.get_port(1);
    if (!p) return 3;
    ann.initialize(MessageType::Announce, 0, p->get_identity());
    if (!bc.process_message(1, static_cast<std::uint8_t>(MessageType::Announce), &ann, sizeof(ann), Timestamp{}).is_success()) return 4;

    // Provide one full offset calculation sample: T2,T3,T4 and Follow_Up (with T1)
    SyncMessage sync{}; sync.header.setMessageType(MessageType::Sync);
    FollowUpMessage fu{}; fu.header.setMessageType(MessageType::Follow_Up);
    fu.body.preciseOriginTimestamp = make_ns(1000); // T1 = 1,000 ns

    // T2 later than T1
    if (!bc.process_message(1, static_cast<std::uint8_t>(MessageType::Sync), &sync, sizeof(sync), make_ns(4000)).is_success()) return 5; // T2 = 4,000 ns
    // Simulate local Delay_Req emission (stores T3)
    DelayReqMessage dreq{}; dreq.header.setMessageType(MessageType::Delay_Req);
    if (!bc.process_message(1, static_cast<std::uint8_t>(MessageType::Delay_Req), &dreq, sizeof(dreq), make_ns(5000)).is_success()) return 6; // T3 = 5,000 ns
    // Provide Delay_Resp with T4 (master timestamp)
    DelayRespMessage dr{}; dr.header.setMessageType(MessageType::Delay_Resp);
    dr.body.requestingPortIdentity = p->get_identity();
    dr.body.receiveTimestamp = make_ns(9000); // T4 = 9,000 ns
    if (!bc.process_message(1, static_cast<std::uint8_t>(MessageType::Delay_Resp), &dr, sizeof(dr), Timestamp{}).is_success()) return 7;

    // Provide Follow_Up (contains T1); this should compute offset
    if (!bc.process_message(1, static_cast<std::uint8_t>(MessageType::Follow_Up), &fu, sizeof(fu), Timestamp{}).is_success()) return 8;

    // Assert health snapshot reflects computed offset and validations
    auto report = Common::utils::health::self_test();
    if (report.offsetsComputed == 0) return 9;
    if (report.validationsPassed == 0) return 10;
    if (!report.basicSynchronizedLikely) return 11;

    std::puts("health_aggregation_integration: PASS");
    return 0;
}
