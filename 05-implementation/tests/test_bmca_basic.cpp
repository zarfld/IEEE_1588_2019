// ---
// title: "BMCA Basic Selection Red Test"
// specType: test
// testId: TEST-BMCA-SELECT-001
// status: draft
// relatedRequirements:
//   - REQ-F-002
// relatedDesign:
//   - DES-C-031
//   - DES-I-032
//   - DES-D-033
// purpose: "Initial failing test for Best Master Clock selection (IEEE 1588-2019 Section 9.3)."
// traceStatus: planned
// ---
// NOTE: Using comment-based front matter to avoid schema validation until test spec schemas are applied.
// This test intentionally EXPECTS failure (Red phase) until BMCA compare logic implemented.

#include <cstdio>
#include <vector>
#include <cstdint>
#include <cassert>
#include "bmca.hpp"

using namespace IEEE::_1588::PTP::_2019::BMCA;

int main() {
    // Arrange: Two candidate priority vectors where second SHOULD win per Section 9.3 ordering
    PriorityVector a{}; // lower quality
    a.priority1 = 128; a.clockClass = 248; a.clockAccuracy = 0xFFFF; a.variance = 65535; a.priority2 = 128; a.grandmasterIdentity = 0xABCDEF01ULL; a.stepsRemoved = 2;
    PriorityVector b{}; // better: lower priority1 & clockClass, better accuracy & variance, fewer steps
    b.priority1 = 100; b.clockClass = 128; b.clockAccuracy = 0x0100; b.variance = 100; b.priority2 = 100; b.grandmasterIdentity = 0xABCDEF02ULL; b.stepsRemoved = 1;

    // Act: Compare vectors (expected BBetter)
    auto cmp = comparePriorityVectors(a, b);

    // Assert (EXPECT FAIL now): cmp must indicate b is better
    if (cmp != CompareResult::BBetter) {
        std::fprintf(stderr, "TEST-BMCA-SELECT-001 FAILED: Expected BBetter, got different (stub incomplete)\n");
        return 1; // Fail triggers Red phase
    }

    // Act: Select best index from list {a,b}
    std::vector<PriorityVector> list{a, b};
    int idx = selectBestIndex(list);

    // Assert (EXPECT FAIL now): index should be 1 (b)
    if (idx != 1) {
        std::fprintf(stderr, "TEST-BMCA-SELECT-001 FAILED: Expected index 1 for best master, got %d (stub incomplete)\n", idx);
        return 2; // Fail triggers Red phase
    }

    std::puts("TEST-BMCA-SELECT-001 PASS (unexpected for Red phase)");

    // --- Edge/equality tests: vectors differing only in identity and stepsRemoved ---
    // TEST-BMCA-COMPARE-001: identical quality fields, differing stepsRemoved should prefer lower steps
    PriorityVector x = b; // use previously better vector as baseline
    PriorityVector y = b;
    x.stepsRemoved = 5;
    y.stepsRemoved = 3; // y better due to fewer steps
    auto cmp_steps = comparePriorityVectors(x, y);
    if (cmp_steps != CompareResult::BBetter) {
        std::fprintf(stderr, "TEST-BMCA-COMPARE-001 FAILED: Expected y with fewer stepsRemoved to win\n");
        return 3;
    }

    // If all fields including stepsRemoved equal except identity, ordering should be by identity
    // Lower identity wins per tie-breaker sequence (lexicographic last key)
    PriorityVector i1 = b;
    PriorityVector i2 = b;
    i1.grandmasterIdentity = 0x0000000000000001ULL;
    i2.grandmasterIdentity = 0x0000000000000002ULL;
    auto cmp_id = comparePriorityVectors(i1, i2);
    if (cmp_id != CompareResult::ABetter) {
        std::fprintf(stderr, "TEST-BMCA-COMPARE-001 FAILED: Expected lower grandmasterIdentity to win tie\n");
        return 4;
    }

    return 0; // All checks passed
}
