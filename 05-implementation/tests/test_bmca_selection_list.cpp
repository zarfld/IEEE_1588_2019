/*
Test: TEST-UNIT-BMCA-SELECTION
Phase: 05-implementation
Traceability:
  Design: DES-C-003  # BMCA Engine Component
  Requirements: REQ-F-002  # BMCA state machine
  Code: 05-implementation/src/bmca.cpp, bmca.hpp
Notes: Covers list selection including empty list and multi-candidate ordering.
*/

#include <cstdio>
#include <vector>
#include "bmca.hpp"

using namespace IEEE::_1588::PTP::_2019::BMCA;

static PriorityVector mk(uint8_t p1, uint8_t cls, uint16_t acc, uint16_t var,
                         uint8_t p2, uint64_t id, uint16_t steps) {
    PriorityVector v{};
    v.priority1 = p1;
    v.clockClass = cls;
    v.clockAccuracy = acc;
    v.variance = var;
    v.priority2 = p2;
    v.grandmasterIdentity = id;
    v.stepsRemoved = steps;
    return v;
}

int main() {
    // Empty list returns -1
    {
        std::vector<PriorityVector> empty;
        if (selectBestIndex(empty) != -1) {
            std::fprintf(stderr, "Empty list should return -1\n");
            return 1;
        }
    }

    // Multiple candidates: ensure lexicographically best is selected
    {
        std::vector<PriorityVector> list{
            mk(128, 248, 0x0200, 1000, 128, 0x10ULL, 4),
            mk(127, 248, 0x0200, 1000, 128, 0x0FULL, 4), // best by priority1
            mk(128, 100, 0x0200, 1000, 128, 0x11ULL, 4),
            mk(128, 248, 0x0100, 1000, 128, 0x12ULL, 4),
            mk(128, 248, 0x0200,  500, 128, 0x13ULL, 4)
        };
        int idx = selectBestIndex(list);
        if (idx != 1) {
            std::fprintf(stderr, "Expected best index 1, got %d\n", idx);
            return 2;
        }
    }

    // Equality case: first occurrence should remain selected
    {
        auto a = mk(100,100,100,100,100,0x01ULL,1);
        auto b = a; // identical
        std::vector<PriorityVector> list{a, b};
        int idx = selectBestIndex(list);
        if (idx != 0) {
            std::fprintf(stderr, "Equal vectors: expected index 0, got %d\n", idx);
            return 3;
        }
    }

    return 0;
}
