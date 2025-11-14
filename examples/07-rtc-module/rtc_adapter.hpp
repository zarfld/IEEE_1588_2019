/**
 * @file rtc_adapter.hpp
 * @brief RTC Module Time Source Adapter for IEEE 1588-2019 PTP
 * 
 * Adapter that interfaces with Real-Time Clock modules (DS3231, DS1307, PCF8523)
 * and can act as BOTH a time source AND a time sink for synchronization.
 * 
 * This example demonstrates:
 * - Using Types::TimeSource::Internal_Oscillator (0xA0) from the library
 * - RTC as TIME SOURCE when no better source available (clockClass 248)
 * - RTC as TIME SINK synchronized by GPS/NTP/DCF77 (BMCA driven)
 * - Bidirectional time synchronization pattern
 * 
 * IMPORTANT: This adapter USES the library's types, it does NOT duplicate them!
 * 
 * RTC Module Background:
 * - DS3231: High-precision I2C RTC with TCXO (±2ppm accuracy)
 * - DS1307: Basic I2C RTC with external crystal (±250ppm accuracy)
 * - PCF8523: Low-power I2C RTC (±3ppm accuracy)
 * - Battery backup maintains time during power loss
 * - Typical accuracy: ±1-5 seconds/day (without external sync)
 * - With GPS/NTP sync: Maintains ±1ms during sync loss (holdover)
 * 
 * Hardware Requirements:
 * - DS3231/DS1307/PCF8523 RTC module
 * - I2C interface (SDA, SCL pins)
 * - Arduino, ESP32, or any microcontroller with I2C
 * - Pull-up resistors on I2C lines (4.7kΩ typical)
 * - Battery backup (CR2032 for DS3231/DS1307)
 * 
 * Use Cases:
 * 1. **Fallback Time Source**: When GPS/NTP/DCF77 unavailable
 * 2. **Time Persistence**: Maintain time across power cycles
 * 3. **Low-Power Operation**: Battery-backed time keeping
 * 4. **Holdover Mode**: Bridge gaps in primary source availability
 * 
 * @see IEEE 1588-2019, Section 8.6.2.7 "timeSource"
 * @see IEEE 1588-2019, Table 6 "timeSource enumeration" (INTERNAL_OSCILLATOR = 0xA0)
 * @see include/IEEE/1588/PTP/2019/types.hpp for Types::ClockQuality
 * @see include/clocks.hpp for DefaultDataSet and TimePropertiesDataSet
 */

#ifndef RTC_ADAPTER_HPP
#define RTC_ADAPTER_HPP

#include "IEEE/1588/PTP/2019/types.hpp"  // Use library's Types::ClockQuality, Types::TimeSource
#include <cstdint>
#include <chrono>

namespace Examples {
namespace RTC {

// Namespace alias for IEEE 1588-2019 Types
namespace Types = IEEE::_1588::PTP::_2019::Types;

/**
 * @brief RTC Module Type
 */
enum class RTCModuleType {
    DS3231,     ///< High-precision TCXO (±2ppm, -40°C to +85°C)
    DS1307,     ///< Basic crystal (±250ppm, 0°C to +70°C)
    PCF8523,    ///< Low-power (±3ppm, -40°C to +85°C)
    Unknown
};

/**
 * @brief RTC Time Data
 */
struct RTCTime {
    uint16_t year;      ///< 2000-2099
    uint8_t month;      ///< 1-12
    uint8_t day;        ///< 1-31
    uint8_t hour;       ///< 0-23
    uint8_t minute;     ///< 0-59
    uint8_t second;     ///< 0-59
    uint8_t weekday;    ///< 0-6 (Sunday = 0)
};

/**
 * @brief RTC Module Adapter
 * 
 * Interfaces with hardware RTC modules and computes IEEE 1588-2019 clock quality.
 * Can operate as BOTH time source (read) and time sink (write).
 * 
 * Example Usage - RTC as Time Source:
 * @code
 * RTCAdapter rtc(0x68, RTCModuleType::DS3231);
 * rtc.initialize();
 * 
 * // Read time from RTC
 * Types::Timestamp time = rtc.get_current_time();
 * Types::ClockQuality quality = rtc.get_clock_quality();  // clockClass 248
 * 
 * // Update PTP clock
 * auto& ds = ptp_clock.get_default_data_set();
 * ds.clockQuality = quality;
 * 
 * auto& tp = ptp_clock.get_time_properties_data_set();
 * tp.timeSource = static_cast<uint8_t>(Types::TimeSource::Internal_Oscillator);
 * @endcode
 * 
 * Example Usage - RTC as Time Sink (synchronized by GPS):
 * @code
 * RTCAdapter rtc(0x68, RTCModuleType::DS3231);
 * GPSAdapter gps("/dev/ttyUSB0");
 * 
 * rtc.initialize();
 * gps.initialize();
 * 
 * // GPS provides better time
 * if (gps.update() && gps.is_synchronized()) {
 *     Types::Timestamp gps_time = gps.get_current_time();
 *     Types::ClockQuality gps_quality = gps.get_clock_quality();
 *     
 *     // Synchronize RTC with GPS (BMCA: GPS clockClass 6 > RTC clockClass 248)
 *     if (gps_quality.clock_class < rtc.get_clock_quality().clock_class) {
 *         rtc.set_time(gps_time);  // Write GPS time to RTC
 *         std::cout << "RTC synchronized with GPS" << std::endl;
 *     }
 * }
 * 
 * // Later: GPS lost, RTC provides fallback time
 * if (!gps.is_synchronized()) {
 *     Types::Timestamp rtc_time = rtc.get_current_time();  // Read from RTC
 *     ptp_clock.tick(rtc_time);  // Use RTC as fallback
 * }
 * @endcode
 */
class RTCAdapter {
public:
    /**
     * @brief Construct RTC adapter
     * 
     * @param i2c_address I2C address (0x68 for DS3231/DS1307, 0x68 for PCF8523)
     * @param module_type RTC module type for accuracy characteristics
     */
    explicit RTCAdapter(
        uint8_t i2c_address = 0x68,
        RTCModuleType module_type = RTCModuleType::DS3231);
    
    ~RTCAdapter() = default;
    
    /**
     * @brief Initialize RTC module
     * 
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Update RTC status and drift tracking
     * 
     * Should be called periodically to track RTC drift when synchronized
     * with external sources.
     * 
     * @return true if RTC is accessible
     */
    bool update();
    
    /**
     * @brief Get current time from RTC
     * 
     * Reads current time from RTC module and converts to PTP Timestamp format.
     * 
     * @return PTP Timestamp (seconds + nanoseconds since Unix epoch)
     * @note Returns {0, 0} if RTC read fails
     */
    Types::Timestamp get_current_time() const;
    
    /**
     * @brief Set RTC time (synchronize from external source)
     * 
     * Writes time to RTC module, typically from a better time source
     * selected by BMCA (GPS, NTP, DCF77).
     * 
     * @param time PTP Timestamp to write to RTC
     * @return true if write successful
     * 
     * @note This enables RTC as TIME SINK
     * @note Records sync time for holdover quality computation
     */
    bool set_time(const Types::Timestamp& time);
    
    /**
     * @brief Get clock quality using LIBRARY's Types::ClockQuality
     * 
     * Computes clock quality based on:
     * - Module type (DS3231 ±2ppm vs DS1307 ±250ppm)
     * - Time since last external synchronization
     * - Estimated drift accumulation
     * 
     * Clock class mapping:
     * - Recently synced (<1 hour): clockClass 52 (degraded by asymmetric path)
     * - Synced (<24 hours): clockClass 187 (degraded accuracy)
     * - Holdover (>24 hours): clockClass 248 (default/unsynchronized)
     * - Never synced: clockClass 248 (default)
     * 
     * @return Library's Types::ClockQuality struct
     */
    Types::ClockQuality get_clock_quality() const;
    
    /**
     * @brief Get time source type - always Internal_Oscillator from library enum
     * 
     * @return Types::TimeSource::Internal_Oscillator (0xA0) from library
     */
    constexpr uint8_t get_time_source() const {
        return static_cast<uint8_t>(IEEE::_1588::PTP::_2019::Types::TimeSource::Internal_Oscillator);
    }
    
    /**
     * @brief Check if RTC has been synchronized from external source
     * 
     * @return true if set_time() has been called successfully
     */
    bool is_synchronized() const;
    
    /**
     * @brief Get seconds since last external synchronization
     * 
     * @return Seconds since last set_time() call, or -1 if never synchronized
     */
    int32_t get_seconds_since_sync() const;
    
    /**
     * @brief Get estimated time offset from ideal (nanoseconds)
     * 
     * Based on module drift characteristics and time since sync.
     * 
     * @return Estimated offset in nanoseconds
     */
    int64_t get_estimated_offset_ns() const;
    
    /**
     * @brief Get RTC module temperature (DS3231 only)
     * 
     * @return Temperature in °C, or NaN if not supported
     */
    float get_temperature_celsius() const;
    
    /**
     * @brief Get module type
     * 
     * @return RTC module type
     */
    RTCModuleType get_module_type() const { return module_type_; }

private:
    // Configuration
    uint8_t i2c_address_;
    RTCModuleType module_type_;
    
    // Synchronization tracking
    bool synchronized_{false};
    std::chrono::steady_clock::time_point last_sync_time_;
    Types::Timestamp last_sync_value_{0, 0};
    
    // Drift tracking
    int64_t estimated_drift_ppm_{0};  ///< Measured drift in parts per million
    
    /**
     * @brief Read time from RTC hardware
     * 
     * @param[out] time RTC time structure
     * @return true if read successful
     */
    bool read_rtc_time(RTCTime& time) const;
    
    /**
     * @brief Write time to RTC hardware
     * 
     * @param time RTC time structure
     * @return true if write successful
     */
    bool write_rtc_time(const RTCTime& time);
    
    /**
     * @brief Convert RTC time to PTP Timestamp
     * 
     * @param rtc_time RTC time structure
     * @return PTP Timestamp (Unix epoch)
     */
    Types::Timestamp rtc_time_to_timestamp(const RTCTime& rtc_time) const;
    
    /**
     * @brief Convert PTP Timestamp to RTC time
     * 
     * @param timestamp PTP Timestamp (Unix epoch)
     * @return RTC time structure
     */
    RTCTime timestamp_to_rtc_time(const Types::Timestamp& timestamp) const;
    
    /**
     * @brief Convert time since sync to IEEE 1588-2019 clockClass
     * 
     * Mapping based on holdover performance:
     * - <1 hour: clockClass 52 (degraded by asymmetric path)
     * - <24 hours: clockClass 187 (degraded accuracy)
     * - >24 hours: clockClass 248 (default, unsynchronized)
     * 
     * @return IEEE 1588-2019 clockClass value
     */
    uint8_t time_since_sync_to_clock_class() const;
    
    /**
     * @brief Get module drift characteristics (ppm)
     * 
     * @return Typical drift in parts per million
     */
    int32_t get_module_drift_ppm() const;
    
    /**
     * @brief Compute clock accuracy from drift and time since sync
     * 
     * @return IEEE 1588-2019 clockAccuracy enumeration
     */
    uint8_t compute_clock_accuracy() const;
    
    /**
     * @brief Compute offset scaled log variance
     * 
     * @return IEEE 1588-2019 offsetScaledLogVariance
     */
    uint16_t compute_offset_scaled_log_variance() const;
    
    /**
     * @brief Read single byte from RTC register
     * 
     * @param reg Register address
     * @param[out] value Read value
     * @return true if read successful
     */
    bool read_register(uint8_t reg, uint8_t& value) const;
    
    /**
     * @brief Write single byte to RTC register
     * 
     * @param reg Register address
     * @param value Value to write
     * @return true if write successful
     */
    bool write_register(uint8_t reg, uint8_t value);
    
    /**
     * @brief BCD to decimal conversion
     */
    static uint8_t bcd_to_dec(uint8_t bcd) {
        return (bcd >> 4) * 10 + (bcd & 0x0F);
    }
    
    /**
     * @brief Decimal to BCD conversion
     */
    static uint8_t dec_to_bcd(uint8_t dec) {
        return ((dec / 10) << 4) | (dec % 10);
    }
};

} // namespace RTC
} // namespace Examples

#endif // RTC_ADAPTER_HPP
