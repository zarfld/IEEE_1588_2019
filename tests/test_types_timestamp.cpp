// @satisfies STR-STD-002 - Message Format Correctness (timestamp fields handling)
// TEST: TEST-TIMESTAMP-ARITH-001
// REQ Trace: REQ-F-003 (Clock Offset Calculation), REQ-NF-P-001 (Determinism)
// Purpose: Validate Timestamp helpers (validate, operators, arithmetic)
#include <cstdio>
#include <cstdint>
#include "IEEE/1588/PTP/2019/types.hpp"

using namespace IEEE::_1588::PTP::_2019::Types;

static int run_timestamp_tests() {
    Timestamp t{};
    t.setTotalSeconds(1);
    t.nanoseconds = 500'000'000U; // 0.5s

    if (!t.validate().isSuccess()) {
        std::fprintf(stderr, "FAIL: valid timestamp reported invalid\n");
        return 1;
    }

    Timestamp t2{};
    t2.setTotalSeconds(2);
    t2.nanoseconds = 100'000'000U; // 2.1s

    if (!(t2 > t)) { std::fprintf(stderr, "FAIL: comparison operator> failed\n"); return 2; }

    // Subtraction to TimeInterval (t2 - t = 0.6s => scaled by 2^16)
    auto dt = t2 - t;
    const double ns = dt.toNanoseconds();
    if (ns < 599'000'000.0 || ns > 601'000'000.0) {
        std::fprintf(stderr, "FAIL: interval nanoseconds out of tolerance: %f\n", ns);
        return 3;
    }

    // Multiplication
    auto t3 = t * 2; // expect 2.0s
    if (t3.getTotalSeconds() != 3 || t3.nanoseconds != 0) {
        // 1.5s * 2 = 3.0s
        std::fprintf(stderr, "FAIL: timestamp multiply incorrect: %llu.%u\n",
                     (unsigned long long)t3.getTotalSeconds(), (unsigned)t3.nanoseconds);
        return 4;
    }

    // Invalid timestamp
    Timestamp bad = t;
    bad.nanoseconds = 1'000'000'000U; // invalid
    if (bad.validate().isSuccess()) {
        std::fprintf(stderr, "FAIL: invalid timestamp accepted\n");
        return 5;
    }

    std::puts("test_types_timestamp: PASS");
    return 0;
}

int timestamp_main() { return run_timestamp_tests(); }
