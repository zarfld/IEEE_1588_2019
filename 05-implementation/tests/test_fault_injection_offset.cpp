/*
Test: TEST-UNIT-FI-OffsetJitter
Phase: 05-implementation
Traceability:
  Design: DES-I-006  # Fault injection interface
  Requirements: REQ-NF-REL-002  # Fault injection toggles
  Code: include/Common/utils/fault_injection.hpp, include/clocks.hpp
Notes: Verifies offset jitter injection adds the configured nanoseconds.
*/

#include <cstdio>
#include <cstdint>
#include <cmath>
#include "IEEE/1588/PTP/2019/types.hpp"
#include "clocks.hpp"
#include "Common/utils/fault_injection.hpp"

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
    Common::utils::fi::reset();

    // Baseline offset calculation without jitter
    Clocks::SynchronizationData s1{};
    auto T1 = make_ts(0, 0);
    auto T2 = make_ts(0, 1000);     // 1000 ns
    auto T3 = make_ts(0, 10);       // 10 ns
    auto T4 = make_ts(0, 20);       // 20 ns
    auto r1 = s1.calculateOffset(T1, T2, T3, T4);
    if (!r1.isSuccess()) { std::fprintf(stderr, "Offset calc baseline error\n"); return 1; }
    double base = r1.getValue().toNanoseconds(); // expected 495 ns

    // Enable jitter of +250 ns and verify
    Common::utils::fi::set_offset_jitter_ns(250);
    Common::utils::fi::enable_offset_jitter(true);

    Clocks::SynchronizationData s2{};
    auto r2 = s2.calculateOffset(T1, T2, T3, T4);
    if (!r2.isSuccess()) { std::fprintf(stderr, "Offset calc with jitter error\n"); return 2; }
    double with_jitter = r2.getValue().toNanoseconds();

    if (!nearlyEqual(with_jitter, base + 250.0)) {
        std::fprintf(stderr, "Expected base+250 ns (%.3f), got %.3f\n", base + 250.0, with_jitter);
        return 3;
    }

    return 0;
}
