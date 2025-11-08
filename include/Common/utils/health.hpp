/*
Module: include/Common/utils/health.hpp
Phase: 05-implementation
Traceability:
  Design: DES-I-007  # Health/self-test interface design
  Requirements: REQ-NF-REL-004  # Health/self-test API
  Tests: TEST-UNIT-HealthSelfTest
Notes: Header-only. Provides self-test report aggregating metrics and last offset/BMCA selection. No dynamic allocation.
*/

#pragma once

#include <atomic>
#include <cstdint>
#include "Common/utils/metrics.hpp"
#include "Common/utils/fault_injection.hpp"

namespace Common {
namespace utils {
namespace health {

struct SelfTestReport {
    // Metrics snapshot
    std::uint64_t offsetsComputed{0};
    std::uint64_t bmcaSelections{0};
    std::uint64_t bmcaCandidateUpdates{0};
    std::uint64_t validationsFailed{0};
    std::uint64_t validationsPassed{0};
    std::uint64_t bmcaLocalWins{0};
    std::uint64_t bmcaForeignWins{0};
    std::uint64_t bmcaPassiveWins{0};
    // Recent calculation data
    long long lastOffsetNanoseconds{0};
    int lastBMCABestIndex{-1};
    bool lastBMCALocalWin{false};
    // Fault injection status
    bool faultInjectionActive{false};
    // Simple derived health indicators
    bool basicSynchronizedLikely{false};
    // Fault injection telemetry
    bool bmcaTieForcedLast{false};
};

namespace detail {
    inline std::atomic<int>& last_bmca_index() noexcept {
        static std::atomic<int> v{-1};
        return v;
    }
    inline std::atomic<long long>& last_offset_ns() noexcept {
        static std::atomic<long long> v{0};
        return v;
    }
    inline std::atomic<bool>& bmca_tie_forced() noexcept {
        static std::atomic<bool> v{false};
        return v;
    }
}

inline void record_bmca_selection(int index) noexcept {
    detail::last_bmca_index().store(index, std::memory_order_relaxed);
}

inline void record_offset_ns(long long ns) noexcept {
    detail::last_offset_ns().store(ns, std::memory_order_relaxed);
}

inline void record_bmca_forced_tie(bool forced) noexcept {
    detail::bmca_tie_forced().store(forced, std::memory_order_relaxed);
}

// Self-test collector
inline SelfTestReport self_test() noexcept {
    auto snap = Common::utils::metrics::snapshot();
    SelfTestReport r{};
    r.offsetsComputed = snap.offsetsComputed;
    r.bmcaSelections = snap.bmcaSelections;
    r.bmcaCandidateUpdates = snap.bmcaCandidateUpdates;
    r.validationsFailed = snap.validationsFailed;
    r.validationsPassed = snap.validationsPassed;
    r.bmcaLocalWins = snap.bmcaLocalWins;
    r.bmcaForeignWins = snap.bmcaForeignWins;
    r.bmcaPassiveWins = snap.bmcaPassiveWins;
    r.lastOffsetNanoseconds = detail::last_offset_ns().load(std::memory_order_relaxed);
    r.lastBMCABestIndex = detail::last_bmca_index().load(std::memory_order_relaxed);
    r.lastBMCALocalWin = (r.lastBMCABestIndex == 0);
    r.faultInjectionActive = Common::utils::fi::is_offset_jitter_enabled();
    r.bmcaTieForcedLast = detail::bmca_tie_forced().load(std::memory_order_relaxed);
    // Heuristic: if we have at least one offset computed and validationsFailed == 0 treat as synchronized likely
    r.basicSynchronizedLikely = (r.offsetsComputed > 0) && (r.validationsFailed == 0);
    return r;
}

// Optional observer callback for metrics emission
using health_observer_t = void(*)(const SelfTestReport&);

namespace detail {
    inline std::atomic<health_observer_t>& observer() noexcept {
        static std::atomic<health_observer_t> obs{nullptr};
        return obs;
    }
}

inline void set_observer(health_observer_t cb) noexcept {
    detail::observer().store(cb, std::memory_order_relaxed);
}

inline void emit() noexcept {
    auto cb = detail::observer().load(std::memory_order_relaxed);
    if (cb) {
        cb(self_test());
    }
}

} // namespace health
} // namespace utils
} // namespace Common
