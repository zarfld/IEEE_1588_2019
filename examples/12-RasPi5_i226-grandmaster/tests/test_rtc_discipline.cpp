/**
 * @file test_rtc_discipline.cpp
 * @brief TDD tests for RTC aging offset discipline (REFACTORED_VALIDATION_PLAN.md Priority #1)
 * 
 * Tests verify RTC drift discipline per deb.md Recommendations A + E:
 * - Drift buffer: 120 samples (20 minutes @ 10s intervals)
 * - Stability gate: stddev < 0.3 ppm
 * - Proportional control: delta_lsb = round(drift_avg_ppm / 0.1)
 * - Clamped to [-3, +3] LSB range
 * - Minimum 1200s between adjustments
 * - Requires 60+ samples before first adjustment
 * 
 * @see REFACTORED_VALIDATION_PLAN.md Section 1
 * @see IMPLEMENTATION_PLAN.md RTC Discipline Improvements
 */

#include "../src/rtc_drift_discipline.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstdint>
#include <vector>
#include <cmath>

// Test utilities
static int tests_total = 0;
static int tests_passed = 0;

static void print_test_header(const char* test_name) {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(58) << test_name << "║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
}

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   RTC Drift Discipline - TDD Red Phase                    ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n⚠️  Tests WILL FAIL until RtcDriftDiscipline implemented!\n\n";
    
    // Test 1: Drift averaging window (120 samples)
    {
        print_test_header("Test 1: Drift Averaging Window (120 samples)");
        tests_total++;
        
        RtcDriftDisciplineConfig config;
        config.buffer_size = 120;
        RtcDriftDiscipline discipline(config);
        
        // Add 120 samples
        for (size_t i = 0; i < 120; i++) {
            discipline.add_sample(0.5, i * 10);
        }
        
        size_t count = discipline.get_sample_count();
        std::cout << "Sample count: " << count << " (expected 120)\n";
        
        if (count == 120) {
            std::cout << "✅ PASS\n";
            tests_passed++;
        } else {
            std::cout << "❌ FAIL - Expected 120 samples, got " << count << "\n";
        }
    }
    
    // Test 2: Stability gate
    {
        print_test_header("Test 2: Stability Gate (stddev < 0.3 ppm)");
        tests_total++;
        
        RtcDriftDisciplineConfig config;
        RtcDriftDiscipline discipline(config);
        
        // Add 60 stable samples over 600 seconds
        for (size_t i = 0; i < 60; i++) {
            discipline.add_sample(0.5, i * 10);
        }
        
        double stddev = discipline.get_stddev();
        bool should_adjust = discipline.should_adjust(1200);  // After 20 minutes
        
        std::cout << "Stddev: " << std::fixed << std::setprecision(3) << stddev << " ppm\n";
        std::cout << "Should adjust: " << (should_adjust ? "true" : "false") << "\n";
        
        if (stddev < 0.3 && should_adjust) {
            std::cout << "✅ PASS\n";
            tests_passed++;
        } else {
            std::cout << "❌ FAIL - Expected stddev < 0.3 and should_adjust=true\n";
        }
    }
    
    // Test 3: Proportional control law
    {
        print_test_header("Test 3: Proportional Control (0.176 ppm → 2 LSB)");
        tests_total++;
        
        RtcDriftDisciplineConfig config;
        RtcDriftDiscipline discipline(config);
        
        // Add samples with 0.176 ppm average
        for (size_t i = 0; i < 60; i++) {
            discipline.add_sample(0.176, i * 10);
        }
        
        int lsb = discipline.calculate_lsb_adjustment();
        std::cout << "LSB adjustment: " << lsb << " (expected 2)\n";
        
        if (lsb == 2) {
            std::cout << "✅ PASS\n";
            tests_passed++;
        } else {
            std::cout << "❌ FAIL - Expected LSB=2, got " << lsb << "\n";
        }
    }
    
    // Test 4: LSB clamping
    {
        print_test_header("Test 4: LSB Clamping (0.5 ppm → clamp to +3)");
        tests_total++;
        
        RtcDriftDisciplineConfig config;
        config.max_lsb_delta = 3;
        RtcDriftDiscipline discipline(config);
        
        // Add samples with 0.5 ppm (would be 5 LSB, should clamp to 3)
        for (size_t i = 0; i < 60; i++) {
            discipline.add_sample(0.5, i * 10);
        }
        
        int lsb = discipline.calculate_lsb_adjustment();
        std::cout << "LSB adjustment: " << lsb << " (expected 3, clamped)\n";
        
        if (lsb == 3) {
            std::cout << "✅ PASS\n";
            tests_passed++;
        } else {
            std::cout << "❌ FAIL - Expected clamped LSB=3, got " << lsb << "\n";
        }
    }
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   TDD RED PHASE RESULTS: " << tests_passed << "/" << tests_total << " PASSED                     ║\n";
    std::cout << "║   Next: Implement src/rtc_drift_discipline.hpp/.cpp       ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    
    return (tests_passed == tests_total) ? 0 : 1;
}

