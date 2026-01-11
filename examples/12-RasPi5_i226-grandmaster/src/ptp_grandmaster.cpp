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
#include "IEEE/1588/PTP/2019/messages.hpp"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <getopt.h>
#include <cmath>

using namespace IEEE::_1588::PTP::_2019;
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
    
    // PHC discipline servo state
    struct {
        double kp = 0.7;              // Proportional gain
        double ki = 0.00003;          // Integral gain (reduced 10000x to prevent windup)
        double integral = 0.0;        // Integral accumulator
        int64_t last_offset_ns = 0;   // Last offset measurement
        uint64_t last_gps_seconds = 0;  // Last GPS time for frequency measurement
        bool locked = false;          // True when PHC is locked to GPS
        bool freq_calibrated = false; // True when iterative frequency calibration complete
        const double integral_max = 10000000000.0;  // ±10 seconds max integral (anti-windup)
        const int32_t freq_max_ppb = 500000;        // ±500ppm per adjustment (safe iterative steps)
    } phc_servo;
    
    // Fast drift tracking with 60-sample moving average (1 minute @ 1 sec intervals)
    constexpr size_t drift_buffer_size = 60;         // 60 samples = 60 seconds = 1 minute
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
    constexpr uint64_t min_adjustment_interval_sec = 600; // 10 minutes minimum between aging offset adjustments
    uint64_t last_aging_offset_adjustment_time = 0;  // GPS time of last aging offset adjustment
    bool rtc_initial_sync_done = false;              // Flag to force initial RTC sync on GPS lock
    
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

            // ═══════════════════════════════════════════════════════════════════════════
            // RTC Drift Measurement and Discipline (runs INDEPENDENTLY of PHC calibration)
            // ═══════════════════════════════════════════════════════════════════════════
            // Drift measurement based on ACTUAL elapsed GPS time (not iteration counter)
            // Loop timing varies (NMEA I/O overhead), so iteration count is unreliable
            // Use actual GPS time difference for precise 1-second drift measurements
            if (last_drift_calc_time > 0) {
                uint64_t elapsed_sec = gps_seconds - last_drift_calc_time;
                
                // Only perform drift measurement when GPS time has advanced (1+ seconds)
                if (elapsed_sec >= 1) {
                    uint64_t rtc_seconds = 0;
                    uint32_t rtc_nanoseconds = 0;
                    
                    if (rtc_adapter.get_ptp_time(&rtc_seconds, &rtc_nanoseconds)) {
                        // Calculate time error (RTC - GPS)
                        // NOTE: RTC is set to GPS+1 second (see sync_from_gps in rtc_adapter.cpp)
                        // to compensate for I2C write latency and 1-second RTC resolution.
                        // 
                        // RACE CONDITION FIX: RTC has 1-second resolution and ticks independently.
                        // Depending on timing between GPS read and RTC read, RTC might show:
                        //   - GPS second (if RTC hasn't ticked yet)
                        //   - GPS+1 second (if RTC ticked already - this is expected)
                        // 
                        // Solution: Compare RTC to whichever is closer (GPS or GPS+1)
                        int64_t rtc_time_ns = static_cast<int64_t>(rtc_seconds) * 1000000000LL + rtc_nanoseconds;
                        int64_t gps_time_ns = static_cast<int64_t>(gps_seconds) * 1000000000LL + gps_nanoseconds;
                        int64_t gps_plus1_ns = static_cast<int64_t>(gps_seconds + 1) * 1000000000LL + gps_nanoseconds;
                        
                        // Calculate error for both possibilities
                        int64_t error_vs_gps = rtc_time_ns - gps_time_ns;
                        int64_t error_vs_gps_plus1 = rtc_time_ns - gps_plus1_ns;
                        
                        // Use whichever comparison gives smaller absolute error
                        int64_t time_error_ns = (std::abs(error_vs_gps) < std::abs(error_vs_gps_plus1)) 
                                               ? error_vs_gps : error_vs_gps_plus1;
                        
                        // DEBUG: Print actual time values to verify fix
                        printf("[RTC Drift] GPS=%lu.%09u RTC=%lu.%09u err_vs_GPS=%ld err_vs_GPS+1=%ld USED=%ld (%.3fms)\n",
                               gps_seconds, gps_nanoseconds, 
                               rtc_seconds, rtc_nanoseconds,
                               error_vs_gps, error_vs_gps_plus1,
                               time_error_ns, time_error_ns / 1000000.0);
                        
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
                        // Best practice: small incremental adjustments, not full recalculation
                        // Wait minimum interval between adjustments to allow settling
                        uint64_t time_since_last_adjustment = last_aging_offset_adjustment_time > 0 
                            ? (gps_seconds - last_aging_offset_adjustment_time) : UINT64_MAX;
                        
                        if (sync_counter > 1200 && 
                            std::abs(drift_avg) > drift_tolerance_ppm &&
                            time_since_last_adjustment >= min_adjustment_interval_sec) {
                            
                            // Read current aging offset from register
                            int8_t current_offset = rtc_adapter.read_aging_offset();
                            
                            // Calculate small adjustment (±1 or ±2 LSB max)
                            // drift_avg is in ppm, each LSB = 0.1 ppm
                            int8_t adjustment = 0;
                            if (drift_avg > 0.15) {
                                adjustment = -2;  // Speeding up too much, slow down
                            } else if (drift_avg > 0.05) {
                                adjustment = -1;
                            } else if (drift_avg < -0.15) {
                                adjustment = 2;   // Slowing down too much, speed up
                            } else if (drift_avg < -0.05) {
                                adjustment = 1;
                            }
                            
                            if (adjustment != 0) {
                                int8_t new_offset = current_offset + adjustment;
                                
                                std::cout << "[RTC Discipline] ⚠ Drift " << drift_avg << " ppm exceeds ±" 
                                         << drift_tolerance_ppm << " ppm threshold\n";
                                std::cout << "[RTC Discipline] Applying incremental aging offset adjustment...\n";
                                std::cout << "[RTC Discipline] Current offset: " << static_cast<int>(current_offset) 
                                         << " LSB → New: " << static_cast<int>(new_offset) 
                                         << " LSB (Δ=" << static_cast<int>(adjustment) << ")\n";
                                
                                if (rtc_adapter.write_aging_offset(new_offset)) {
                                    std::cout << "[RTC Discipline] ✓ Aging offset adjusted: " 
                                             << static_cast<int>(new_offset) << " LSB (" 
                                             << (new_offset * 0.1) << " ppm)\n";
                                    
                                    last_aging_offset_adjustment_time = gps_seconds;
                                    
                                    // Clear buffer after frequency adjustment
                                    drift_buffer_count = 0;
                                    drift_buffer_index = 0;
                                    drift_valid = false;  // Invalidate until new measurement
                                    last_drift_calc_time = 0;  // Reset measurement baseline
                                    last_time_error_ns = 0;    // Reset error baseline
                                    std::cout << "[RTC Discipline] ℹ Drift buffer cleared (re-measuring)\n";
                                } else {
                                    std::cerr << "[RTC Discipline] ✗ Failed to apply aging offset\n";
                                }
                            }
                        }
                        
                        // Phase 2: Sync RTC time only if absolute error exceeds tolerance
                        // NOTE: RTC has 1-second resolution, so we expect ~1 sec constant offset
                        // Only sync if error changes significantly (indicates actual drift)
                        
                        // Force initial sync when GPS first locks (during warmup phase)
                        bool force_sync = !rtc_initial_sync_done;
                        bool sync_happened = false;  // Track if we synced
                        if (force_sync || std::abs(time_error_ns) > time_sync_tolerance_ns) {
                            // Check if this is just the expected 1-second quantization
                            double error_ms = time_error_ns / 1000000.0;
                            double abs_error_ms = std::abs(error_ms);
                            
                            // If error is close to 1 second (±50ms tolerance), it's just quantization
                            // BUT: always sync on first GPS lock (force_sync=true)
                            bool is_quantization_error = !force_sync && (abs_error_ms > 950.0 && abs_error_ms < 1050.0);
                            
                            if (!is_quantization_error) {
                                if (force_sync) {
                                    std::cout << "[RTC Sync] Initial sync to GPS time (error=" << error_ms << "ms)\n";
                                } else {
                                    std::cout << "[RTC Sync] ⚠ Time error " << error_ms
                                             << " ms exceeds ±" << (time_sync_tolerance_ns / 1000000.0) 
                                             << " ms threshold (not quantization)\n";
                                }
                                std::cout << "[RTC Sync] Synchronizing RTC to GPS time...\n";
                                
                                if (rtc_adapter.sync_from_gps(gps_seconds, gps_nanoseconds)) {
                                    std::cout << "[RTC Sync] ✓ RTC synchronized\n";
                                    rtc_initial_sync_done = true;  // Mark initial sync complete
                                    sync_happened = true;  // Mark that sync occurred
                                    
                                    // Clear drift buffer (measurement invalid after time jump)
                                    drift_buffer_count = 0;
                                    drift_buffer_index = 0;
                                    last_drift_calc_time = 0;  // Reset to restart measurement
                                    last_time_error_ns = 0;    // Reset error baseline (prevent false drift from old error)
                                    // NOTE: Don't clear drift_valid - keep showing last known drift
                                    std::cout << "[RTC Sync] ℹ Drift buffer cleared (time discontinuity)\n";
                                } else {
                                    std::cerr << "[RTC Sync] ✗ Failed to sync RTC\n";
                                }
                            }
                            // else: Ignore 1-second quantization error - drift measurement still valid
                        }
                        
                        // Only update baseline if we didn't just sync (sync already reset to 0)
                        if (!sync_happened) {
                            last_drift_calc_time = gps_seconds;
                            last_time_error_ns = time_error_ns;
                        }
                    } else {
                        // RTC read failed - reset measurement
                        last_drift_calc_time = 0;
                    }
                } else {
                    // Waiting for 1 second to elapse for drift measurement
                }
            } else {
                // Initialize drift measurement baseline on first GPS lock
                last_drift_calc_time = gps_seconds;
                uint64_t rtc_seconds = 0;
                uint32_t rtc_nanoseconds = 0;
                if (rtc_adapter.get_ptp_time(&rtc_seconds, &rtc_nanoseconds)) {
                    int64_t rtc_time_ns = static_cast<int64_t>(rtc_seconds) * 1000000000LL + rtc_nanoseconds;
                    int64_t gps_time_ns = static_cast<int64_t>(gps_seconds) * 1000000000LL + gps_nanoseconds;
                    int64_t gps_plus1_ns = static_cast<int64_t>(gps_seconds + 1) * 1000000000LL + gps_nanoseconds;
                    int64_t error_vs_gps = rtc_time_ns - gps_time_ns;
                    int64_t error_vs_gps_plus1 = rtc_time_ns - gps_plus1_ns;
                    last_time_error_ns = (std::abs(error_vs_gps) < std::abs(error_vs_gps_plus1)) 
                                        ? error_vs_gps : error_vs_gps_plus1;
                }
            }

            // ═══════════════════════════════════════════════════════════════════════════
            // PHC Discipline to GPS (frequency calibration then PI servo)
            // ═══════════════════════════════════════════════════════════════════════════
            // Discipline i226 PHC to GPS time
            uint64_t phc_seconds = 0;
            uint32_t phc_nanoseconds = 0;
            if (ptp_hal.get_phc_time(&phc_seconds, &phc_nanoseconds)) {
                int64_t gps_time_ns = static_cast<int64_t>(gps_seconds) * 1000000000LL + gps_nanoseconds;
                int64_t phc_time_ns = static_cast<int64_t>(phc_seconds) * 1000000000LL + phc_nanoseconds;
                int64_t offset_ns = gps_time_ns - phc_time_ns;
                
                // Initialize baseline on first GPS sample
                if (phc_servo.last_gps_seconds == 0) {
                    phc_servo.last_offset_ns = offset_ns;
                    phc_servo.last_gps_seconds = gps_seconds;
                    std::cout << "[PHC Calibration] Baseline set, will measure frequency offset in 2 seconds...\n";
                }
                
                // CRITICAL: Measure and correct frequency offset BEFORE stepping time
                // i226 PHC can have huge frequency offsets (100,000+ ppm) that cause
                // rapid drift. Must calibrate frequency first or servo will never lock.
                // Use ITERATIVE calibration: apply ±500ppm steps repeatedly until drift < 100ppm
                if (!phc_servo.freq_calibrated && phc_servo.last_gps_seconds > 0) {
                    uint64_t elapsed_sec = gps_seconds - phc_servo.last_gps_seconds;
                    
                    if (elapsed_sec >= 2) {  // Need at least 2 seconds for measurement
                        // Calculate frequency offset from drift rate
                        int64_t drift_ns = offset_ns - phc_servo.last_offset_ns;
                        double drift_ppm = (drift_ns / 1000.0) / elapsed_sec;
                        
                        // Check if still drifting significantly
                        if (std::abs(drift_ppm) > 100) {  // Still needs calibration
                            // CRITICAL FIX: Read current frequency first (read-modify-write pattern)
                            // adjust_phc_frequency() sets ABSOLUTE value, not cumulative!
                            int32_t current_freq_ppb = 0;
                            if (!ptp_hal.get_phc_frequency(&current_freq_ppb)) {
                                std::cerr << "[PHC ERROR] Failed to read current frequency!\n";
                                phc_servo.last_offset_ns = offset_ns;
                                phc_servo.last_gps_seconds = gps_seconds;
                                continue;
                            }
                            
                            // Calculate correction needed (clamped per iteration)
                            int32_t correction_ppb = static_cast<int32_t>(-drift_ppm * 1000.0);
                            if (correction_ppb > phc_servo.freq_max_ppb) {
                                correction_ppb = phc_servo.freq_max_ppb;
                            } else if (correction_ppb < -phc_servo.freq_max_ppb) {
                                correction_ppb = -phc_servo.freq_max_ppb;
                            }
                            
                            // Calculate new TOTAL frequency (cumulative)
                            int32_t new_freq_ppb = current_freq_ppb + correction_ppb;
                            
                            // Clamp total frequency to hardware limits (±500,000 ppb = ±500 ppm)
                            const int32_t max_total_freq = 500000;  // i226 hardware limit
                            if (new_freq_ppb > max_total_freq) new_freq_ppb = max_total_freq;
                            if (new_freq_ppb < -max_total_freq) new_freq_ppb = -max_total_freq;
                            
                            std::cout << "[PHC Calibration] Iteration: Measured " << drift_ppm << " ppm drift\n"
                                      << "  Current freq: " << current_freq_ppb << " ppb, "
                                      << "Correction: " << correction_ppb << " ppb, "
                                      << "New total: " << new_freq_ppb << " ppb\n";
                            
                            // Apply new TOTAL frequency (absolute set)
                            ptp_hal.adjust_phc_frequency(new_freq_ppb);
                            
                            // Reset baseline for next iteration
                            phc_servo.last_offset_ns = offset_ns;
                            phc_servo.last_gps_seconds = gps_seconds;
                            continue;  // Measure again in 2 seconds
                        } else {
                            std::cout << "[PHC Calibration] ✓ Complete! Final drift: " << drift_ppm << " ppm (acceptable)\n";
                            phc_servo.freq_calibrated = true;
                        }
                    }
                }
                
                // During frequency calibration, skip step corrections to avoid corrupting measurement
                if (!phc_servo.freq_calibrated) {
                    continue;  // Skip this iteration, let PHC drift naturally for measurement
                }
                
                // After calibration complete on first iteration, step time once to eliminate accumulated offset
                if (phc_servo.freq_calibrated && phc_servo.last_offset_ns == 0 && phc_servo.last_gps_seconds > 0) {
                    std::cout << "[PHC Calibration] Stepping time to eliminate accumulated offset from calibration\n";
                    ptp_hal.set_phc_time(gps_seconds, gps_nanoseconds);
                    phc_servo.last_offset_ns = -1;  // Mark as done (non-zero)
                    phc_servo.integral = 0.0;
                    continue;
                }
                
                // Step correction for large offsets (>100ms for faster initial lock)
                if (std::abs(offset_ns) > 100000000LL) {  // 100ms threshold
                    if (verbose) {
                        std::cout << "[PHC Discipline] Step correction: " << (offset_ns / 1000000.0) << " ms\n";
                    }
                    ptp_hal.set_phc_time(gps_seconds, gps_nanoseconds);
                    phc_servo.integral = 0.0;  // Reset integral on step
                    phc_servo.locked = false;
                } else {
                    // PI servo for smooth tracking
                    phc_servo.integral += offset_ns;
                    
                    // Anti-windup: Clamp integral to prevent accumulation
                    if (phc_servo.integral > phc_servo.integral_max) {
                        phc_servo.integral = phc_servo.integral_max;
                    } else if (phc_servo.integral < -phc_servo.integral_max) {
                        phc_servo.integral = -phc_servo.integral_max;
                    }
                    
                    // Calculate frequency adjustment
                    double adjustment = phc_servo.kp * offset_ns + phc_servo.ki * phc_servo.integral;
                    int32_t freq_ppb = static_cast<int32_t>(adjustment / 1000.0);
                    
                    // Clamp frequency adjustment to safe bounds
                    if (freq_ppb > phc_servo.freq_max_ppb) {
                        freq_ppb = phc_servo.freq_max_ppb;
                    } else if (freq_ppb < -phc_servo.freq_max_ppb) {
                        freq_ppb = -phc_servo.freq_max_ppb;
                    }
                    
                    ptp_hal.adjust_phc_frequency(freq_ppb);
                    
                    // Lock detection at 1µs threshold (looser than original 100ns)
                    if (std::abs(offset_ns) < 1000 && !phc_servo.locked) {
                        std::cout << "[PHC Discipline] ✓ Locked to GPS (offset < 1µs)\n";
                        phc_servo.locked = true;
                    } else if (std::abs(offset_ns) > 10000 && phc_servo.locked) {
                        // Lost lock if offset exceeds 10µs
                        phc_servo.locked = false;
                        if (verbose) {
                            std::cout << "[PHC Discipline] ⚠ Lock lost (offset > 10µs)\n";
                        }
                    }
                    
                    if (verbose && (sync_counter % 10 == 0)) {
                        std::cout << "[PHC Discipline] Offset: " << offset_ns << " ns, Freq adj: " << freq_ppb 
                                 << " ppb, Integral: " << (phc_servo.integral / 1000000.0) << " ms\n";
                    }
                }
                
                phc_servo.last_offset_ns = offset_ns;
            }

            // RTC drift measurement already handled earlier (before PHC calibration)
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
            // Construct IEEE 1588-2019 Announce message
            AnnounceMessage announce_msg;
            
            // Initialize common header
            PortIdentity source_port;
            
            // Derive clock identity from MAC address (IEEE 1588-2019 Section 7.5.2.2.2)
            uint8_t mac[6];
            if (ptp_hal.get_interface_mac(mac)) {
                // EUI-64 format: MAC[0:2] || 0xFF || 0xFE || MAC[3:5]
                source_port.clock_identity[0] = mac[0];
                source_port.clock_identity[1] = mac[1];
                source_port.clock_identity[2] = mac[2];
                source_port.clock_identity[3] = 0xFF;
                source_port.clock_identity[4] = 0xFE;
                source_port.clock_identity[5] = mac[3];
                source_port.clock_identity[6] = mac[4];
                source_port.clock_identity[7] = mac[5];
            } else {
                // Fallback if MAC retrieval fails
                std::memcpy(source_port.clock_identity.data(), "\x00\x00\x00\xFF\xFE\x00\x00\x01", 8);
            }
            source_port.port_number = detail::host_to_be16(1);
            
            announce_msg.initialize(MessageType::Announce, 0, source_port);
            announce_msg.header.sequenceId = detail::host_to_be16(static_cast<uint16_t>(announce_counter));
            announce_msg.header.logMessageInterval = 1; // 2 seconds = 2^1
            
            // Set Announce body from GPS clock quality
            uint8_t clock_class, clock_accuracy;
            uint16_t offset_variance;
            gps_adapter.get_ptp_clock_quality(&clock_class, &clock_accuracy, &offset_variance);
            
            announce_msg.body.grandmasterPriority1 = 128;
            announce_msg.body.grandmasterClockClass = clock_class;
            announce_msg.body.grandmasterClockAccuracy = clock_accuracy;
            announce_msg.body.grandmasterClockVariance = detail::host_to_be16(offset_variance);
            announce_msg.body.grandmasterPriority2 = 128;
            std::copy(source_port.clock_identity.begin(), source_port.clock_identity.end(), announce_msg.body.grandmasterIdentity.begin());
            announce_msg.body.stepsRemoved = detail::host_to_be16(0);
            announce_msg.body.timeSource = static_cast<uint8_t>(TimeSource::GPS); // Use repository enum
            
            // Send via HAL
            HardwareTimestamp tx_ts;
            int sent = ptp_hal.send_message(&announce_msg, announce_msg.getMessageSize(), &tx_ts);
            
            if (verbose && sent > 0) {
                std::cout << "→ Announce sent (Class=" << static_cast<int>(clock_class) 
                         << ", Acc=" << static_cast<int>(clock_accuracy) << ")\n";
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
        if (sync_counter++ % 1 == 0 && gps_available) {
            // Construct IEEE 1588-2019 Sync message
            SyncMessage sync_msg;
            
            // Initialize common header (TWO_STEP mode)
            PortIdentity source_port;
            std::memcpy(source_port.clock_identity.data(), "\x00\x00\x00\xFF\xFE\x00\x00\x01", 8);
            source_port.port_number = detail::host_to_be16(1);
            
            sync_msg.initialize(MessageType::Sync, 0, source_port);
            sync_msg.header.sequenceId = detail::host_to_be16(static_cast<uint16_t>(sync_counter));
            sync_msg.header.flagField = detail::host_to_be16(Flags::TWO_STEP); // Two-step grandmaster
            sync_msg.header.logMessageInterval = 0; // 1 second = 2^0
            
            // Origin timestamp (will be replaced by hardware TX timestamp)
            sync_msg.body.originTimestamp.setTotalSeconds(gps_seconds);
            sync_msg.body.originTimestamp.nanoseconds = gps_nanoseconds;
            
            // Send via HAL with hardware TX timestamp
            HardwareTimestamp tx_ts;
            int sent = ptp_hal.send_message(&sync_msg, sync_msg.getMessageSize(), &tx_ts);
            
            if (sent > 0) {
                // Send Follow_Up with precise TX timestamp
                FollowUpMessage followup_msg;
                followup_msg.initialize(MessageType::Follow_Up, 0, source_port);
                followup_msg.header.sequenceId = sync_msg.header.sequenceId; // Match Sync sequence
                followup_msg.header.logMessageInterval = 0;
                
                // Precise origin timestamp from hardware
                followup_msg.body.preciseOriginTimestamp.setTotalSeconds(tx_ts.seconds);
                followup_msg.body.preciseOriginTimestamp.nanoseconds = tx_ts.nanoseconds;
                
                ptp_hal.send_message(&followup_msg, followup_msg.getMessageSize(), nullptr);
                
                if (verbose) {
                    std::cout << "→ Sync + Follow_Up sent (tx=" << tx_ts.seconds << "." 
                             << std::setfill('0') << std::setw(9) << tx_ts.nanoseconds << ")\n";
                }
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
