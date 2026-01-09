/**
 * @file gps_adapter.hpp
 * @brief GPS Time Source Adapter for IEEE 1588-2019 PTP
 * @details Interfaces with u-blox GPS module for primary time reference
 * 
 * Hardware:
 *   - u-blox G70xx GPS module
 *   - NMEA-0183 output (9600 baud)
 *   - 1PPS output on GPIO
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#pragma once

#include <cstdint>
#include <string>
#include <ctime>
#include <sys/timepps.h>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {

/**
 * @brief GPS fix quality indicator
 */
enum class GpsFixQuality : uint8_t {
    NO_FIX = 0,              ///< No GPS fix available
    GPS_FIX = 1,             ///< Standard GPS fix
    DGPS_FIX = 2,            ///< Differential GPS fix
    PPS_FIX = 3,             ///< GPS with PPS
    RTK_FIX = 4,             ///< Real-Time Kinematic
    RTK_FLOAT = 5,           ///< RTK Float
    ESTIMATED = 6,           ///< Estimated/Dead reckoning
    MANUAL = 7,              ///< Manual input mode
    SIMULATION = 8           ///< Simulation mode
};

/**
 * @brief GPS time and position data
 */
struct GpsData {
    // Time information
    uint8_t  hours;          ///< UTC hours (0-23)
    uint8_t  minutes;        ///< UTC minutes (0-59)
    uint8_t  seconds;        ///< UTC seconds (0-59)
    uint16_t year;           ///< UTC year (2000+)
    uint8_t  month;          ///< UTC month (1-12)
    uint8_t  day;            ///< UTC day (1-31)
    
    // Fix quality
    GpsFixQuality fix_quality;    ///< GPS fix quality
    uint8_t       satellites;     ///< Number of satellites
    
    // Position (optional)
    double latitude;         ///< Latitude in degrees
    double longitude;        ///< Longitude in degrees
    double altitude;         ///< Altitude in meters
    
    // Validity flags
    bool time_valid;         ///< Time data is valid
    bool position_valid;     ///< Position data is valid
};

/**
 * @brief PPS (Pulse-Per-Second) signal data
 */
struct PpsData {
    uint64_t assert_sec;     ///< PPS assert timestamp (seconds)
    uint32_t assert_nsec;    ///< PPS assert timestamp (nanoseconds)
    uint64_t sequence;       ///< PPS sequence number
    uint32_t jitter_nsec;    ///< Estimated jitter (nanoseconds)
    bool     valid;          ///< PPS signal is valid
};

/**
 * @brief GPS Adapter for PTP Time Synchronization
 * @details Provides GPS time reference with PPS disciplining
 * 
 * Reuses code from: examples/04-gps-nmea-sync/
 */
class GpsAdapter {
public:
    /**
     * @brief Construct GPS adapter
     * @param serial_device GPS serial device path (e.g., "/dev/ttyACM0")
     * @param pps_device PPS device path (e.g., "/dev/pps0")
     * @param baud_rate Serial baud rate (default: 9600)
     */
    GpsAdapter(const std::string& serial_device, 
               const std::string& pps_device,
               uint32_t baud_rate = 9600);
    
    /**
     * @brief Destructor - cleanup resources
     */
    ~GpsAdapter();
    
    /**
     * @brief Initialize GPS and PPS interfaces
     * @return true on success, false on failure
     */
    bool initialize();
    
    /**
     * @brief Update GPS data (call periodically)
     * @return true if new data received, false otherwise
     */
    bool update();
    
    /**
     * @brief Check if GPS has valid fix
     * @return true if fix available, false otherwise
     */
    bool has_fix() const { return gps_data_.time_valid; }
    
    /**
     * @brief Get GPS time in PTP format (TAI)
     * @param seconds Output: Seconds since PTP epoch
     * @param nanoseconds Output: Nanoseconds
     * @return true on success, false if no valid time
     */
    bool get_ptp_time(uint64_t* seconds, uint32_t* nanoseconds);
    
    /**
     * @brief Get latest GPS data
     * @return GPS data structure
     */
    const GpsData& get_gps_data() const { return gps_data_; }
    
    /**
     * @brief Get latest PPS data
     * @return PPS data structure
     */
    const PpsData& get_pps_data() const { return pps_data_; }
    
    /**
     * @brief Get GPS fix quality
     * @return Fix quality indicator
     */
    GpsFixQuality get_fix_quality() const { return gps_data_.fix_quality; }
    
    /**
     * @brief Get number of satellites
     * @return Satellite count
     */
    uint8_t get_satellite_count() const { return gps_data_.satellites; }
    
    /**
     * @brief Calculate clock quality for PTP based on GPS status
     * @param clock_class Output: PTP clock class
     * @param clock_accuracy Output: PTP clock accuracy
     * @param offset_variance Output: PTP offset variance (optional, can be nullptr)
     * @return true if GPS provides valid clock quality, false otherwise
     */
    bool get_ptp_clock_quality(uint8_t* clock_class, uint8_t* clock_accuracy, uint16_t* offset_variance = nullptr);

private:
    std::string serial_device_;   ///< GPS serial device path
    std::string pps_device_;      ///< PPS device path
    uint32_t    baud_rate_;       ///< Serial baud rate
    
    int         serial_fd_;       ///< Serial port file descriptor
    int         pps_fd_;          ///< PPS device file descriptor
    pps_handle_t pps_handle_;     ///< PPS handle
    
    GpsData     gps_data_;        ///< Latest GPS data
    PpsData     pps_data_;        ///< Latest PPS data
    
    char        rx_buffer_[256];  ///< Serial receive buffer
    size_t      rx_buffer_len_;   ///< Buffer length
    bool        pps_valid_;       ///< PPS validity flag
    
    // Private helper methods
    bool open_serial_port();
    bool open_pps_device();
    bool initialize_pps();
    bool read_gps_data(GpsData* gps_data);
    bool parse_nmea_sentence(const char* sentence, GpsData* gps_data);
    bool parse_gprmc(const char* sentence, GpsData* gps_data);
    bool parse_gpgga(const char* sentence, GpsData* gps_data);
    bool validate_nmea_checksum(const char* sentence);
    bool read_pps_event(PpsData* pps_data);
    void convert_utc_to_tai(uint64_t* seconds);
};

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
