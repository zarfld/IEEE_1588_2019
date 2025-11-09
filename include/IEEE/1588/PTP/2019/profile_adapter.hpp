/**
 * @file profile_adapter.hpp
 * @brief Minimal adapter to apply a ProfileConfig to a PTP PortConfiguration.
 *
 * Hardware-agnostic mapping to keep standards layer clean while enabling
 * tests to select gPTP vs AES67 behavior deterministically.
 *
 * Traceability:
 *  - REQ-F-201 Profile Strategy Selection
 */

#pragma once

#include "profile.hpp"
#include "clocks.hpp"

namespace IEEE {
namespace _1588 {
namespace _2019 {

inline void applyProfileToPortConfig(const ProfileConfig& profile,
                                     PTP::_2019::Clocks::PortConfiguration& cfg) noexcept {
    // Delay mechanism mapping
    cfg.delay_mechanism_p2p = (profile.delayMechanism == DelayMechanism::PeerToPeer);
    // Version and defaults are left as-is; higher-level config can override.
}

} // namespace _2019
} // namespace _1588
} // namespace IEEE
