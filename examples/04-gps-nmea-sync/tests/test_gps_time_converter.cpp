/**
 * @file test_gps_time_converter.cpp
 * @brief Unit Tests for GPS Time Converter
 * 
 * Tests GPS time to PTP timestamp conversion.
 */

#include "gps_time_converter.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace GPS::Time;
using namespace GPS::NMEA;

/**
 * @brief Test GPS time to PTP conversion
 */
void test_gps_to_ptp_conversion() {
    GPSTimeConverter converter;
    
    // Create GPS time data: 2025-11-13 08:32:18.00 UTC
    GPSTimeData gps_data;
    gps_data.year = 2025;
    gps_data.month = 11;
    gps_data.day = 13;
    gps_data.hours = 8;
    gps_data.minutes = 32;
    gps_data.seconds = 18;
    gps_data.centiseconds = 0;
    gps_data.time_valid = true;
    gps_data.date_valid = true;
    
    PTPTimestamp ptp_ts;
    bool result = converter.convert_to_ptp(gps_data, ptp_ts);
    
    assert(result && "Conversion should succeed");
    assert(ptp_ts.seconds > 0 && "PTP seconds should be positive");
    assert(ptp_ts.nanoseconds == 0 && "Nanoseconds should be 0 for .00 centiseconds");
    
    std::cout << "✓ GPS to PTP conversion test passed" << std::endl;
}

/**
 * @brief Test clock offset calculation
 */
void test_clock_offset_calculation() {
    GPSTimeConverter converter;
    
    PTPTimestamp gps_time(1000, 500000000);   // 1000.5 seconds
    PTPTimestamp local_time(1000, 400000000); // 1000.4 seconds
    
    int64_t offset = converter.calculate_clock_offset(gps_time, local_time);
    
    // Offset should be 100ms = 100,000,000 ns (local behind GPS)
    assert(offset == 100000000LL && "Clock offset calculation incorrect");
    
    std::cout << "✓ Clock offset calculation test passed" << std::endl;
}

/**
 * @brief Test centiseconds to nanoseconds conversion
 */
void test_centiseconds_conversion() {
    GPSTimeConverter converter;
    
    // Test 0.50 centiseconds (50 centiseconds = 500ms)
    GPSTimeData gps_data;
    gps_data.year = 2025;
    gps_data.month = 1;
    gps_data.day = 1;
    gps_data.hours = 0;
    gps_data.minutes = 0;
    gps_data.seconds = 0;
    gps_data.centiseconds = 50;  // 500ms
    gps_data.time_valid = true;
    gps_data.date_valid = true;
    
    PTPTimestamp ptp_ts;
    converter.convert_to_ptp(gps_data, ptp_ts);
    
    // 50 centiseconds = 500,000,000 nanoseconds
    assert(ptp_ts.nanoseconds == 500000000 && "Centisecond conversion incorrect");
    
    std::cout << "✓ Centiseconds conversion test passed" << std::endl;
}

/**
 * @brief Main test runner
 */
int main() {
    std::cout << "Running GPS Time Converter Unit Tests\n" << std::endl;
    
    try {
        test_gps_to_ptp_conversion();
        test_clock_offset_calculation();
        test_centiseconds_conversion();
        
        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
