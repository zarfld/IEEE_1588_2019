// @satisfies STR-PERF-002 - Timing Determinism
// Measures execution time of a representative Sync + Follow_Up + Delay_Req + Delay_Resp path
// and asserts it is below a conservative threshold (<100 microseconds) for logical processing.
// NOTE: This is a logical timing determinism proxy; real hardware timestamp capture is excluded.

#include <cstdio>
#include <chrono>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Types::PTPError stub_send_announce(const AnnounceMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_sync(const SyncMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_follow_up(const FollowUpMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_req(const DelayReqMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_resp(const DelayRespMessage&) { return Types::PTPError::Success; }
static Types::Timestamp stub_get_ts() { Types::Timestamp t{}; t.setTotalSeconds(10); return t; }
static Types::PTPError stub_get_tx_ts(std::uint16_t, Types::Timestamp* t) { *t = stub_get_ts(); return Types::PTPError::Success; }
static Types::PTPError stub_adjust_clock(std::int64_t) { return Types::PTPError::Success; }
static Types::PTPError stub_adjust_freq(double) { return Types::PTPError::Success; }
static void stub_on_state_change(PortState, PortState) {}
static void stub_on_fault(const char*) {}

int main() {
    StateCallbacks cbs{ stub_send_announce, stub_send_sync, stub_send_follow_up, stub_send_delay_req, stub_send_delay_resp,
                        stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq, stub_on_state_change, stub_on_fault };
    PortConfiguration cfg{};
    cfg.delay_mechanism_p2p = false;
    OrdinaryClock clock(cfg, cbs);
    if (!clock.initialize().is_success() || !clock.start().is_success()) return 100;
    auto& port = const_cast<PtpPort&>(clock.get_port());

    // Prepare messages with minimal fields to exercise processing logic.
    SyncMessage sync{}; sync.header.setMessageType(MessageType::Sync); sync.header.setVersion(2); sync.header.sequenceId = 7; sync.body.originTimestamp = stub_get_ts();
    Types::Timestamp t2 = stub_get_ts();
    FollowUpMessage fu{}; fu.header.setMessageType(MessageType::Follow_Up); fu.header.setVersion(2); fu.header.sequenceId = 7; fu.body.preciseOriginTimestamp = sync.body.originTimestamp;
    DelayReqMessage dr{}; dr.header.setMessageType(MessageType::Delay_Req); dr.header.setVersion(2);
    Types::Timestamp t3 = stub_get_ts();
    DelayRespMessage dresp{}; dresp.header.setMessageType(MessageType::Delay_Resp); dresp.header.setVersion(2); dresp.body.receiveTimestamp = stub_get_ts();

    // Measure timing for a full logical processing slice.
    auto start = std::chrono::high_resolution_clock::now();
    port.process_sync(sync, t2);
    port.process_follow_up(fu);
    port.process_delay_req(dr, t3);
    port.process_delay_resp(dresp);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::fprintf(stderr, "TIMING_DETERMINISM duration_us=%lld\n", (long long)duration_us);

    // Determinism assertion (logical path should be extremely fast)
    if (duration_us > 100) {
        std::fprintf(stderr, "Timing determinism threshold exceeded: %lld us > 100 us\n", (long long)duration_us);
        return 1;
    }
    std::printf("TIMING_DETERMINISM PASS (%lld us)\n", (long long)duration_us);
    return 0;
}
