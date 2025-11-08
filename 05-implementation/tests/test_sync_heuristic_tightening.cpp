/*
Module: 05-implementation/tests/test_sync_heuristic_tightening.cpp
Phase: 05-implementation
Traceability:
  Design: DES-C-010 (time sync), DES-I-007 (health)
  Requirements: REQ-F-003 (E2E offset), REQ-NF-REL-003 (observability)
  Test ID: TEST-UNIT-SyncHeuristic
Notes: Validates that port remains UNCALIBRATED until >=3 successful offsets with zero validation failures, then transitions to SLAVE.
*/

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"
#include "Common/utils/metrics.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Types::Timestamp make_ns(uint64_t ns_total) {
    Types::Timestamp t{};
    t.setTotalSeconds(ns_total / 1'000'000'000ULL);
    t.nanoseconds = static_cast<std::uint32_t>(ns_total % 1'000'000'000ULL);
    return t;
}

int main() {
    Common::utils::metrics::reset();

    StateCallbacks cbs{};
    cbs.get_timestamp = [](){ return make_ns(0); };

    PortConfiguration cfg{};
    cfg.port_number = 1;

    PtpPort port(cfg, cbs);
    if (!port.initialize().is_success()) { std::fprintf(stderr, "init failed\n"); return 1; }
    if (!port.start().is_success()) { std::fprintf(stderr, "start failed\n"); return 2; }

    // Simulate entering Uncalibrated
    (void)port.process_event(StateEvent::RS_SLAVE); // move to Uncalibrated
    if (port.get_state() != PortState::Uncalibrated) { std::fprintf(stderr, "not uncalibrated\n"); return 3; }

    // Provide Sync (T2), Follow_Up (T1), Delay_Req (T3), Delay_Resp (T4) sequences
    SyncMessage sync{}; sync.header.setMessageType(MessageType::Sync);
    FollowUpMessage fu{}; fu.header.setMessageType(MessageType::Follow_Up);
    fu.body.preciseOriginTimestamp = make_ns(0);

    // First two samples: should not transition yet
    for (int i = 0; i < 2; ++i) {
        (void)port.process_sync(sync, make_ns(1'000 + i));
        (void)port.process_delay_req(DelayReqMessage{}, Types::Timestamp{}); // ensures have_delay_req_
        DelayRespMessage dr{}; dr.body.requestingPortIdentity = port.get_identity(); dr.body.receiveTimestamp = make_ns(2'000 + i);
        (void)port.process_delay_resp(dr);
        (void)port.process_follow_up(fu);
        if (port.get_state() != PortState::Uncalibrated) {
            std::fprintf(stderr, "transitioned too early at sample %d\n", i+1);
            return 4;
        }
    }

    // Third sample: should transition to SLAVE if no validation failures
    (void)port.process_sync(sync, make_ns(3'000));
    (void)port.process_delay_req(DelayReqMessage{}, Types::Timestamp{});
    DelayRespMessage dr{}; dr.body.requestingPortIdentity = port.get_identity(); dr.body.receiveTimestamp = make_ns(4'000);
    (void)port.process_delay_resp(dr);
    (void)port.process_follow_up(fu);

    if (port.get_state() != PortState::Slave) {
        std::fprintf(stderr, "expected transition to SLAVE after three samples\n");
        return 5;
    }

    return 0;
}
