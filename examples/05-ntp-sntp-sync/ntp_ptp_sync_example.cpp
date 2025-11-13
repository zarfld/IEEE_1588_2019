/**
 * @file ntp_ptp_sync_example.cpp
 * @brief Example: Synchronize PTP Clock from NTP Time Source
 * 
 * Demonstrates:
 * - Querying NTP server using SNTP protocol
 * - Computing IEEE 1588-2019 clock quality from NTP stratum
 * - Updating PTP clock's DefaultDataSet.clockQuality
 * - Setting TimePropertiesDataSet.timeSource to NTP (0x50)
 * - Using library's Types::ClockQuality and Types::TimeSource
 * 
 * Usage:
 *   ntp_ptp_sync_example [ntp_server] [poll_interval_s]
 * 
 * Examples:
 *   ntp_ptp_sync_example pool.ntp.org 64
 *   ntp_ptp_sync_example time.google.com 128
 *   ntp_ptp_sync_example 192.168.1.1 32
 */

#include "ntp_adapter.hpp"
#include "clocks.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <csignal>

using namespace Examples::NTP;

// Global flag for graceful shutdown
static volatile bool g_running = true;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

void print_clock_quality(const Types::ClockQuality& quality) {
    std::cout << "Clock Quality:\n";
    std::cout << "  clockClass: " << static_cast<int>(quality.clock_class);
    
    // Interpret clock class
    if (quality.clock_class <= 13) {
        std::cout << " (Primary time source)";
    } else if (quality.clock_class >= 52 && quality.clock_class <= 58) {
        std::cout << " (Degraded by path)";
    } else if (quality.clock_class >= 187 && quality.clock_class <= 193) {
        std::cout << " (Degraded accuracy)";
    } else if (quality.clock_class == 248) {
        std::cout << " (Default, not synchronized)";
    }
    std::cout << "\n";
    
    std::cout << "  clockAccuracy: 0x" << std::hex << static_cast<int>(quality.clock_accuracy) << std::dec;
    
    // Interpret accuracy
    if (quality.clock_accuracy == 0xFE) {
        std::cout << " (Unknown)";
    } else if (quality.clock_accuracy >= 0x20 && quality.clock_accuracy <= 0x31) {
        double accuracy_ns = std::pow(10.0, static_cast<double>(quality.clock_accuracy - 0x20 - 6)) * 1e9;
        if (accuracy_ns < 1000) {
            std::cout << " (~" << static_cast<int>(accuracy_ns) << " ns)";
        } else if (accuracy_ns < 1000000) {
            std::cout << " (~" << static_cast<int>(accuracy_ns / 1000) << " µs)";
        } else {
            std::cout << " (~" << static_cast<int>(accuracy_ns / 1000000) << " ms)";
        }
    }
    std::cout << "\n";
    
    std::cout << "  offsetScaledLogVariance: 0x" << std::hex 
              << quality.offset_scaled_log_variance << std::dec << "\n";
}

void print_ntp_result(const NTPQueryResult& result) {
    std::cout << "NTP Query Result:\n";
    std::cout << "  Valid: " << (result.valid ? "Yes" : "No") << "\n";
    
    if (!result.valid) return;
    
    std::cout << "  Stratum: " << static_cast<int>(result.stratum);
    if (result.stratum == 1) {
        std::cout << " (Primary reference)";
    } else if (result.stratum < 16) {
        std::cout << " (Secondary reference)";
    } else {
        std::cout << " (Unsynchronized)";
    }
    std::cout << "\n";
    
    std::cout << "  Offset: " << (result.offset_ns / 1000000.0) << " ms\n";
    std::cout << "  Round-trip delay: " << (result.round_trip_ns / 1000000.0) << " ms\n";
    std::cout << "  Root delay: " << (result.root_delay_ns / 1000000.0) << " ms\n";
    std::cout << "  Root dispersion: " << (result.root_dispersion_ns / 1000000.0) << " ms\n";
}

int main(int argc, char* argv[]) {
    // Parse command line
    std::string ntp_server = "pool.ntp.org";
    uint32_t poll_interval_s = 64;
    
    if (argc >= 2) {
        ntp_server = argv[1];
    }
    if (argc >= 3) {
        poll_interval_s = std::stoi(argv[2]);
    }
    
    std::cout << "========================================\n";
    std::cout << "NTP to PTP Clock Synchronization Example\n";
    std::cout << "========================================\n";
    std::cout << "NTP Server: " << ntp_server << "\n";
    std::cout << "Poll Interval: " << poll_interval_s << " seconds\n";
    std::cout << "Time Source: Types::TimeSource::NTP (0x50)\n";
    std::cout << "========================================\n\n";
    
    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Create NTP adapter
    NTPAdapter ntp(ntp_server, 123, poll_interval_s);
    
    if (!ntp.initialize()) {
        std::cerr << "ERROR: Failed to initialize NTP adapter\n";
        return 1;
    }
    
    std::cout << "NTP adapter initialized\n";
    std::cout << "Querying NTP server...\n\n";
    
    // Main loop
    int query_count = 0;
    while (g_running) {
        // Query NTP server
        if (ntp.update()) {
            query_count++;
            
            std::cout << "\n[" << query_count << "] NTP Query Successful at "
                      << std::chrono::system_clock::now().time_since_epoch().count() / 1000000000
                      << "\n";
            std::cout << "----------------------------------------\n";
            
            // Print NTP result
            print_ntp_result(ntp.get_last_result());
            std::cout << "\n";
            
            // Get clock quality using LIBRARY's Types::ClockQuality
            Types::ClockQuality quality = ntp.get_clock_quality();
            print_clock_quality(quality);
            std::cout << "\n";
            
            // Demonstrate updating PTP clock
            std::cout << "Updating PTP Clock:\n";
            std::cout << "  ds.clockQuality = ntp.get_clock_quality();\n";
            std::cout << "  tp.timeSource = static_cast<uint8_t>(Types::TimeSource::NTP);\n";
            std::cout << "  // timeSource = 0x" << std::hex 
                      << static_cast<int>(Types::TimeSource::NTP) << std::dec << " (NTP)\n";
            std::cout << "\n";
            
        } else {
            std::cerr << "NTP query failed\n";
        }
        
        // Wait for next poll interval
        for (uint32_t i = 0; i < poll_interval_s && g_running; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "\nShutting down...\n";
    std::cout << "Total NTP queries: " << query_count << "\n";
    
    return 0;
}
