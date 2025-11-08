// @satisfies STR-PERF-002 - Timing Determinism (bounded calculations and clamping)
/*
Test: TEST-UNIT-offset-clamp-boundary
Phase: 05-implementation
Traceability:
  Requirements: REQ-F-003, REQ-NF-REL-003
  Design: DES-C-010 (time sync clamp), DES-I-007 (health/metrics)
  SFMEA: FM-002 (offset clamp), FM-013 (overflow handling)
Purpose: Verify offset clamping activates at defined boundary and emits metrics/logging evidence.
*/

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"
#include "Common/utils/metrics.hpp"

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
    // Reset metrics counters for deterministic assertions
    Common::utils::metrics::reset();

    // Arrange: Choose deltas such that ((T2-T1) - (T4-T3))/2 exceeds clamp bound
    // Using 3 seconds for (T2-T1) and 0 for (T4-T3) ensures post-division scaled exceeds 2^46.
    const uint64_t three_seconds_ns = 3ULL * 1'000'000'000ULL;
    Timestamp T1 = make_ns(0);
    Timestamp T2 = make_ns(three_seconds_ns);
    Timestamp T3 = make_ns(0);
    Timestamp T4 = make_ns(0);

    SynchronizationData sync;
    auto res = sync.calculateOffset(T1, T2, T3, T4);
    if (!res.is_success()) {
        std::fprintf(stderr, "calculateOffset failed unexpectedly\n");
        return 1;
    }

    // Expected clamp in nanoseconds: (1<<46) scaled units / 65536 = 1<<30 ns exactly
    const double expected_clamp_ns = static_cast<double>(1ULL << 30); // 1,073,741,824 ns
    const double actual_ns = res.getValue().toNanoseconds();

    if (actual_ns != expected_clamp_ns) {
        std::fprintf(stderr, "Clamp value mismatch: got %0.0f ns, expected %0.0f ns\n", actual_ns, expected_clamp_ns);
        return 2;
    }

    // Metrics: one offset computed, one validation failed due to clamp, zero validations passed
    auto s = Common::utils::metrics::snapshot();
    if (s.offsetsComputed != 1 || s.validationsFailed != 1 || s.validationsPassed != 0) {
        std::fprintf(stderr, "Metrics mismatch: Offsets=%llu, ValFail=%llu, ValPass=%llu (expected 1,1,0)\n",
                     static_cast<unsigned long long>(s.offsetsComputed),
                     static_cast<unsigned long long>(s.validationsFailed),
                     static_cast<unsigned long long>(s.validationsPassed));
        return 3;
    }

    std::puts("offset_clamp_boundary: PASS");
    return 0;
}
