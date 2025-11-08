/*
Test: TEST-UNIT-OFFSET-CALCULATION
Phase: 05-implementation
Traceability:
  Design: DES-C-010  # Time sync calculations
  Requirements: REQ-F-003  # Offset calculation (E2E)
  Code: include/IEEE/1588/PTP/2019/types.hpp, include/clocks.hpp
Notes: Validates E2E offset calculation formula and edge cases (negative, large seconds).
*/

#include <cstdio>
#include <cstdint>
#include <cmath>
#include "IEEE/1588/PTP/2019/types.hpp"
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;

static Types::Timestamp make_ts(uint64_t sec, uint32_t ns) {
    Types::Timestamp t{};
    t.setTotalSeconds(sec);
    t.nanoseconds = ns;
    return t;
}

static bool nearlyEqual(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

int main() {
    // Happy path: offset = ((T2-T1) - (T4-T3)) / 2
    {
        Clocks::SynchronizationData s{};
        auto T1 = make_ts(0, 0);
        auto T2 = make_ts(0, 1000);     // 1000 ns
        auto T3 = make_ts(0, 10);       // 10 ns
        auto T4 = make_ts(0, 20);       // 20 ns
        auto r = s.calculateOffset(T1, T2, T3, T4);
        if (!r.isSuccess()) { std::fprintf(stderr, "Offset calc unexpected error\n"); return 1; }
        double ns = r.getValue().toNanoseconds();
        // ((1000 - 0) - (20 - 10)) / 2 = (1000 - 10) / 2 = 495 ns
        if (!nearlyEqual(ns, 495.0)) {
            std::fprintf(stderr, "Expected 495 ns, got %.3f ns\n", ns);
            return 2;
        }
    }

    // Negative offset case
    {
        Clocks::SynchronizationData s{};
        auto T1 = make_ts(0, 0);
        auto T2 = make_ts(0, 100);      // 100 ns
        auto T3 = make_ts(0, 0);
        auto T4 = make_ts(0, 500);      // 500 ns
        auto r = s.calculateOffset(T1, T2, T3, T4);
        if (!r.isSuccess()) { std::fprintf(stderr, "Offset calc error (negative case)\n"); return 3; }
        double ns = r.getValue().toNanoseconds();
        // ((100 - 0) - (500 - 0)) / 2 = (-400)/2 = -200 ns
        if (!nearlyEqual(ns, -200.0)) {
            std::fprintf(stderr, "Expected -200 ns, got %.3f ns\n", ns);
            return 4;
        }
    }

    // Large seconds to ensure 64-bit arithmetic behaves
    {
        Clocks::SynchronizationData s{};
        auto T1 = make_ts(1'000'000, 100);    // large base
        auto T2 = make_ts(1'000'000, 1'000'100); // +1 ms
        auto T3 = make_ts(1'000'000, 50);     // 50 ns
        auto T4 = make_ts(1'000'000, 150);    // 150 ns
        auto r = s.calculateOffset(T1, T2, T3, T4);
        if (!r.isSuccess()) { std::fprintf(stderr, "Offset calc error (large seconds)\n"); return 5; }
        double ns = r.getValue().toNanoseconds();
        // ((1,000,100 - 100) - (150 - 50)) / 2 = (1,000,000 - 100) / 2 = 499,950 ns
        // Wait carefully: (900,000? recalc) -> (1,000,000 ns) - (100 ns) = 999,900 ns; halve => 499,950 ns
        if (!nearlyEqual(ns, 499'950.0)) {
            std::fprintf(stderr, "Expected 499950 ns, got %.3f ns\n", ns);
            return 6;
        }
    }

    return 0;
}
