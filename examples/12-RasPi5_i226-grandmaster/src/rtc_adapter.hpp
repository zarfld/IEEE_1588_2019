/**
 * @file rtc_adapter.hpp
 * @brief RTC Holdover Adapter for IEEE 1588-2019 PTP
 * @details Provides time holdover during GPS outages using DS3231 RTC
 * 
 * Hardware:
 *   - DS3231 Real-Time Clock (I2C)
 *   - Temperature compensated crystal oscillator
 *   - Battery backup for time retention
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#pragma once

#include <cstdint>
#include <string>
#include <ctime>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {

/**
 * @brief RTC time data
 */
struct RtcTime {
    uint8_t  seconds;        ///< Seconds (0-59)
    uint8_t  minutes;        ///< Minutes (0-59)
    uint8_t  hours;          ///< Hours (0-23)
    uint8_t  day;            ///< Day of month (1-31)
    uint8_t  month;          ///< Month (1-12)
    uint16_t year;           ///< Year (2000+)
    bool     valid;          ///< Time is valid
};

/**
 * @brief RTC Holdover Adapter
 * @details Provides time continuity during GPS outages
 */
class RtcAdapter {
public:
    /**
     * @brief Construct RTC adapter
     * @param rtc_device RTC device path (e.g., "/dev/rtc1")
     * @param sqw_device SQW PPS device path (e.g., "/dev/pps1", optional)
     */
    explicit RtcAdapter(const std::string& rtc_device, const std::string& sqw_device = "");
    
    /**
     * @brief Destructor - cleanup resources
     */
    ~RtcAdapter();
    
    /**
     * @brief Initialize RTC interface
     * @return true on success, false on failure
     */
    bool initialize();
    
    /**
     * @brief Read current RTC time
     * @param rtc_time Output: RTC time structure
     * @return true on success, false on failure
     */
    bool read_time(RtcTime* rtc_time);
    
    /**
     * @brief Set RTC time
     * @param rtc_time RTC time to set
     * @return true on success, false on failure
     */
    bool set_time(const RtcTime& rtc_time);
    
    /**
     * @brief Get RTC time in PTP format
     * @param seconds Output: Seconds since PTP epoch
     * @param nanoseconds Output: Nanoseconds (always 0 for RTC)
     * @return true on success, false on failure
     */
    bool get_ptp_time(uint64_t* seconds, uint32_t* nanoseconds);
    
    /**
     * @brief Set RTC from PTP timestamp
     * @param seconds Seconds since PTP epoch
     * @param nanoseconds Nanoseconds (ignored for RTC precision)
     * @return true on success, false on failure
     */
    bool set_ptp_time(uint64_t seconds, uint32_t nanoseconds);
    
    /**
     * @brief Synchronize RTC from GPS time
     * @param gps_seconds GPS time (seconds)
     * @param gps_nanoseconds GPS time (nanoseconds)
     * @return true on success, false on failure
     * 
     * @note Should be called periodically when GPS is available
     */
    bool sync_from_gps(uint64_t gps_seconds, uint32_t gps_nanoseconds);
    
    /**
     * @brief Get RTC temperature (if supported) - public interface
     * @param temperature_c Output: Temperature in Celsius
     * @return true if temperature available, false otherwise
     */
    bool get_temperature(float* temperature_c) {
        *temperature_c = static_cast<float>(get_temperature());
        return true;
    }
    
    /**
     * @brief Calculate holdover clock quality - public interface
     * @param holdover_duration_sec Seconds since GPS was lost
     * @param clock_class Output: PTP clock class
     * @param clock_accuracy Output: PTP clock accuracy
     * @return true on success
     * 
     * Clock class mapping:
     *   - < 1 hour:  Clock Class 7  (holdover < 1h)
     *   - < 24 hour: Clock Class 52 (holdover < 24h)
     *   - > 24 hour: Clock Class 187 (holdover > 24h)
     */
    bool calculate_holdover_quality(uint32_t holdover_duration_sec,
                                   uint8_t* clock_class,
                                   uint8_t* clock_accuracy) {
        // Map duration to clock class
        if (holdover_duration_sec < 3600) {  // < 1 hour
            *clock_class = 7;
            *clock_accuracy = 0x21;  // Within 100ns
        } else if (holdover_duration_sec < 86400) {  // < 24 hours
            *clock_class = 52;
            *clock_accuracy = 0x22;  // Within 250ns
        } else {
            *clock_class = 187;  // Holdover > 24h
            *clock_accuracy = 0xFE;  // Unknown
        }
        return true;
    }

private:
    std::string rtc_device_;     ///< RTC device path
    std::string sqw_device_;     ///< SQW PPS device path (optional, for edge detection)
    int         rtc_fd_;         ///< RTC device file descriptor
    int         i2c_fd_;         ///< I2C bus file descriptor for DS3231
    int         pps_fd_;         ///< PPS device file descriptor for SQW (optional)
    
    uint64_t    last_sync_sec_;  ///< Last GPS sync time (seconds)
    uint64_t    last_sync_nsec_; ///< Last GPS sync time (nanoseconds)
    uint64_t    last_sync_time_; ///< Last GPS sync combined time
    uint32_t    drift_ppb_;      ///< Estimated RTC drift (parts-per-billion)
    double      measured_drift_ppm_; ///< Measured drift in PPM
    
    uint64_t    last_pps_sec_;   ///< Last PPS timestamp (seconds)
    uint32_t    last_pps_nsec_;  ///< Last PPS timestamp (nanoseconds)
    int         last_pps_seq_;   ///< Last PPS sequence number
    
    // Private helper methods
    void convert_rtc_to_ptp(const RtcTime& rtc, uint64_t* seconds);
    void convert_ptp_to_rtc(uint64_t seconds, RtcTime* rtc);
    bool update_drift_estimate(uint64_t gps_seconds, uint64_t rtc_seconds);
    uint32_t calculate_holdover_quality(uint64_t current_time_sec);

public:
    // RTC frequency discipline methods (public for manual drift measurement)
    double measure_drift_ppm(uint64_t gps_time_ns, uint64_t rtc_time_ns, uint32_t interval_sec);
    int8_t calculate_aging_offset(double drift_ppm);
    bool apply_frequency_discipline(double drift_ppm);
    int8_t read_aging_offset();
    bool write_aging_offset(int8_t offset);  // Direct write for incremental adjustments
    double get_temperature();
    
    /**
     * @brief Get RTC time with nanosecond precision (from SQW if available)
     * @param seconds Output: Seconds since epoch
     * @param nanoseconds Output: Nanoseconds (from PPS if SQW enabled, else 0)
     * @return true on success, false on failure
     * 
     * @note If SQW is available, reads PPS timestamp for nanosecond precision.
     *       Otherwise falls back to integer-second RTC reading.
     */
    bool get_time(uint64_t* seconds, uint32_t* nanoseconds);
    
    // DS3231 Square Wave Output (1Hz) - for high-precision drift measurement
    bool enable_sqw_output(bool enable = true);  ///< Enable/disable 1Hz square wave output
    bool is_sqw_available() const { return !sqw_device_.empty(); }  ///< Check if SQW configured
    const std::string& get_sqw_device() const { return sqw_device_; }  ///< Get SQW PPS device path
};

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
