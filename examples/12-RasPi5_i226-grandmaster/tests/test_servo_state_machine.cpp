/**
 * @file test_servo_state_machine.cpp
 * @brief Unit tests for ServoStateMachine
 * 
 * Tests state transitions, lock detection, and GPS recovery logic.
 */

#include "../src/servo_state_machine.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>

// Test utilities
static int tests_total = 0;
static int tests_passed = 0;

static void print_test_header(const char* test_name) {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(58) << test_name << "║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
}

static void print_result(bool success) {
    if (success) {
        std::cout << "✅ PASS\n";
    } else {
        std::cout << "❌ FAIL\n";
    }
}

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║      ServoStateMachine Unit Test Suite                    ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    
    // Test 1: Initial State
    {
        print_test_header("TEST 1: Initial State (RECOVERY_GPS)");
        tests_total++;
        
        ServoStateMachine sm;
        
        bool success = (sm.get_state() == ServoState::RECOVERY_GPS) &&
                      sm.is_recovering() &&
                      !sm.is_locked() &&
                      !sm.is_holdover();
        
        if (success) {
            std::cout << "State: RECOVERY_GPS ✓\n";
            std::cout << "is_recovering(): true ✓\n";
            std::cout << "is_locked(): false ✓\n";
            std::cout << "is_holdover(): false ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 2: Recovery to Locked Transition (Good GPS)
    {
        print_test_header("TEST 2: RECOVERY_GPS → LOCKED_GPS");
        tests_total++;
        
        ServoStateMachineConfig config;
        config.recovery_samples = 5;  // Require 5 good samples
        config.phase_lock_threshold_ns = 100;
        config.freq_lock_threshold_ppb = 5.0;
        config.lock_stability_samples = 3;
        
        ServoStateMachine sm(config);
        
        // Feed 5 consecutive good GPS samples
        for (int i = 0; i < 5; i++) {
            sm.update(true, true, 50, 2.0, 1000 + i);  // PPS valid, ToD valid, small errors
        }
        
        bool success = (sm.get_state() == ServoState::LOCKED_GPS) &&
                      sm.is_locked() == false &&  // Not yet stable (need 3 more locked samples)
                      !sm.is_recovering() &&
                      !sm.is_holdover();
        
        if (success) {
            std::cout << "After 5 good samples: LOCKED_GPS ✓\n";
            std::cout << "Not yet stable (need " << config.lock_stability_samples << " locked samples) ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 3: Lock Stability Detection
    {
        print_test_header("TEST 3: Lock Stability Detection");
        tests_total++;
        
        ServoStateMachineConfig config;
        config.recovery_samples = 3;
        config.phase_lock_threshold_ns = 100;
        config.freq_lock_threshold_ppb = 5.0;
        config.lock_stability_samples = 5;  // Need 5 consecutive locked samples
        
        ServoStateMachine sm(config);
        
        // Get to LOCKED_GPS state
        for (int i = 0; i < 3; i++) {
            sm.update(true, true, 50, 2.0, 1000 + i);
        }
        
        assert(sm.get_state() == ServoState::LOCKED_GPS);
        assert(!sm.is_locked());  // Not stable yet
        
        // Feed locked samples (phase < 100ns, freq < 5ppb)
        for (int i = 0; i < 5; i++) {
            sm.update(true, true, 80, 3.0, 1010 + i);  // Within lock thresholds
        }
        
        bool success = sm.is_locked();  // Should be stable now
        
        if (success) {
            std::cout << "After 5 locked samples: is_locked() = true ✓\n";
            tests_passed++;
        } else {
            std::cout << "is_locked() still false (expected true)\n";
        }
        
        print_result(success);
    }
    
    // Test 4: Locked to Holdover Transition (GPS Loss)
    {
        print_test_header("TEST 4: LOCKED_GPS → HOLDOVER_RTC (GPS loss)");
        tests_total++;
        
        ServoStateMachineConfig config;
        config.recovery_samples = 3;
        
        ServoStateMachine sm(config);
        
        // Get to LOCKED_GPS
        for (int i = 0; i < 3; i++) {
            sm.update(true, true, 50, 2.0, 1000 + i);
        }
        
        assert(sm.get_state() == ServoState::LOCKED_GPS);
        
        // Simulate GPS loss (PPS dropout)
        sm.update(false, true, 0, 0, 1010);  // PPS invalid
        
        bool success = (sm.get_state() == ServoState::HOLDOVER_RTC) &&
                      sm.is_holdover() &&
                      !sm.is_locked() &&
                      !sm.is_recovering();
        
        if (success) {
            std::cout << "After PPS dropout: HOLDOVER_RTC ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 5: Holdover to Recovery Transition (GPS Return)
    {
        print_test_header("TEST 5: HOLDOVER_RTC → RECOVERY_GPS (GPS return)");
        tests_total++;
        
        ServoStateMachineConfig config;
        config.recovery_samples = 3;
        
        ServoStateMachine sm(config);
        
        // Get to LOCKED_GPS then HOLDOVER_RTC
        for (int i = 0; i < 3; i++) {
            sm.update(true, true, 50, 2.0, 1000 + i);
        }
        sm.update(false, true, 0, 0, 1010);  // GPS loss
        
        assert(sm.get_state() == ServoState::HOLDOVER_RTC);
        
        // GPS returns
        sm.update(true, true, 200, 10.0, 1020);  // GPS back (but not yet stable)
        
        bool success = (sm.get_state() == ServoState::RECOVERY_GPS) &&
                      sm.is_recovering() &&
                      !sm.is_locked() &&
                      !sm.is_holdover();
        
        if (success) {
            std::cout << "After GPS return: RECOVERY_GPS ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 6: Recovery Counter Reset on Bad Sample
    {
        print_test_header("TEST 6: Recovery Counter Reset");
        tests_total++;
        
        ServoStateMachineConfig config;
        config.recovery_samples = 5;
        
        ServoStateMachine sm(config);
        
        // Feed 4 good samples (almost ready for lock)
        for (int i = 0; i < 4; i++) {
            sm.update(true, true, 50, 2.0, 1000 + i);
        }
        
        assert(sm.get_state() == ServoState::RECOVERY_GPS);
        
        // One bad sample should reset counter
        sm.update(false, true, 0, 0, 1010);  // PPS dropout
        
        assert(sm.get_state() == ServoState::RECOVERY_GPS);
        
        // Need 5 more good samples now
        for (int i = 0; i < 4; i++) {
            sm.update(true, true, 50, 2.0, 1020 + i);
        }
        
        // Should still be in RECOVERY (need 1 more)
        bool success = (sm.get_state() == ServoState::RECOVERY_GPS);
        
        // One more good sample should trigger transition
        sm.update(true, true, 50, 2.0, 1025);
        
        success = success && (sm.get_state() == ServoState::LOCKED_GPS);
        
        if (success) {
            std::cout << "Counter reset after bad sample ✓\n";
            std::cout << "Required full 5 samples after reset ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 7: Lock Stability Lost
    {
        print_test_header("TEST 7: Lock Stability Lost");
        tests_total++;
        
        ServoStateMachineConfig config;
        config.recovery_samples = 3;
        config.phase_lock_threshold_ns = 100;
        config.freq_lock_threshold_ppb = 5.0;
        config.lock_stability_samples = 3;
        
        ServoStateMachine sm(config);
        
        // Get to stable lock
        for (int i = 0; i < 3; i++) {
            sm.update(true, true, 50, 2.0, 1000 + i);
        }
        for (int i = 0; i < 3; i++) {
            sm.update(true, true, 80, 3.0, 1010 + i);
        }
        
        assert(sm.is_locked());
        
        // Feed sample with large phase error (exceeds threshold)
        sm.update(true, true, 150, 3.0, 1020);  // Phase = 150ns > 100ns threshold
        
        bool success = !sm.is_locked() &&  // Should lose lock stability
                      (sm.get_state() == ServoState::LOCKED_GPS);  // Still in LOCKED state
        
        if (success) {
            std::cout << "After large phase error: is_locked() = false ✓\n";
            std::cout << "Still in LOCKED_GPS state (not holdover yet) ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 8: Reset Functionality
    {
        print_test_header("TEST 8: Reset Functionality");
        tests_total++;
        
        ServoStateMachine sm;
        
        // Get to some state
        for (int i = 0; i < 10; i++) {
            sm.update(true, true, 50, 2.0, 1000 + i);
        }
        
        assert(sm.get_state() == ServoState::LOCKED_GPS);
        
        // Reset
        sm.reset();
        
        bool success = (sm.get_state() == ServoState::RECOVERY_GPS) &&
                      sm.is_recovering() &&
                      !sm.is_locked() &&
                      !sm.is_holdover();
        
        if (success) {
            std::cout << "After reset: Back to RECOVERY_GPS ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 9: State Duration Tracking
    {
        print_test_header("TEST 9: State Duration Tracking");
        tests_total++;
        
        ServoStateMachine sm;
        
        // Initial state at t=1000
        sm.update(true, true, 50, 2.0, 1000);
        
        // Check duration at t=1005 (should be 5 seconds)
        uint64_t duration = sm.get_time_in_state(1005);
        
        bool success = (duration == 5);
        
        if (success) {
            std::cout << "Time in RECOVERY_GPS: " << duration << " seconds ✓\n";
            tests_passed++;
        } else {
            std::cout << "Time in state: " << duration << " (expected 5)\n";
        }
        
        print_result(success);
    }
    
    // Test 10: Full Cycle (Recovery → Locked → Holdover → Recovery → Locked)
    {
        print_test_header("TEST 10: Full State Cycle");
        tests_total++;
        
        ServoStateMachineConfig config;
        config.recovery_samples = 3;
        config.lock_stability_samples = 2;
        
        ServoStateMachine sm(config);
        
        // 1. RECOVERY_GPS → LOCKED_GPS
        for (int i = 0; i < 3; i++) {
            sm.update(true, true, 50, 2.0, 1000 + i);
        }
        assert(sm.get_state() == ServoState::LOCKED_GPS);
        
        // Get stable lock
        for (int i = 0; i < 2; i++) {
            sm.update(true, true, 80, 3.0, 1010 + i);
        }
        assert(sm.is_locked());
        
        // 2. LOCKED_GPS → HOLDOVER_RTC (GPS loss)
        sm.update(false, true, 0, 0, 1020);
        assert(sm.get_state() == ServoState::HOLDOVER_RTC);
        
        // 3. HOLDOVER_RTC → RECOVERY_GPS (GPS return)
        sm.update(true, true, 100, 5.0, 1030);
        assert(sm.get_state() == ServoState::RECOVERY_GPS);
        
        // 4. RECOVERY_GPS → LOCKED_GPS (stabilization)
        for (int i = 0; i < 3; i++) {
            sm.update(true, true, 50, 2.0, 1040 + i);
        }
        
        bool success = (sm.get_state() == ServoState::LOCKED_GPS);
        
        if (success) {
            std::cout << "Full cycle completed successfully:\n";
            std::cout << "  RECOVERY → LOCKED → HOLDOVER → RECOVERY → LOCKED ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Summary
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║                   TEST SUMMARY                            ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Passed: " << tests_passed << "/" << tests_total << std::string(52 - std::to_string(tests_passed).length() - std::to_string(tests_total).length(), ' ') << "║\n";
    
    if (tests_passed == tests_total) {
        std::cout << "║ Result: ✅ ALL TESTS PASSED                               ║\n";
    } else {
        std::cout << "║ Result: ❌ SOME TESTS FAILED                              ║\n";
    }
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    return (tests_passed == tests_total) ? 0 : 1;
}
