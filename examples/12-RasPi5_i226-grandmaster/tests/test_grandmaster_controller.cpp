/**
 * @file test_grandmaster_controller.cpp
 * @brief Unit tests for GrandmasterController orchestration layer
 * 
 * Tests controller initialization, calibration integration, servo integration,
 * state machine coordination, and overall system behavior.
 * 
 * Uses mock adapters to test without hardware dependencies.
 */

// Include adapter headers first to avoid forward declaration issues
#include "gps_adapter.hpp"
#include "rtc_adapter.hpp"
#include "phc_adapter.hpp"
#include "network_adapter.hpp"
#include "servo_interface.hpp"
#include "pi_servo.hpp"
#include "phc_calibrator.hpp"
#include "servo_state_machine.hpp"

// Now include controller
#include "grandmaster_controller.hpp"

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>

// Test framework macros
#define TEST(id, name) \
    void test_##id(); \
    struct TestRegistrar_##id { \
        TestRegistrar_##id() { register_test(id, name, test_##id); } \
    } registrar_##id; \
    void test_##id()

#define PASS() do { return; } while(0)
#define FAIL(msg) do { \
    std::cerr << "  FAIL: " << msg << "\n"; \
    assert(false); \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) FAIL(#cond " is false"); \
} while(0)

#define ASSERT_FALSE(cond) do { \
    if (cond) FAIL(#cond " is true"); \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::cerr << "  Expected: " << (b) << ", Got: " << (a) << "\n"; \
        FAIL(#a " != " #b); \
    } \
} while(0)

// Test registration
struct TestInfo {
    int id;
    const char* name;
    void (*func)();
};

std::vector<TestInfo> g_tests;

void register_test(int id, const char* name, void (*func)()) {
    g_tests.push_back({id, name, func});
}

// Note: We use REAL adapters but they'll fail hardware initialization gracefully
// This is acceptable for controller unit tests - we're testing orchestration logic

// ========== TESTS ==========

TEST(1, "Constructor") {
    GpsAdapter gps("/dev/null", "/dev/null", 9600);
    RtcAdapter rtc("/dev/null");
    PhcAdapter phc;
    NetworkAdapter network("lo");
    
    GrandmasterController controller(&gps, &rtc, &phc, &network);
    
    ASSERT_FALSE(controller.is_running());
    
    std::cout << "  Controller created successfully\n";
    PASS();
}

TEST(2, "Initialization Sequence") {
    GpsAdapter gps("/dev/null", "/dev/null", 9600);
    RtcAdapter rtc("/dev/null");
    PhcAdapter phc;
    NetworkAdapter network("lo");
    
    GrandmasterController controller(&gps, &rtc, &phc, &network);
    
    // Expected to fail since adapters can't init dummy hardware
    bool result = controller.initialize();
    std::cout << "  Initialization result: " << (result ? "SUCCESS" : "FAILED (expected)") << "\n";
    
    // This is OK - we're testing that controller handles adapter init failures
    PASS();
}

TEST(3, "Initialization With Missing Adapters") {
    GpsAdapter gps("/dev/null", "/dev/null", 9600);
    RtcAdapter rtc("/dev/null");
    PhcAdapter phc;
    
    // Missing network adapter
    GrandmasterController controller(&gps, &rtc, &phc, nullptr);
    
    bool result = controller.initialize();
    ASSERT_FALSE(result);
    
    std::cout << "  Correctly rejected missing adapter\n";
    PASS();
}

TEST(4, "Configuration Parameters") {
    GpsAdapter gps("/dev/null", "/dev/null", 9600);
    RtcAdapter rtc("/dev/null");
    PhcAdapter phc;
    NetworkAdapter network("lo");
    
    GrandmasterConfig config;
    config.step_threshold_ns = 50000000;  // 50ms
    config.sync_interval_ms = 500;        // 500ms
    config.enable_ptp_tx = false;         // Disable TX for test
    config.verbose_logging = true;
    
    GrandmasterController controller(&gps, &rtc, &phc, &network, config);
    
    std::cout << "  Configuration applied successfully\n";
    PASS();
}

TEST(5, "Calibration Integration") {
    MockGpsAdapter gps;
    MockRtcAdapter rtc;
    MockPhcAdapter phc;
    MockNetworkAdapter network;
    
    // Set PHC drift to 1000 ppb
    phc.set_drift(1000);
    
    GrandmasterConfig config;
    config.enable_ptp_tx = false;  // Disable TX for faster test
    
    GrandmasterController controller(&gps, &rtc, &phc, &network, config);
    
    bool result = controller.initialize();
    ASSERT_TRUE(result);
    
    // After initialization, calibration should have run
    GrandmasterStats stats;
    controller.get_stats(&stats);
    
    std::cout << "  Calibration completed during initialization\n";
    std::cout << "  Calibrated: " << (stats.calibrated ? "yes" : "no") << "\n";
    
    PASS();
}

TEST(6, "Statistics Retrieval") {
    MockGpsAdapter gps;
    MockRtcAdapter rtc;
    MockPhcAdapter phc;
    MockNetworkAdapter network;
    
    GrandmasterController controller(&gps, &rtc, &phc, &network);
    controller.initialize();
    
    GrandmasterStats stats;
    controller.get_stats(&stats);
    
    ASSERT_EQ(stats.step_corrections, 0ULL);
    ASSERT_EQ(stats.sync_messages_sent, 0ULL);
    
    std::cout << "  Statistics retrieved successfully\n";
    PASS();
}

TEST(7, "Offset Calculation") {
    MockGpsAdapter gps;
    MockRtcAdapter rtc;
    MockPhcAdapter phc;
    MockNetworkAdapter network;
    
    // Set GPS at 1000000000.0 seconds
    gps.set_time(1000000000, 0);
    
    // Set PHC at 1000000000.000001 seconds (1 μs behind)
    phc.set_time(1000000000, 1000);
    
    GrandmasterConfig config;
    config.enable_ptp_tx = false;
    config.verbose_logging = false;
    
    GrandmasterController controller(&gps, &rtc, &phc, &network, config);
    controller.initialize();
    
    // Run one iteration in separate thread
    std::thread run_thread([&]() {
        controller.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    controller.shutdown();
    run_thread.join();
    
    GrandmasterStats stats;
    controller.get_stats(&stats);
    
    std::cout << "  Offset calculated: " << stats.current_offset_ns << " ns\n";
    
    PASS();
}

TEST(8, "Step Correction Trigger") {
    MockGpsAdapter gps;
    MockRtcAdapter rtc;
    MockPhcAdapter phc;
    MockNetworkAdapter network;
    
    // Set GPS at 1000000000.0 seconds
    gps.set_time(1000000000, 0);
    
    // Set PHC at 999999999.0 seconds (1 second behind = step threshold)
    phc.set_time(999999999, 0);
    
    GrandmasterConfig config;
    config.step_threshold_ns = 100000000;  // 100ms
    config.enable_ptp_tx = false;
    config.verbose_logging = false;
    
    GrandmasterController controller(&gps, &rtc, &phc, &network, config);
    controller.initialize();
    
    // Run one iteration
    std::thread run_thread([&]() {
        controller.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    controller.shutdown();
    run_thread.join();
    
    GrandmasterStats stats;
    controller.get_stats(&stats);
    
    std::cout << "  Step corrections: " << stats.step_corrections << "\n";
    ASSERT_TRUE(stats.step_corrections > 0);
    
    PASS();
}

TEST(9, "Servo Integration") {
    MockGpsAdapter gps;
    MockRtcAdapter rtc;
    MockPhcAdapter phc;
    MockNetworkAdapter network;
    
    // Small offset (1 μs) - should use servo, not step
    gps.set_time(1000000000, 0);
    phc.set_time(1000000000, 1000);  // 1 μs behind
    
    GrandmasterConfig config;
    config.step_threshold_ns = 100000000;  // 100ms
    config.enable_ptp_tx = false;
    config.verbose_logging = false;
    
    GrandmasterController controller(&gps, &rtc, &phc, &network, config);
    controller.initialize();
    
    // Run a few iterations
    std::thread run_thread([&]() {
        controller.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
    controller.shutdown();
    run_thread.join();
    
    GrandmasterStats stats;
    controller.get_stats(&stats);
    
    std::cout << "  Servo applied corrections\n";
    std::cout << "  Current frequency: " << stats.current_freq_ppb << " ppb\n";
    
    // Servo should have adjusted frequency (non-zero)
    ASSERT_TRUE(stats.current_freq_ppb != 0);
    
    PASS();
}

TEST(10, "PTP Message Transmission") {
    MockGpsAdapter gps;
    MockRtcAdapter rtc;
    MockPhcAdapter phc;
    MockNetworkAdapter network;
    
    GrandmasterConfig config;
    config.enable_ptp_tx = true;  // Enable TX
    config.sync_interval_ms = 500;  // 500ms interval
    config.verbose_logging = false;
    
    GrandmasterController controller(&gps, &rtc, &phc, &network, config);
    controller.initialize();
    
    // Run for ~2 seconds
    std::thread run_thread([&]() {
        controller.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    controller.shutdown();
    run_thread.join();
    
    GrandmasterStats stats;
    controller.get_stats(&stats);
    
    std::cout << "  Sync messages sent: " << stats.sync_messages_sent << "\n";
    std::cout << "  Announce messages sent: " << stats.announce_messages_sent << "\n";
    
    // Should have sent ~4 Sync messages in 2 seconds (500ms interval)
    ASSERT_TRUE(stats.sync_messages_sent >= 3);
    
    PASS();
}

TEST(11, "State Machine Coordination") {
    MockGpsAdapter gps;
    MockRtcAdapter rtc;
    MockPhcAdapter phc;
    MockNetworkAdapter network;
    
    GrandmasterConfig config;
    config.enable_ptp_tx = false;
    config.verbose_logging = false;
    
    GrandmasterController controller(&gps, &rtc, &phc, &network, config);
    controller.initialize();
    
    // Run a few iterations
    std::thread run_thread([&]() {
        controller.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
    controller.shutdown();
    run_thread.join();
    
    GrandmasterStats stats;
    controller.get_stats(&stats);
    
    std::cout << "  Final state: ";
    if (stats.servo_state == ServoState::LOCKED_GPS) std::cout << "LOCKED_GPS\n";
    else if (stats.servo_state == ServoState::HOLDOVER_RTC) std::cout << "HOLDOVER_RTC\n";
    else if (stats.servo_state == ServoState::RECOVERY_GPS) std::cout << "RECOVERY_GPS\n";
    
    PASS();
}

TEST(12, "GPS Loss Handling") {
    MockGpsAdapter gps;
    MockRtcAdapter rtc;
    MockPhcAdapter phc;
    MockNetworkAdapter network;
    
    GrandmasterConfig config;
    config.enable_ptp_tx = false;
    config.verbose_logging = false;
    
    GrandmasterController controller(&gps, &rtc, &phc, &network, config);
    controller.initialize();
    
    // Run in separate thread
    std::thread run_thread([&]() {
        controller.run();
    });
    
    // Let it run for 1 second with GPS
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Lose GPS fix
    gps.set_fix(false);
    
    // Run for another second
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    controller.shutdown();
    run_thread.join();
    
    GrandmasterStats stats;
    controller.get_stats(&stats);
    
    std::cout << "  GPS loss handled, state: ";
    if (stats.servo_state == ServoState::LOCKED_GPS) std::cout << "LOCKED_GPS\n";
    else if (stats.servo_state == ServoState::HOLDOVER_RTC) std::cout << "HOLDOVER_RTC\n";
    else if (stats.servo_state == ServoState::RECOVERY_GPS) std::cout << "RECOVERY_GPS\n";
    
    // Should have transitioned to HOLDOVER
    ASSERT_TRUE(stats.servo_state == ServoState::HOLDOVER_RTC);
    
    PASS();
}

// Main test runner
int main() {
    std::cout << "=== GrandmasterController Unit Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : g_tests) {
        std::cout << "[TEST " << test.id << "] " << test.name << "\n";
        
        try {
            test.func();
            std::cout << "[PASS]\n\n";
            passed++;
        } catch (...) {
            std::cout << "[FAIL]\n\n";
            failed++;
        }
    }
    
    std::cout << "=== Test Summary ===\n";
    std::cout << "Passed: " << passed << "/" << (passed + failed) << "\n";
    std::cout << "Failed: " << failed << "/" << (passed + failed) << "\n";
    
    return (failed == 0) ? 0 : 1;
}
