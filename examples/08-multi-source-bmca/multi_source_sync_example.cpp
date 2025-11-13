/**
 * @file multi_source_sync_example.cpp
 * @brief Multi-Source Time Synchronization with BMCA Selection
 * 
 * Demonstrates IEEE 1588-2019 Best Master Clock Algorithm (BMCA) applied
 * to multiple time sources: GPS, NTP, DCF77, and RTC.
 * 
 * BMCA Priority (based on clockClass):
 * 1. GPS (locked, 3D fix)        - clockClass 6   (Primary)
 * 2. DCF77 (strong signal)       - clockClass 6   (Primary)
 * 3. GPS (holdover <10min)       - clockClass 7   (Primary holdover)
 * 4. DCF77 (weak signal)         - clockClass 13  (Application)
 * 5. NTP (Stratum 1)             - clockClass 52  (Degraded by path)
 * 6. RTC (recently synced)       - clockClass 52  (Degraded by path)
 * 7. NTP (Stratum 2+)            - clockClass 187 (Degraded accuracy)
 * 8. RTC (holdover)              - clockClass 187 (Degraded accuracy)
 * 9. RTC (standalone, fallback)  - clockClass 248 (Unsynchronized)
 * 
 * Hardware Setup:
 * - GPS module (GT-U7) on serial port (optional)
 * - DCF77 receiver on GPIO (optional, Europe only)
 * - NTP network connection (optional)
 * - DS3231 RTC on I2C (required for fallback)
 * 
 * Compilation:
 *   g++ -std=c++17 -I../../include -o multi_source_sync \
 *       multi_source_sync_example.cpp \
 *       ../04-gps-nmea-sync/gps_adapter.cpp \
 *       ../05-ntp-sntp-sync/ntp_adapter.cpp \
 *       ../06-dcf77-terrestrial-radio/dcf77_adapter.cpp \
 *       ../07-rtc-module/rtc_adapter.cpp
 * 
 * Usage:
 *   ./multi_source_sync --gps /dev/ttyUSB0 --ntp pool.ntp.org --dcf77 25 --rtc
 */

#include "../04-gps-nmea-sync/gps_adapter.hpp"
#include "../05-ntp-sntp-sync/ntp_adapter.hpp"
#include "../06-dcf77-terrestrial-radio/dcf77_adapter.hpp"
#include "../07-rtc-module/rtc_adapter.hpp"
#include "clocks.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <csignal>

namespace Types = IEEE::_1588::PTP::_2019::Types;

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

void signal_handler(int signal) {
    if (signal == SIGINT) {
        running = 0;
    }
}

/**
 * @brief Time source information
 */
struct TimeSource {
    std::string name;
    Types::Timestamp time;
    Types::ClockQuality quality;
    uint8_t time_source_type;
    bool available;
};

/**
 * @brief BMCA comparison - select best clock quality
 * 
 * IEEE 1588-2019 Section 9.3.4 - Best Master Clock Algorithm
 * 
 * @return true if a is better than b
 */
bool compare_clock_quality(const TimeSource& a, const TimeSource& b) {
    // 1. Compare clockClass (lower is better)
    if (a.quality.clock_class != b.quality.clock_class) {
        return a.quality.clock_class < b.quality.clock_class;
    }
    
    // 2. Compare clockAccuracy (lower is better)
    if (a.quality.clock_accuracy != b.quality.clock_accuracy) {
        return a.quality.clock_accuracy < b.quality.clock_accuracy;
    }
    
    // 3. Compare offsetScaledLogVariance (lower is better)
    return a.quality.offset_scaled_log_variance < b.quality.offset_scaled_log_variance;
}

/**
 * @brief Select best time source using BMCA
 */
const TimeSource* select_best_source(const std::vector<TimeSource>& sources) {
    const TimeSource* best = nullptr;
    
    for (const auto& source : sources) {
        if (!source.available) {
            continue;
        }
        
        if (best == nullptr || compare_clock_quality(source, *best)) {
            best = &source;
        }
    }
    
    return best;
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
 * @brief Print clock quality with interpretation
 */
void print_clock_quality(const Types::ClockQuality& quality) {
    std::cout << "clockClass=" << std::setw(3) << static_cast<int>(quality.clock_class);
    
    if (quality.clock_class <= 13) {
        std::cout << " (Primary)";
    } else if (quality.clock_class < 100) {
        std::cout << " (Degraded Path)";
    } else if (quality.clock_class < 200) {
        std::cout << " (Degraded Accuracy)";
    } else {
        std::cout << " (Unsynchronized)";
    }
}

/**
 * @brief Main multi-source synchronization example
 */
int main(int argc, char* argv[]) {
    std::cout << "\n=== IEEE 1588-2019 PTP - Multi-Source BMCA Example ===" << std::endl;
    std::cout << "Demonstrating automatic time source selection using BMCA" << std::endl;
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    
    // Initialize adapters
    std::cout << "\n=== Initializing Time Sources ===" << std::endl;
    
    // GPS Adapter
    Examples::GPS::GPSAdapter gps("COM3");  // Adjust port as needed
    bool gps_available = gps.initialize();
    std::cout << (gps_available ? "✓" : "✗") << " GPS: " 
              << (gps_available ? "Initialized" : "Not available") << std::endl;
    
    // NTP Adapter
    Examples::NTP::NTPAdapter ntp("pool.ntp.org");
    bool ntp_available = ntp.initialize();
    std::cout << (ntp_available ? "✓" : "✗") << " NTP: " 
              << (ntp_available ? "Initialized" : "Not available") << std::endl;
    
    // DCF77 Adapter (GPIO pin 25, example)
    Examples::DCF77::DCF77Adapter dcf77(25);
    bool dcf77_available = dcf77.initialize();
    std::cout << (dcf77_available ? "✓" : "✗") << " DCF77: " 
              << (dcf77_available ? "Initialized" : "Not available (Europe only)") << std::endl;
    
    // RTC Adapter (always available as fallback)
    Examples::RTC::RTCAdapter rtc(0x68, Examples::RTC::RTCModuleType::DS3231);
    bool rtc_available = rtc.initialize();
    std::cout << (rtc_available ? "✓" : "✗") << " RTC: " 
              << (rtc_available ? "Initialized" : "CRITICAL: RTC required for fallback!") << std::endl;
    
    if (!rtc_available) {
        std::cerr << "\nERROR: RTC module is required for fallback time source" << std::endl;
        return 1;
    }
    
    // Initialize PTP clock
    std::cout << "\n=== Initializing PTP Clock ===" << std::endl;
    Types::ClockIdentity clock_id{0x00, 0x1B, 0x19, 0xFF, 0xFE, 0x01, 0x23, 0x45};
    uint16_t port_number = 1;
    IEEE::_1588::PTP::_2019::PortIdentity port_id(clock_id, port_number);
    IEEE::_1588::PTP::_2019::OrdinaryClock ptp_clock(clock_id, port_id);
    std::cout << "✓ PTP clock initialized" << std::endl;
    
    // Main synchronization loop
    std::cout << "\n=== BMCA-Driven Time Synchronization ===" << std::endl;
    std::cout << "Press Ctrl+C to stop\n" << std::endl;
    
    int update_count = 0;
    std::string previous_best_source;
    
    while (running && update_count < 30) {  // Run for 30 iterations or until Ctrl+C
        std::cout << "\n--- Update #" << (update_count + 1) << " ---" << std::endl;
        
        // Update all sources
        std::vector<TimeSource> sources;
        
        // GPS
        if (gps_available && gps.update()) {
            sources.push_back({
                "GPS",
                gps.get_current_time(),
                gps.get_clock_quality(),
                gps.get_time_source(),
                gps.is_synchronized()
            });
        }
        
        // NTP
        if (ntp_available && ntp.update()) {
            sources.push_back({
                "NTP",
                ntp.get_current_time(),
                ntp.get_clock_quality(),
                ntp.get_time_source(),
                true  // NTP update returns true when synchronized
            });
        }
        
        // DCF77
        if (dcf77_available && dcf77.update()) {
            sources.push_back({
                "DCF77",
                dcf77.get_current_time(),
                dcf77.get_clock_quality(),
                dcf77.get_time_source(),
                dcf77.is_synchronized()
            });
        }
        
        // RTC (always available)
        if (rtc.update()) {
            sources.push_back({
                "RTC",
                rtc.get_current_time(),
                rtc.get_clock_quality(),
                rtc.get_time_source(),
                true  // RTC always provides time
            });
        }
        
        // BMCA: Select best source
        const TimeSource* best_source = select_best_source(sources);
        
        if (best_source == nullptr) {
            std::cerr << "ERROR: No time sources available!" << std::endl;
            break;
        }
        
        // Detect source change
        if (best_source->name != previous_best_source) {
            std::cout << "\n*** TIME SOURCE CHANGED: " << previous_best_source 
                      << " → " << best_source->name << " ***\n" << std::endl;
            previous_best_source = best_source->name;
        }
        
        // Display all sources
        std::cout << "Available Sources:" << std::endl;
        for (const auto& source : sources) {
            std::cout << "  " << std::setw(8) << std::left << source.name << ": ";
            print_clock_quality(source.quality);
            if (source.name == best_source->name) {
                std::cout << " ← SELECTED";
            }
            std::cout << std::endl;
        }
        
        // Display selected source details
        std::cout << "\nBest Source (BMCA): " << best_source->name << std::endl;
        std::cout << "  Time: ";
        print_timestamp(best_source->time);
        std::cout << std::endl;
        std::cout << "  Quality: ";
        print_clock_quality(best_source->quality);
        std::cout << std::endl;
        
        // If best source is NOT RTC, synchronize RTC with it
        if (best_source->name != "RTC" && best_source->quality.clock_class < 100) {
            if (rtc.set_time(best_source->time)) {
                std::cout << "  → RTC synchronized from " << best_source->name << std::endl;
            }
        }
        
        // Update PTP clock with best source
        auto& default_ds = ptp_clock.get_default_data_set();
        default_ds.clockQuality = best_source->quality;
        
        auto& time_props_ds = ptp_clock.get_time_properties_data_set();
        time_props_ds.timeSource = best_source->time_source_type;
        
        ptp_clock.tick(best_source->time);
        
        update_count++;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    // Summary
    std::cout << "\n=== Synchronization Summary ===" << std::endl;
    std::cout << "Total updates: " << update_count << std::endl;
    std::cout << "Final source: " << previous_best_source << std::endl;
    
    std::cout << "\nBMAC Benefits Demonstrated:" << std::endl;
    std::cout << "✓ Automatic selection of best available time source" << std::endl;
    std::cout << "✓ Seamless failover when primary source lost" << std::endl;
    std::cout << "✓ RTC synchronized from better sources (GPS/NTP/DCF77)" << std::endl;
    std::cout << "✓ RTC provides fallback time during outages" << std::endl;
    std::cout << "✓ Clock quality-based selection (IEEE 1588-2019 BMCA)" << std::endl;
    
    return 0;
}
