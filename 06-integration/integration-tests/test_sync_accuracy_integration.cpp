/**
 * @file test_sync_accuracy_integration.cpp
 * @brief Integration tests for IEEE 1588-2019 synchronization accuracy
 * 
 * Tests offset calculation, delay measurement, and sync convergence
 * according to IEEE 1588-2019 specifications.
 */

#include "IEEE/1588/PTP/2019/sync_integration.hpp"
#include "clocks.hpp"
#include <cstdio>
#include <cmath>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

// Helper to create timestamp
static Types::Timestamp make_timestamp(std::uint64_t seconds, std::uint32_t nanoseconds) {
    Types::Timestamp ts{};
    ts.seconds_high = static_cast<std::uint16_t>((seconds >> 32) & 0xFFFF);
    ts.seconds_low = static_cast<std::uint32_t>(seconds & 0xFFFFFFFF);
    ts.nanoseconds = nanoseconds;
    return ts;
}

// Test 1: Coordinator lifecycle (start/stop)
static int test_coordinator_lifecycle() {
    StateCallbacks callbacks{};
    callbacks.get_timestamp = []() { return Types::Timestamp{}; };
    
    PortConfiguration config{};
    config.port_number = 1;
    
    PtpPort port(config, callbacks);
    port.initialize();
    port.start();
    
    SyncIntegration coordinator(port);
    
    // Should start successfully
    auto result = coordinator.start();
    if (!result.is_success()) {
        std::fprintf(stderr, "Test 1 FAIL: Start failed\n");
        return 1;
    }
    
    if (!coordinator.is_running()) {
        std::fprintf(stderr, "Test 1 FAIL: Should be running after start\n");
        return 2;
    }
    
    // Should stop successfully
    result = coordinator.stop();
    if (!result.is_success()) {
        std::fprintf(stderr, "Test 1 FAIL: Stop failed\n");
        return 3;
    }
    
    if (coordinator.is_running()) {
        std::fprintf(stderr, "Test 1 FAIL: Should not be running after stop\n");
        return 4;
    }
    
    std::printf("Test 1 PASS: Coordinator lifecycle\n");
    return 0;
}

// Test 2: Configuration validation
static int test_configuration_validation() {
    StateCallbacks callbacks{};
    callbacks.get_timestamp = []() { return Types::Timestamp{}; };
    
    PortConfiguration config{};
    config.port_number = 1;
    
    PtpPort port(config, callbacks);
    port.initialize();
    port.start();
    
    SyncIntegration coordinator(port);
    
    // Valid configuration
    SyncIntegration::Configuration valid_config{};
    valid_config.sampling_interval_ms = 1000;
    valid_config.synchronized_threshold_ns = 1000.0;
    valid_config.degraded_threshold_ns = 10000.0;
    valid_config.critical_threshold_ns = 100000.0;
    
    auto result = coordinator.configure(valid_config);
    if (!result.is_success()) {
        std::fprintf(stderr, "Test 2 FAIL: Valid configuration rejected\n");
        return 1;
    }
    
    // Invalid: zero interval
    SyncIntegration::Configuration invalid_interval{};
    invalid_interval.sampling_interval_ms = 0;
    
    result = coordinator.configure(invalid_interval);
    if (result.is_success()) {
        std::fprintf(stderr, "Test 2 FAIL: Zero interval accepted\n");
        return 2;
    }
    
    // Invalid: thresholds not ordered
    SyncIntegration::Configuration invalid_thresholds{};
    invalid_thresholds.sampling_interval_ms = 1000;
    invalid_thresholds.synchronized_threshold_ns = 10000.0;
    invalid_thresholds.degraded_threshold_ns = 1000.0;  // Less than sync threshold
    
    result = coordinator.configure(invalid_thresholds);
    if (result.is_success()) {
        std::fprintf(stderr, "Test 2 FAIL: Invalid threshold ordering accepted\n");
        return 3;
    }
    
    std::printf("Test 2 PASS: Configuration validation\n");
    return 0;
}

// Test 3: Statistics collection
static int test_statistics_collection() {
    StateCallbacks callbacks{};
    callbacks.get_timestamp = []() { return Types::Timestamp{}; };
    
    PortConfiguration config{};
    config.port_number = 1;
    
    PtpPort port(config, callbacks);
    port.initialize();
    port.start();
    
    SyncIntegration coordinator(port);
    coordinator.start();
    
    // Take first sample
    auto t0 = make_timestamp(0, 0);
    coordinator.sample_now(t0);
    
    auto stats = coordinator.get_statistics();
    
    // Should have 1 sample
    if (stats.total_offset_samples != 1) {
        std::fprintf(stderr, "Test 3 FAIL: Expected 1 offset sample, got %llu\n",
                    (unsigned long long)stats.total_offset_samples);
        return 1;
    }
    
    // Take second sample
    auto t1 = make_timestamp(1, 0);
    coordinator.sample_now(t1);
    
    stats = coordinator.get_statistics();
    
    if (stats.total_offset_samples != 2) {
        std::fprintf(stderr, "Test 3 FAIL: Expected 2 offset samples, got %llu\n",
                    (unsigned long long)stats.total_offset_samples);
        return 2;
    }
    
    // Verify mechanism tracking (default is E2E)
    if (stats.using_p2p_delay) {
        std::fprintf(stderr, "Test 3 FAIL: Should be using E2E by default\n");
        return 3;
    }
    
    if (stats.e2e_measurements != 2) {
        std::fprintf(stderr, "Test 3 FAIL: Expected 2 E2E measurements, got %llu\n",
                    (unsigned long long)stats.e2e_measurements);
        return 4;
    }
    
    std::printf("Test 3 PASS: Statistics collection\n");
    return 0;
}

// Test 4: Health monitoring
static int test_health_monitoring() {
    StateCallbacks callbacks{};
    callbacks.get_timestamp = []() { return Types::Timestamp{}; };
    
    PortConfiguration config{};
    config.port_number = 1;
    
    PtpPort port(config, callbacks);
    port.initialize();
    port.start();
    
    SyncIntegration coordinator(port);
    
    // Enable health monitoring
    SyncIntegration::Configuration sync_config{};
    sync_config.enable_health_monitoring = true;
    coordinator.configure(sync_config);
    coordinator.start();
    
    // Take sample
    auto t0 = make_timestamp(0, 0);
    coordinator.tick(t0);
    
    // Get health status
    auto health = coordinator.get_health_status();
    
    // Should have valid status (not uninitialized)
    if (health.status != SyncHealthStatus::Status::Synchronized &&
        health.status != SyncHealthStatus::Status::Converging &&
        health.status != SyncHealthStatus::Status::Degraded &&
        health.status != SyncHealthStatus::Status::Critical) {
        std::fprintf(stderr, "Test 4 FAIL: Health status not initialized\n");
        return 1;
    }
    
    // After first sample with default port (offset=0), status will be Synchronized
    // This is correct behavior - offset=0 means perfect sync (even if artificial)
    if (health.status != SyncHealthStatus::Status::Synchronized) {
        std::fprintf(stderr, "Test 4 FAIL: Status should be Synchronized with offset=0, got %d\n",
                    static_cast<int>(health.status));
        return 2;
    }
    
    // Should have message
    if (health.message.empty()) {
        std::fprintf(stderr, "Test 4 FAIL: Health message should not be empty\n");
        return 3;
    }
    
    std::printf("Test 4 PASS: Health monitoring\n");
    return 0;
}

// Test 5: Periodic sampling
static int test_periodic_sampling() {
    StateCallbacks callbacks{};
    callbacks.get_timestamp = []() { return Types::Timestamp{}; };
    
    PortConfiguration config{};
    config.port_number = 1;
    
    PtpPort port(config, callbacks);
    port.initialize();
    port.start();
    
    SyncIntegration coordinator(port);
    
    // Configure 1-second sampling interval
    SyncIntegration::Configuration sync_config{};
    sync_config.sampling_interval_ms = 1000;
    coordinator.configure(sync_config);
    coordinator.start();
    
    // Tick at t=0 (should sample)
    auto t0 = make_timestamp(0, 0);
    coordinator.tick(t0);
    
    auto stats = coordinator.get_statistics();
    if (stats.total_offset_samples != 1) {
        std::fprintf(stderr, "Test 5 FAIL: Should have 1 sample at t=0, got %llu\n",
                    (unsigned long long)stats.total_offset_samples);
        return 1;
    }
    
    // Tick at t=0.5s (should NOT sample - interval not elapsed)
    auto t05 = make_timestamp(0, 500'000'000);
    coordinator.tick(t05);
    
    stats = coordinator.get_statistics();
    if (stats.total_offset_samples != 1) {
        std::fprintf(stderr, "Test 5 FAIL: Should still have 1 sample at t=0.5s, got %llu\n",
                    (unsigned long long)stats.total_offset_samples);
        return 2;
    }
    
    // Tick at t=1.0s (should sample - interval elapsed)
    auto t1 = make_timestamp(1, 0);
    coordinator.tick(t1);
    
    stats = coordinator.get_statistics();
    if (stats.total_offset_samples != 2) {
        std::fprintf(stderr, "Test 5 FAIL: Should have 2 samples at t=1s, got %llu\n",
                    (unsigned long long)stats.total_offset_samples);
        return 3;
    }
    
    std::printf("Test 5 PASS: Periodic sampling\n");
    return 0;
}

// Test 6: Variance calculation
static int test_variance_calculation() {
    StateCallbacks callbacks{};
    callbacks.get_timestamp = []() { return Types::Timestamp{}; };
    
    PortConfiguration config{};
    config.port_number = 1;
    
    PtpPort port(config, callbacks);
    port.initialize();
    port.start();
    
    SyncIntegration coordinator(port);
    
    // Configure small variance window
    SyncIntegration::Configuration sync_config{};
    sync_config.variance_window_samples = 5;
    coordinator.configure(sync_config);
    coordinator.start();
    
    // Take multiple samples
    for (int i = 0; i < 10; i++) {
        auto t = make_timestamp(i, 0);
        coordinator.sample_now(t);
    }
    
    auto stats = coordinator.get_statistics();
    
    // Should have calculated variance
    if (stats.offset_variance_ns2 < 0.0) {
        std::fprintf(stderr, "Test 6 FAIL: Variance should be non-negative\n");
        return 1;
    }
    
    // Standard deviation should be calculated
    if (stats.offset_std_dev_ns < 0.0) {
        std::fprintf(stderr, "Test 6 FAIL: Std dev should be non-negative\n");
        return 2;
    }
    
    // Std dev should be sqrt of variance (approximately, given rounding)
    double expected_std_dev = std::sqrt(stats.offset_variance_ns2);
    double diff = std::abs(stats.offset_std_dev_ns - expected_std_dev);
    if (diff > 0.01) {
        std::fprintf(stderr, "Test 6 FAIL: Std dev %f != sqrt(variance) %f\n",
                    stats.offset_std_dev_ns, expected_std_dev);
        return 3;
    }
    
    std::printf("Test 6 PASS: Variance calculation\n");
    return 0;
}

// Test 7: Reset functionality
static int test_reset() {
    StateCallbacks callbacks{};
    callbacks.get_timestamp = []() { return Types::Timestamp{}; };
    
    PortConfiguration config{};
    config.port_number = 1;
    
    PtpPort port(config, callbacks);
    port.initialize();
    port.start();
    
    SyncIntegration coordinator(port);
    coordinator.start();
    
    // Take some samples
    auto t0 = make_timestamp(0, 0);
    coordinator.sample_now(t0);
    auto t1 = make_timestamp(1, 0);
    coordinator.sample_now(t1);
    
    auto stats_before = coordinator.get_statistics();
    if (stats_before.total_offset_samples == 0) {
        std::fprintf(stderr, "Test 7 FAIL: Should have samples before reset\n");
        return 1;
    }
    
    // Reset
    coordinator.reset();
    
    auto stats_after = coordinator.get_statistics();
    
    // All counters should be zero
    if (stats_after.total_offset_samples != 0) {
        std::fprintf(stderr, "Test 7 FAIL: Samples not reset\n");
        return 2;
    }
    
    if (stats_after.total_delay_samples != 0) {
        std::fprintf(stderr, "Test 7 FAIL: Delay samples not reset\n");
        return 3;
    }
    
    std::printf("Test 7 PASS: Reset functionality\n");
    return 0;
}

int main() {
    int failures = 0;
    
    failures += test_coordinator_lifecycle();
    failures += test_configuration_validation();
    failures += test_statistics_collection();
    failures += test_health_monitoring();
    failures += test_periodic_sampling();
    failures += test_variance_calculation();
    failures += test_reset();
    
    if (failures == 0) {
        std::printf("\n✅ All Sync Accuracy Integration tests PASSED (7/7)\n");
        return 0;
    } else {
        std::fprintf(stderr, "\n❌ %d Sync Accuracy Integration test(s) FAILED\n", failures);
        return 1;
    }
}
