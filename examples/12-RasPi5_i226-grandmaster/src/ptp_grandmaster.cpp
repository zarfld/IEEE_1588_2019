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
#include <pthread.h>
#include <sched.h>
#include <atomic>
#include <mutex>
#include <condition_variable>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Linux;

// Global flag for graceful shutdown (atomic for thread safety)
static std::atomic<bool> g_running{true};

// Shared data between RT thread and worker thread
struct SharedTimingData {
    std::mutex mutex;
    std::condition_variable cv;
    
    // PHC calibration results (written by RT thread, read by worker)
    int64_t phc_at_pps_ns{0};
    uint64_t pps_sequence{0};
    bool phc_sample_valid{false};
    
    // GPS time (written by worker thread, read by RT thread)
    uint64_t gps_seconds{0};
    uint32_t gps_nanoseconds{0};
    bool gps_available{false};
    
    // PPS data (read by RT thread)
    PpsData pps_data{};
    uint32_t pps_max_jitter_ns{0};
    bool pps_ready{false};
};

void signal_handler(int signum)
{
    std::cout << "\n Signal " << signum << " received. Shutting down...\n";
    g_running = false;
}

/**
 * @brief RT Thread Arguments
 * @details Passed to rt_thread_func for PPS/PHC access
 */
struct RtThreadArgs {
    time_t pps_handle;              ///< PPS handle from GpsAdapter
    LinuxPtpHal* ptp_hal;          ///< HAL for get_phc_sys_offset()
    SharedTimingData* shared;       ///< Coordination data
    std::atomic<bool>* running;     ///< Shutdown flag
};

/**
 * @brief Worker Thread Arguments
 * @details Passed to worker_thread_func for GPS/RTC access
 */
struct WorkerThreadArgs {
    GpsAdapter* gps_adapter;        ///< GPS adapter for NMEA updates
    SharedTimingData* shared;       ///< Coordination data
    std::atomic<bool>* running;     ///< Shutdown flag
};

/**
 * RT Thread: PPS monitoring + PHC sampling (CPU2, FIFO 80)
 * 
 * Per deb.md specification - mapped to OUR codebase:
 * - Uses: pps_handle_ with time_pps_fetch() [OUR existing API]
 * - Uses: LinuxPtpHal::get_phc_sys_offset() [OUR existing API]
 * - No blocking I/O except PPS wait
 * - No NMEA parsing, no I2C, no malloc
 * 
 * CRITICAL PATH - Must execute with minimal latency:
 * 1. time_pps_fetch() - wait for PPS edge
 * 2. IMMEDIATELY sample PHC via get_phc_sys_offset()
 * 3. Push observation to ring buffer for RTC thread
 * 
 * Target: < 10ms latency from PPS edge to PHC sample
 */
void* rt_thread_func(void* arg) {
    RtThreadArgs* args = static_cast<RtThreadArgs*>(arg);
    
    // Set thread name for debugging
    pthread_setname_np(pthread_self(), "ptp_rt");
    
    std::cout << "[RT Thread] Started on CPU" << sched_getcpu() << " (priority FIFO 80)\n";
    
    // Verify PPS handle is valid
    if (args->pps_handle < 0) {
        std::cerr << "[RT Thread] ERROR: Invalid PPS handle\n";
        return nullptr;
    }
    
    // Statistics for monitoring
    uint64_t pps_count = 0;
    uint64_t phc_sample_count = 0;
    uint64_t timeout_count = 0;
    
    // Track last PPS sequence to detect new events
    uint64_t last_pps_sequence = 0;
    
    while (*args->running) {
        // Wait for PPS edge (10ms timeout for responsive shutdown)
        pps_info_t pps_info;
        struct timespec timeout = {0, 10000000};  // 10ms
        
        int ret = time_pps_fetch(args->pps_handle, PPS_TSFMT_TSPEC, &pps_info, &timeout);
        
        if (ret == 0 && pps_info.assert_sequence > last_pps_sequence) {
            pps_count++;
            last_pps_sequence = pps_info.assert_sequence;
            
            // CRITICAL: Sample PHC IMMEDIATELY after PPS event
            int64_t phc_ns, sys_ns;
            if (args->ptp_hal->get_phc_sys_offset(&phc_ns, &sys_ns)) {
                phc_sample_count++;
                
                // Calculate PHC time at PPS edge (extrapolate backwards)
                int64_t pps_sys_ns = (int64_t)pps_info.assert_timestamp.tv_sec * 1000000000LL 
                                   + pps_info.assert_timestamp.tv_nsec;
                int64_t sampling_latency_ns = sys_ns - pps_sys_ns;
                int64_t phc_at_pps = phc_ns - sampling_latency_ns;
                
                // Update shared data (with mutex)
                {
                    std::lock_guard<std::mutex> lock(args->shared->mutex);
                    args->shared->phc_at_pps_ns = phc_at_pps;
                    args->shared->pps_sequence = pps_info.assert_sequence;
                    args->shared->pps_data.assert_sec = static_cast<uint64_t>(pps_info.assert_timestamp.tv_sec);
                    args->shared->pps_data.assert_nsec = static_cast<uint32_t>(pps_info.assert_timestamp.tv_nsec);
                    args->shared->pps_data.sequence = pps_info.assert_sequence;
                    args->shared->phc_sample_valid = true;
                    args->shared->cv.notify_all();
                }
                
                // Monitor latency (warn if > 10ms)
                if (sampling_latency_ns > 10000000LL) {  // > 10ms
                    std::cerr << "[RT Thread] ⚠️  Sampling latency: " 
                             << (sampling_latency_ns / 1000000.0) << " ms\n";
                }
            }
        } else if (ret != 0 && errno != ETIMEDOUT) {
            // Log errors (except timeouts which are normal)
            std::cerr << "[RT Thread] time_pps_fetch error: " << strerror(errno) << "\n";
        } else if (ret != 0) {
            timeout_count++;
        }
    }
    
    std::cout << "[RT Thread] Shutdown (PPS: " << pps_count 
             << ", PHC samples: " << phc_sample_count
             << ", Timeouts: " << timeout_count << ")\n";
    return nullptr;
}

/**
 * Worker Thread: GPS/RTC/PTP messaging (CPU0/1/3)
 * 
 * Per deb.md specification - mapped to OUR codebase:
 * - Uses: GpsAdapter::update() [OUR existing API]
 * - Uses: RtcAdapter methods [OUR existing API]
 * - Uses: poll() on serial FD for non-blocking reads
 * - Updates atomic GPS label for RT thread consumption
 * 
 * NON-CRITICAL PATH - Can tolerate delays:
 * 1. GPS serial read/parse (blocking OK - won't affect RT thread)
 * 2. RTC drift measurement (consume observation buffer)
 * 3. PTP message transmission
 * 4. Logging
 */
void* worker_thread_func(void* arg) {
    WorkerThreadArgs* args = static_cast<WorkerThreadArgs*>(arg);
    
    // Set thread name
    pthread_setname_np(pthread_self(), "ptp_worker");
    
    std::cout << "[Worker Thread] Started on CPU" << sched_getcpu() << "\n";
    
    while (*args->running) {
        // Read GPS NMEA data and parse (blocking on serial I/O is OK here)
        args->gps_adapter->update();
        
        // Update shared GPS time if we have a valid fix
        if (args->gps_adapter->has_fix()) {
            uint64_t gps_sec = 0;
            uint32_t gps_ns = 0;
            
            if (args->gps_adapter->get_ptp_time(&gps_sec, &gps_ns)) {
                // Update shared data (mutex-protected)
                std::lock_guard<std::mutex> lock(args->shared->mutex);
                args->shared->gps_seconds = gps_sec;
                args->shared->gps_nanoseconds = gps_ns;
                args->shared->gps_available = true;
                args->shared->cv.notify_all();
            }
        } else {
            // No GPS fix - mark as unavailable
            std::lock_guard<std::mutex> lock(args->shared->mutex);
            args->shared->gps_available = false;
        }
        
        // Sleep briefly to avoid busy-waiting (100ms update rate)
        usleep(100000);
    }
    
    std::cout << "[Worker Thread] Shutdown\n";
    return nullptr;
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
    std::cout << "RTC: " << rtc_device << "\n";
    std::cout << "\nℹ️  TAI-UTC offset is automatically retrieved from kernel via adjtimex()\n";
    std::cout << "   To verify/set: adjtimex --print (shows 'tai' field)\n\n";
    
    // EXPERT ADVICE (deb.md): Verify PHC device mapping to interface
    // Check sysfs: /sys/class/net/<if>/ptp should point to our PHC device
    std::cout << "⚠️  IMPORTANT: Verify PHC mapping with:\n";
    std::cout << "   readlink -f /sys/class/net/" << interface << "/ptp\n";
    std::cout << "   (should show: /sys/class/ptp/" << phc_device.substr(5) << ")\n\n";

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

    // Shared data for thread coordination
    SharedTimingData shared_data;
    
    // Launch RT thread with SCHED_FIFO priority and CPU2 affinity
    std::cout << "\nLaunching RT thread (CPU2, FIFO 80)...\n";
    
    RtThreadArgs rt_args = {
        .pps_handle = gps_adapter.get_pps_handle(),
        .ptp_hal = &ptp_hal,
        .shared = &shared_data,
        .running = &g_running
    };
    
    pthread_t rt_thread;
    pthread_attr_t rt_attr;
    pthread_attr_init(&rt_attr);
    
    // Set FIFO scheduling with priority 80
    struct sched_param rt_param;
    rt_param.sched_priority = 80;
    pthread_attr_setschedpolicy(&rt_attr, SCHED_FIFO);
    pthread_attr_setschedparam(&rt_attr, &rt_param);
    
    // Pin to CPU2 (isolated from USB IRQs)
    cpu_set_t rt_cpuset;
    CPU_ZERO(&rt_cpuset);
    CPU_SET(2, &rt_cpuset);
    pthread_attr_setaffinity_np(&rt_attr, sizeof(rt_cpuset), &rt_cpuset);
    
    if (pthread_create(&rt_thread, &rt_attr, rt_thread_func, &rt_args) != 0) {
        std::cerr << "ERROR: Failed to create RT thread: " << strerror(errno) << "\n";
        pthread_attr_destroy(&rt_attr);
        return 1;
    }
    pthread_attr_destroy(&rt_attr);
    std::cout << "  ✓ RT thread launched\n";

    // Launch worker thread with normal priority and CPU0/1/3 affinity
    std::cout << "Launching worker thread (CPU0/1/3, normal priority)...\n";
    
    WorkerThreadArgs worker_args = {
        .gps_adapter = &gps_adapter,
        .shared = &shared_data,
        .running = &g_running
    };
    
    pthread_t worker_thread;
    pthread_attr_t worker_attr;
    pthread_attr_init(&worker_attr);
    
    // Pin to CPUs 0, 1, 3 (away from RT thread on CPU2)
    cpu_set_t worker_cpuset;
    CPU_ZERO(&worker_cpuset);
    CPU_SET(0, &worker_cpuset);
    CPU_SET(1, &worker_cpuset);
    CPU_SET(3, &worker_cpuset);
    pthread_attr_setaffinity_np(&worker_attr, sizeof(worker_cpuset), &worker_cpuset);
    
    if (pthread_create(&worker_thread, &worker_attr, worker_thread_func, &worker_args) != 0) {
        std::cerr << "ERROR: Failed to create worker thread: " << strerror(errno) << "\n";
        pthread_attr_destroy(&worker_attr);
        return 1;
    }
    pthread_attr_destroy(&worker_attr);
    std::cout << "  ✓ Worker thread launched\n";

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
        int32_t cumulative_freq_ppb = 0;  // SOFTWARE TRACKING: Total frequency adjustment applied
        
        // PPS-based frequency measurement (expert-recommended approach)
        uint32_t baseline_pps_seq = 0;     // PPS sequence number at start of measurement
        int64_t baseline_phc_ns = 0;       // PHC time (ns) at baseline PPS edge
        uint32_t calib_interval_pulses = 20; // Measure over 20 PPS pulses (20 seconds)
        
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
        // Check for PPS output (every 10 pulses)
        // Note: GPS NMEA updates now handled by worker thread
        PpsData pps_data;
        uint32_t pps_max_jitter_ns = 0;
        bool pps_ready = gps_adapter.get_pps_data(&pps_data, &pps_max_jitter_ns);
        
        // Get PHC sample from RT thread (low-latency PPS+PHC capture)
        // RT thread samples PHC immediately after PPS edge with <10ms latency
        int64_t phc_at_pps_ns = 0;
        bool phc_sample_valid = false;
        if (pps_ready && pps_data.sequence > 0) {
            // Read RT thread's PHC sample (mutex-protected)
            std::lock_guard<std::mutex> lock(shared_data.mutex);
            
            // Verify RT thread has matching PPS sequence
            if (shared_data.phc_sample_valid && shared_data.pps_sequence == pps_data.sequence) {
                phc_at_pps_ns = shared_data.phc_at_pps_ns;
                phc_sample_valid = true;
                
                // RT thread captured this sample with low latency
                // No need for HIGH LATENCY warning - RT thread already monitors this
            }
        }
        
        if (verbose && (sync_counter % 10 == 0)) {
            std::cout << "\n[GPS Debug] Fix: " << (gps_adapter.has_fix() ? "YES" : "NO")
                     << ", Satellites: " << static_cast<int>(gps_adapter.get_satellite_count())
                     << ", Quality: " << static_cast<int>(gps_adapter.get_fix_quality())
                     << "\n";
        }
        
        // Get GPS time from shared data (updated by worker thread)
        uint64_t gps_seconds = 0;
        uint32_t gps_nanoseconds = 0;
        bool gps_available = false;
        {
            std::lock_guard<std::mutex> lock(shared_data.mutex);
            gps_seconds = shared_data.gps_seconds;
            gps_nanoseconds = shared_data.gps_nanoseconds;
            gps_available = shared_data.gps_available;
        }

        if (gps_available) {
            // Rate-limit GPS time logging to once per second (reduces console I/O overhead)
            static uint64_t last_gps_log_time = 0;
            if (verbose && gps_seconds != last_gps_log_time) {
                std::cout << "GPS Time: " << gps_seconds << "." << gps_nanoseconds << " TAI\n";
                last_gps_log_time = gps_seconds;
            }

            // ═══════════════════════════════════════════════════════════════════════════
            // RTC Drift Measurement and Discipline (runs INDEPENDENTLY of PHC calibration)
            // ═══════════════════════════════════════════════════════════════════════════
            // EXPERT FIX (deb.md): Use integer-seconds reference from PPS-UTC mapping
            // RTC has 1-second resolution, so compare against UTC second boundary (not fractional GPS time)
            // Discontinuities (>100ms) trigger buffer reset and skip (prevent contamination)
            if (last_drift_calc_time > 0 && pps_ready) {
                uint64_t elapsed_sec = gps_seconds - last_drift_calc_time;
                
                // Only perform drift measurement when GPS time has advanced (1+ seconds)
                if (elapsed_sec >= 1) {
                    uint64_t rtc_seconds = 0;
                    uint32_t rtc_nanoseconds = 0;
                    
                    // Declare variables that will be used after the get_base_mapping block
                    int64_t time_error_ns = 0;
                    double drift_avg = 0.0;
                    
                    if (rtc_adapter.get_ptp_time(&rtc_seconds, &rtc_nanoseconds)) {
                        // EXPERT FIX: Use expected UTC second from PPS-UTC mapping (integer seconds domain)
                        // This is the CORRECT reference for a 1Hz RTC (DS3231)
                        uint64_t expected_utc_sec_at_pps = 0;
                        if (gps_adapter.get_base_mapping(&expected_utc_sec_at_pps)) {
                            static bool first_mapping_success = true;
                            if (first_mapping_success) {
                                std::cout << "[RTC Drift] ✓ Base mapping available, starting drift measurement\n";
                                first_mapping_success = false;
                            }
                            // expected_utc_sec_at_pps is already the UTC second for current PPS
                            // Convert to TAI for comparison with RTC (which is set to TAI)
                            expected_utc_sec_at_pps += 37;  // TAI-UTC offset
                            
                            // Compare RTC (integer seconds) to expected second
                            // RTC might read same second or next second depending on read latency
                            int64_t err_vs_exp = static_cast<int64_t>(rtc_seconds) - static_cast<int64_t>(expected_utc_sec_at_pps);
                            int64_t err_vs_exp_plus1 = static_cast<int64_t>(rtc_seconds) - static_cast<int64_t>(expected_utc_sec_at_pps + 1);
                            
                            // Choose closest
                            int64_t error_sec = (std::llabs(err_vs_exp) <= std::llabs(err_vs_exp_plus1)) ? err_vs_exp : err_vs_exp_plus1;
                            
                            // EXPERT FIX: Discontinuity detection (>= 1 second = RTC is off)
                            if (std::llabs(error_sec) >= 1) {
                                std::cout << "[RTC Discontinuity] ⚠️ RTC off by " << error_sec << " second(s)\n"
                                         << "  RTC: " << rtc_seconds << " TAI\n"
                                         << "  Expected: " << expected_utc_sec_at_pps << " TAI\n"
                                         << "  → Resetting drift buffer and skipping this sample\n";
                                
                                // Reset ring buffer
                                drift_buffer_count = 0;
                                drift_buffer_index = 0;
                                drift_valid = false;
                                
                                // Optionally step RTC (for now, just log)
                                // rtc_adapter.sync_from_gps(expected_utc_sec_at_pps - 37, 0);  // Convert back to UTC
                                
                                last_drift_calc_time = gps_seconds;
                                // last_time_error_ns is intentionally NOT updated (no valid baseline yet)
                            } else {
                            
                                // RTC aligned to correct second (error_sec == 0)
                                // Sub-second error tracking uses nanoseconds field (0 for DS3231)
                                time_error_ns = static_cast<int64_t>(rtc_nanoseconds);
                                
                                // EXPERT FIX: Require baseline sample after reset
                                if (!drift_valid || drift_buffer_count == 0) {
                                    // First valid sample after reset - establish baseline
                                    last_time_error_ns = time_error_ns;
                                    last_drift_calc_time = gps_seconds;
                                    drift_valid = false;  // Still not valid until we have drift calculation
                                    std::cout << "[RTC Drift] Baseline established: " << time_error_ns << " ns\n";
                                } else {
                            
                                    // Calculate drift: change in error over time interval
                                    int64_t error_change_ns = time_error_ns - last_time_error_ns;
                                    double drift_ppm = (error_change_ns / 1000.0) / static_cast<double>(elapsed_sec);
                                    
                                    static bool first_drift_calc = true;
                                    if (first_drift_calc) {
                                        std::cout << "[RTC Drift] ℹ️ First drift calculation: " << drift_ppm << " ppm\n";
                                        first_drift_calc = false;
                                    }
                                    
                                    // Sanity check: drift should be small (sub-ppm for DS3231)
                                    // If >100 ppm, something is wrong (maybe sub-second rollover artifact)
                                    if (std::abs(drift_ppm) > 100.0) {
                                        std::cout << "[RTC Drift] ⚠️ Suspicious drift " << drift_ppm << " ppm (>100 ppm)\n"
                                                 << "  → Resetting drift buffer\n";
                                        drift_buffer_count = 0;
                                        drift_buffer_index = 0;
                                        drift_valid = false;
                                        last_time_error_ns = time_error_ns;
                                        last_drift_calc_time = gps_seconds;
                                    } else {
                            
                            // Add to circular buffer (now clean, no outliers)
                            drift_buffer[drift_buffer_index] = drift_ppm;
                            drift_buffer_index = (drift_buffer_index + 1) % drift_buffer_size;
                            if (drift_buffer_count < drift_buffer_size) {
                                drift_buffer_count++;
                            }
                            
                            // Calculate moving average
                            drift_avg = 0.0;
                            for (size_t i = 0; i < drift_buffer_count; i++) {
                                drift_avg += drift_buffer[i];
                            }
                            drift_avg /= drift_buffer_count;
                            current_drift_ppm = drift_ppm;
                            current_drift_avg = drift_avg;
                            current_time_error_ms = time_error_ns / 1000000.0;
                            drift_valid = true;
                            
                                        // Log RTC drift measurement progress every 10 seconds
                                        static uint64_t last_drift_progress_log = 0;
                                        if (gps_seconds - last_drift_progress_log >= 10) {
                                            std::cout << "[RTC Drift] Measured: " << std::fixed << std::setprecision(3) 
                                                     << drift_ppm << " ppm | Avg(" << drift_buffer_count << "): " 
                                                     << drift_avg << " ppm | Error: " 
                                                     << (time_error_ns / 1000000.0) << " ms\n";
                                            last_drift_progress_log = gps_seconds;
                                        }
                                    }  // end if (!sanity check)
                                }  // end else (!baseline)
                            }  // end else (!discontinuity)
                        } else {
                            // get_base_mapping() returned false - mapping not locked yet
                            static uint64_t last_mapping_warning = 0;
                            if (gps_seconds - last_mapping_warning >= 10) {
                                std::cout << "[RTC Drift] ⚠️ Waiting for PPS-UTC base mapping lock...\n";
                                last_mapping_warning = gps_seconds;
                            }
                        }  // end if (gps_adapter.get_base_mapping(...))
                        
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
                std::cout << "[RTC Drift] ℹ️ Initializing drift measurement (last_drift_calc_time was 0)\n";
                std::cout << "[RTC Drift] DEBUG: gps_seconds=" << gps_seconds << ", pps_ready=" << pps_ready << "\n";
                last_drift_calc_time = gps_seconds;
                std::cout << "[RTC Drift] DEBUG: Set last_drift_calc_time to " << last_drift_calc_time << "\n";
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
                
                // Initialize baseline on first PPS edge (PPS-based measurement per expert advice)
                // CRITICAL: Use PHC sample taken IMMEDIATELY after PPS (at top of loop)
                // Expert (deb.md): "Sampling latency corrupts measurements - sample at PPS edge!"
                if (phc_servo.baseline_pps_seq == 0 && phc_sample_valid) {
                    phc_servo.baseline_pps_seq = pps_data.sequence;
                    phc_servo.baseline_phc_ns = phc_at_pps_ns;
                    phc_servo.last_offset_ns = offset_ns;  // For phase servo later
                    
                    std::cout << "[PHC Calibration] Baseline set at PPS #" << pps_data.sequence 
                              << " (PHC: " << phc_at_pps_ns << " ns)\n"
                              << "  (PHC sampled immediately after PPS - low latency)\n"
                              << "  Will measure over " << phc_servo.calib_interval_pulses << " pulses...\n";
                }
                
                // CRITICAL: Measure and correct frequency offset BEFORE stepping time
                // i226 PHC can have huge frequency offsets (100,000+ ppm) that cause
                // rapid drift. Must calibrate frequency first or servo will never lock.
                // 
                // EXPERT ADVICE (deb.md): Use PPS-count-based reference interval!
                // - Do NOT use GPS time-of-day (has NMEA latency, cached updates)
                // - DO use PPS pulse count as exact 1-second intervals
                // - Calculate: drift_ppm = ((phc_delta_ns - ref_delta_ns) / ref_delta_ns) * 1e6
                if (!phc_servo.freq_calibrated && phc_servo.baseline_pps_seq > 0) {
                    // Calculate elapsed PPS pulses (each pulse = exactly 1 second)
                    uint32_t elapsed_pulses = pps_data.sequence - phc_servo.baseline_pps_seq;
                    
                    // Progress logging every 5 pulses during calibration
                    static uint32_t last_progress_pulses = 0;
                    if (elapsed_pulses > 0 && elapsed_pulses % 5 == 0 && elapsed_pulses != last_progress_pulses) {
                        std::cout << "[PHC Calibration] Progress: " << elapsed_pulses << "/" 
                                  << phc_servo.calib_interval_pulses << " pulses (PPS #" 
                                  << pps_data.sequence << ")...\n";
                        last_progress_pulses = elapsed_pulses;
                    }
                    
                    if (elapsed_pulses >= phc_servo.calib_interval_pulses) {  // Enough pulses for measurement
                        // CRITICAL: Use PHC sample taken IMMEDIATELY after PPS (at top of loop)
                        // Expert (deb.md): "Sampling latency corrupts measurements - sample at PPS edge!"
                        if (phc_sample_valid) {
                            // PURE INTEGER NANOSECOND DELTAS (no floats until final ratio)
                            int64_t phc_delta_ns = phc_at_pps_ns - phc_servo.baseline_phc_ns;
                            int64_t ref_delta_ns = static_cast<int64_t>(elapsed_pulses) * 1000000000LL;  // N pulses × 1 sec/pulse
                            
                            // Calculate drift in ppm (parts per million)
                            // drift_ppm = ((PHC_measured - reference) / reference) × 10^6
                            double drift_ppm = (static_cast<double>(phc_delta_ns - ref_delta_ns) / ref_delta_ns) * 1e6;
                            
                            // EXPERT SANITY CHECK: Reject unrealistic drift (likely sampling error)
                            // Normal PHC crystal offset: < ±100 ppm, generous threshold: 2000 ppm
                            if (std::abs(drift_ppm) > 2000) {
                                std::cerr << "[PHC Calibration] ❌ INVALID MEASUREMENT: " << std::fixed << std::setprecision(1)
                                          << drift_ppm << " ppm (exceeds ±2000 ppm threshold)\n"
                                          << "  PHC delta: " << phc_delta_ns << " ns, Ref delta: " << ref_delta_ns << " ns\n"
                                          << "  LIKELY CAUSES:\n"
                                          << "    1. Wrong PHC device (verify: readlink /sys/class/net/eth1/ptp)\n"
                                          << "    2. PHC time discontinuity (clock step during measurement)\n"
                                          << "  Resetting baseline and retrying...\n";
                                // Reset baseline and try again (use fresh snapshot)
                                phc_servo.baseline_pps_seq = pps_data.sequence;
                                phc_servo.baseline_phc_ns = phc_at_pps_ns;
                                continue;
                            }
                        
                            // Check if still drifting significantly
                            if (std::abs(drift_ppm) > 100) {  // Still needs calibration
                                // CRITICAL: Track cumulative frequency in SOFTWARE
                                // Linux kernel doesn't provide a way to READ current frequency!
                                
                                // Calculate correction needed (negate drift to compensate)
                                int32_t correction_ppb = static_cast<int32_t>(-drift_ppm * 1000.0);  // ppm → ppb
                                
                                // Clamp correction per iteration (safe incremental steps)
                                if (correction_ppb > phc_servo.freq_max_ppb) {
                                    correction_ppb = phc_servo.freq_max_ppb;
                                } else if (correction_ppb < -phc_servo.freq_max_ppb) {
                                    correction_ppb = -phc_servo.freq_max_ppb;
                                }
                                
                                // Calculate new TOTAL frequency (cumulative in software)
                                int32_t new_freq_ppb = phc_servo.cumulative_freq_ppb + correction_ppb;
                                
                                // Clamp total frequency to hardware limits (±500,000 ppb = ±500 ppm)
                                const int32_t max_total_freq = 500000;  // i226 hardware limit
                                if (new_freq_ppb > max_total_freq) new_freq_ppb = max_total_freq;
                                if (new_freq_ppb < -max_total_freq) new_freq_ppb = -max_total_freq;
                                
                                std::cout << "[PHC Calibration] Iteration (" << elapsed_pulses << " pulses): Measured " 
                                          << std::fixed << std::setprecision(1) << drift_ppm << " ppm drift\n"
                                          << "  PHC delta: " << phc_delta_ns << " ns, Ref delta: " << ref_delta_ns << " ns\n"
                                          << "  (PHC sampled immediately after PPS - low latency)\n"
                                          << "  Current total: " << phc_servo.cumulative_freq_ppb << " ppb, "
                                          << "Correction: " << correction_ppb << " ppb, "
                                          << "New total: " << new_freq_ppb << " ppb\n";
                                
                                // Apply new TOTAL frequency (absolute set)
                                ptp_hal.adjust_phc_frequency(new_freq_ppb);
                                
                                // Update our software tracker
                                phc_servo.cumulative_freq_ppb = new_freq_ppb;
                                
                                // Reset baseline for next measurement interval (use current PPS-correlated PHC)
                                phc_servo.baseline_pps_seq = pps_data.sequence;
                                phc_servo.baseline_phc_ns = phc_at_pps_ns;
                                continue;  // Measure again over next N pulses
                            } else {
                                // Calibration complete! Final drift is acceptable
                                std::cout << "[PHC Calibration] ✓ Complete! Final drift: " 
                                          << std::fixed << std::setprecision(1) << drift_ppm << " ppm (acceptable)\n"
                                          << "  Final cumulative: " << phc_servo.cumulative_freq_ppb << " ppb\n";
                                std::cout << "[DEBUG Calibration Handoff] PHC calibration finished at PPS " << pps_data.sequence << "\n"
                                          << "  ⚠️ Expert prediction: Next 1-3 PPS cycles may show transient errors\n"
                                          << "  ⚠️ These should be SKIPPED from drift calculation to avoid contamination\n";
                                phc_servo.freq_calibrated = true;
                            }
                        } else {
                            std::cerr << "[PHC Calibration] ERROR: Failed to correlate PHC time at PPS edge!\n";
                            continue;
                        }
                    } else if (elapsed_pulses > 0) {
                        // Show we're waiting for enough pulses, but don't skip servo entirely
                        // This allows the calibration check to run on every iteration
                    }
                }
                
                // During frequency calibration, skip PHASE corrections to avoid corrupting frequency measurement
                // But we still need to check calibration progress above!
                if (!phc_servo.freq_calibrated) {
                    continue;  // Skip phase servo, let PHC drift naturally for frequency measurement
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
                    
                    // Calculate frequency adjustment from PI controller
                    // CRITICAL: This is the TOTAL frequency needed, not incremental!
                    // The PI servo integrates over time, so the output IS the total correction.
                    double adjustment = phc_servo.kp * offset_ns + phc_servo.ki * phc_servo.integral;
                    int32_t freq_ppb = static_cast<int32_t>(adjustment / 1000.0);
                    
                    // Clamp frequency adjustment to safe bounds
                    if (freq_ppb > phc_servo.freq_max_ppb) {
                        freq_ppb = phc_servo.freq_max_ppb;
                    } else if (freq_ppb < -phc_servo.freq_max_ppb) {
                        freq_ppb = -phc_servo.freq_max_ppb;
                    }
                    
                    // PI servo output is TOTAL frequency (calibration already applied)
                    // Add to calibrated base frequency for final adjustment
                    int32_t total_freq_ppb = phc_servo.cumulative_freq_ppb + freq_ppb;
                    
                    // Clamp total to hardware limits
                    const int32_t max_total_freq = 500000;
                    if (total_freq_ppb > max_total_freq) total_freq_ppb = max_total_freq;
                    if (total_freq_ppb < -max_total_freq) total_freq_ppb = -max_total_freq;
                    
                    ptp_hal.adjust_phc_frequency(total_freq_ppb);
                    
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

    // Wait for threads to finish
    std::cout << "\nWaiting for worker thread...\n";
    pthread_join(worker_thread, nullptr);
    
    std::cout << "Waiting for RT thread...\n";
    pthread_join(rt_thread, nullptr);

    std::cout << "\n=== Shutdown Complete ===\n";
    return 0;
}
