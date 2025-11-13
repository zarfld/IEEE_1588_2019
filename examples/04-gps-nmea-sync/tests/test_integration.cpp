/**
 * @file test_integration.cpp
 * @brief Integration Tests for GPS NMEA Synchronization
 * 
 * Tests end-to-end GPS NMEA parsing and PTP synchronization using real log files.
 */

#include "nmea_parser.hpp"
#include "gps_time_converter.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

using namespace GPS::NMEA;
using namespace GPS::Time;

/**
 * @brief Test parsing real GPS log file
 */
void test_parse_log_file() {
    std::cout << "Testing with real GPS log file..." << std::endl;
    
    // Try to open Log1.log from test_data directory
    std::ifstream log_file("test_data/Log1.log");
    
    if (!log_file.is_open()) {
        std::cout << "⚠ Warning: test_data/Log1.log not found, skipping log file test" << std::endl;
        return;
    }
    
    NMEAParser parser;
    GPSTimeConverter converter;
    
    std::string line;
    int parsed_count = 0;
    int converted_count = 0;
    
    while (std::getline(log_file, line)) {
        GPSTimeData gps_data;
        
        if (parser.parse_sentence(line.c_str(), gps_data)) {
            ++parsed_count;
            
            // Try to convert to PTP
            PTPTimestamp ptp_ts;
            if (converter.convert_to_ptp(gps_data, ptp_ts)) {
                ++converted_count;
            }
        }
    }
    
    log_file.close();
    
    std::cout << "  Parsed " << parsed_count << " NMEA sentences" << std::endl;
    std::cout << "  Converted " << converted_count << " to PTP timestamps" << std::endl;
    
    assert(parsed_count > 0 && "Should parse at least one sentence");
    
    std::cout << "✓ Log file parsing test passed" << std::endl;
}

/**
 * @brief Test end-to-end synchronization workflow
 */
void test_sync_workflow() {
    NMEAParser parser;
    GPSTimeConverter converter;
    
    // Simulate GPS sentences
    const char* sentences[] = {
        "$GPRMC,083218.00,V,,,,,,,131125,,,N*78",
        "$GPGGA,083217.00,,,,,0,00,99.99,,,,,,*69"
    };
    
    int successful_syncs = 0;
    
    for (const char* sentence : sentences) {
        GPSTimeData gps_data;
        
        if (parser.parse_sentence(sentence, gps_data)) {
            PTPTimestamp ptp_ts;
            
            if (converter.convert_to_ptp(gps_data, ptp_ts)) {
                ++successful_syncs;
                
                // Verify PTP timestamp is reasonable
                assert(ptp_ts.seconds > 1700000000LL && "PTP seconds should be after 2023");
            }
        }
    }
    
    assert(successful_syncs > 0 && "Should have at least one successful sync");
    
    std::cout << "✓ Synchronization workflow test passed" << std::endl;
}

/**
 * @brief Main test runner
 */
int main() {
    std::cout << "Running GPS NMEA Integration Tests\n" << std::endl;
    
    try {
        test_sync_workflow();
        test_parse_log_file();
        
        std::cout << "\n✓ All integration tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
