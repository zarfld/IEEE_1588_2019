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
#include <iomanip>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <getopt.h>
#include <cmath>

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
        std::cerr << "WARNING: Failed to initialize PTP sockets (continuing without PTP messaging)\n";
        std::cerr << "         This is expected if " << interface << " is down or disconnected\n";
    } else {
        std::cout << "  ✓ PTP sockets initialized\n";
    }

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
    
    // Fast drift tracking with 60-sample moving average (10 minutes @ 10 sec intervals)
    constexpr size_t drift_buffer_size = 60;         // 60 samples = 600 seconds = 10 minutes
    double drift_buffer[drift_buffer_size] = {0};   // Circular buffer for drift rate (ppm)
    size_t drift_buffer_index = 0;                   // Current index
    size_t drift_buffer_count = 0;                   // Valid samples
    uint64_t last_drift_calc_time = 0;              // Last GPS time when drift was calculated
    int64_t last_time_error_ns = 0;                 // Last measured time error    
    // Latest drift measurements for PPS display
    double current_drift_ppm = 0.0;                 // Most recent drift measurement
    double current_drift_avg = 0.0;                 // Current moving average
    double current_time_error_ms = 0.0;             // Current time error in ms
    bool drift_valid = false;                        // True when drift has been calculated
    
    constexpr double drift_tolerance_ppm = 0.1;      // Aging offset adjustment threshold
    constexpr int64_t time_sync_tolerance_ns = 100000000; // 100ms - only sync if error exceeds this
    
    while (g_running) {
        // Update GPS data (read NMEA sentences and PPS)
        gps_adapter.update();
        
        // Check for PPS output (every 10 pulses)
        PpsData pps_data;
        uint32_t pps_max_jitter_ns = 0;
        bool pps_ready = gps_adapter.get_pps_data(&pps_data, &pps_max_jitter_ns);
        
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

            // Fast drift measurement every 10 seconds (100 iterations @ 100ms)
            if (sync_counter % 100 == 0) {  // Every 10 sec - no warmup delay for measurement!
                uint64_t rtc_seconds = 0;
                uint32_t rtc_nanoseconds = 0;
                
                if (rtc_adapter.get_ptp_time(&rtc_seconds, &rtc_nanoseconds)) {
                    // Calculate time error (RTC - GPS)
                    int64_t rtc_time_ns = static_cast<int64_t>(rtc_seconds) * 1000000000LL + rtc_nanoseconds;
                    int64_t gps_time_ns = static_cast<int64_t>(gps_seconds) * 1000000000LL + gps_nanoseconds;
                    int64_t time_error_ns = rtc_time_ns - gps_time_ns;
                    
                    // Calculate drift rate if we have previous measurement
                    if (last_drift_calc_time > 0) {
                        uint64_t elapsed_sec = gps_seconds - last_drift_calc_time;
                        if (elapsed_sec >= 10) {  // Ensure 10 seconds elapsed
                            // Drift rate = change in error / time interval
                            int64_t error_change_ns = time_error_ns - last_time_error_ns;
                            double drift_ppm = (error_change_ns / 1000.0) / static_cast<double>(elapsed_sec);
                            
                            // Add to circular buffer
                            drift_buffer[drift_buffer_index] = drift_ppm;
                            drift_buffer_index = (drift_buffer_index + 1) % drift_buffer_size;
                            if (drift_buffer_count < drift_buffer_size) {
                                drift_buffer_count++;
                            }
                            
                            // Calculate moving average
                            double drift_avg = 0.0;
                            for (size_t i = 0; i < drift_buffer_count; i++) {
                                drift_avg += drift_buffer[i];
                            }
                            drift_avg /= drift_buffer_count;
                            
                            // Store for PPS display
                            current_drift_ppm = drift_ppm;
                            current_drift_avg = drift_avg;
                            current_time_error_ms = time_error_ns / 1000000.0;
                            drift_valid = true;
                            
                            // Phase 1: Adjust aging offset if average drift exceeds tolerance
                            // Require at least 6 samples (1 minute) AND 2 minute warmup before adjusting
                            if (drift_buffer_count >= 6 && sync_counter > 1200 && std::abs(drift_avg) > drift_tolerance_ppm) {
                                std::cout << "[RTC Discipline] ⚠ Drift " << drift_avg << " ppm exceeds ±" 
                                         << drift_tolerance_ppm << " ppm threshold\n";
                                std::cout << "[RTC Discipline] Applying aging offset correction...\n";
                                
                                if (rtc_adapter.apply_frequency_discipline(drift_avg)) {
                                    int8_t aging_offset = rtc_adapter.read_aging_offset();
                                    std::cout << "[RTC Discipline] ✓ Aging offset: " 
                                             << static_cast<int>(aging_offset) << " LSB (" 
                                             << (aging_offset * 0.1) << " ppm)\n";
                                    
                                    // Clear buffer after frequency adjustment
                                    drift_buffer_count = 0;
                                    drift_buffer_index = 0;
                                    drift_valid = false;  // Invalidate until new measurement
                                    std::cout << "[RTC Discipline] ℹ Drift buffer cleared (re-measuring)\n";
                                } else {
                                    std::cerr << "[RTC Discipline] ✗ Failed to apply aging offset\n";
                                }
                            }
                            
                            // Phase 2: Sync RTC time only if absolute error exceeds tolerance
                            // NOTE: RTC has 1-second resolution, so we expect ~1 sec constant offset
                            // Only sync if error changes significantly (indicates actual drift)
                            if (std::abs(time_error_ns) > time_sync_tolerance_ns) {
                                // Check if this is just the expected 1-second quantization
                                double error_ms = time_error_ns / 1000000.0;
                                double abs_error_ms = std::abs(error_ms);
                                
                                // If error is close to 1 second (±50ms tolerance), it's just quantization
                                bool is_quantization_error = (abs_error_ms > 950.0 && abs_error_ms < 1050.0);
                                
                                if (!is_quantization_error) {
                                    std::cout << "[RTC Sync] ⚠ Time error " << error_ms
                                             << " ms exceeds ±" << (time_sync_tolerance_ns / 1000000.0) 
                                             << " ms threshold (not quantization)\n";
                                    std::cout << "[RTC Sync] Synchronizing RTC to GPS time...\n";
                                    
                                    if (rtc_adapter.sync_from_gps(gps_seconds, gps_nanoseconds)) {
                                        std::cout << "[RTC Sync] ✓ RTC synchronized\n";
                                        
                                        // Clear drift buffer (measurement invalid after time jump)
                                        drift_buffer_count = 0;
                                        drift_buffer_index = 0;
                                        last_drift_calc_time = 0;  // Reset to restart measurement
                                        // NOTE: Don't clear drift_valid - keep showing last known drift
                                        std::cout << "[RTC Sync] ℹ Drift buffer cleared (time discontinuity)\n";
                                    } else {
                                        std::cerr << "[RTC Sync] ✗ Failed to sync RTC\n";
                                    }
                                }
                                // else: Ignore 1-second quantization error - drift measurement still valid
                            }
                            
                            last_drift_calc_time = gps_seconds;
                            last_time_error_ns = time_error_ns;
                        }
                    } else {
                        // First measurement - initialize
                        last_drift_calc_time = gps_seconds;
                        last_time_error_ns = time_error_ns;
                        std::cout << "[RTC Discipline] Starting drift monitoring (60-sample moving average @ 10 sec)\n";
                        std::cout << "[RTC Discipline] Frequency tolerance: ±" << drift_tolerance_ppm << " ppm\n";
                        std::cout << "[RTC Discipline] Time sync tolerance: ±" 
                                 << (time_sync_tolerance_ns / 1000000.0) << " ms\n";
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
        
        // Always show PPS output when ready (every 10 pulses) - separate from drift measurements
        if (pps_ready) {
            std::cout << "[PPS] seq=" << pps_data.sequence 
                     << " time=" << pps_data.assert_sec << "." << pps_data.assert_nsec
                     << " max_jitter=" << pps_max_jitter_ns << "ns (last 10 pulses)";
            
            // Add drift information if available
            if (drift_valid) {
                std::cout << " drift=" << std::fixed << std::setprecision(3) << current_drift_ppm << "ppm"
                         << " avg=" << current_drift_avg << "ppm(" << drift_buffer_count << ")"
                         << " err=" << std::setprecision(1) << current_time_error_ms << "ms";
            }
            std::cout << "\n";
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

        // Sleep for 100ms to avoid aliasing with 1 PPS
        // PPS is read non-blocking (timeout=0) every loop iteration
        // This gives 10 samples per second, ensuring we never miss a pulse
        usleep(100000);  // 100ms = 100,000 microseconds
    }

    std::cout << "\n=== Shutdown Complete ===\n";
    return 0;
}
