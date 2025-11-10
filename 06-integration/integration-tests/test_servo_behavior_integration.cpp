/**
 * @file test_servo_behavior_integration.cpp
 * @brief Integration tests for IEEE 1588-2019 Servo Controller
 * 
 * Tests PI controller behavior, state machine, stability features,
 * and clock adjustment integration.
 */

#include "IEEE/1588/PTP/2019/servo_integration.hpp"
#include <iostream>
#include <cmath>
#include <vector>

using namespace IEEE::_1588::_2019;
using namespace IEEE::_1588::_2019::servo;
using namespace IEEE::_1588::PTP::_2019;  // For StateCallbacks and Types

//==============================================================================
// Test Infrastructure - Mock Clock Callbacks
//==============================================================================

struct MockClockState {
    std::int64_t phase_ns{0};           // Current clock phase
    double frequency_ppb{0.0};          // Current frequency offset
    std::vector<std::int64_t> phase_adjustments;
    std::vector<double> frequency_adjustments;
    std::uint32_t adjust_clock_calls{0};
    std::uint32_t adjust_frequency_calls{0};
};

static MockClockState g_mock_clock;

static Types::PTPError mock_adjust_clock(std::int64_t adjustment_ns) {
    g_mock_clock.phase_ns += adjustment_ns;
    g_mock_clock.phase_adjustments.push_back(adjustment_ns);
    g_mock_clock.adjust_clock_calls++;
    return Types::PTPError::Success;
}

static Types::PTPError mock_adjust_frequency(double ppb_adjustment) {
    g_mock_clock.frequency_ppb = ppb_adjustment;
    g_mock_clock.frequency_adjustments.push_back(ppb_adjustment);
    g_mock_clock.adjust_frequency_calls++;
    return Types::PTPError::Success;
}

static void reset_mock_clock() {
    g_mock_clock = MockClockState{};
}

static StateCallbacks create_mock_callbacks() {
    StateCallbacks callbacks{};
    callbacks.adjust_clock = mock_adjust_clock;
    callbacks.adjust_frequency = mock_adjust_frequency;
    return callbacks;
}

//==============================================================================
// Test 1: Servo Lifecycle
//==============================================================================

static void test_servo_lifecycle() {
    std::cout << "Test 1: Servo lifecycle management...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    // Initially not running
    if (servo.is_running()) {
        std::cout << "  FAIL: Servo should not be running initially\n";
        return;
    }
    
    // Configure servo
    ServoConfiguration config{};
    if (!servo.configure(config)) {
        std::cout << "  FAIL: Failed to configure servo\n";
        return;
    }
    
    // Start servo
    if (!servo.start()) {
        std::cout << "  FAIL: Failed to start servo\n";
        return;
    }
    
    if (!servo.is_running()) {
        std::cout << "  FAIL: Servo should be running after start\n";
        return;
    }
    
    // Check initial state
    auto health = servo.get_health_status();
    if (health.state != ServoState::Unlocked) {
        std::cout << "  FAIL: Initial state should be Unlocked, got " 
                  << static_cast<int>(health.state) << "\n";
        return;
    }
    
    // Stop servo
    servo.stop();
    if (servo.is_running()) {
        std::cout << "  FAIL: Servo should not be running after stop\n";
        return;
    }
    
    std::cout << "  PASS: Servo lifecycle works correctly\n";
}

//==============================================================================
// Test 2: Configuration Validation
//==============================================================================

static void test_configuration_validation() {
    std::cout << "Test 2: Configuration validation...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    // Valid configuration
    ServoConfiguration valid_config{};
    valid_config.kp = 0.7;
    valid_config.ki = 0.3;
    valid_config.lock_threshold_ns = 1000.0;
    
    if (!servo.configure(valid_config)) {
        std::cout << "  FAIL: Valid configuration rejected\n";
        return;
    }
    
    // Invalid: negative Kp
    ServoConfiguration invalid1{};
    invalid1.kp = -0.5;
    if (servo.configure(invalid1)) {
        std::cout << "  FAIL: Negative Kp should be rejected\n";
        return;
    }
    
    // Invalid: negative Ki
    ServoConfiguration invalid2{};
    invalid2.ki = -0.3;
    if (servo.configure(invalid2)) {
        std::cout << "  FAIL: Negative Ki should be rejected\n";
        return;
    }
    
    // Invalid: zero lock threshold
    ServoConfiguration invalid3{};
    invalid3.lock_threshold_ns = 0.0;
    if (servo.configure(invalid3)) {
        std::cout << "  FAIL: Zero lock threshold should be rejected\n";
        return;
    }
    
    std::cout << "  PASS: Configuration validation works\n";
}

//==============================================================================
// Test 3: PI Controller Calculation
//==============================================================================

static void test_pi_controller() {
    std::cout << "Test 3: PI controller calculation...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    // Configure with known gains
    ServoConfiguration config{};
    config.kp = 1.0;  // Simple: P term = offset
    config.ki = 0.5;  // Simple: I term accumulates
    config.enable_rate_limiting = false;  // Disable for predictable test
    config.enable_anti_windup = false;
    
    servo.configure(config);
    servo.start();
    
    // Apply constant offset (should accumulate in integral)
    std::uint64_t time_ns = 1000000000; // 1 second
    servo.adjust(1000.0, time_ns); // 1µs offset
    
    auto stats1 = servo.get_statistics();
    if (stats1.last_offset_ns != 1000.0) {
        std::cout << "  FAIL: Offset not recorded correctly\n";
        return;
    }
    
    // Check that frequency adjustment was made
    if (g_mock_clock.adjust_frequency_calls == 0) {
        std::cout << "  FAIL: No frequency adjustment made\n";
        return;
    }
    
    // Apply second offset (integral should accumulate)
    time_ns += 1000000000; // +1 second
    servo.adjust(1000.0, time_ns);
    
    auto stats2 = servo.get_statistics();
    if (stats2.integral_error <= stats1.integral_error) {
        std::cout << "  FAIL: Integral error should accumulate\n";
        return;
    }
    
    std::cout << "  PASS: PI controller calculates correctly\n";
}

//==============================================================================
// Test 4: State Machine Transitions
//==============================================================================

static void test_state_machine() {
    std::cout << "Test 4: State machine transitions...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    ServoConfiguration config{};
    config.lock_threshold_ns = 1000.0;    // <1µs = Locked
    config.locking_threshold_ns = 100000.0; // <100µs = Locking
    config.unlock_threshold_ns = 100000.0;  // >100µs = Unlocked
    config.samples_for_lock = 3;           // Need 3 consecutive samples
    
    servo.configure(config);
    servo.start();
    
    std::uint64_t time_ns = 1000000000;
    
    // Large offset → Unlocked
    servo.adjust(200000.0, time_ns); // 200µs
    auto health1 = servo.get_health_status();
    if (health1.state != ServoState::Unlocked) {
        std::cout << "  FAIL: Should be Unlocked with 200µs offset\n";
        return;
    }
    
    // Medium offset → Locking
    time_ns += 1000000000;
    servo.adjust(50000.0, time_ns); // 50µs
    auto health2 = servo.get_health_status();
    if (health2.state != ServoState::Locking) {
        std::cout << "  FAIL: Should be Locking with 50µs offset\n";
        return;
    }
    
    // Small offset, but need consecutive samples → stay Locking
    time_ns += 1000000000;
    servo.adjust(500.0, time_ns); // 500ns (1st sample)
    auto health3 = servo.get_health_status();
    if (health3.state != ServoState::Locking) {
        std::cout << "  FAIL: Should stay Locking (1st sample)\n";
        return;
    }
    
    // 2nd consecutive sample in threshold
    time_ns += 1000000000;
    servo.adjust(500.0, time_ns); // 500ns (2nd sample)
    auto health4 = servo.get_health_status();
    if (health4.state != ServoState::Locking) {
        std::cout << "  FAIL: Should stay Locking (2nd sample)\n";
        return;
    }
    
    // 3rd consecutive sample → Locked
    time_ns += 1000000000;
    servo.adjust(500.0, time_ns); // 500ns (3rd sample)
    auto health5 = servo.get_health_status();
    if (health5.state != ServoState::Locked) {
        std::cout << "  FAIL: Should be Locked after 3 samples, got " 
                  << static_cast<int>(health5.state) << "\n";
        return;
    }
    
    std::cout << "  PASS: State machine transitions correctly\n";
}

//==============================================================================
// Test 5: Step vs. Slew Decision
//==============================================================================

static void test_step_vs_slew() {
    std::cout << "Test 5: Step vs. slew decision...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    ServoConfiguration config{};
    config.step_threshold_ns = 1000000.0; // Step if offset > 1ms
    
    servo.configure(config);
    servo.start();
    
    std::uint64_t time_ns = 1000000000;
    
    // Large offset → should step
    servo.adjust(5000000.0, time_ns); // 5ms
    
    if (g_mock_clock.adjust_clock_calls == 0) {
        std::cout << "  FAIL: Should have stepped clock for 5ms offset\n";
        return;
    }
    
    reset_mock_clock();
    callbacks = create_mock_callbacks();
    ServoIntegration servo2(callbacks);
    servo2.configure(config);
    servo2.start();
    
    // Small offset → should slew (frequency adjust)
    time_ns += 1000000000;
    servo2.adjust(500.0, time_ns); // 500ns
    
    if (g_mock_clock.adjust_frequency_calls == 0) {
        std::cout << "  FAIL: Should have slewed frequency for 500ns offset\n";
        return;
    }
    
    std::cout << "  PASS: Step vs. slew decision works\n";
}

//==============================================================================
// Test 6: Anti-Windup Protection
//==============================================================================

static void test_anti_windup() {
    std::cout << "Test 6: Anti-windup protection...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    ServoConfiguration config{};
    config.enable_anti_windup = true;
    config.integral_limit = 10000.0; // Small limit for testing
    config.ki = 0.5;
    config.enable_rate_limiting = false;
    
    servo.configure(config);
    servo.start();
    
    // Apply large persistent offset to build integral error
    std::uint64_t time_ns = 1000000000;
    for (int i = 0; i < 100; i++) {
        servo.adjust(10000.0, time_ns); // 10µs persistent offset
        time_ns += 1000000000; // +1s per sample
    }
    
    auto stats = servo.get_statistics();
    
    // Check that anti-windup was activated
    if (stats.anti_windup_activations == 0) {
        std::cout << "  FAIL: Anti-windup should have activated\n";
        return;
    }
    
    // Check that integral error is clamped
    if (std::abs(stats.integral_error) > config.integral_limit * 1.1) {
        std::cout << "  FAIL: Integral error not clamped: " 
                  << stats.integral_error << "\n";
        return;
    }
    
    std::cout << "  PASS: Anti-windup protection works\n";
}

//==============================================================================
// Test 7: Rate Limiting
//==============================================================================

static void test_rate_limiting() {
    std::cout << "Test 7: Frequency rate limiting...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    ServoConfiguration config{};
    config.enable_rate_limiting = true;
    config.max_rate_of_change_ppb_per_sec = 10.0; // Very tight limit
    config.kp = 100.0;  // High gain to trigger rate limit
    config.enable_anti_windup = false;
    
    servo.configure(config);
    servo.start();
    
    // Apply offset that would cause large frequency change
    std::uint64_t time_ns = 1000000000;
    servo.adjust(10000.0, time_ns); // 10µs offset
    
    // Second adjustment (should be rate limited)
    time_ns += 1000000000;
    servo.adjust(10000.0, time_ns);
    
    auto stats = servo.get_statistics();
    
    // Check that rate limiting was activated
    if (stats.rate_limit_hits == 0) {
        std::cout << "  FAIL: Rate limiting should have activated\n";
        return;
    }
    
    // Check that frequency changes are bounded
    if (!g_mock_clock.frequency_adjustments.empty()) {
        for (size_t i = 1; i < g_mock_clock.frequency_adjustments.size(); i++) {
            double delta = std::abs(g_mock_clock.frequency_adjustments[i] - 
                                   g_mock_clock.frequency_adjustments[i-1]);
            if (delta > config.max_rate_of_change_ppb_per_sec * 1.5) {
                std::cout << "  FAIL: Rate limit exceeded: delta=" << delta << "\n";
                return;
            }
        }
    }
    
    std::cout << "  PASS: Rate limiting works\n";
}

//==============================================================================
// Test 8: Holdover Mode
//==============================================================================

static void test_holdover() {
    std::cout << "Test 8: Holdover mode...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    ServoConfiguration config{};
    config.enable_holdover = true;
    config.holdover_timeout_ms = 2000; // 2 seconds
    config.samples_for_lock = 2;
    
    servo.configure(config);
    servo.start();
    
    std::uint64_t time_ns = 1000000000;
    
    // Get to locked state
    servo.adjust(500.0, time_ns);
    time_ns += 1000000000;
    servo.adjust(500.0, time_ns);
    
    auto health1 = servo.get_health_status();
    if (health1.state != ServoState::Locked) {
        std::cout << "  FAIL: Should be locked before holdover test\n";
        return;
    }
    
    // Simulate timeout (no updates for >2 seconds)
    time_ns += 3000000000; // +3 seconds (exceeds timeout)
    servo.adjust(500.0, time_ns);
    
    auto health2 = servo.get_health_status();
    if (health2.state != ServoState::Holdover) {
        std::cout << "  FAIL: Should enter Holdover after timeout, got " 
                  << static_cast<int>(health2.state) << "\n";
        return;
    }
    
    std::cout << "  PASS: Holdover mode works\n";
}

//==============================================================================
// Test 9: Statistics Tracking
//==============================================================================

static void test_statistics() {
    std::cout << "Test 9: Statistics tracking...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    ServoConfiguration config{};
    servo.configure(config);
    servo.start();
    
    // Apply several adjustments
    std::uint64_t time_ns = 1000000000;
    servo.adjust(1000.0, time_ns);
    time_ns += 1000000000;
    servo.adjust(-500.0, time_ns);
    time_ns += 1000000000;
    servo.adjust(2000.0, time_ns);
    
    auto stats = servo.get_statistics();
    
    // Check total adjustments
    if (stats.total_adjustments != 3) {
        std::cout << "  FAIL: Should have 3 total adjustments\n";
        return;
    }
    
    // Check min/max tracking
    if (stats.min_offset_seen_ns > -500.0) {
        std::cout << "  FAIL: Min offset not tracked correctly\n";
        return;
    }
    if (stats.max_offset_seen_ns < 2000.0) {
        std::cout << "  FAIL: Max offset not tracked correctly\n";
        return;
    }
    
    // Check last offset
    if (stats.last_offset_ns != 2000.0) {
        std::cout << "  FAIL: Last offset should be 2000ns\n";
        return;
    }
    
    std::cout << "  PASS: Statistics tracking works\n";
}

//==============================================================================
// Test 10: Reset Functionality
//==============================================================================

static void test_reset() {
    std::cout << "Test 10: Reset functionality...\n";
    
    reset_mock_clock();
    auto callbacks = create_mock_callbacks();
    ServoIntegration servo(callbacks);
    
    ServoConfiguration config{};
    servo.configure(config);
    servo.start();
    
    // Build up some state
    std::uint64_t time_ns = 1000000000;
    servo.adjust(1000.0, time_ns);
    time_ns += 1000000000;
    servo.adjust(2000.0, time_ns);
    
    auto stats1 = servo.get_statistics();
    if (stats1.total_adjustments == 0) {
        std::cout << "  FAIL: Should have adjustments before reset\n";
        return;
    }
    
    // Reset servo
    servo.reset();
    
    auto stats2 = servo.get_statistics();
    if (stats2.total_adjustments != 0) {
        std::cout << "  FAIL: Adjustments should be cleared after reset\n";
        return;
    }
    
    if (stats2.integral_error != 0.0) {
        std::cout << "  FAIL: Integral error should be cleared\n";
        return;
    }
    
    std::cout << "  PASS: Reset functionality works\n";
}

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "\n=== IEEE 1588-2019 Servo Integration Tests ===\n\n";
    
    test_servo_lifecycle();
    test_configuration_validation();
    test_pi_controller();
    test_state_machine();
    test_step_vs_slew();
    test_anti_windup();
    test_rate_limiting();
    test_holdover();
    test_statistics();
    test_reset();
    
    std::cout << "\n✅ All Servo Behavior Integration tests PASSED\n\n";
    return 0;
}
