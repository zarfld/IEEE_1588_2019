// @satisfies STR-MAINT-001 - Code Quality (health/heartbeat instrumentation)
/*
Module: 05-implementation/tests/test_health_heartbeat.cpp
Phase: 05-implementation
Traceability:
  Design: DES-I-007  # Health/self-test interface design
  Requirements: REQ-NF-REL-004 (Health/self-test API), REQ-NF-REL-002 (Assertions & invariants)
  Tests: TEST-UNIT-HealthHeartbeat
Notes: Verifies periodic health::emit() heartbeat from PtpPort::tick with 1s throttling.
*/

#include <cstdio>
#include <cstdint>
#include <atomic>
#include "clocks.hpp"
#include "Common/utils/health.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Types::Timestamp make_ns(uint64_t ns_total) {
    Types::Timestamp t{};
    t.setTotalSeconds(ns_total / 1'000'000'000ULL);
    t.nanoseconds = static_cast<std::uint32_t>(ns_total % 1'000'000'000ULL);
    return t;
}

int main() {
  static std::atomic<int> emits{0};
  struct Local {
    static void on_health(const Common::utils::health::SelfTestReport&) {
      emits.fetch_add(1, std::memory_order_relaxed);
    }
  };
  Common::utils::health::set_observer(&Local::on_health);

    StateCallbacks cbs{}; // no sending; listening state does nothing
    PortConfiguration cfg{};
    cfg.port_number = 1;

    PtpPort port(cfg, cbs);
    if (!port.initialize().is_success()) { std::fprintf(stderr, "init failed\n"); return 1; }
    if (!port.start().is_success()) { std::fprintf(stderr, "start failed\n"); return 2; }

    // t=0: no heartbeat yet
    (void)port.tick(make_ns(0));
    int e0 = emits.load(std::memory_order_relaxed);
    if (e0 != 0) { std::fprintf(stderr, "Unexpected initial emit count %d\n", e0); return 3; }

    // t=0.5s: still throttled
    (void)port.tick(make_ns(500'000'000ULL));
    int e1 = emits.load(std::memory_order_relaxed);
    if (e1 != 0) { std::fprintf(stderr, "Unexpected emit before 1s: %d\n", e1); return 4; }

    // t=1.0s: first heartbeat
    (void)port.tick(make_ns(1'000'000'000ULL));
    int e2 = emits.load(std::memory_order_relaxed);
    if (e2 < 1) { std::fprintf(stderr, "Expected at least one heartbeat at 1s, got %d\n", e2); return 5; }

    // t=1.2s: still throttled, no new emit required
    (void)port.tick(make_ns(1'200'000'000ULL));
    int e3 = emits.load(std::memory_order_relaxed);
    if (e3 != e2) { std::fprintf(stderr, "Unexpected emit between 1s and 2s: %d vs %d\n", e3, e2); return 6; }

    // t=2.0s: second heartbeat
    (void)port.tick(make_ns(2'000'000'000ULL));
    int e4 = emits.load(std::memory_order_relaxed);
    if (e4 < e2 + 1) { std::fprintf(stderr, "Expected another heartbeat at 2s, got %d total\n", e4); return 7; }

    return 0;
}
