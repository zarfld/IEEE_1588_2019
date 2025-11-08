/*
Module: include/Common/utils/metrics.hpp
Phase: 05-implementation
Traceability:
  Design: DES-C-005  # Reliability metrics interface (04-design/interfaces/metrics-interface.md)
  Requirements: REQ-NF-REL-001, REQ-NF-REL-003  # Reliability hooks and observability
  Tests: TEST-UNIT-MetricsCounters
Notes: Header-only, thread-safe counters using std::atomic; zero external deps.
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <array>

namespace Common {
namespace utils {
namespace metrics {

// Enumerated counters for library-wide observability.
enum class CounterId : std::uint16_t {
    OffsetsComputed = 0,
    BMCA_Selections = 1,
    BMCA_CandidateUpdates = 2,
    ValidationsFailed = 3,
    ValidationsPassed = 4,
    BMCA_LocalWins = 5,          // Number of times local clock selected as best (master)
    BMCA_ForeignWins = 6,        // Number of times a foreign master was selected
    BMCA_PassiveWins = 7,        // Number of times tie/passive recommendation was made
    // Future: MessagesProcessed_Sync, _Announce, etc.
    COUNT
};

struct Snapshot {
    std::uint64_t offsetsComputed{0};
    std::uint64_t bmcaSelections{0};
    std::uint64_t bmcaCandidateUpdates{0};
    std::uint64_t validationsFailed{0};
    std::uint64_t validationsPassed{0};
    std::uint64_t bmcaLocalWins{0};
    std::uint64_t bmcaForeignWins{0};
    std::uint64_t bmcaPassiveWins{0};
};

namespace detail {
    inline std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(CounterId::COUNT)>& counters() noexcept {
        static std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(CounterId::COUNT)> g{}; // zero-initialized
        return g;
    }
}

inline void reset() noexcept {
    auto& g = detail::counters();
    for (auto& c : g) c.store(0, std::memory_order_relaxed);
}

inline void increment(CounterId id, std::uint64_t delta = 1) noexcept {
    auto& g = detail::counters();
    g[static_cast<std::size_t>(id)].fetch_add(delta, std::memory_order_relaxed);
}

inline std::uint64_t get(CounterId id) noexcept {
    auto& g = detail::counters();
    return g[static_cast<std::size_t>(id)].load(std::memory_order_relaxed);
}

inline Snapshot snapshot() noexcept {
    auto& g = detail::counters();
    Snapshot s;
    s.offsetsComputed      = g[static_cast<std::size_t>(CounterId::OffsetsComputed)].load(std::memory_order_relaxed);
    s.bmcaSelections       = g[static_cast<std::size_t>(CounterId::BMCA_Selections)].load(std::memory_order_relaxed);
    s.bmcaCandidateUpdates = g[static_cast<std::size_t>(CounterId::BMCA_CandidateUpdates)].load(std::memory_order_relaxed);
    s.validationsFailed    = g[static_cast<std::size_t>(CounterId::ValidationsFailed)].load(std::memory_order_relaxed);
    s.validationsPassed    = g[static_cast<std::size_t>(CounterId::ValidationsPassed)].load(std::memory_order_relaxed);
    s.bmcaLocalWins        = g[static_cast<std::size_t>(CounterId::BMCA_LocalWins)].load(std::memory_order_relaxed);
    s.bmcaForeignWins      = g[static_cast<std::size_t>(CounterId::BMCA_ForeignWins)].load(std::memory_order_relaxed);
    s.bmcaPassiveWins      = g[static_cast<std::size_t>(CounterId::BMCA_PassiveWins)].load(std::memory_order_relaxed);
    return s;
}

} // namespace metrics
} // namespace utils
} // namespace Common
