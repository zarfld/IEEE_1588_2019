/*
Test: TEST-UNIT-health-snapshot
Phase: 05-implementation
Traceability:
  Requirements: REQ-NF-REL-004 (health API), REQ-NF-REL-003 (observability)
  Design: DES-I-007 (health interface), DES-C-010 (time sync offset recording)
  SFMEA: FM-008 (heuristic gating evidence), FM-002 (offset clamp evidence when applicable)
Purpose: Validate health::self_test() produces coherent snapshot after offset calculations and responds to fault injection toggles.
*/

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"
#include "Common/utils/health.hpp"
#include "Common/utils/metrics.hpp"
#include "Common/utils/fault_injection.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Timestamp make_ns(uint64_t ns_total) {
    Timestamp t{};
    t.setTotalSeconds(ns_total / 1'000'000'000ULL);
    t.nanoseconds = static_cast<std::uint32_t>(ns_total % 1'000'000'000ULL);
    return t;
}

int main() {
    Common::utils::metrics::reset();
    Common::utils::fi::reset();

    // Compute two offsets without validation failures.
    SynchronizationData sync;
    Timestamp T1 = make_ns(0);
    Timestamp T2 = make_ns(1'000'000'000ULL); // 1s later
    Timestamp T3 = make_ns(0);
    Timestamp T4 = make_ns(0); // mean path delay yields positive offset

    auto r1 = sync.calculateOffset(T1, T2, T3, T4);
    if (!r1.is_success()) { std::fprintf(stderr, "Offset calc 1 failed\n"); return 1; }
    auto r2 = sync.calculateOffset(T1, T2, T3, T4);
    if (!r2.is_success()) { std::fprintf(stderr, "Offset calc 2 failed\n"); return 2; }

    auto report = Common::utils::health::self_test();
    if (report.offsetsComputed != 2) { std::fprintf(stderr, "Expected offsetsComputed=2 got %llu\n", (unsigned long long)report.offsetsComputed); return 3; }
    if (report.validationsFailed != 0) { std::fprintf(stderr, "Expected validationsFailed=0 got %llu\n", (unsigned long long)report.validationsFailed); return 4; }
    if (!report.basicSynchronizedLikely) { std::fprintf(stderr, "basicSynchronizedLikely should be true after successful offsets\n"); return 5; }
    if (report.lastOffsetNanoseconds == 0) { std::fprintf(stderr, "lastOffsetNanoseconds should be non-zero\n"); return 6; }

    // Enable offset jitter fault injection and recompute once; health should reflect active fault flag.
    Common::utils::fi::enable_offset_jitter(true);
    Common::utils::fi::set_offset_jitter_ns(10); // 10ns simulated jitter
    auto r3 = sync.calculateOffset(T1, T2, T3, T4);
    if (!r3.is_success()) { std::fprintf(stderr, "Offset calc 3 failed under FI\n"); return 7; }
    auto report2 = Common::utils::health::self_test();
    if (!report2.faultInjectionActive) { std::fprintf(stderr, "faultInjectionActive should be true after enabling jitter\n"); return 8; }
    if (report2.offsetsComputed != 3) { std::fprintf(stderr, "Expected offsetsComputed=3 got %llu\n", (unsigned long long)report2.offsetsComputed); return 9; }

    std::puts("health_snapshot: PASS");
    return 0;
}
