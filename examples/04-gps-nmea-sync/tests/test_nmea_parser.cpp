/**
 * @file test_nmea_parser.cpp
 * @brief Unit Tests for NMEA Parser
 * 
 * Tests NMEA-0183 sentence parsing using real GPS log files.
 */

#include "nmea_parser.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

using namespace GPS::NMEA;

/**
 * @brief Test GPRMC sentence parsing
 */
void test_gprmc_parsing() {
    NMEAParser parser;
    GPSTimeData gps_data;
    
    // Test sentence from Log1.log: Time-only mode (V status)
    const char* gprmc = "$GPRMC,083218.00,V,,,,,,,131125,,,N*78";
    
    bool result = parser.parse_sentence(gprmc, gps_data);
    assert(result && "GPRMC parsing failed");
    assert(gps_data.time_valid && "Time should be valid");
    assert(gps_data.hours == 8 && "Hours should be 8");
    assert(gps_data.minutes == 32 && "Minutes should be 32");
    assert(gps_data.seconds == 18 && "Seconds should be 18");
    assert(gps_data.centiseconds == 0 && "Centiseconds should be 0");
    assert(gps_data.day == 13 && "Day should be 13");
    assert(gps_data.month == 11 && "Month should be 11");
    assert(gps_data.year == 2025 && "Year should be 2025");
    
    std::cout << "✓ GPRMC parsing test passed" << std::endl;
}

/**
 * @brief Test GPGGA sentence parsing
 */
void test_gpgga_parsing() {
    NMEAParser parser;
    GPSTimeData gps_data;
    
    // Test sentence from Log1.log
    const char* gpgga = "$GPGGA,083217.00,,,,,0,00,99.99,,,,,,*69";
    
    bool result = parser.parse_sentence(gpgga, gps_data);
    assert(result && "GPGGA parsing failed");
    assert(gps_data.time_valid && "Time should be valid");
    assert(gps_data.hours == 8 && "Hours should be 8");
    assert(gps_data.minutes == 32 && "Minutes should be 32");
    assert(gps_data.seconds == 17 && "Seconds should be 17");
    
    std::cout << "✓ GPGGA parsing test passed" << std::endl;
}

/**
 * @brief Test checksum validation
 */
void test_checksum_validation() {
    NMEAParser parser;
    GPSTimeData gps_data;
    
    // Valid checksum
    assert(parser.parse_sentence("$GPRMC,083218.00,V,,,,,,,131125,,,N*78", gps_data));
    
    // Invalid checksum (changed last digit)
    assert(!parser.parse_sentence("$GPRMC,083218.00,V,,,,,,,131125,,,N*79", gps_data));
    
    std::cout << "✓ Checksum validation test passed" << std::endl;
}

/**
 * @brief Main test runner
 */
int main() {
    std::cout << "Running NMEA Parser Unit Tests\n" << std::endl;
    
    try {
        test_gprmc_parsing();
        test_gpgga_parsing();
        test_checksum_validation();
        
        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
