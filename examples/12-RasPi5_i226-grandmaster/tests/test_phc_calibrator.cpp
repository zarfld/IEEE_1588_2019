/**
 * @file test_phc_calibrator.cpp
 * @brief Unit tests for PhcCalibrator
 * 
 * Tests calibration algorithm with synthetic PHC drift scenarios.
 * Uses mock PHC adapter to verify calibration logic without hardware.
 */

#include "../src/phc_calibrator.hpp"
#include "../src/phc_adapter.hpp"
#include "../src/gps_adapter.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

// ============================================================================
// Mock PHC Adapter for Testing
// ============================================================================

class MockPhcAdapter : public PhcAdapter {
public:
    MockPhcAdapter() 
        : simulated_freq_ppb_(0)
        , applied_freq_ppb_(0)
        , adjust_count_(0)
    {}
    
    bool initialize(const char* interface) override {
        (void)interface;
        return true;
    }
    
    bool get_time(uint64_t* sec, uint32_t* nsec) override {
        (void)sec;
        (void)nsec;
        return true;
    }
    
    bool set_time(uint64_t sec, uint32_t nsec) override {
        (void)sec;
        (void)nsec;
        return true;
    }
    
    bool adjust_frequency(int32_t ppb) override {
        applied_freq_ppb_ = ppb;
        adjust_count_++;
        return true;
    }
    
    // Test helpers
    void set_simulated_frequency(int32_t ppb) {
        simulated_freq_ppb_ = ppb;
    }
    
    int32_t get_applied_frequency() const {
        return applied_freq_ppb_;
    }
    
    int get_adjust_count() const {
        return adjust_count_;
    }
    
    void reset_counters() {
        adjust_count_ = 0;
    }
    
    // Simulate PHC time with drift
    // phc_time = reference_time + drift_effect
    int64_t simulate_phc_time(int64_t reference_ns) const {
        // Apply simulated frequency offset
        // drift_ns = (reference_ns * freq_ppb) / 1e9
        double drift_factor = simulated_freq_ppb_ / 1e9;
        int64_t drift_ns = static_cast<int64_t>(reference_ns * drift_factor);
        return reference_ns + drift_ns;
    }

private:
    int32_t simulated_freq_ppb_;  // Simulated PHC frequency offset
    int32_t applied_freq_ppb_;    // Last applied frequency correction
    int adjust_count_;            // Number of frequency adjustments
};

// ============================================================================
// GPS adapter not needed for PHC calibration tests (calibrator only uses PHC)
// Pass nullptr for gps parameter in initialize()
// ============================================================================

// ============================================================================
// Test Utilities
// ============================================================================

void print_test_header(const char* test_name) {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(57) << test_name << " ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
}

void print_result(bool success) {
    if (success) {
        std::cout << "✅ PASS\n";
    } else {
        std::cout << "❌ FAIL\n";
    }
}

// ============================================================================
// Main Test Suite
// ============================================================================

int main() {
    int tests_total = 0;
    int tests_passed = 0;
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║         PhcCalibrator Unit Test Suite                     ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    
    // Test 1: Basic Initialization
    {
        print_test_header("TEST 1: Basic Initialization");
        tests_total++;
        
        PhcCalibratorConfig config = {
            .interval_pulses = 20,
            .max_correction_ppb = 500000,
            .drift_threshold_ppm = 100.0,
            .sanity_threshold_ppm = 2000.0,
            .max_iterations = 5
        };
        
        PhcCalibrator calibrator(config);
        MockPhcAdapter phc;
        
        int result = calibrator.initialize(&phc, nullptr);
        
        bool success = (result == 0) && !calibrator.is_calibrated();
        
        if (success) {
            std::cout << "Calibrator initialized successfully\n";
            std::cout << "Initial state: NOT calibrated ✓\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 2: Perfect Clock (Zero Drift)
    {
        print_test_header("TEST 2: Perfect Clock (Zero Drift)");
        tests_total++;
        
        PhcCalibratorConfig config = {
            .interval_pulses = 20,
            .max_correction_ppb = 500000,
            .drift_threshold_ppm = 100.0,
            .sanity_threshold_ppm = 2000.0,
            .max_iterations = 5
        };
        
        PhcCalibrator calibrator(config);
        MockPhcAdapter phc;
        
        calibrator.initialize(&phc, nullptr);
        phc.set_simulated_frequency(0);  // Perfect clock
        
        // Start calibration
        calibrator.start_calibration(100, 0);
        
        // Simulate 20 PPS pulses with perfect clock
        int64_t ref_ns = 0;
        int result = 0;
        for (uint32_t i = 1; i <= 20; i++) {
            ref_ns += 1000000000LL;  // 1 second per pulse
            int64_t phc_ns = phc.simulate_phc_time(ref_ns);
            result = calibrator.update_calibration(100 + i, phc_ns);
        }
        
        PhcCalibrationState state;
        calibrator.get_state(&state);
        
        bool success = (result == 1) &&  // Calibration complete
                      calibrator.is_calibrated() &&
                      (std::abs(state.last_drift_ppm) < 1.0);  // Near zero drift
        
        if (success) {
            std::cout << "Perfect clock detected\n";
            std::cout << "Drift: " << std::fixed << std::setprecision(3) 
                     << state.last_drift_ppm << " ppm (< 1 ppm threshold)\n";
            std::cout << "Calibration completed in " << state.iterations << " iteration(s)\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 3: Small Drift (+50 ppm)
    {
        print_test_header("TEST 3: Small Drift (+50 ppm)");
        tests_total++;
        
        PhcCalibratorConfig config = {
            .interval_pulses = 20,
            .max_correction_ppb = 500000,
            .drift_threshold_ppm = 100.0,
            .sanity_threshold_ppm = 2000.0,
            .max_iterations = 5
        };
        
        PhcCalibrator calibrator(config);
        MockPhcAdapter phc;
        
        calibrator.initialize(&phc, nullptr);
        phc.set_simulated_frequency(50000);  // +50 ppm = +50000 ppb
        
        calibrator.start_calibration(200, 0);
        
        int64_t ref_ns = 0;
        int result = 0;
        for (uint32_t i = 1; i <= 20; i++) {
            ref_ns += 1000000000LL;
            int64_t phc_ns = phc.simulate_phc_time(ref_ns);
            result = calibrator.update_calibration(200 + i, phc_ns);
        }
        
        PhcCalibrationState state;
        calibrator.get_state(&state);
        
        // Should detect ~50 ppm drift and complete in one iteration
        bool success = (result == 1) &&
                      calibrator.is_calibrated() &&
                      (std::abs(state.last_drift_ppm - 50.0) < 5.0);  // Within 5 ppm
        
        if (success) {
            std::cout << "Detected drift: " << std::fixed << std::setprecision(1) 
                     << state.last_drift_ppm << " ppm (expected ~50 ppm)\n";
            std::cout << "Correction applied: " << state.cumulative_freq_ppb << " ppb\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 4: Large Drift Requiring Iterations (+150 ppm)
    {
        print_test_header("TEST 4: Large Drift Requiring Iterations");
        tests_total++;
        
        PhcCalibratorConfig config = {
            .interval_pulses = 10,  // Shorter for faster test
            .max_correction_ppb = 500000,
            .drift_threshold_ppm = 100.0,
            .sanity_threshold_ppm = 2000.0,
            .max_iterations = 5
        };
        
        PhcCalibrator calibrator(config);
        MockPhcAdapter phc;
        
        calibrator.initialize(&phc, nullptr);
        phc.set_simulated_frequency(150000);  // +150 ppm (exceeds threshold)
        
        calibrator.start_calibration(300, 0);
        
        // First iteration
        int64_t ref_ns = 0;
        int result = 0;
        for (uint32_t i = 1; i <= 10; i++) {
            ref_ns += 1000000000LL;
            int64_t phc_ns = phc.simulate_phc_time(ref_ns);
            result = calibrator.update_calibration(300 + i, phc_ns);
        }
        
        // Should continue (drift > threshold)
        bool needs_more_iterations = (result == 0);
        
        PhcCalibrationState state;
        calibrator.get_state(&state);
        
        bool success = needs_more_iterations && 
                      (state.iterations == 1) &&
                      (std::abs(state.last_drift_ppm - 150.0) < 10.0);
        
        if (success) {
            std::cout << "First iteration detected: " << std::fixed << std::setprecision(1) 
                     << state.last_drift_ppm << " ppm\n";
            std::cout << "Requires more iterations (drift > " << config.drift_threshold_ppm << " ppm)\n";
            std::cout << "Iterations so far: " << state.iterations << "\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 5: Measurement Rejection (Unrealistic Drift)
    {
        print_test_header("TEST 5: Measurement Rejection");
        tests_total++;
        
        PhcCalibratorConfig config = {
            .interval_pulses = 20,
            .max_correction_ppb = 500000,
            .drift_threshold_ppm = 100.0,
            .sanity_threshold_ppm = 2000.0,  // Will reject > 2000 ppm
            .max_iterations = 5
        };
        
        PhcCalibrator calibrator(config);
        MockPhcAdapter phc;
        
        calibrator.initialize(&phc, nullptr);
        phc.set_simulated_frequency(5000000);  // +5000 ppm (unrealistic)
        
        calibrator.start_calibration(400, 0);
        
        int64_t ref_ns = 0;
        int result = 0;
        for (uint32_t i = 1; i <= 20; i++) {
            ref_ns += 1000000000LL;
            int64_t phc_ns = phc.simulate_phc_time(ref_ns);
            result = calibrator.update_calibration(400 + i, phc_ns);
        }
        
        // Should reject measurement and continue (result = 0, not calibrated)
        bool success = (result == 0) && !calibrator.is_calibrated();
        
        if (success) {
            std::cout << "Unrealistic drift rejected (> 2000 ppm threshold)\n";
            std::cout << "Baseline reset for retry\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 6: Negative Drift (-80 ppm)
    {
        print_test_header("TEST 6: Negative Drift");
        tests_total++;
        
        PhcCalibratorConfig config = {
            .interval_pulses = 20,
            .max_correction_ppb = 500000,
            .drift_threshold_ppm = 100.0,
            .sanity_threshold_ppm = 2000.0,
            .max_iterations = 5
        };
        
        PhcCalibrator calibrator(config);
        MockPhcAdapter phc;
        
        calibrator.initialize(&phc, nullptr);
        phc.set_simulated_frequency(-80000);  // -80 ppm
        
        calibrator.start_calibration(500, 0);
        
        int64_t ref_ns = 0;
        int result = 0;
        for (uint32_t i = 1; i <= 20; i++) {
            ref_ns += 1000000000LL;
            int64_t phc_ns = phc.simulate_phc_time(ref_ns);
            result = calibrator.update_calibration(500 + i, phc_ns);
        }
        
        PhcCalibrationState state;
        calibrator.get_state(&state);
        
        // Negative drift should be detected correctly
        bool success = (result == 1) &&
                      (state.last_drift_ppm < -70.0 && state.last_drift_ppm > -90.0);
        
        if (success) {
            std::cout << "Detected drift: " << std::fixed << std::setprecision(1) 
                     << state.last_drift_ppm << " ppm (expected ~-80 ppm)\n";
            std::cout << "Correction applied: " << state.cumulative_freq_ppb << " ppb\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 7: Reset Functionality
    {
        print_test_header("TEST 7: Reset Functionality");
        tests_total++;
        
        PhcCalibratorConfig config = {
            .interval_pulses = 20,
            .max_correction_ppb = 500000,
            .drift_threshold_ppm = 100.0,
            .sanity_threshold_ppm = 2000.0,
            .max_iterations = 5
        };
        
        PhcCalibrator calibrator(config);
        MockPhcAdapter phc;
        
        calibrator.initialize(&phc, nullptr);
        phc.set_simulated_frequency(50000);
        
        // Perform calibration
        calibrator.start_calibration(600, 0);
        int64_t ref_ns = 0;
        for (uint32_t i = 1; i <= 20; i++) {
            ref_ns += 1000000000LL;
            int64_t phc_ns = phc.simulate_phc_time(ref_ns);
            calibrator.update_calibration(600 + i, phc_ns);
        }
        
        // Verify calibrated
        bool was_calibrated = calibrator.is_calibrated();
        
        // Reset
        calibrator.reset();
        
        // Verify reset
        PhcCalibrationState state;
        calibrator.get_state(&state);
        
        bool success = was_calibrated &&
                      !calibrator.is_calibrated() &&
                      (state.cumulative_freq_ppb == 0) &&
                      (state.iterations == 0);
        
        if (success) {
            std::cout << "Was calibrated: YES\n";
            std::cout << "After reset:\n";
            std::cout << "  Calibrated: NO\n";
            std::cout << "  Cumulative freq: 0 ppb\n";
            std::cout << "  Iterations: 0\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Print summary
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
