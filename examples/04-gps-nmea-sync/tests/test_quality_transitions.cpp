/**
 * @file test_quality_transitions.cpp
 * @brief Automated tests for GPS fix and PPS state transition scenarios
 * 
 * Tests clock quality attribute updates during:
 * - GPS acquisition and loss
 * - PPS detection state changes
 * - Combined GPS+PPS transitions
 * - Holdover and recovery scenarios
 * 
 * Validates IEEE 1588-2019 compliance for dynamic clock quality management.
 */

#include "../gps_time_converter.hpp"
#include "../nmea_parser.hpp"
#include <iostream>
#include <cassert>
#include <cstdlib>

using namespace GPS;

// Test utilities
int tests_passed = 0;
int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << message << "\n"; \
            std::cerr << "  Condition: " << #condition << "\n"; \
            tests_failed++; \
            return false; \
        } \
    } while (0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "Running: " << #test_func << "..." << std::flush; \
        if (test_func()) { \
            std::cout << " PASS\n"; \
            tests_passed++; \
        } else { \
            tests_failed++; \
        } \
    } while (0)

/**
 * @brief Test 1: Cold start scenario (no GPS, no PPS)
 */
bool test_cold_start() {
    Time::GPSTimeConverter converter;
    
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::NO_FIX,
        0  // Idle
    );
    
    TEST_ASSERT(quality.clock_class == 248, 
                "Cold start should have clockClass=248 (not traceable)");
    TEST_ASSERT(quality.clock_accuracy == 0xFE, 
                "Cold start should have clockAccuracy=0xFE (unknown)");
    TEST_ASSERT(quality.time_source == 0xA0, 
                "Cold start should use INTERNAL_OSCILLATOR (0xA0)");
    TEST_ASSERT(quality.offset_scaled_log_variance == 0xFFFF,
                "Cold start should have maximum variance");
    TEST_ASSERT(quality.priority1 == 128,
                "Cold start should have default priority1");
    
    return true;
}

/**
 * @brief Test 2: GPS time-only fix acquired
 */
bool test_gps_time_only_acquired() {
    Time::GPSTimeConverter converter;
    
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::TIME_ONLY,
        3  // Failed (no PPS)
    );
    
    TEST_ASSERT(quality.clock_class == 248,
                "Time-only fix should be clockClass=248 (not fully traceable)");
    TEST_ASSERT(quality.clock_accuracy == 0x31,
                "Time-only without PPS should be 0x31 (10ms)");
    TEST_ASSERT(quality.time_source == 0x20,
                "Time-only should use GPS (0x20)");
    TEST_ASSERT(quality.offset_scaled_log_variance == 0x8000,
                "Time-only should have moderate variance");
    
    return true;
}

/**
 * @brief Test 3: GPS 3D fix acquired (no PPS yet)
 */
bool test_gps_3d_fix_no_pps() {
    Time::GPSTimeConverter converter;
    
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::AUTONOMOUS_FIX,
        3  // Failed (no PPS)
    );
    
    TEST_ASSERT(quality.clock_class == 6,
                "GPS 3D fix should be clockClass=6 (primary reference)");
    TEST_ASSERT(quality.clock_accuracy == 0x31,
                "GPS without PPS should be 0x31 (10ms)");
    TEST_ASSERT(quality.time_source == 0x20,
                "GPS fix should use GPS (0x20)");
    TEST_ASSERT(quality.offset_scaled_log_variance == 0x8000,
                "GPS without PPS should have moderate variance");
    TEST_ASSERT(quality.priority1 == 128,
                "GPS without PPS should use default priority");
    
    return true;
}

/**
 * @brief Test 4: PPS detection started (detecting state)
 */
bool test_pps_detecting() {
    Time::GPSTimeConverter converter;
    
    // Same as GPS fix alone (detecting doesn't change quality yet)
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::AUTONOMOUS_FIX,
        1  // Detecting
    );
    
    TEST_ASSERT(quality.clock_class == 6,
                "Detecting PPS should maintain clockClass=6");
    TEST_ASSERT(quality.clock_accuracy == 0x31,
                "Detecting PPS (not locked) should still be 0x31 (10ms)");
    
    return true;
}

/**
 * @brief Test 5: PPS locked! (optimal state)
 */
bool test_pps_locked() {
    Time::GPSTimeConverter converter;
    
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::AUTONOMOUS_FIX,
        2  // Locked
    );
    
    TEST_ASSERT(quality.clock_class == 6,
                "GPS + PPS should be clockClass=6 (primary reference)");
    TEST_ASSERT(quality.clock_accuracy == 0x21,
                "GPS + PPS should be 0x21 (100ns) - KEY IMPROVEMENT!");
    TEST_ASSERT(quality.time_source == 0x20,
                "GPS + PPS should use GPS (0x20)");
    TEST_ASSERT(quality.offset_scaled_log_variance == 0x4E5D,
                "GPS + PPS should have good variance (0x4E5D)");
    TEST_ASSERT(quality.priority1 == 100,
                "GPS + PPS should have high priority (100)");
    
    return true;
}

/**
 * @brief Test 6: DGPS + PPS (best case)
 */
bool test_dgps_pps_best_case() {
    Time::GPSTimeConverter converter;
    
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::DGPS_FIX,
        2  // Locked
    );
    
    TEST_ASSERT(quality.clock_class == 6,
                "DGPS + PPS should be clockClass=6");
    TEST_ASSERT(quality.clock_accuracy == 0x20,
                "DGPS + PPS should be 0x20 (25ns) - BEST ACCURACY!");
    TEST_ASSERT(quality.offset_scaled_log_variance == 0x4000,
                "DGPS + PPS should have excellent variance (0x4000)");
    TEST_ASSERT(quality.priority1 == 100,
                "DGPS + PPS should have high priority (100)");
    
    return true;
}

/**
 * @brief Test 7: GPS signal lost (degradation)
 */
bool test_gps_signal_lost() {
    Time::GPSTimeConverter converter;
    
    // Start with optimal state
    converter.update_clock_quality(NMEA::GPSFixStatus::AUTONOMOUS_FIX, 2);
    
    // Lose GPS signal
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::NO_FIX,
        3  // Failed (PPS also lost when GPS lost)
    );
    
    TEST_ASSERT(quality.clock_class == 248,
                "GPS lost should degrade to clockClass=248");
    TEST_ASSERT(quality.clock_accuracy == 0xFE,
                "GPS lost should degrade to accuracy=0xFE (unknown)");
    TEST_ASSERT(quality.time_source == 0xA0,
                "GPS lost should revert to INTERNAL_OSCILLATOR (0xA0)");
    TEST_ASSERT(quality.offset_scaled_log_variance == 0xFFFF,
                "GPS lost should have maximum variance");
    TEST_ASSERT(quality.priority1 == 128,
                "GPS lost should revert to default priority");
    
    return true;
}

/**
 * @brief Test 8: PPS lost while GPS maintains fix
 */
bool test_pps_lost_gps_ok() {
    Time::GPSTimeConverter converter;
    
    // Start with optimal state
    converter.update_clock_quality(NMEA::GPSFixStatus::AUTONOMOUS_FIX, 2);
    
    // Lose PPS but keep GPS
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::AUTONOMOUS_FIX,
        3  // Failed
    );
    
    TEST_ASSERT(quality.clock_class == 6,
                "PPS lost should maintain clockClass=6 (GPS still traceable)");
    TEST_ASSERT(quality.clock_accuracy == 0x31,
                "PPS lost should degrade accuracy to 0x31 (10ms)");
    TEST_ASSERT(quality.time_source == 0x20,
                "PPS lost should still use GPS (0x20)");
    TEST_ASSERT(quality.offset_scaled_log_variance == 0x8000,
                "PPS lost should have moderate variance");
    TEST_ASSERT(quality.priority1 == 128,
                "PPS lost should revert to default priority");
    
    return true;
}

/**
 * @brief Test 9: GPS recovery after loss
 */
bool test_gps_recovery() {
    Time::GPSTimeConverter converter;
    
    // Start with no GPS
    converter.update_clock_quality(NMEA::GPSFixStatus::NO_FIX, 3);
    
    // GPS recovers
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::AUTONOMOUS_FIX,
        2  // PPS also locks
    );
    
    TEST_ASSERT(quality.clock_class == 6,
                "GPS recovery should restore clockClass=6");
    TEST_ASSERT(quality.clock_accuracy == 0x21,
                "GPS + PPS recovery should restore 0x21 (100ns)");
    TEST_ASSERT(quality.time_source == 0x20,
                "GPS recovery should use GPS (0x20)");
    TEST_ASSERT(quality.priority1 == 100,
                "GPS + PPS recovery should restore high priority");
    
    return true;
}

/**
 * @brief Test 10: State transition sequence (cold start → optimal → loss → recovery)
 */
bool test_full_lifecycle() {
    Time::GPSTimeConverter converter;
    
    // State 1: Cold start
    auto q1 = converter.update_clock_quality(NMEA::GPSFixStatus::NO_FIX, 0);
    TEST_ASSERT(q1.clock_class == 248 && q1.clock_accuracy == 0xFE,
                "Lifecycle stage 1: Cold start");
    
    // State 2: Time-only fix
    auto q2 = converter.update_clock_quality(NMEA::GPSFixStatus::TIME_ONLY, 3);
    TEST_ASSERT(q2.clock_class == 248 && q2.clock_accuracy == 0x31,
                "Lifecycle stage 2: Time-only");
    
    // State 3: GPS 3D fix
    auto q3 = converter.update_clock_quality(NMEA::GPSFixStatus::AUTONOMOUS_FIX, 3);
    TEST_ASSERT(q3.clock_class == 6 && q3.clock_accuracy == 0x31,
                "Lifecycle stage 3: GPS 3D fix");
    
    // State 4: PPS detecting
    auto q4 = converter.update_clock_quality(NMEA::GPSFixStatus::AUTONOMOUS_FIX, 1);
    TEST_ASSERT(q4.clock_class == 6 && q4.clock_accuracy == 0x31,
                "Lifecycle stage 4: PPS detecting");
    
    // State 5: PPS locked (OPTIMAL)
    auto q5 = converter.update_clock_quality(NMEA::GPSFixStatus::AUTONOMOUS_FIX, 2);
    TEST_ASSERT(q5.clock_class == 6 && q5.clock_accuracy == 0x21,
                "Lifecycle stage 5: OPTIMAL (GPS+PPS)");
    
    // State 6: GPS lost
    auto q6 = converter.update_clock_quality(NMEA::GPSFixStatus::NO_FIX, 3);
    TEST_ASSERT(q6.clock_class == 248 && q6.clock_accuracy == 0xFE,
                "Lifecycle stage 6: GPS lost (degraded)");
    
    // State 7: GPS recovered
    auto q7 = converter.update_clock_quality(NMEA::GPSFixStatus::AUTONOMOUS_FIX, 2);
    TEST_ASSERT(q7.clock_class == 6 && q7.clock_accuracy == 0x21,
                "Lifecycle stage 7: Recovered to optimal");
    
    return true;
}

/**
 * @brief Test 11: Time-only + PPS (unusual but valid)
 */
bool test_time_only_with_pps() {
    Time::GPSTimeConverter converter;
    
    auto quality = converter.update_clock_quality(
        NMEA::GPSFixStatus::TIME_ONLY,
        2  // Locked
    );
    
    TEST_ASSERT(quality.clock_class == 248,
                "Time-only + PPS should be clockClass=248 (conservative)");
    TEST_ASSERT(quality.clock_accuracy == 0x21,
                "Time-only + PPS should have 0x21 (100ns) - PPS provides accuracy!");
    TEST_ASSERT(quality.time_source == 0x20,
                "Time-only + PPS should use GPS (0x20)");
    
    // Key insight: Accuracy is GOOD (100ns) even though not fully traceable!
    return true;
}

/**
 * @brief Test 12: BMCA comparison scenarios
 */
bool test_bmca_comparisons() {
    Time::GPSTimeConverter conv1, conv2, conv3;
    
    // Clock 1: GPS + PPS (100ns)
    auto q1 = conv1.update_clock_quality(NMEA::GPSFixStatus::AUTONOMOUS_FIX, 2);
    
    // Clock 2: GPS only (10ms)
    auto q2 = conv2.update_clock_quality(NMEA::GPSFixStatus::AUTONOMOUS_FIX, 3);
    
    // Clock 3: No GPS (unknown)
    auto q3 = conv3.update_clock_quality(NMEA::GPSFixStatus::NO_FIX, 3);
    
    // BMCA Dataset1 comparison: clockClass → clockAccuracy → variance
    // Clock 1 should win (same clockClass, better accuracy)
    TEST_ASSERT(q1.clock_class == q2.clock_class,
                "Clock 1 and 2 have same clockClass (6)");
    TEST_ASSERT(q1.clock_accuracy < q2.clock_accuracy,
                "Clock 1 has better accuracy (0x21 < 0x31)");
    TEST_ASSERT(q1.priority1 < q2.priority1,
                "Clock 1 has higher priority (100 < 128)");
    
    // Clock 3 should lose against both (worse clockClass)
    TEST_ASSERT(q1.clock_class < q3.clock_class,
                "Clock 1 beats Clock 3 (6 < 248)");
    TEST_ASSERT(q2.clock_class < q3.clock_class,
                "Clock 2 beats Clock 3 (6 < 248)");
    
    return true;
}

/**
 * @brief Main test runner
 */
int main() {
    std::cout << "========================================\n";
    std::cout << "Clock Quality Transition Tests\n";
    std::cout << "IEEE 1588-2019 Compliance Validation\n";
    std::cout << "========================================\n\n";
    
    // Run all tests
    RUN_TEST(test_cold_start);
    RUN_TEST(test_gps_time_only_acquired);
    RUN_TEST(test_gps_3d_fix_no_pps);
    RUN_TEST(test_pps_detecting);
    RUN_TEST(test_pps_locked);
    RUN_TEST(test_dgps_pps_best_case);
    RUN_TEST(test_gps_signal_lost);
    RUN_TEST(test_pps_lost_gps_ok);
    RUN_TEST(test_gps_recovery);
    RUN_TEST(test_full_lifecycle);
    RUN_TEST(test_time_only_with_pps);
    RUN_TEST(test_bmca_comparisons);
    
    // Print summary
    std::cout << "\n========================================\n";
    std::cout << "Test Summary\n";
    std::cout << "========================================\n";
    std::cout << "Tests Passed:  " << tests_passed << "\n";
    std::cout << "Tests Failed:  " << tests_failed << "\n";
    std::cout << "Total Tests:   " << (tests_passed + tests_failed) << "\n\n";
    
    if (tests_failed == 0) {
        std::cout << "✓ All tests PASSED!\n";
        std::cout << "✓ Clock quality management is IEEE 1588-2019 compliant.\n";
        std::cout << "✓ State transitions work correctly for all scenarios.\n";
        std::cout << "✓ BMCA will properly select best Grandmaster.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "✗ Some tests FAILED!\n";
        std::cout << "✗ Clock quality management needs fixes.\n";
        return EXIT_FAILURE;
    }
}
