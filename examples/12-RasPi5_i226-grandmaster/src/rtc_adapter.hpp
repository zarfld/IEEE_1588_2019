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
     */
    explicit RtcAdapter(const std::string& rtc_device);
    
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
     * @brief Get RTC temperature (if supported)
     * @param temperature_c Output: Temperature in Celsius
     * @return true if temperature available, false otherwise
     */
    bool get_temperature(float* temperature_c);
    
    /**
     * @brief Calculate holdover clock quality
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
                                   uint8_t* clock_accuracy);

private:
    std::string rtc_device_;     ///< RTC device path
    int         rtc_fd_;         ///< RTC device file descriptor
    
    uint64_t    last_sync_sec_;  ///< Last GPS sync time (seconds)
    uint32_t    drift_ppb_;      ///< Estimated RTC drift (parts-per-billion)
    
    // Private helper methods
    void convert_rtc_to_ptp(const RtcTime& rtc, uint64_t* seconds);
    void convert_ptp_to_rtc(uint64_t seconds, RtcTime* rtc);
    bool update_drift_estimate(uint64_t gps_seconds, uint64_t rtc_seconds);
};

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
