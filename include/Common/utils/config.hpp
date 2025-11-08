//
// Module: include/Common/utils/config.hpp
// Phase: 05-implementation
// Traceability:
//   Requirements: REQ-NF-Reliability-001
//   Design: DES-C-Common-Config
//   Tests: TEST-UNIT-rounding-bias-characterization
// Notes: Minimal configuration toggles for standards layer behavior switches.

#ifndef COMMON_UTILS_CONFIG_HPP
#define COMMON_UTILS_CONFIG_HPP

#include <atomic>

namespace Common {
namespace utils {
namespace config {

// Optional compensation toggle for FM-014 (rounding bias mitigation).
inline std::atomic<bool>& rounding_compensation_flag() {
    static std::atomic<bool> flag{false};
    return flag;
}

inline void set_rounding_compensation_enabled(bool enabled) {
    rounding_compensation_flag().store(enabled, std::memory_order_relaxed);
}

inline bool is_rounding_compensation_enabled() {
    return rounding_compensation_flag().load(std::memory_order_relaxed);
}

} // namespace config
} // namespace utils
} // namespace Common

#endif // COMMON_UTILS_CONFIG_HPP
