/**
 * @file dcf77_ptp_sync_example.cpp
 * @brief Example: Synchronize PTP Clock from DCF77 Terrestrial Radio
 * 
 * Demonstrates:
 * - Decoding DCF77 time signals (77.5 kHz longwave)
 * - Computing IEEE 1588-2019 clock quality from signal strength
 * - Updating PTP clock's DefaultDataSet.clockQuality
 * - Setting TimePropertiesDataSet.timeSource to TERRESTRIAL_RADIO (0x30)
 * - Using library's Types::ClockQuality and Types::TimeSource
 * 
 * Hardware:
 * - DCF77 receiver module (e.g., Pollin DCF1, Conrad DCF77)
 * - ESP32, Arduino, or compatible microcontroller
 * - Connection: DCF77 data pin → GPIO pin (with 10kΩ pull-up)
 * 
 * Coverage:
 * - ~2000 km from Mainflingen, Germany (50°01'N, 9°00'E)
 * - Central Europe: Germany, Austria, Switzerland, Netherlands, Belgium, etc.
 * 
 * Usage:
 *   dcf77_ptp_sync_example [gpio_pin] [invert_signal]
 * 
 * Examples:
 *   dcf77_ptp_sync_example 4 0     # GPIO4, normal polarity
 *   dcf77_ptp_sync_example 5 1     # GPIO5, inverted polarity
 */

#include "dcf77_adapter.hpp"
#include "clocks.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <csignal>

using namespace Examples::DCF77;

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
    
    if (quality.clock_accuracy == 0x29) {
        std::cout << " (±1 ms - DCF77 specification)";
    } else if (quality.clock_accuracy == 0xFE) {
        std::cout << " (Unknown)";
    }
    std::cout << "\n";
    
    std::cout << "  offsetScaledLogVariance: 0x" << std::hex 
              << quality.offset_scaled_log_variance << std::dec << "\n";
}

void print_dcf77_frame(const DCF77Frame& frame) {
    if (!frame.valid) {
        std::cout << "No valid DCF77 frame decoded yet\n";
        return;
    }
    
    std::cout << "DCF77 Time: ";
    std::cout << std::setfill('0') 
              << std::setw(4) << (2000 + frame.year) << "-"
              << std::setw(2) << static_cast<int>(frame.month) << "-"
              << std::setw(2) << static_cast<int>(frame.day) << " "
              << std::setw(2) << static_cast<int>(frame.hour) << ":"
              << std::setw(2) << static_cast<int>(frame.minute) << ":00";
    
    if (frame.cest) {
        std::cout << " CEST (UTC+2)";
    } else if (frame.cet) {
        std::cout << " CET (UTC+1)";
    }
    std::cout << "\n";
    
    std::cout << "Signal Strength: " << static_cast<int>(frame.signal_strength) << "%\n";
    std::cout << "Decode Errors: " << static_cast<int>(frame.decode_errors) << "\n";
    
    if (frame.leap_second) {
        std::cout << "⚠️  LEAP SECOND ANNOUNCEMENT\n";
    }
}

void print_statistics(const DCF77Statistics& stats) {
    std::cout << "Statistics:\n";
    std::cout << "  Frames received: " << stats.frames_received << "\n";
    std::cout << "  Frames failed: " << stats.frames_failed << "\n";
    std::cout << "  Signal losses: " << stats.signal_losses << "\n";
    
    uint32_t total = stats.frames_received + stats.frames_failed;
    if (total > 0) {
        double success_rate = (static_cast<double>(stats.frames_received) / total) * 100.0;
        std::cout << "  Success rate: " << std::fixed << std::setprecision(1) 
                  << success_rate << "%\n";
    }
}

int main(int argc, char* argv[]) {
    // Parse command line
    uint8_t gpio_pin = 4;
    bool invert_signal = false;
    
    if (argc >= 2) {
        gpio_pin = static_cast<uint8_t>(std::stoi(argv[1]));
    }
    if (argc >= 3) {
        invert_signal = (std::stoi(argv[2]) != 0);
    }
    
    std::cout << "========================================\n";
    std::cout << "DCF77 to PTP Clock Synchronization Example\n";
    std::cout << "========================================\n";
    std::cout << "Frequency: 77.5 kHz longwave\n";
    std::cout << "Location: Mainflingen, Germany\n";
    std::cout << "Coverage: ~2000 km (Central Europe)\n";
    std::cout << "Accuracy: ±1 ms to PTB atomic clocks\n";
    std::cout << "GPIO Pin: " << static_cast<int>(gpio_pin) << "\n";
    std::cout << "Signal Polarity: " << (invert_signal ? "Inverted" : "Normal") << "\n";
    std::cout << "Time Source: Types::TimeSource::Terrestrial_Radio (0x30)\n";
    std::cout << "========================================\n\n";
    
    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Create DCF77 adapter
    DCF77Adapter dcf77(gpio_pin, invert_signal);
    
    if (!dcf77.initialize()) {
        std::cerr << "ERROR: Failed to initialize DCF77 adapter\n";
        return 1;
    }
    
    std::cout << "DCF77 adapter initialized\n";
    std::cout << "Waiting for signal (this may take 1-2 minutes)...\n\n";
    
    // Main loop
    int frame_count = 0;
    auto last_print = std::chrono::steady_clock::now();
    
    while (g_running) {
        // Update DCF77 decoder (call frequently!)
        if (dcf77.update()) {
            // New frame decoded
            frame_count++;
            
            std::cout << "\n[" << frame_count << "] DCF77 Frame Decoded\n";
            std::cout << "----------------------------------------\n";
            
            // Print decoded time
            print_dcf77_frame(dcf77.get_last_frame());
            std::cout << "\n";
            
            // Get clock quality using LIBRARY's Types::ClockQuality
            Types::ClockQuality quality = dcf77.get_clock_quality();
            print_clock_quality(quality);
            std::cout << "\n";
            
            // Demonstrate updating PTP clock
            std::cout << "Updating PTP Clock:\n";
            std::cout << "  ds.clockQuality = dcf77.get_clock_quality();\n";
            std::cout << "  tp.timeSource = static_cast<uint8_t>(Types::TimeSource::Terrestrial_Radio);\n";
            std::cout << "  // timeSource = 0x" << std::hex 
                      << static_cast<int>(Types::TimeSource::Terrestrial_Radio) << std::dec 
                      << " (TERRESTRIAL_RADIO)\n";
            std::cout << "\n";
            
            // Print statistics
            print_statistics(dcf77.get_statistics());
            std::cout << "\n";
        }
        
        // Print status every 10 seconds
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_print).count() >= 10) {
            int32_t seconds_since_sync = dcf77.get_seconds_since_sync();
            
            std::cout << "Status: ";
            if (dcf77.is_synchronized()) {
                std::cout << "Synchronized (" << seconds_since_sync << "s ago)";
            } else {
                std::cout << "Acquiring signal...";
            }
            std::cout << " | Frames: " << frame_count << "\n";
            
            last_print = now;
        }
        
        // Sleep briefly (DCF77 bits arrive every second)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::cout << "\nShutting down...\n";
    std::cout << "Total DCF77 frames decoded: " << frame_count << "\n";
    print_statistics(dcf77.get_statistics());
    
    return 0;
}
