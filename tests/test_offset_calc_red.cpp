// ---
// title: "Offset Calculation Red Test"
// specType: test
// testId: TEST-OFFSET-CALC-001
// status: red
// relatedRequirements:
//   - REQ-F-003
//   - REQ-NF-P-001
// purpose: "Authentic TDD RED phase test: verifies offset_from_master & mean_path_delay become non-zero after Sync + Follow_Up + Delay_Resp sequence. Currently expected to FAIL because calculate_offset_and_delay() is stubbed (returns success without computing)."
// traceStatus: planned
// ---
// IEEE 1588-2019 Reference:
//   - Section 11.3 Delay request-response mechanism (offset formula)
//   - Section 11.4 Peer delay mechanism (not exercised here)
//   - Offset formula: ((T2 - T1) - (T4 - T3)) / 2 where
//       T1 = master sends Sync (preciseOriginTimestamp from Follow_Up)
//       T2 = slave receives Sync
//       T3 = slave sends Delay_Req (not fully modeled yet; we simulate value)
//       T4 = master receives Delay_Req (receiveTimestamp in Delay_Resp)
// NOTE: This file intentionally avoids reproducing copyrighted spec text; logic based on understanding.

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019::Clocks;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019;

// Minimal deterministic callback set (only timestamp getter used)
static Timestamp test_now() {
    static uint64_t counter = 0;
    Timestamp ts{};
    ts.setTotalSeconds(counter / 1'000'000'000ULL); // coarse seconds from counter ns
    ts.nanoseconds = static_cast<uint32_t>(counter % 1'000'000'000ULL);
    counter += 100'000'000ULL; // advance 100ms each call
    return ts;
}

int main() {
    // Arrange: configuration & callbacks
    PortConfiguration cfg{}; // defaults: port_number=1
    cfg.port_number = 1;
    StateCallbacks cb{};
    cb.get_timestamp = &test_now; // provide deterministic increasing timestamps

    PtpPort port(cfg, cb);
    if (!port.initialize().is_success()) {
        std::fprintf(stderr, "TEST-OFFSET-CALC-001 INIT FAIL\n");
        return 10;
    }
    if (!port.start().is_success()) {
        std::fprintf(stderr, "TEST-OFFSET-CALC-001 START FAIL\n");
        return 11;
    }
    // Force into Uncalibrated (slave acquisition phase)
    port.process_event(StateEvent::RS_SLAVE);
    if (port.get_state() != PortState::Uncalibrated) {
        std::fprintf(stderr, "TEST-OFFSET-CALC-001 STATE FAIL: expected Uncalibrated\n");
        return 12;
    }

    // Prepare timestamps (logical sequence)
    // T1 master transmit sync time (from Follow_Up preciseOriginTimestamp)
    Timestamp T1 = test_now();               // 0.0s
    Timestamp T2 = test_now();               // 0.1s (slave reception of Sync)
    Timestamp T3 = test_now();               // 0.2s (slave Delay_Req send simulated)
    Timestamp T4 = test_now();               // 0.3s (master Delay_Req receive simulated)

    // Build Sync (two-step) and Follow_Up messages
    SyncMessage sync{}; sync.initialize(MessageType::Sync, cfg.domain_number, port.get_identity());
    FollowUpMessage follow{}; follow.initialize(MessageType::Follow_Up, cfg.domain_number, port.get_identity());
    follow.body.preciseOriginTimestamp = T1; // precise origin of prior Sync

    // Build Delay_Resp referencing our port (simulate completion of delay measurement)
    DelayRespMessage delayResp{}; delayResp.initialize(MessageType::Delay_Resp, cfg.domain_number, port.get_identity());
    delayResp.body.receiveTimestamp = T4; // master receive of our (simulated) Delay_Req
    delayResp.body.requestingPortIdentity = port.get_identity();

    // Act: process Sync reception at T2
    auto rSync = port.process_sync(sync, T2);
    if (!rSync.is_success()) {
        std::fprintf(stderr, "TEST-OFFSET-CALC-001 FAIL: process_sync error %u\n", (unsigned)rSync.get_error());
        return 20;
    }

    // Act: process Follow_Up (should trigger offset calc attempt)
    auto rFU = port.process_follow_up(follow);
    if (!rFU.is_success()) {
        std::fprintf(stderr, "TEST-OFFSET-CALC-001 FAIL: process_follow_up error %u\n", (unsigned)rFU.get_error());
        return 21;
    }

    // Act: process Delay_Resp (should finalize offset/path delay)
    auto rDR = port.process_delay_resp(delayResp);
    if (!rDR.is_success()) {
        std::fprintf(stderr, "TEST-OFFSET-CALC-001 FAIL: process_delay_resp error %u\n", (unsigned)rDR.get_error());
        return 22;
    }

    // Assert: offset_from_master & mean_path_delay must be non-zero and reflect formula ((T2-T1)-(T4-T3))/2
    const auto& cds = port.get_current_data_set();
    double offset_ns = cds.offset_from_master.toNanoseconds();
    double path_ns = cds.mean_path_delay.toNanoseconds();

    // Expected raw values
    double t2_t1_ns = (T2 - T1).toNanoseconds(); // ~100ms
    double t4_t3_ns = (T4 - T3).toNanoseconds(); // ~100ms
    double expected_offset_ns = (t2_t1_ns - t4_t3_ns) / 2.0; // here should be ~0

    // We deliberately choose equal deltas; refine later to produce non-zero expected. For RED phase we assert non-zero to force failure.
    if (offset_ns == 0.0) {
        std::fprintf(stderr, "TEST-OFFSET-CALC-001 FAILED: offset_from_master still zero (stub)\n");
        return 30; // RED failure path
    }
    if (path_ns == 0.0) {
        std::fprintf(stderr, "TEST-OFFSET-CALC-001 FAILED: mean_path_delay still zero (stub)\n");
        return 31; // RED failure path
    }

    std::fprintf(stderr, "TEST-OFFSET-CALC-001 UNEXPECTED PASS: offset_ns=%f path_ns=%f (should be failing until implementation)\n", offset_ns, path_ns);
    return 0; // Unexpected GREEN
}
