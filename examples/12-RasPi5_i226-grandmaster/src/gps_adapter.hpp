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
    bool     dropout_detected; ///< Missed PPS pulse(s) detected (seq_delta != 1)
    uint32_t seq_delta;      ///< Sequence delta from last pulse (1 = normal, >1 = dropout)
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
     * @brief Get PPS data including jitter
     * @param pps_data Output PPS data structure
     * @param max_jitter_ns Output maximum jitter over last interval
     * @return true if PPS data available
     */
    bool get_pps_data(PpsData* pps_data, uint32_t* max_jitter_ns);
    
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
     * @brief Get PPS handle for direct time_pps_fetch() access
     * @return PPS handle (-1 if not initialized)
     * @note Used by RT thread for low-latency PPS monitoring
     */
    time_t get_pps_handle() const { return pps_handle_; }
    
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
    
    /**
     * @brief Get PPS-UTC base mapping for RTC discipline
     * @param expected_utc_sec Output: Expected UTC second for current PPS
     * @return true if mapping is locked and valid, false otherwise
     * @note Used by RTC drift measurement to get integer-seconds reference
     *       Implements expert fix from deb.md: RTC should be compared against
     *       integer UTC seconds from PPS mapping, not fractional GPS time
     */
    bool get_base_mapping(uint64_t* expected_utc_sec);

    /**
     * @brief Notify GPS adapter of PHC timescale step correction
     * @param step_delta_ns PHC step amount in nanoseconds (new_time - old_time)
     * 
     * When PHC is stepped, PPS timestamps captured before the step are in the old
     * timescale. This method tracks cumulative step corrections to adjust PPS
     * timestamps for accurate TAI time calculation.
     */
    void notify_phc_stepped(int64_t step_delta_ns);

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
    uint64_t    last_pps_fetch_ms_; ///< Last PPS fetch timestamp (ms)
    
    // PPS-UTC association state (fix for ±1 sec oscillation per deb.md)
    // BASE MAPPING MODEL: UTC(pps_seq) = base_utc_sec + (pps_seq - base_pps_seq)
    uint64_t    base_pps_seq_;              ///< Base PPS sequence for UTC mapping
    uint64_t    base_utc_sec_;              ///< Base UTC second (epoch) for base_pps_seq
    bool        pps_utc_locked_;            ///< Association locked?
    bool        nmea_labels_last_pps_;      ///< True: RMC labels last PPS, False: next
    uint32_t    association_sample_count_;  ///< Samples for association detection
    int64_t     association_dt_sum_;        ///< Sum of dt samples
    uint64_t    last_nmea_time_;            ///< Last NMEA time processed (UTC seconds)
    
    // PHC timescale tracking
    int64_t     cumulative_phc_steps_ns_;   ///< Total PHC step corrections (ns)
    
    // Private helper methods
    bool open_serial_port();
    bool open_pps_device();
    bool initialize_pps();
    bool update_pps_data();
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
