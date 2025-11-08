/*
Test: TEST-UNIT-TIMESTAMP-ORDERING
Phase: 05-implementation
Traceability:
  Design: DES-C-010  # Time sync calculations
  Requirements: REQ-NF-REL-002 (Assertions & invariants), REQ-F-003 (Offset E2E)
Notes: Verifies that ordering violations (T2<T1 or T4<T3) trigger validation telemetry without crashing.
*/

#include <cstdio>
#include <cstdint>
#include "IEEE/1588/PTP/2019/types.hpp"
#include "clocks.hpp"
#include "Common/utils/metrics.hpp"

using namespace IEEE::_1588::PTP::_2019;

static Types::Timestamp make_ns(uint64_t ns_total) {
    Types::Timestamp t{};
    t.setTotalSeconds(ns_total / 1'000'000'000ULL);
    t.nanoseconds = static_cast<std::uint32_t>(ns_total % 1'000'000'000ULL);
    return t;
}

int main() {
    Common::utils::metrics::reset();

    Clocks::SynchronizationData s{};
    // Create ordering violations: T2 < T1 and T4 < T3
    auto T1 = make_ns(1'000);
    auto T2 = make_ns(900);   // earlier than T1
    auto T3 = make_ns(2'000);
    auto T4 = make_ns(1'500); // earlier than T3

    auto beforeFailed = Common::utils::metrics::get(Common::utils::metrics::CounterId::ValidationsFailed);
    auto res = s.calculateOffset(T1, T2, T3, T4);
    if (!res.isSuccess()) {
        std::fprintf(stderr, "calculateOffset returned error unexpectedly\n");
        return 1;
    }
    auto afterFailed = Common::utils::metrics::get(Common::utils::metrics::CounterId::ValidationsFailed);
    if (afterFailed < beforeFailed + 1) { // at least one violation should be recorded
        std::fprintf(stderr, "Expected ValidationsFailed to increment on ordering violation (before=%llu after=%llu)\n",
                     static_cast<unsigned long long>(beforeFailed),
                     static_cast<unsigned long long>(afterFailed));
        return 2;
    }

    return 0;
}
