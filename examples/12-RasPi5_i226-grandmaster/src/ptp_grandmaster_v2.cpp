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

using namespace IEEE::_1588::PTP::_2019::Linux;

// Global flag for graceful shutdown
static volatile sig_atomic_t g_running = 1;

void signal_handler(int signum) {
    std::cout << "\n Signal " << signum << " received. Shutting down...\n";
    g_running = 0;
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

        // Main control loop
        std::cout << "🚀 Grandmaster running...\n\n";
        
        uint64_t loop_count = 0;
        auto last_stats_time = std::chrono::steady_clock::now();
        
        while (g_running) {
            // Update GPS data (non-blocking)
            gps.update();
            
            // Run controller iteration
            controller.run();
            
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
            
            // Small sleep to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            loop_count++;
        }

        // Shutdown
        std::cout << "\nShutting down gracefully...\n";
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
