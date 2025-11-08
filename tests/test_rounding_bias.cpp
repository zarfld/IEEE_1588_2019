// @satisfies STR-PERF-003 - Clock Servo Performance (rounding/jitter characterization)
/*
Test: TEST-UNIT-rounding-bias-characterization
Phase: 05-implementation
Traceability:
  Requirements: REQ-F-003, REQ-NF-Reliability-001
  Design: DES-C-021, DES-I-022
  SFMEA: FM-014
Purpose: Characterize rounding in integer /2 path for offset calculation; verify no bias occurs for integral-nanosecond timestamps, and that enabling compensation toggle does not change results for such cases.
*/

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"
#include "Common/utils/config.hpp"

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
    // Arrange a range of pairs where (T2-T1) - (T4-T3) results in odd/even integer nanosecond deltas,
    // but scaled domain remains multiples of 2^16 (no half increments), so division by 2 is exact.
    struct Case { int64_t t2_t1_ns; int64_t t4_t3_ns; };
    Case cases[] = {
        {1001, 1},   // diff = 1000
        {1000, 1},   // diff = 999
        {1, 1000},   // diff = -999
        {2001, 1000},// diff = 1001
        {0, 0},      // diff = 0
        {5, 3},      // diff = 2
        {7, 4},      // diff = 3
    };

    // We don't need a full port; use SynchronizationData directly for unit characterization
    SynchronizationData sync;

    for (const auto& c : cases) {
        // Build timestamps to realize the deltas:
        // T1=0, T2=c.t2_t1_ns; T3=0, T4=c.t4_t3_ns
        Timestamp T1 = make_ns(0);
        Timestamp T2 = make_ns(static_cast<uint64_t>(c.t2_t1_ns >= 0 ? c.t2_t1_ns : 0));
        if (c.t2_t1_ns < 0) { T1 = make_ns(static_cast<uint64_t>(-c.t2_t1_ns)); T2 = make_ns(0); }
        Timestamp T3 = make_ns(0);
        Timestamp T4 = make_ns(static_cast<uint64_t>(c.t4_t3_ns >= 0 ? c.t4_t3_ns : 0));
        if (c.t4_t3_ns < 0) { T3 = make_ns(static_cast<uint64_t>(-c.t4_t3_ns)); T4 = make_ns(0); }

        // Disable compensation and compute
        Common::utils::config::set_rounding_compensation_enabled(false);
        auto r1 = sync.calculateOffset(T1, T2, T3, T4);
    if (!r1.is_success()) return 1;
    double off1_ns = r1.getValue().toNanoseconds();

        // Enable compensation and compute again
        Common::utils::config::set_rounding_compensation_enabled(true);
        auto r2 = sync.calculateOffset(T1, T2, T3, T4);
    if (!r2.is_success()) return 2;
    double off2_ns = r2.getValue().toNanoseconds();

        // Expected ns: ((T2-T1) - (T4-T3)) / 2
        double expected_ns = (static_cast<double>(c.t2_t1_ns) - static_cast<double>(c.t4_t3_ns)) / 2.0;

        // Check both results equal expected exactly for integral ns deltas
        if (off1_ns != expected_ns || off2_ns != expected_ns) {
            std::fprintf(stderr, "Rounding characterization failed: case (t2t1=%lld,t4t3=%lld) got off1=%f off2=%f exp=%f\n",
                         static_cast<long long>(c.t2_t1_ns), static_cast<long long>(c.t4_t3_ns), off1_ns, off2_ns, expected_ns);
            return 3;
        }
    }

    std::puts("rounding_bias_characterization: PASS");
    return 0;
}
