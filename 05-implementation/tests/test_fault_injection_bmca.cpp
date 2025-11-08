/*
Test: TEST-UNIT-FI-BMCA-Tie
Phase: 05-implementation
Traceability:
  Design: DES-I-006  # Fault injection interface
  Requirements: REQ-NF-REL-002  # Fault injection toggles
  Code: include/Common/utils/fault_injection.hpp, 05-implementation/src/bmca.cpp
Notes: Forces a single BMCA comparison tie so selection does not update.
*/

#include <cstdio>
#include <vector>
#include <cstdint>
#include "bmca.hpp"
#include "Common/utils/fault_injection.hpp"

using namespace IEEE::_1588::PTP::_2019::BMCA;

int main() {
    Common::utils::fi::reset();

    PriorityVector a{}; // worse
    a.priority1 = 128; a.clockClass = 248; a.clockAccuracy = 0xFFFF; a.variance = 65535; a.priority2 = 128; a.grandmasterIdentity = 0xABCDEF01ULL; a.stepsRemoved = 2;
    PriorityVector b{}; // better
    b.priority1 = 100; b.clockClass = 128; b.clockAccuracy = 0x0100; b.variance = 100; b.priority2 = 100; b.grandmasterIdentity = 0xABCDEF02ULL; b.stepsRemoved = 1;

    std::vector<PriorityVector> list{a, b};

    // Force next comparison as tie: selection should remain index 0
    Common::utils::fi::force_bmca_tie_next(1);
    int idx = selectBestIndex(list);

    if (idx != 0) {
        std::fprintf(stderr, "Expected forced tie to keep best index at 0, got %d\n", idx);
        return 1;
    }

    // Without tie, B should win (index 1)
    idx = selectBestIndex(list);
    if (idx != 1) {
        std::fprintf(stderr, "Expected index 1 without tie, got %d\n", idx);
        return 2;
    }

    return 0;
}
