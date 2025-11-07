#ifndef IEEE_1588_2019_BMCA_HPP
#define IEEE_1588_2019_BMCA_HPP

// BMCA initial public surface for Phase 05 TDD
// Traceability: REQ-F-002; Design: DES-C-031/DES-I-032/DES-D-033
// References: IEEE 1588-2019 Section 9.3 (Best master clock algorithm)

#include <cstdint>
#include <vector>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace BMCA {

struct PriorityVector {
    std::uint8_t priority1{};
    std::uint8_t clockClass{};
    std::uint16_t clockAccuracy{}; // simplified for increment 1
    std::uint16_t variance{};      // simplified for increment 1
    std::uint8_t priority2{};
    std::uint64_t grandmasterIdentity{};
    std::uint16_t stepsRemoved{};
};

enum class CompareResult { ABetter, BBetter, Equal };

// Compare two vectors per ordering defined by Section 9.3 (incremental coverage)
CompareResult comparePriorityVectors(const PriorityVector& a, const PriorityVector& b);

// Return index of best vector in list or -1 if empty
int selectBestIndex(const std::vector<PriorityVector>& list);

} // namespace BMCA
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE

#endif // IEEE_1588_2019_BMCA_HPP
