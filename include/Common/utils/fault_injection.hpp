/*
Module: include/Common/utils/fault_injection.hpp
Phase: 05-implementation
Traceability:
  Design: DES-I-006  # Fault injection interface (04-design/interfaces/fault-injection.md)
  Requirements: REQ-NF-REL-002  # Provide feature flags for fault injection
  Tests: TEST-UNIT-FI-OffsetJitter, TEST-UNIT-FI-BMCA-Tie
Notes: Header-only, thread-safe toggles via std::atomic. No platform deps.
*/

#pragma once

#include <atomic>
#include <cstdint>

namespace Common {
namespace utils {
namespace fi {

namespace detail {
    inline std::atomic<bool>& offset_jitter_enabled() noexcept {
        static std::atomic<bool> v{false};
        return v;
    }
    inline std::atomic<long long>& offset_jitter_ns() noexcept {
        static std::atomic<long long> v{0};
        return v;
    }
    // Number of upcoming BMCA comparisons to force as ties
    inline std::atomic<int>& bmca_tie_tokens() noexcept {
        static std::atomic<int> v{0};
        return v;
    }
}

// Global reset (tests)
inline void reset() noexcept {
    detail::offset_jitter_enabled().store(false, std::memory_order_relaxed);
    detail::offset_jitter_ns().store(0, std::memory_order_relaxed);
    detail::bmca_tie_tokens().store(0, std::memory_order_relaxed);
}

// Offset jitter controls (nanoseconds)
inline void enable_offset_jitter(bool enable) noexcept {
    detail::offset_jitter_enabled().store(enable, std::memory_order_relaxed);
}
inline void set_offset_jitter_ns(long long ns) noexcept {
    detail::offset_jitter_ns().store(ns, std::memory_order_relaxed);
}
inline bool is_offset_jitter_enabled() noexcept {
    return detail::offset_jitter_enabled().load(std::memory_order_relaxed);
}
inline long long get_offset_jitter_ns() noexcept {
    return detail::offset_jitter_ns().load(std::memory_order_relaxed);
}

// BMCA tie injection
inline void force_bmca_tie_next(int count = 1) noexcept {
    if (count < 0) count = 0;
    detail::bmca_tie_tokens().store(count, std::memory_order_relaxed);
}
inline bool consume_bmca_tie_token() noexcept {
    int cur = detail::bmca_tie_tokens().load(std::memory_order_relaxed);
    while (cur > 0) {
        if (detail::bmca_tie_tokens().compare_exchange_weak(cur, cur - 1, std::memory_order_relaxed)) {
            return true;
        }
        // cur reloaded by CAS failure
    }
    return false;
}

} // namespace fi
} // namespace utils
} // namespace Common
