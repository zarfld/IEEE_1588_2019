/**
 * @file gps_nmea_sync_example.cpp
 * @brief GPS NMEA Time Synchronization Example
 * 
 * Demonstrates IEEE 1588-2019 PTP clock synchronization using GPS as external
 * time reference via NMEA-0183 serial protocol.
 * 
 * This example:
 * 1. Opens serial connection to GPS module (9600 baud, 8N1)
 * 2. Reads and parses NMEA sentences (GPRMC, GPGGA)
 * 3. Converts GPS time to PTP timestamps
 * 4. Calculates clock offset from GPS reference
 * 5. Displays synchronization status and accuracy
 * 
 * Hardware Requirements:
 * - GPS module with NMEA-0183 output (e.g., u-blox NEO-6M, NEO-7M)
 * - Serial connection: USB-to-TTL adapter or direct UART
 * - GPS antenna with clear sky view
 * 
 * @see GPS_NMEA_Specification_Refinement.md for detailed design
 * @see README.md for hardware setup instructions
 */

#include "serial_hal_interface.hpp"
#include "nmea_parser.hpp"
#include "gps_time_converter.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>

// Platform-specific headers for clock access
#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
#endif

using namespace HAL::Serial;
using namespace GPS::NMEA;
using namespace GPS::Time;

/**
 * @brief Get current system time as PTP timestamp
 * 
 * Uses platform-specific high-resolution clock.
 * 
 * @note This is a simplified implementation. Real PTP would use hardware timestamps.
 */
PTPTimestamp get_system_ptp_time() {
#ifdef _WIN32
    // Windows: Use GetSystemTimePreciseAsFileTime (Win8+)
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    
    // Convert FILETIME (100ns intervals since 1601-01-01) to Unix epoch
    uint64_t filetime = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    
    // FILETIME epoch is 1601-01-01, Unix epoch is 1970-01-01
    // Difference: 11644473600 seconds
    const uint64_t FILETIME_TO_UNIX_OFFSET = 11644473600ULL;
    
    uint64_t unix_100ns = filetime - (FILETIME_TO_UNIX_OFFSET * 10000000ULL);
    uint64_t unix_seconds = unix_100ns / 10000000ULL;
    uint32_t unix_nanoseconds = (unix_100ns % 10000000ULL) * 100;
    
    // Convert UTC to TAI (add 37 seconds for PTP)
    return PTPTimestamp(unix_seconds + TAI_UTC_OFFSET_SECONDS, unix_nanoseconds);
#else
    // Linux: Use clock_gettime with CLOCK_REALTIME
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Convert UTC to TAI (add 37 seconds for PTP)
    return PTPTimestamp(
        static_cast<uint64_t>(ts.tv_sec) + TAI_UTC_OFFSET_SECONDS,
        static_cast<uint32_t>(ts.tv_nsec)
    );
#endif
}

/**
 * @brief Format PTP timestamp for display
 */
std::string format_ptp_timestamp(const PTPTimestamp& ts) {
    std::ostringstream oss;
    oss << ts.seconds << "." << std::setfill('0') << std::setw(9) << ts.nanoseconds;
    return oss.str();
}

/**
 * @brief Format GPS fix status for display
 */
const char* fix_status_to_string(GPSFixStatus status) {
    switch (status) {
        case GPSFixStatus::NO_FIX:          return "NO_FIX";
        case GPSFixStatus::TIME_ONLY:       return "TIME_ONLY";
        case GPSFixStatus::AUTONOMOUS_FIX:  return "GPS_FIX";
        case GPSFixStatus::DGPS_FIX:        return "DGPS_FIX";
        case GPSFixStatus::SIGNAL_LOST:     return "SIGNAL_LOST";
        default:                            return "UNKNOWN";
    }
}

/**
 * @brief Display GPS synchronization status
 */
void display_sync_status(const GPSTimeData& gps_data, 
                        const PTPTimestamp& gps_ptp, 
                        const PTPTimestamp& system_ptp,
                        int64_t offset_ns) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "GPS Synchronization Status" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // GPS time
    std::cout << "GPS Time (UTC): " 
              << std::setfill('0') << std::setw(2) << static_cast<int>(gps_data.hours) << ":"
              << std::setfill('0') << std::setw(2) << static_cast<int>(gps_data.minutes) << ":"
              << std::setfill('0') << std::setw(2) << static_cast<int>(gps_data.seconds) << "."
              << std::setfill('0') << std::setw(2) << static_cast<int>(gps_data.centiseconds)
              << std::endl;
    
    // GPS date
    if (gps_data.date_valid) {
        std::cout << "GPS Date:       " 
                  << std::setfill('0') << std::setw(4) << gps_data.year << "-"
                  << std::setfill('0') << std::setw(2) << static_cast<int>(gps_data.month) << "-"
                  << std::setfill('0') << std::setw(2) << static_cast<int>(gps_data.day)
                  << std::endl;
    }
    
    // GPS status
    std::cout << "Fix Status:     " << fix_status_to_string(gps_data.fix_status) << std::endl;
    std::cout << "Satellites:     " << static_cast<int>(gps_data.satellites) << std::endl;
    
    // PTP timestamps
    std::cout << "GPS PTP Time:   " << format_ptp_timestamp(gps_ptp) << " (TAI)" << std::endl;
    std::cout << "System Time:    " << format_ptp_timestamp(system_ptp) << " (TAI)" << std::endl;
    
    // Clock offset
    double offset_us = offset_ns / 1000.0;
    std::cout << "Clock Offset:   " << std::fixed << std::setprecision(3) << offset_us << " μs";
    if (offset_ns > 0) {
        std::cout << " (system behind GPS)";
    } else {
        std::cout << " (system ahead of GPS)";
    }
    std::cout << std::endl;
    
    // Accuracy assessment
    if (gps_data.is_valid_for_ptp()) {
        if (std::abs(offset_ns) < 100000) {  // < 100 μs
            std::cout << "Sync Quality:   EXCELLENT (within ±100 μs target)" << std::endl;
        } else if (std::abs(offset_ns) < 1000000) {  // < 1 ms
            std::cout << "Sync Quality:   GOOD (within ±1 ms)" << std::endl;
        } else if (std::abs(offset_ns) < 10000000) {  // < 10 ms
            std::cout << "Sync Quality:   FAIR (within ±10 ms)" << std::endl;
        } else {
            std::cout << "Sync Quality:   POOR (>±10 ms offset)" << std::endl;
        }
    }
    
    std::cout << "========================================\n" << std::endl;
}

/**
 * @brief Print usage instructions
 */
void print_usage(const char* program_name) {
    std::cout << "GPS NMEA Time Synchronization Example\n" << std::endl;
    std::cout << "Usage: " << program_name << " <serial_port> [options]\n" << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  serial_port    Serial port name" << std::endl;
#ifdef _WIN32
    std::cout << "                 Windows: COM1, COM3, etc." << std::endl;
#else
    std::cout << "                 Linux: /dev/ttyUSB0, /dev/ttyS0, etc." << std::endl;
#endif
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --baud <rate>  Baud rate (default: 9600)" << std::endl;
    std::cout << "  --help         Show this help message" << std::endl;
    std::cout << "\nExample:" << std::endl;
#ifdef _WIN32
    std::cout << "  " << program_name << " COM3" << std::endl;
#else
    std::cout << "  " << program_name << " /dev/ttyUSB0" << std::endl;
#endif
    std::cout << "\nHardware Setup:" << std::endl;
    std::cout << "  - Connect GPS module TX to computer RX" << std::endl;
    std::cout << "  - GPS module should output NMEA-0183 sentences at 9600 baud" << std::endl;
    std::cout << "  - Place GPS antenna with clear view of sky for best results" << std::endl;
}

/**
 * @brief Main application entry point
 */
int main(int argc, char* argv[]) {
    // Parse command-line arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* serial_port = nullptr;
    uint32_t baud_rate = 9600;
    
    // Check for help flag
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    // First argument is serial port
    serial_port = argv[1];
    
    // Parse options
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--baud") == 0 && i + 1 < argc) {
            baud_rate = static_cast<uint32_t>(atoi(argv[++i]));
        }
    }
    
    std::cout << "GPS NMEA Time Synchronization Example" << std::endl;
    std::cout << "======================================\n" << std::endl;
    std::cout << "Serial Port: " << serial_port << std::endl;
    std::cout << "Baud Rate:   " << baud_rate << std::endl;
    std::cout << "\nOpening serial port..." << std::endl;
    
    // Create serial interface
    SerialInterface* serial = create_serial_interface();
    if (!serial) {
        std::cerr << "ERROR: Failed to create serial interface" << std::endl;
        return 1;
    }
    
    // Configure serial port for GPS NMEA (9600, 8N1, 1s timeout)
    SerialConfig config = SerialConfig::gps_nmea_default();
    config.baud_rate = baud_rate;
    
    // Open serial port
    SerialError err = serial->open(serial_port, config);
    if (err != SerialError::SUCCESS) {
        std::cerr << "ERROR: Failed to open serial port: " << serial_port << std::endl;
        delete serial;
        return 1;
    }
    
    std::cout << "Serial port opened successfully" << std::endl;
    std::cout << "Waiting for GPS NMEA sentences...\n" << std::endl;
    
    // Create NMEA parser and time converter
    NMEAParser parser;
    GPSTimeConverter converter;
    
    // Main loop: read NMEA sentences and display sync status
    char line_buffer[256];
    int valid_sentence_count = 0;
    
    while (true) {
        // Read NMEA sentence
        err = serial->read_line(line_buffer, sizeof(line_buffer));
        
        if (err == SerialError::TIMEOUT) {
            std::cout << "." << std::flush;  // Show activity
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        if (err != SerialError::SUCCESS) {
            std::cerr << "ERROR: Failed to read from serial port" << std::endl;
            break;
        }
        
        // Parse NMEA sentence
        GPSTimeData gps_data;
        if (parser.parse_sentence(line_buffer, gps_data)) {
            ++valid_sentence_count;
            
            // Convert to PTP timestamp
            PTPTimestamp gps_ptp;
            if (converter.convert_to_ptp(gps_data, gps_ptp)) {
                // Get system time
                PTPTimestamp system_ptp = get_system_ptp_time();
                
                // Calculate clock offset
                int64_t offset_ns = converter.calculate_clock_offset(gps_ptp, system_ptp);
                
                // Display status every 5 valid sentences
                if (valid_sentence_count % 5 == 0) {
                    display_sync_status(gps_data, gps_ptp, system_ptp, offset_ns);
                }
            }
        }
    }
    
    // Cleanup
    serial->close();
    delete serial;
    
    std::cout << "\nShutting down..." << std::endl;
    return 0;
}
