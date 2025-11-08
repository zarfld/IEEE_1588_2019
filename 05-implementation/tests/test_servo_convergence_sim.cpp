// @satisfies STR-PERF-003 - Servo convergence under deterministic conditions (evidence test)
// Purpose: Demonstrate that, given accurate offset estimation per IEEE 1588-2019 E2E model,
// a simple proportional servo reduces absolute offset monotonically and converges below a
// tight threshold within bounded iterations. This is hardware-agnostic, uses only protocol
// time math (T1..T4) via SynchronizationData::calculateOffset.

#include <cstdint>
#include <cmath>
#include <cassert>
#include <vector>
#include <algorithm>

#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using IEEE::_1588::PTP::_2019::Types::Timestamp;
using IEEE::_1588::PTP::_2019::Types::TimeInterval;

// Helper: construct Timestamp from total nanoseconds
static Timestamp make_ts(std::uint64_t total_ns) {
    Timestamp ts{};
    ts.setTotalSeconds(total_ns / 1000000000ULL);
    ts.nanoseconds = static_cast<std::uint32_t>(total_ns % 1000000000ULL);
    return ts;
}

int main() {
    // Deterministic simulation parameters
    const std::uint64_t t0_ns = 10ULL * 1000000000ULL; // start at 10s to avoid edge effects
    const std::uint64_t cycle_period_ns = 1000000000ULL; // 1s sync interval
    const std::uint64_t one_way_delay_ns = 100000; // 100 µs symmetric path delay
    double alpha = 0.5; // proportional gain (phase correction only)

    // Start with 500 µs offset between slave and master
    long long offset_ns = 500000; // positive: slave ahead of master
    const long long threshold_ns = 100; // convergence target

    Clocks::SynchronizationData sync{};

    std::vector<long long> offsets_abs;
    offsets_abs.reserve(16);

    for (int i = 0; i < 12; ++i) {
        const std::uint64_t t_master_send = t0_ns + static_cast<std::uint64_t>(i) * cycle_period_ns; // T1 master domain
        const std::uint64_t t_master_arrival = t_master_send + one_way_delay_ns; // true arrival at slave (master time)

        // Slave-local times are master times plus current offset (no drift in this minimal evidence)
        const std::uint64_t T1_ns = t_master_send; // master domain
        const std::uint64_t T2_ns = t_master_arrival + static_cast<std::uint64_t>(offset_ns >= 0 ? offset_ns : 0) + static_cast<std::uint64_t>(offset_ns < 0 ? 0 : 0);
        // Send Delay_Req 200 ms after sync arrival in master time for determinism
        const std::uint64_t delay_req_depart_true = t_master_send + 200000000ULL; // true/master time of intent
        const std::uint64_t T3_ns = delay_req_depart_true + static_cast<std::uint64_t>(offset_ns >= 0 ? offset_ns : 0);
        const std::uint64_t T4_ns = delay_req_depart_true + one_way_delay_ns; // master domain arrival

        const Timestamp T1 = make_ts(T1_ns);
        const Timestamp T2 = make_ts(T2_ns);
        const Timestamp T3 = make_ts(T3_ns);
        const Timestamp T4 = make_ts(T4_ns);

        auto res = sync.calculateOffset(T1, T2, T3, T4);
        assert(res.hasValue());
        const double est_offset_ns_d = res.getValue().toNanoseconds();
        const long long est_offset_ns = static_cast<long long>(std::llround(est_offset_ns_d));

        // Theoretical check: estimation should match actual offset exactly in this symmetric model.
        // Allow tiny tolerance due to double conversion (<= 1 ns).
        assert(std::llabs(est_offset_ns - offset_ns) <= 1);

        // Record absolute offset for monotonic decrease check
        offsets_abs.push_back(std::llabs(offset_ns));

        // Apply proportional correction (phase step)
        const double correction = alpha * static_cast<double>(est_offset_ns);
        offset_ns = static_cast<long long>(std::llround(static_cast<double>(offset_ns) - correction));
    }

    // Monotonic non-increasing absolute offset sequence (with exact estimation, P-controller halves each step)
    for (size_t k = 1; k < offsets_abs.size(); ++k) {
        assert(offsets_abs[k] <= offsets_abs[k-1]);
    }

    // Convergence: final absolute offset within strict threshold
    assert(std::llabs(offset_ns) <= threshold_ns);

    return 0;
}
