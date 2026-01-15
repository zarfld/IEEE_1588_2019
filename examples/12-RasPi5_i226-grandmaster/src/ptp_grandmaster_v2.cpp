/**
 * @file ptp_grandmaster_v2.cpp
 * @brief Refactored GPS-Disciplined PTP Grandmaster (Clean Architecture)
 * @details Uses modular GrandmasterController with tested components
 * 
 * Hardware:
 *   - Raspberry Pi 5
 *   - Intel i226 PCIe NIC (hardware timestamping)
 *   - u-blox G70xx GPS module (NMEA + PPS)
 *   - DS3231 RTC (holdover during GPS outages)
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#include "grandmaster_controller.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <getopt.h>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace IEEE::_1588::PTP::_2019::Linux;

// Shared data structure for RT thread coordination (from original ptp_grandmaster.cpp)
struct SharedTimingData {
    std::mutex mutex;
    std::condition_variable cv;
    int64_t phc_at_pps_ns;
    uint32_t pps_sequence;
    bool phc_sample_valid;
    
    SharedTimingData() : phc_at_pps_ns(0), pps_sequence(0), phc_sample_valid(false) {}
};

// RT thread arguments
struct RtThreadArgs {
    int pps_handle;
    PhcAdapter* phc_adapter;
    SharedTimingData* shared;
    std::atomic<bool>* running;
};

// Worker thread arguments  
struct WorkerThreadArgs {
    GpsAdapter* gps_adapter;
    RtcAdapter* rtc_adapter;
    GrandmasterController* controller;
    SharedTimingData* shared;
    std::atomic<bool>* running;
};

/**
 * RT Thread: PPS capture + PHC sampling (CPU2, SCHED_FIFO priority 80)
 * 
 * Critical path for low-latency PPS timestamping. Runs on isolated CPU2.
 * Target: <10ms latency from PPS edge to PHC sample.
 */
void* rt_thread_func(void* arg) {
    RtThreadArgs* args = static_cast<RtThreadArgs*>(arg);
    
    pthread_setname_np(pthread_self(), "ptp_rt");
    std::cout << "[RT Thread] Started on CPU" << sched_getcpu() << " (priority FIFO 80)\n";
    
    uint64_t pps_count = 0;
    uint64_t last_pps_sequence = 0;
    
    while (args->running->load()) {
        // Simplified: Just sleep and signal (real implementation would use PPS API)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Update shared data
        {
            std::lock_guard<std::mutex> lock(args->shared->mutex);
            args->shared->pps_sequence++;
            args->shared->phc_sample_valid = true;
            pps_count++;
        }
    }
    
    std::cout << "[RT Thread] Shutdown (PPS samples: " << pps_count << ")\n";
    return nullptr;
}

/**
 * Worker Thread: GPS/RTC/Controller updates (CPU0/1/3, SCHED_OTHER)
 * 
 * Non-critical path for GPS parsing, RTC discipline, and PTP messaging.
 */
void* worker_thread_func(void* arg) {
    WorkerThreadArgs* args = static_cast<WorkerThreadArgs*>(arg);
    
    pthread_setname_np(pthread_self(), "ptp_worker");
    std::cout << "[Worker Thread] Started on CPU" << sched_getcpu() << "\n";
    
    while (args->running->load()) {
        // Update GPS data
        args->gps_adapter->update();
        
        // Run controller iteration
        args->controller->run();
        
        // Small sleep to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "[Worker Thread] Shutdown\n";
    return nullptr;
}

// Global flag for graceful shutdown
static std::atomic<bool> g_running_atomic(true);
static volatile sig_atomic_t g_running = 1;

void signal_handler(int signum) {
    std::cout << "\nSignal " << signum << " received. Shutting down...\n";
    g_running = 0;
    g_running_atomic = false;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --interface <name>    Network interface (default: eth1)\n"
              << "  --gps-dev <path>      GPS serial device (default: /dev/ttyACM0)\n"
              << "  --gps-pps <path>      GPS PPS device (default: /dev/pps0)\n"
              << "  --gps-baud <rate>     GPS baud rate (default: 38400)\n"
              << "  --rtc <path>          RTC device (default: /dev/rtc1)\n"
              << "  --rtc-sqw <path>      RTC square wave PPS (default: /dev/pps1)\n"
              << "  --phc <path>          PHC device (default: /dev/ptp0)\n"
              << "  --verbose             Enable verbose logging\n"
              << "  --help                Show this help message\n\n"
              << "Example:\n"
              << "  sudo " << program_name << " --interface eth1 --rtc /dev/rtc1 --rtc-sqw /dev/pps1\n";
}

int main(int argc, char* argv[]) {
    // Default configuration
    std::string interface = "eth1";
    std::string gps_device = "/dev/ttyACM0";
    std::string gps_pps = "/dev/pps0";
    uint32_t gps_baud = 38400;
    std::string rtc_device = "/dev/rtc1";
    std::string rtc_sqw = "/dev/pps1";
    std::string phc_device = "/dev/ptp0";
    bool verbose = false;

    // Command-line argument parsing
    static struct option long_options[] = {
        {"interface", required_argument, 0, 'i'},
        {"gps-dev",   required_argument, 0, 'g'},
        {"gps-pps",   required_argument, 0, 'p'},
        {"gps-baud",  required_argument, 0, 'b'},
        {"rtc",       required_argument, 0, 'r'},
        {"rtc-sqw",   required_argument, 0, 's'},
        {"phc",       required_argument, 0, 'c'},
        {"verbose",   no_argument,       0, 'v'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "i:g:p:b:r:s:c:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i': interface = optarg; break;
            case 'g': gps_device = optarg; break;
            case 'p': gps_pps = optarg; break;
            case 'b': gps_baud = std::stoul(optarg); break;
            case 'r': rtc_device = optarg; break;
            case 's': rtc_sqw = optarg; break;
            case 'c': phc_device = optarg; break;
            case 'v': verbose = true; break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Print banner
    std::cout << "=== GPS-Disciplined PTP Grandmaster (Refactored v2) ===\n"
              << "Interface: " << interface << "\n"
              << "PHC: " << phc_device << "\n"
              << "GPS: " << gps_device << "\n"
              << "PPS: " << gps_pps << "\n"
              << "RTC: " << rtc_device << "\n"
              << "RTC SQW: " << rtc_sqw << " (1Hz edge detection)\n\n"
              << "ℹ️  TAI-UTC offset is automatically retrieved from kernel via adjtimex()\n"
              << "   To verify/set: adjtimex --print (shows 'tai' field)\n\n"
              << "⚠️  IMPORTANT: Verify PHC mapping with:\n"
              << "   readlink -f /sys/class/net/" << interface << "/ptp\n"
              << "   (should show: /sys/class/ptp/" << phc_device.substr(5) << ")\n\n";

    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        // Create hardware adapters
        std::cout << "Initializing hardware adapters...\n";
        
        GpsAdapter gps(gps_device, gps_pps, gps_baud);
        if (!gps.initialize()) {
            std::cerr << "ERROR: Failed to initialize GPS adapter\n";
            return 1;
        }
        std::cout << "  ✓ GPS adapter initialized\n";

        RtcAdapter rtc(rtc_device, rtc_sqw);
        if (!rtc.initialize()) {
            std::cerr << "ERROR: Failed to initialize RTC adapter\n";
            return 1;
        }
        std::cout << "  ✓ RTC adapter initialized\n";

        PhcAdapter phc;
        if (!phc.initialize(interface.c_str())) {  // Use interface name, not device path
            std::cerr << "ERROR: Failed to initialize PHC adapter\n";
            return 1;
        }
        std::cout << "  ✓ PHC adapter initialized\n";

        NetworkAdapter network(interface.c_str());
        if (!network.initialize()) {
            std::cerr << "ERROR: Failed to initialize network adapter\n";
            return 1;
        }
        std::cout << "  ✓ Network adapter initialized\n";

        // Create grandmaster controller with custom config
        GrandmasterConfig config;
        config.step_threshold_ns = 100000000;  // 100ms
        config.sync_interval_ms = 1000;        // 1 second
        config.enable_ptp_tx = true;           // Enable PTP message transmission
        
        std::cout << "\nCreating GrandmasterController...\n";
        GrandmasterController controller(&gps, &rtc, &phc, &network, config);

        // Initialize controller
        std::cout << "Initializing controller...\n";
        if (!controller.initialize()) {
            std::cerr << "ERROR: Controller initialization failed\n";
            return 1;
        }
        std::cout << "  ✓ Controller initialized\n\n";

        // Shared data for thread coordination
        SharedTimingData shared_data;
        
        // Launch RT thread with SCHED_FIFO priority 80 and CPU2 affinity
        std::cout << "Launching RT thread (CPU2, FIFO 80)...\n";
        
        RtThreadArgs rt_args = {
            .pps_handle = -1,  // TODO: Get from GPS adapter
            .phc_adapter = &phc,
            .shared = &shared_data,
            .running = &g_running_atomic
        };
        
        pthread_t rt_thread;
        pthread_attr_t rt_attr;
        pthread_attr_init(&rt_attr);
        
        // CRITICAL: Must set inherit sched to EXPLICIT to use our scheduling parameters
        pthread_attr_setinheritsched(&rt_attr, PTHREAD_EXPLICIT_SCHED);
        
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
            std::cerr << "       (May need root privileges: sudo ./ptp_grandmaster_v2)\n";
            pthread_attr_destroy(&rt_attr);
            return 1;
        }
        pthread_attr_destroy(&rt_attr);
        std::cout << "  ✓ RT thread launched\n";

        // Launch worker thread with normal priority and CPU0/1/3 affinity
        std::cout << "Launching worker thread (CPU0/1/3, normal priority)...\n";
        
        WorkerThreadArgs worker_args = {
            .gps_adapter = &gps,
            .rtc_adapter = &rtc,
            .controller = &controller,
            .shared = &shared_data,
            .running = &g_running_atomic
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

        // Main control loop - now just monitors and prints statistics
        std::cout << "\n🚀 Grandmaster running with RT threading...\n\n";
        
        auto last_stats_time = std::chrono::steady_clock::now();
        
        while (g_running) {
            // Print statistics every 10 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 10) {
                GrandmasterStats stats;
                controller.get_stats(&stats);
                
                if (verbose) {
                    std::cout << "\n[Statistics] "
                              << "Uptime: " << stats.uptime_seconds << "s, "
                              << "Syncs: " << stats.sync_messages_sent << ", "
                              << "Steps: " << stats.step_corrections << "\n";
                }
                
                last_stats_time = now;
            }
            
            // Small sleep - worker thread handles controller updates
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Signal threads to stop
        g_running_atomic = false;
        
        // Wait for threads to finish
        std::cout << "\nWaiting for threads to finish...\n";
        pthread_join(rt_thread, nullptr);
        pthread_join(worker_thread, nullptr);

        // Shutdown
        std::cout << "Shutting down gracefully...\n";
        controller.shutdown();
        
        // Final statistics
        GrandmasterStats final_stats;
        controller.get_stats(&final_stats);
        std::cout << "\n=== Final Statistics ===\n"
                  << "  Total runtime: " << final_stats.uptime_seconds << " seconds\n"
                  << "  Sync messages sent: " << final_stats.sync_messages_sent << "\n"
                  << "  Step corrections: " << final_stats.step_corrections << "\n\n";

        std::cout << "=== Shutdown Complete ===\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << "\n";
        return 1;
    }
}
