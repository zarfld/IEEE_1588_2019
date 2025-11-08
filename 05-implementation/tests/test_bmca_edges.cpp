/*
Test: TEST-UNIT-BMCA-EDGES
Phase: 05-implementation
Traceability:
  Design: DES-C-003  # BMCA Engine Component
  Requirements: REQ-F-002  # BMCA state machine
  Code: 05-implementation/src/bmca.cpp, bmca.hpp
Notes: Covers comparison ordering edges: equality and single-field precedence.
*/

#include <cstdio>
#include <cstdint>
#include "bmca.hpp"

using namespace IEEE::_1588::PTP::_2019::BMCA;

static PriorityVector base() {
    PriorityVector v{};
    v.priority1 = 128;
    v.clockClass = 248;
    v.clockAccuracy = 0x0200;
    v.variance = 1000;
    v.priority2 = 128;
    v.grandmasterIdentity = 0xABCDEF0000000001ULL;
    v.stepsRemoved = 4;
    return v;
}

int main() {
    // Equality
    {
        auto a = base();
        auto b = a;
        auto cmp = comparePriorityVectors(a, b);
        if (cmp != CompareResult::Equal) {
            std::fprintf(stderr, "TEST-UNIT-BMCA-EDGES equal failed\n");
            return 1;
        }
    }

    // priority1 precedence (lower is better)
    {
        auto a = base();
        auto b = base(); b.priority1 = a.priority1 - 1;
        if (comparePriorityVectors(a, b) != CompareResult::BBetter) {
            std::fprintf(stderr, "priority1 ordering failed\n");
            return 2;
        }
    }

    // clockClass precedence (lower is better)
    {
        auto a = base();
        auto b = base(); b.clockClass = a.clockClass - 1;
        if (comparePriorityVectors(a, b) != CompareResult::BBetter) {
            std::fprintf(stderr, "clockClass ordering failed\n");
            return 3;
        }
    }

    // clockAccuracy precedence (lower is better)
    {
        auto a = base();
        auto b = base(); b.clockAccuracy = a.clockAccuracy - 1;
        if (comparePriorityVectors(a, b) != CompareResult::BBetter) {
            std::fprintf(stderr, "clockAccuracy ordering failed\n");
            return 4;
        }
    }

    // variance precedence (lower is better)
    {
        auto a = base();
        auto b = base(); b.variance = a.variance - 1;
        if (comparePriorityVectors(a, b) != CompareResult::BBetter) {
            std::fprintf(stderr, "variance ordering failed\n");
            return 5;
        }
    }

    // priority2 precedence (lower is better)
    {
        auto a = base();
        auto b = base(); b.priority2 = a.priority2 - 1;
        if (comparePriorityVectors(a, b) != CompareResult::BBetter) {
            std::fprintf(stderr, "priority2 ordering failed\n");
            return 6;
        }
    }

    // stepsRemoved as tie-breaker (lower is better)
    {
        auto a = base();
        auto b = base(); b.stepsRemoved = a.stepsRemoved - 1;
        if (comparePriorityVectors(a, b) != CompareResult::BBetter) {
            std::fprintf(stderr, "stepsRemoved ordering failed\n");
            return 7;
        }
    }

    // grandmasterIdentity as final tie-breaker (lower is better)
    {
        auto a = base();
        auto b = base(); b.grandmasterIdentity = a.grandmasterIdentity - 1ULL;
        if (comparePriorityVectors(a, b) != CompareResult::BBetter) {
            std::fprintf(stderr, "grandmasterIdentity ordering failed\n");
            return 8;
        }
    }

    return 0;
}
