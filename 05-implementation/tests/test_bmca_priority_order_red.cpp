/*
Test: TEST-UNIT-BMCA-PriorityOrder (RED phase - GAP-BMCA-001)
Phase: 05-implementation
Traceability:
    Stakeholder: StR-EXTS-003  # Cross-standard synchronization requirements
    Requirement: REQ-F-202     # BMCA with forced tie detection
    Design: DES-C-003          # BMCA Engine Component
    CAP: GAP-BMCA-001          # Full BMCA priority vector ordering
Notes: Comprehensive RED test for IEEE 1588-2019 Section 9.3 priority vector ordering.
       Tests all 7 fields in lexicographic order: priority1, clockClass, clockAccuracy,
       variance, priority2, stepsRemoved, grandmasterIdentity.
*/

// @req REQ-F-202
// @req StR-EXTS-003
// @satisfies GAP-BMCA-001
// @test-category: protocol-compliance
// @test-priority: P0
// @test-type: unit

#include "bmca.hpp"
#include "Common/utils/metrics.hpp"
#include <cstdio>
#include <cstdint>
#include <cassert>

using namespace IEEE::_1588::PTP::_2019::BMCA;

// Helper to create baseline "good" priority vector
static PriorityVector make_baseline() {
    PriorityVector v{};
    v.priority1 = 128;
    v.clockClass = 128;
    v.clockAccuracy = 0x2000;  // mid-range
    v.variance = 5000;
    v.priority2 = 128;
    v.grandmasterIdentity = 0x0000AABBCCDD0000ULL;
    v.stepsRemoved = 2;
    return v;
}

// Test structure: each test case validates one field dominance
struct TestCase {
    const char* name;
    PriorityVector a;
    PriorityVector b;
    CompareResult expected;
};

int main() {
    Common::utils::metrics::reset();
    int failures = 0;
    int test_count = 0;

    // =============================================================================
    // IEEE 1588-2019 Section 9.3.2.4.1: Priority Vector Comparison
    // Lexicographic order: priority1 > clockClass > clockAccuracy > variance >
    //                     priority2 > stepsRemoved > grandmasterIdentity
    // Lower values are "better" (except grandmasterIdentity uses numeric comparison)
    // =============================================================================

    // Test 1: priority1 dominates all other fields
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        a.priority1 = 100;  // better (lower)
        b.priority1 = 200;  // worse (higher)
        
        // Make b better in ALL other fields to ensure priority1 dominates
        b.clockClass = 10;           // b better
        b.clockAccuracy = 0x0100;   // b better
        b.variance = 100;           // b better
        b.priority2 = 50;           // b better
        b.stepsRemoved = 0;         // b better
        b.grandmasterIdentity = 0x0000000000000001ULL; // b better
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: priority1 should dominate (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 2: clockClass dominates when priority1 equal
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        a.priority1 = 128;     // equal
        b.priority1 = 128;
        a.clockClass = 100;    // better (lower)
        b.clockClass = 200;    // worse (higher)
        
        // Make b better in all subsequent fields
        b.clockAccuracy = 0x0100;
        b.variance = 100;
        b.priority2 = 50;
        b.stepsRemoved = 0;
        b.grandmasterIdentity = 0x0000000000000001ULL;
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: clockClass should dominate when priority1 equal (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 3: clockAccuracy dominates when priority1 and clockClass equal
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        a.priority1 = 128;
        b.priority1 = 128;
        a.clockClass = 128;
        b.clockClass = 128;
        a.clockAccuracy = 0x1000;  // better (lower)
        b.clockAccuracy = 0x3000;  // worse (higher)
        
        // Make b better in all subsequent fields
        b.variance = 100;
        b.priority2 = 50;
        b.stepsRemoved = 0;
        b.grandmasterIdentity = 0x0000000000000001ULL;
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: clockAccuracy should dominate when priority1/clockClass equal (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 4: variance dominates when priority1, clockClass, clockAccuracy equal
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        a.priority1 = 128;
        b.priority1 = 128;
        a.clockClass = 128;
        b.clockClass = 128;
        a.clockAccuracy = 0x2000;
        b.clockAccuracy = 0x2000;
        a.variance = 1000;  // better (lower)
        b.variance = 9000;  // worse (higher)
        
        // Make b better in all subsequent fields
        b.priority2 = 50;
        b.stepsRemoved = 0;
        b.grandmasterIdentity = 0x0000000000000001ULL;
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: variance should dominate when priority1/clockClass/accuracy equal (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 5: priority2 dominates when first 4 fields equal
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        a.priority1 = 128;
        b.priority1 = 128;
        a.clockClass = 128;
        b.clockClass = 128;
        a.clockAccuracy = 0x2000;
        b.clockAccuracy = 0x2000;
        a.variance = 5000;
        b.variance = 5000;
        a.priority2 = 100;  // better (lower)
        b.priority2 = 200;  // worse (higher)
        
        // Make b better in remaining fields
        b.stepsRemoved = 0;
        b.grandmasterIdentity = 0x0000000000000001ULL;
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: priority2 should dominate when first 4 fields equal (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 6: stepsRemoved dominates when first 5 fields equal
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        a.priority1 = 128;
        b.priority1 = 128;
        a.clockClass = 128;
        b.clockClass = 128;
        a.clockAccuracy = 0x2000;
        b.clockAccuracy = 0x2000;
        a.variance = 5000;
        b.variance = 5000;
        a.priority2 = 128;
        b.priority2 = 128;
        a.stepsRemoved = 1;  // better (fewer hops)
        b.stepsRemoved = 5;  // worse (more hops)
        
        // Make b better in remaining field
        b.grandmasterIdentity = 0x0000000000000001ULL;
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: stepsRemoved should dominate when first 5 fields equal (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 7: grandmasterIdentity is final tiebreaker
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        // All fields equal except grandmasterIdentity
        a.priority1 = 128;
        b.priority1 = 128;
        a.clockClass = 128;
        b.clockClass = 128;
        a.clockAccuracy = 0x2000;
        b.clockAccuracy = 0x2000;
        a.variance = 5000;
        b.variance = 5000;
        a.priority2 = 128;
        b.priority2 = 128;
        a.stepsRemoved = 2;
        b.stepsRemoved = 2;
        
        a.grandmasterIdentity = 0x0000000000000001ULL;  // better (lower)
        b.grandmasterIdentity = 0x0000000000000002ULL;  // worse (higher)
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: grandmasterIdentity should break tie when all other fields equal (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 8: Exact equality (all fields identical)
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::Equal) {
            std::fprintf(stderr, "[FAIL] Test %d: Identical vectors should return Equal (got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 9: Boundary values - maximum priority1 (worst)
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        a.priority1 = 128;
        b.priority1 = 255;  // maximum value (worst)
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: priority1=128 should beat priority1=255 (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 10: Boundary values - minimum stepsRemoved (best)
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        // All quality fields equal
        a.priority1 = b.priority1 = 128;
        a.clockClass = b.clockClass = 128;
        a.clockAccuracy = b.clockAccuracy = 0x2000;
        a.variance = b.variance = 5000;
        a.priority2 = b.priority2 = 128;
        
        a.stepsRemoved = 0;     // minimum (best - direct GM)
        b.stepsRemoved = 65535; // maximum (worst - many hops)
        
        auto result = comparePriorityVectors(a, b);
        if (result != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: stepsRemoved=0 should beat stepsRemoved=65535 (expected ABetter, got %d)\n",
                        test_count, static_cast<int>(result));
            failures++;
        }
    }

    // Test 11: Transitivity check (if a > b and b > c, then a > c)
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        PriorityVector c = make_baseline();
        
        a.priority1 = 100;  // best
        b.priority1 = 150;  // middle
        c.priority1 = 200;  // worst
        
        auto ab = comparePriorityVectors(a, b);
        auto bc = comparePriorityVectors(b, c);
        auto ac = comparePriorityVectors(a, c);
        
        if (ab != CompareResult::ABetter || bc != CompareResult::ABetter || ac != CompareResult::ABetter) {
            std::fprintf(stderr, "[FAIL] Test %d: Transitivity violated (ab=%d, bc=%d, ac=%d)\n",
                        test_count, static_cast<int>(ab), static_cast<int>(bc), static_cast<int>(ac));
            failures++;
        }
    }

    // Test 12: Symmetry check (if a < b, then b > a)
    {
        test_count++;
        PriorityVector a = make_baseline();
        PriorityVector b = make_baseline();
        
        a.clockClass = 100;
        b.clockClass = 200;
        
        auto ab = comparePriorityVectors(a, b);
        auto ba = comparePriorityVectors(b, a);
        
        if (ab != CompareResult::ABetter || ba != CompareResult::BBetter) {
            std::fprintf(stderr, "[FAIL] Test %d: Symmetry violated (ab=%d should be ABetter, ba=%d should be BBetter)\n",
                        test_count, static_cast<int>(ab), static_cast<int>(ba));
            failures++;
        }
    }

    // Summary
    std::printf("\n=== TEST-UNIT-BMCA-PriorityOrder Summary ===\n");
    std::printf("Total tests: %d\n", test_count);
    std::printf("Failures: %d\n", failures);
    
    if (failures > 0) {
        std::fprintf(stderr, "\nRED PHASE: %d tests failed as expected\n", failures);
        std::fprintf(stderr, "Implementation needs enhancement to pass all priority vector ordering tests.\n");
        return 1;
    }
    
    std::printf("\nGREEN PHASE: All priority vector ordering tests passed!\n");
    return 0;
}
