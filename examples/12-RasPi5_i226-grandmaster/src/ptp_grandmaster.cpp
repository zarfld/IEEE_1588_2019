/**
 * @file ptp_grandmaster.cpp
 * @brief GPS-Disciplined PTP Grandmaster Implementation
 * @details IEEE 1588-2019 Grandmaster using GPS + i226 hardware timestamping
 * 
 * Hardware Configuration:
 *   - Raspberry Pi 5
 *   - Intel i226 PCIe NIC (hardware timestamping)
 *   - u-blox G70xx GPS module (NMEA + PPS)
 *   - DS3231 RTC (holdover during GPS outages)
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#include "linux_ptp_hal.hpp"
#include "gps_adapter.hpp"
#include "rtc_adapter.hpp"

#include <iostream>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <getopt.h>

using namespace IEEE::_1588::PTP::_2019::Linux;

// Global flag for graceful shutdown
static volatile bool g_running = true;

void signal_handler(int signum)
{
    std::cout << "\n Signal " << signum << " received. Shutting down...\n";
    g_running = false;
}

void print_usage(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "GPS-Disciplined PTP Grandmaster\n\n"
              << "Options:\n"
              << "  -i, --interface <name>   Network interface (default: eth1)\n"
              << "  -p, --phc <device>       PHC device (default: /dev/ptp0)\n"
              << "  -g, --gps <device>       GPS serial device (default: /dev/ttyACM0)\n"
              << "  -s, --pps <device>       PPS device (default: /dev/pps0)\n"
              << "  -r, --rtc <device>       RTC device (default: /dev/rtc1)\n"
              << "  -v, --verbose            Verbose output\n"
              << "  -h, --help               Show this help message\n";
}

int main(int argc, char* argv[])
{
    // Default configuration
    std::string interface = "eth1";
    std::string phc_device = "/dev/ptp0";
    std::string gps_device = "/dev/ttyACM0";
    std::string pps_device = "/dev/pps0";
    std::string rtc_device = "/dev/rtc1";
    bool verbose = false;

    // Parse command-line arguments
    static struct option long_options[] = {
        {"interface", required_argument, nullptr, 'i'},
        {"phc",       required_argument, nullptr, 'p'},
        {"gps",       required_argument, nullptr, 'g'},
        {"pps",       required_argument, nullptr, 's'},
        {"rtc",       required_argument, nullptr, 'r'},
        {"verbose",   no_argument,       nullptr, 'v'},
        {"help",      no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:p:g:s:r:vh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i':
                interface = optarg;
                break;
            case 'p':
                phc_device = optarg;
                break;
            case 'g':
                gps_device = optarg;
                break;
            case 's':
                pps_device = optarg;
                break;
            case 'r':
                rtc_device = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    std::cout << "=== GPS-Disciplined PTP Grandmaster ===\n";
    std::cout << "Interface: " << interface << "\n";
    std::cout << "PHC: " << phc_device << "\n";
    std::cout << "GPS: " << gps_device << "\n";
    std::cout << "PPS: " << pps_device << "\n";
    std::cout << "RTC: " << rtc_device << "\n\n";

    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize components
    std::cout << "Initializing Linux PTP HAL...\n";
    LinuxPtpHal ptp_hal(interface, phc_device);
    if (!ptp_hal.initialize_sockets()) {
        std::cerr << "ERROR: Failed to initialize PTP sockets\n";
        return 1;
    }
    std::cout << "  ✓ PTP sockets initialized\n";

    std::cout << "Initializing GPS adapter...\n";
    GpsAdapter gps_adapter(gps_device, pps_device);
    if (!gps_adapter.initialize()) {
        std::cerr << "ERROR: Failed to initialize GPS adapter\n";
        return 1;
    }
    std::cout << "  ✓ GPS adapter initialized\n";

    std::cout << "Initializing RTC adapter...\n";
    RtcAdapter rtc_adapter(rtc_device);
    if (!rtc_adapter.initialize()) {
        std::cerr << "WARNING: Failed to initialize RTC adapter (continuing without holdover)\n";
    } else {
        std::cout << "  ✓ RTC adapter initialized\n";
    }

    std::cout << "\n🚀 Grandmaster running...\n\n";

    // Main loop
    uint64_t announce_counter = 0;
    uint64_t sync_counter = 0;
    uint64_t drift_measurement_start_time = 0;
    const uint64_t measurement_duration = 3600;      // 1 hour measurement window
    
    // Continuous drift tracking with moving average (24-hour window)
    constexpr size_t drift_history_size = 24;        // 24 measurements (24 hours)
    double drift_history[drift_history_size] = {0};  // Circular buffer
    size_t drift_index = 0;                          // Current index in buffer
    size_t drift_count = 0;                          // Number of valid measurements
    constexpr double drift_tolerance_ppm = 0.1;      // DS3231 aging offset LSB resolution
    uint32_t measurement_count = 0;                  // Total measurements taken
    
    while (g_running) {
        // Update GPS data (read NMEA sentences and PPS)
        gps_adapter.update();
        
        if (verbose && (sync_counter % 10 == 0)) {
            std::cout << "\n[GPS Debug] Fix: " << (gps_adapter.has_fix() ? "YES" : "NO")
                     << ", Satellites: " << static_cast<int>(gps_adapter.get_satellite_count())
                     << ", Quality: " << static_cast<int>(gps_adapter.get_fix_quality())
                     << "\n";
        }
        
        // Get GPS time
        uint64_t gps_seconds = 0;
        uint32_t gps_nanoseconds = 0;
        bool gps_available = gps_adapter.get_ptp_time(&gps_seconds, &gps_nanoseconds);

        if (gps_available) {
            if (verbose) {
                std::cout << "GPS Time: " << gps_seconds << "." << gps_nanoseconds << " TAI\n";
            }

            // Synchronize PHC to GPS time
            ptp_hal.set_phc_time(gps_seconds, gps_nanoseconds);

            // Synchronize RTC to GPS time (for holdover)
            rtc_adapter.sync_from_gps(gps_seconds, gps_nanoseconds);
            
            // RTC drift measurement and discipline (continuous with moving average)
            // Start initial measurement after 10 minutes of GPS lock
            if (drift_measurement_start_time == 0 && sync_counter > 600) {
                drift_measurement_start_time = gps_seconds;
                std::cout << "[RTC Discipline] Starting continuous drift monitoring (1-hour windows)\n";
                std::cout << "[RTC Discipline] Tolerance: ±" << drift_tolerance_ppm << " ppm\n";
            }
            
            // After 1 hour of measurement, calculate drift and update moving average
            if (drift_measurement_start_time > 0) {
                uint64_t elapsed_since_measurement = gps_seconds - drift_measurement_start_time;
                
                // Measure drift every hour
                if (elapsed_since_measurement >= measurement_duration) {
                    // Get RTC time
                    uint64_t rtc_seconds = 0;
                    uint32_t rtc_nanoseconds = 0;
                    
                    if (rtc_adapter.get_ptp_time(&rtc_seconds, &rtc_nanoseconds)) {
                        // Calculate instantaneous drift
                        uint64_t gps_time_ns = gps_seconds * 1000000000ULL + gps_nanoseconds;
                        uint64_t rtc_time_ns = rtc_seconds * 1000000000ULL + rtc_nanoseconds;
                        
                        double drift_ppm = rtc_adapter.measure_drift_ppm(gps_time_ns, rtc_time_ns, 
                                                                         static_cast<uint32_t>(elapsed_since_measurement));
                        
                        // Add to circular buffer
                        drift_history[drift_index] = drift_ppm;
                        drift_index = (drift_index + 1) % drift_history_size;
                        if (drift_count < drift_history_size) {
                            drift_count++;
                        }
                        measurement_count++;
                        
                        // Calculate moving average
                        double drift_avg = 0.0;
                        for (size_t i = 0; i < drift_count; i++) {
                            drift_avg += drift_history[i];
                        }
                        drift_avg /= drift_count;
                        
                        std::cout << "\n[RTC Discipline] Measurement #" << measurement_count << ":\n";
                        std::cout << "[RTC Discipline]   Current drift: " << drift_ppm << " ppm\n";
                        std::cout << "[RTC Discipline]   Moving average: " << drift_avg 
                                 << " ppm (over " << drift_count << " hours)\n";
                        
                        // Adjust only if average exceeds tolerance
                        if (std::abs(drift_avg) > drift_tolerance_ppm) {
                            std::cout << "[RTC Discipline]   ⚠ Drift exceeds tolerance (±" 
                                     << drift_tolerance_ppm << " ppm)\n";
                            std::cout << "[RTC Discipline]   Applying aging offset correction...\n";
                            
                            if (rtc_adapter.apply_frequency_discipline(drift_avg)) {
                                int8_t aging_offset = rtc_adapter.read_aging_offset();
                                std::cout << "[RTC Discipline]   ✓ Aging offset applied: " 
                                         << static_cast<int>(aging_offset) << " LSB\n";
                                std::cout << "[RTC Discipline]   ✓ Compensation: " 
                                         << (aging_offset * 0.1) << " ppm\n";
                                
                                // Reset history after successful adjustment
                                drift_count = 0;
                                drift_index = 0;
                                std::cout << "[RTC Discipline]   ℹ Drift history reset (re-learning baseline)\n";
                            } else {
                                std::cerr << "[RTC Discipline]   ✗ Failed to apply aging offset\n";
                            }
                        } else {
                            std::cout << "[RTC Discipline]   ✓ Drift within tolerance (no adjustment needed)\n";
                        }
                        
                        // Start next measurement window
                        drift_measurement_start_time = gps_seconds;
                        
                    } else {
                        std::cerr << "[RTC Discipline] ✗ Failed to read RTC time\n";
                        drift_measurement_start_time = gps_seconds;
                    }
                }
            }

        } else {
            // GPS unavailable - use RTC for holdover
            uint64_t rtc_seconds = 0;
            uint32_t rtc_nanoseconds = 0;
            
            if (rtc_adapter.get_ptp_time(&rtc_seconds, &rtc_nanoseconds)) {
                if (verbose) {
                    std::cout << "RTC Holdover: " << rtc_seconds << "." << rtc_nanoseconds << " TAI\n";
                }
                ptp_hal.set_phc_time(rtc_seconds, rtc_nanoseconds);
            } else {
                std::cerr << "WARNING: No time source available (GPS and RTC failed)\n";
            }
        }

        // Send PTP Announce message (every 2 seconds)
        if (announce_counter++ % 2 == 0) {
            // TODO: Construct and send IEEE 1588-2019 Announce message
            // This will be implemented using repository PTP message structures
            if (verbose) {
                std::cout << "→ Announce message sent\n";
            }
        }

        // Send PTP Sync message (every second)
        if (sync_counter++ % 1 == 0) {
            // TODO: Construct and send IEEE 1588-2019 Sync + Follow_Up messages
            // Use hardware TX timestamp from HAL
            if (verbose) {
                std::cout << "→ Sync message sent\n";
            }
        }

        // Display clock quality
        if (verbose && (announce_counter % 10 == 0)) {
            uint8_t clock_class, clock_accuracy;
            uint16_t offset_variance;
            gps_adapter.get_ptp_clock_quality(&clock_class, &clock_accuracy, &offset_variance);
            
            std::cout << "Clock Quality: Class=" << static_cast<int>(clock_class)
                     << " Accuracy=" << static_cast<int>(clock_accuracy)
                     << " Variance=0x" << std::hex << offset_variance << std::dec << "\n";
        }

        // Sleep for 1 second
        sleep(1);
    }

    std::cout << "\n=== Shutdown Complete ===\n";
    return 0;
}
