/**
 * @file rtc_ptp_sync_example.cpp
 * @brief Example: RTC Module as Time Source and Sink with PTP
 * 
 * Demonstrates RTC bidirectional synchronization:
 * - RTC as TIME SOURCE when no external source available
 * - RTC as TIME SINK synchronized by GPS/NTP/DCF77 (BMCA driven)
 * 
 * Hardware Setup:
 * - DS3231 RTC module on I2C (SDA, SCL)
 * - Optional: GPS module (GT-U7) for external sync
 * - Optional: NTP network connection
 * - Pull-up resistors (4.7kΩ) on I2C lines
 * 
 * Compilation:
 *   g++ -std=c++17 -I../../include -o rtc_ptp_sync rtc_ptp_sync_example.cpp rtc_adapter.cpp
 * 
 * Usage Scenario 1 - RTC as Fallback Time Source:
 *   ./rtc_ptp_sync --rtc-only
 *   # Uses RTC as sole time source (clockClass 248)
 * 
 * Usage Scenario 2 - RTC Synchronized by GPS:
 *   ./rtc_ptp_sync --gps /dev/ttyUSB0
 *   # GPS synchronizes RTC, RTC maintains time during GPS loss
 * 
 * Usage Scenario 3 - RTC Synchronized by NTP:
 *   ./rtc_ptp_sync --ntp pool.ntp.org
 *   # NTP synchronizes RTC, RTC provides time during network outage
 */

#include "rtc_adapter.hpp"
#include "clocks.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <csignal>

// For GPS synchronization (optional)
#ifdef ENABLE_GPS_SYNC
#include "../04-gps-nmea-sync/gps_adapter.hpp"
#endif

// For NTP synchronization (optional)
#ifdef ENABLE_NTP_SYNC
#include "../05-ntp-sntp-sync/ntp_adapter.hpp"
#endif

using namespace Examples::RTC;
namespace Types = IEEE::_1588::PTP::_2019::Types;

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

void signal_handler(int signal) {
    if (signal == SIGINT) {
        running = 0;
    }
}

/**
 * @brief Print timestamp in human-readable format
 */
void print_timestamp(const Types::Timestamp& ts) {
    std::time_t unix_time = static_cast<std::time_t>(ts.seconds_field);
    std::tm* tm_time = std::gmtime(&unix_time);
    
    std::cout << std::put_time(tm_time, "%Y-%m-%d %H:%M:%S")
              << "." << std::setw(9) << std::setfill('0') << ts.nanoseconds_field
              << " UTC";
}

/**
 * @brief Print clock quality information
 */
void print_clock_quality(const Types::ClockQuality& quality, const std::string& source) {
    std::cout << source << " Clock Quality:" << std::endl;
    std::cout << "  clockClass: " << static_cast<int>(quality.clock_class);
    
    // Interpret clockClass
    if (quality.clock_class <= 13) {
        std::cout << " (Primary Time Source)";
    } else if (quality.clock_class < 100) {
        std::cout << " (Degraded by Path)";
    } else if (quality.clock_class < 200) {
        std::cout << " (Degraded Accuracy)";
    } else {
        std::cout << " (Unsynchronized/Default)";
    }
    std::cout << std::endl;
    
    std::cout << "  clockAccuracy: 0x" << std::hex << static_cast<int>(quality.clock_accuracy) << std::dec << std::endl;
    std::cout << "  offsetScaledLogVariance: 0x" << std::hex << quality.offset_scaled_log_variance << std::dec << std::endl;
}

/**
 * @brief Print RTC module information
 */
void print_rtc_info(const RTCAdapter& rtc) {
    std::cout << "\n=== RTC Module Information ===" << std::endl;
    
    std::cout << "Module Type: ";
    switch (rtc.get_module_type()) {
        case RTCModuleType::DS3231:
            std::cout << "DS3231 (High-precision TCXO, ±2ppm)";
            break;
        case RTCModuleType::DS1307:
            std::cout << "DS1307 (Basic crystal, ±250ppm)";
            break;
        case RTCModuleType::PCF8523:
            std::cout << "PCF8523 (Low-power, ±3ppm)";
            break;
        default:
            std::cout << "Unknown";
            break;
    }
    std::cout << std::endl;
    
    float temp = rtc.get_temperature_celsius();
    if (!std::isnan(temp)) {
        std::cout << "Temperature: " << std::fixed << std::setprecision(2) << temp << "°C" << std::endl;
    }
    
    std::cout << "Synchronized: " << (rtc.is_synchronized() ? "Yes" : "No") << std::endl;
    
    if (rtc.is_synchronized()) {
        int32_t seconds_since_sync = rtc.get_seconds_since_sync();
        std::cout << "Time since sync: ";
        if (seconds_since_sync < 60) {
            std::cout << seconds_since_sync << " seconds";
        } else if (seconds_since_sync < 3600) {
            std::cout << (seconds_since_sync / 60) << " minutes";
        } else if (seconds_since_sync < 86400) {
            std::cout << (seconds_since_sync / 3600) << " hours";
        } else {
            std::cout << (seconds_since_sync / 86400) << " days";
        }
        std::cout << std::endl;
        
        int64_t offset_ns = rtc.get_estimated_offset_ns();
        std::cout << "Estimated drift: " << (offset_ns / 1000.0) << " µs" << std::endl;
    }
}

/**
 * @brief Main example - RTC as source and sink
 */
int main(int argc, char* argv[]) {
    std::cout << "\n=== IEEE 1588-2019 PTP - RTC Time Source Example ===" << std::endl;
    std::cout << "Demonstrating RTC as both time SOURCE and SINK" << std::endl;
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    
    // Initialize RTC adapter
    std::cout << "\nInitializing RTC module (DS3231 at 0x68)..." << std::endl;
    RTCAdapter rtc(0x68, RTCModuleType::DS3231);
    
    if (!rtc.initialize()) {
        std::cerr << "ERROR: Failed to initialize RTC module" << std::endl;
        std::cerr << "Check I2C connections and module address" << std::endl;
        return 1;
    }
    
    std::cout << "✓ RTC module initialized successfully" << std::endl;
    print_rtc_info(rtc);
    
    // Initialize PTP clock
    std::cout << "\nInitializing PTP clock..." << std::endl;
    Types::ClockIdentity clock_id{0x00, 0x1B, 0x19, 0xFF, 0xFE, 0x01, 0x23, 0x45};
    uint16_t port_number = 1;
    IEEE::_1588::PTP::_2019::PortIdentity port_id(clock_id, port_number);
    
    IEEE::_1588::PTP::_2019::OrdinaryClock ptp_clock(clock_id, port_id);
    
    std::cout << "✓ PTP clock initialized" << std::endl;
    
    // Simulation: Synchronize RTC with external source
    // In real application, this would come from GPS/NTP/DCF77
    std::cout << "\n=== Simulating External Time Source ===" << std::endl;
    std::cout << "In real deployment, GPS/NTP/DCF77 would provide this time" << std::endl;
    
    // Get current system time as "external reference"
    auto system_now = std::chrono::system_clock::now();
    auto system_time_t = std::chrono::system_clock::to_time_t(system_now);
    Types::Timestamp external_time{
        static_cast<uint64_t>(system_time_t),
        0  // Simplified: no nanoseconds
    };
    
    std::cout << "External reference time: ";
    print_timestamp(external_time);
    std::cout << std::endl;
    
    // Synchronize RTC with external source (RTC as SINK)
    std::cout << "\n=== RTC as TIME SINK ===" << std::endl;
    std::cout << "Writing external time to RTC..." << std::endl;
    
    if (rtc.set_time(external_time)) {
        std::cout << "✓ RTC synchronized successfully" << std::endl;
    } else {
        std::cerr << "✗ Failed to synchronize RTC" << std::endl;
        return 1;
    }
    
    // Update RTC info after sync
    print_rtc_info(rtc);
    
    // Main loop: Use RTC as time source
    std::cout << "\n=== RTC as TIME SOURCE ===" << std::endl;
    std::cout << "Reading time from RTC and updating PTP clock..." << std::endl;
    std::cout << "Press Ctrl+C to stop\n" << std::endl;
    
    int update_count = 0;
    while (running && update_count < 10) {  // Run for 10 iterations or until Ctrl+C
        // Update RTC status
        if (!rtc.update()) {
            std::cerr << "Warning: RTC communication error" << std::endl;
        }
        
        // Get time from RTC (RTC as SOURCE)
        Types::Timestamp rtc_time = rtc.get_current_time();
        
        // Get clock quality from RTC
        Types::ClockQuality rtc_quality = rtc.get_clock_quality();
        
        // Update PTP clock with RTC time and quality
        auto& default_ds = ptp_clock.get_default_data_set();
        default_ds.clockQuality = rtc_quality;
        
        auto& time_props_ds = ptp_clock.get_time_properties_data_set();
        time_props_ds.timeSource = rtc.get_time_source();
        
        ptp_clock.tick(rtc_time);
        
        // Display status
        std::cout << "Update #" << (update_count + 1) << ":" << std::endl;
        std::cout << "  RTC Time: ";
        print_timestamp(rtc_time);
        std::cout << std::endl;
        
        print_clock_quality(rtc_quality, "RTC");
        
        std::cout << "  Time Source: Internal_Oscillator (0x" 
                  << std::hex << static_cast<int>(time_props_ds.timeSource) 
                  << std::dec << ")" << std::endl;
        
        if (rtc.is_synchronized()) {
            int64_t offset_ns = rtc.get_estimated_offset_ns();
            std::cout << "  Estimated drift: " << (offset_ns / 1000.0) << " µs" << std::endl;
        }
        
        std::cout << std::endl;
        
        update_count++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Summary
    std::cout << "\n=== Example Summary ===" << std::endl;
    std::cout << "✓ RTC operated as TIME SINK (synchronized from external source)" << std::endl;
    std::cout << "✓ RTC operated as TIME SOURCE (provided time to PTP clock)" << std::endl;
    std::cout << "✓ Clock quality updated based on holdover time" << std::endl;
    
    print_rtc_info(rtc);
    
    std::cout << "\nNext Steps:" << std::endl;
    std::cout << "1. Add GPS module to provide better time source (clockClass 6)" << std::endl;
    std::cout << "2. Implement BMCA to select best source (GPS vs RTC)" << std::endl;
    std::cout << "3. Use multi-source example (08-multi-source-bmca)" << std::endl;
    
    return 0;
}
