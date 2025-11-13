/**
 * @file rtc_adapter.cpp
 * @brief RTC Module Time Source Adapter Implementation
 */

#include "rtc_adapter.hpp"
#include <cstring>
#include <cmath>
#include <ctime>

// Platform-specific I2C access
// NOTE: This is a simplified example - real implementation would use
// ESP32 I2C HAL, Arduino Wire library, or platform-specific APIs
namespace I2C {
    bool begin() {
        // Platform-specific I2C initialization
        // ESP32: i2c_driver_install()
        // Arduino: Wire.begin()
        return true;  // Placeholder
    }
    
    bool write_byte(uint8_t address, uint8_t reg, uint8_t value) {
        // Platform-specific I2C write
        // ESP32: i2c_master_write_to_device()
        // Arduino: Wire.beginTransmission(), Wire.write(), Wire.endTransmission()
        return true;  // Placeholder
    }
    
    bool read_byte(uint8_t address, uint8_t reg, uint8_t& value) {
        // Platform-specific I2C read
        // ESP32: i2c_master_write_read_device()
        // Arduino: Wire.beginTransmission(), Wire.write(), Wire.requestFrom(), Wire.read()
        value = 0;
        return true;  // Placeholder
    }
    
    bool read_bytes(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) {
        // Platform-specific I2C burst read
        for (size_t i = 0; i < length; i++) {
            if (!read_byte(address, reg + i, buffer[i])) {
                return false;
            }
        }
        return true;
    }
}

namespace Examples {
namespace RTC {

// Namespace alias for IEEE 1588-2019 Types (inside our namespace)
namespace Types = IEEE::_1588::PTP::_2019::Types;

// DS3231 Register addresses
constexpr uint8_t DS3231_REG_SECONDS = 0x00;
constexpr uint8_t DS3231_REG_MINUTES = 0x01;
constexpr uint8_t DS3231_REG_HOURS = 0x02;
constexpr uint8_t DS3231_REG_DAY = 0x03;
constexpr uint8_t DS3231_REG_DATE = 0x04;
constexpr uint8_t DS3231_REG_MONTH = 0x05;
constexpr uint8_t DS3231_REG_YEAR = 0x06;
constexpr uint8_t DS3231_REG_TEMP_MSB = 0x11;
constexpr uint8_t DS3231_REG_TEMP_LSB = 0x12;

// Module drift characteristics (ppm - parts per million)
constexpr int32_t DS3231_DRIFT_PPM = 2;      // ±2ppm TCXO
constexpr int32_t DS1307_DRIFT_PPM = 250;    // ±250ppm crystal
constexpr int32_t PCF8523_DRIFT_PPM = 3;     // ±3ppm crystal

RTCAdapter::RTCAdapter(uint8_t i2c_address, RTCModuleType module_type)
    : i2c_address_(i2c_address)
    , module_type_(module_type)
    , estimated_drift_ppm_(0)
{
}

bool RTCAdapter::initialize() {
    // Initialize I2C bus
    if (!I2C::begin()) {
        return false;
    }
    
    // Verify RTC is accessible by reading seconds register
    uint8_t seconds;
    if (!read_register(DS3231_REG_SECONDS, seconds)) {
        return false;
    }
    
    // Check if oscillator is running (bit 7 of seconds register should be 0)
    if (seconds & 0x80) {
        // Oscillator stopped - clear the bit to start it
        seconds &= 0x7F;
        if (!write_register(DS3231_REG_SECONDS, seconds)) {
            return false;
        }
    }
    
    return true;
}

bool RTCAdapter::update() {
    // Check RTC accessibility
    uint8_t dummy;
    return read_register(DS3231_REG_SECONDS, dummy);
}

Types::Timestamp RTCAdapter::get_current_time() const {
    RTCTime rtc_time;
    if (!read_rtc_time(rtc_time)) {
        // Return zero timestamp if read fails
        return Types::Timestamp{0, 0};
    }
    
    return rtc_time_to_timestamp(rtc_time);
}

bool RTCAdapter::set_time(const Types::Timestamp& time) {
    RTCTime rtc_time = timestamp_to_rtc_time(time);
    
    if (!write_rtc_time(rtc_time)) {
        return false;
    }
    
    // Record synchronization
    synchronized_ = true;
    last_sync_time_ = std::chrono::steady_clock::now();
    last_sync_value_ = time;
    
    return true;
}

Types::ClockQuality RTCAdapter::get_clock_quality() const {
    Types::ClockQuality quality;
    
    if (!is_synchronized()) {
        // Never synchronized - use default values
        quality.clock_class = 248;  // Default, not synchronized
        quality.clock_accuracy = 0xFE;  // Unknown
        quality.offset_scaled_log_variance = 0xFFFF;  // Max variance
        return quality;
    }
    
    // Compute quality based on time since synchronization and drift
    quality.clock_class = time_since_sync_to_clock_class();
    quality.clock_accuracy = compute_clock_accuracy();
    quality.offset_scaled_log_variance = compute_offset_scaled_log_variance();
    
    return quality;
}

bool RTCAdapter::is_synchronized() const {
    return synchronized_;
}

int32_t RTCAdapter::get_seconds_since_sync() const {
    if (!synchronized_) {
        return -1;  // Never synchronized
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_sync_time_);
    return static_cast<int32_t>(elapsed.count());
}

int64_t RTCAdapter::get_estimated_offset_ns() const {
    if (!synchronized_) {
        return 0;
    }
    
    int32_t seconds_since_sync = get_seconds_since_sync();
    int32_t drift_ppm = get_module_drift_ppm();
    
    // Calculate accumulated drift: (seconds * drift_ppm) nanoseconds
    // Example: 3600 seconds * 2 ppm = 7200 ns = 7.2 µs
    int64_t offset_ns = static_cast<int64_t>(seconds_since_sync) * drift_ppm * 1000LL;
    
    return offset_ns;
}

float RTCAdapter::get_temperature_celsius() const {
    if (module_type_ != RTCModuleType::DS3231) {
        return std::nanf("");  // Not supported
    }
    
    uint8_t msb, lsb;
    if (!read_register(DS3231_REG_TEMP_MSB, msb) || 
        !read_register(DS3231_REG_TEMP_LSB, lsb)) {
        return std::nanf("");
    }
    
    // DS3231 temperature: MSB is signed integer, LSB bits 7-6 are fractional (0.25°C per bit)
    int8_t temp_int = static_cast<int8_t>(msb);
    float temp_frac = (lsb >> 6) * 0.25f;
    
    return temp_int + temp_frac;
}

bool RTCAdapter::read_rtc_time(RTCTime& time) const {
    // Read 7 bytes from RTC (seconds through year)
    uint8_t buffer[7];
    if (!I2C::read_bytes(i2c_address_, DS3231_REG_SECONDS, buffer, 7)) {
        return false;
    }
    
    // Convert BCD to decimal
    time.second = bcd_to_dec(buffer[0] & 0x7F);  // Mask oscillator bit
    time.minute = bcd_to_dec(buffer[1] & 0x7F);
    time.hour = bcd_to_dec(buffer[2] & 0x3F);    // Mask 12/24 hour bit
    time.weekday = bcd_to_dec(buffer[3] & 0x07);
    time.day = bcd_to_dec(buffer[4] & 0x3F);
    time.month = bcd_to_dec(buffer[5] & 0x1F);   // Mask century bit
    time.year = 2000 + bcd_to_dec(buffer[6]);
    
    // Validate ranges
    if (time.second > 59 || time.minute > 59 || time.hour > 23 ||
        time.day < 1 || time.day > 31 || time.month < 1 || time.month > 12 ||
        time.year < 2000 || time.year > 2099) {
        return false;
    }
    
    return true;
}

bool RTCAdapter::write_rtc_time(const RTCTime& time) {
    // Convert decimal to BCD
    uint8_t buffer[7];
    buffer[0] = dec_to_bcd(time.second);
    buffer[1] = dec_to_bcd(time.minute);
    buffer[2] = dec_to_bcd(time.hour);        // 24-hour format
    buffer[3] = dec_to_bcd(time.weekday);
    buffer[4] = dec_to_bcd(time.day);
    buffer[5] = dec_to_bcd(time.month);
    buffer[6] = dec_to_bcd(time.year - 2000);
    
    // Write 7 bytes to RTC
    for (size_t i = 0; i < 7; i++) {
        if (!I2C::write_byte(i2c_address_, DS3231_REG_SECONDS + i, buffer[i])) {
            return false;
        }
    }
    
    return true;
}

Types::Timestamp RTCAdapter::rtc_time_to_timestamp(const RTCTime& rtc_time) const {
    // Convert to Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
    std::tm tm_time = {};
    tm_time.tm_year = rtc_time.year - 1900;  // tm_year is years since 1900
    tm_time.tm_mon = rtc_time.month - 1;     // tm_mon is 0-11
    tm_time.tm_mday = rtc_time.day;
    tm_time.tm_hour = rtc_time.hour;
    tm_time.tm_min = rtc_time.minute;
    tm_time.tm_sec = rtc_time.second;
    
    // Convert to Unix timestamp
    std::time_t unix_time = std::mktime(&tm_time);
    
    // PTP Timestamp uses Unix epoch (same as time_t)
    uint64_t seconds = static_cast<uint64_t>(unix_time);
    uint32_t nanoseconds = 0;  // RTC has 1-second resolution
    
    return Types::Timestamp{seconds, nanoseconds};
}

RTCTime RTCAdapter::timestamp_to_rtc_time(const Types::Timestamp& timestamp) const {
    // Convert Unix timestamp to calendar time
    std::time_t unix_time = static_cast<std::time_t>(timestamp.seconds_field);
    std::tm* tm_time = std::gmtime(&unix_time);
    
    RTCTime rtc_time;
    rtc_time.year = tm_time->tm_year + 1900;
    rtc_time.month = tm_time->tm_mon + 1;
    rtc_time.day = tm_time->tm_mday;
    rtc_time.hour = tm_time->tm_hour;
    rtc_time.minute = tm_time->tm_min;
    rtc_time.second = tm_time->tm_sec;
    rtc_time.weekday = tm_time->tm_wday;  // 0-6, Sunday = 0
    
    return rtc_time;
}

uint8_t RTCAdapter::time_since_sync_to_clock_class() const {
    int32_t seconds_since_sync = get_seconds_since_sync();
    
    if (seconds_since_sync < 0) {
        return 248;  // Never synchronized
    }
    
    // IEEE 1588-2019 Table 5 - clockClass values
    if (seconds_since_sync < 3600) {  // <1 hour
        return 52;  // Degraded by asymmetric path
    } else if (seconds_since_sync < 86400) {  // <24 hours
        return 187;  // Degraded accuracy
    } else {
        return 248;  // Default, unsynchronized
    }
}

int32_t RTCAdapter::get_module_drift_ppm() const {
    switch (module_type_) {
        case RTCModuleType::DS3231:
            return DS3231_DRIFT_PPM;
        case RTCModuleType::DS1307:
            return DS1307_DRIFT_PPM;
        case RTCModuleType::PCF8523:
            return PCF8523_DRIFT_PPM;
        default:
            return 100;  // Conservative estimate
    }
}

uint8_t RTCAdapter::compute_clock_accuracy() const {
    int64_t offset_ns = std::abs(get_estimated_offset_ns());
    
    // IEEE 1588-2019 Table 6 - clockAccuracy enumeration
    if (offset_ns < 25) return 0x20;          // <25ns
    if (offset_ns < 100) return 0x21;         // <100ns
    if (offset_ns < 250) return 0x22;         // <250ns
    if (offset_ns < 1000) return 0x23;        // <1µs
    if (offset_ns < 2500) return 0x24;        // <2.5µs
    if (offset_ns < 10000) return 0x25;       // <10µs
    if (offset_ns < 25000) return 0x26;       // <25µs
    if (offset_ns < 100000) return 0x27;      // <100µs
    if (offset_ns < 250000) return 0x28;      // <250µs
    if (offset_ns < 1000000) return 0x29;     // <1ms
    if (offset_ns < 2500000) return 0x2A;     // <2.5ms
    if (offset_ns < 10000000) return 0x2B;    // <10ms
    if (offset_ns < 25000000) return 0x2C;    // <25ms
    if (offset_ns < 100000000) return 0x2D;   // <100ms
    if (offset_ns < 250000000) return 0x2E;   // <250ms
    if (offset_ns < 1000000000) return 0x2F;  // <1s
    if (offset_ns < 10000000000LL) return 0x30; // <10s
    return 0x31;  // >10s
}

uint16_t RTCAdapter::compute_offset_scaled_log_variance() const {
    // Allan variance estimation based on drift and time since sync
    int32_t drift_ppm = get_module_drift_ppm();
    int32_t seconds_since_sync = get_seconds_since_sync();
    
    // Conservative variance estimate
    // variance = (drift_ppm * seconds)^2 in nanoseconds^2
    // offsetScaledLogVariance = log2(variance) * 256
    
    if (seconds_since_sync < 0) {
        return 0xFFFF;  // Max variance, never synced
    }
    
    if (seconds_since_sync < 3600) {  // <1 hour
        return 0x4E20;  // Good holdover
    } else if (seconds_since_sync < 86400) {  // <24 hours
        return 0x8000;  // Moderate holdover
    } else {
        return 0xE000;  // Poor holdover
    }
}

bool RTCAdapter::read_register(uint8_t reg, uint8_t& value) const {
    return I2C::read_byte(i2c_address_, reg, value);
}

bool RTCAdapter::write_register(uint8_t reg, uint8_t value) {
    return I2C::write_byte(i2c_address_, reg, value);
}

} // namespace RTC
} // namespace Examples
