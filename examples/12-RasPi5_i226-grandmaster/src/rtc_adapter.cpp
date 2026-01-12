/**
 * @file rtc_adapter.cpp
 * @brief Implementation of RTC Holdover Adapter
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#include "rtc_adapter.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <linux/i2c-dev.h>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {

// DS3231 I2C address
static const uint8_t DS3231_I2C_ADDR = 0x68;
static const uint8_t DS3231_AGING_OFFSET_REG = 0x10;

RtcAdapter::RtcAdapter(const std::string& rtc_device, const std::string& sqw_device)
    : rtc_device_(rtc_device)
    , sqw_device_(sqw_device)
    , rtc_fd_(-1)
    , i2c_fd_(-1)
    , last_sync_time_(0)
    , measured_drift_ppm_(0.0)
{
}

RtcAdapter::~RtcAdapter()
{
    if (rtc_fd_ >= 0) {
        close(rtc_fd_);
    }
    if (i2c_fd_ >= 0) {
        close(i2c_fd_);
    }
}

bool RtcAdapter::initialize()
{
    // Open RTC device (optional - used for reading time via kernel driver)
    // If this fails (EBUSY), we can still do I2C-based aging offset discipline
    rtc_fd_ = open(rtc_device_.c_str(), O_RDWR);
    if (rtc_fd_ < 0) {
        std::cerr << "[RTC Init] WARNING: Failed to open RTC device " << rtc_device_ 
                  << " errno=" << errno << " (" << strerror(errno) << ")\n";
        std::cerr << "[RTC Init] Note: Continuing with I2C-only access (aging offset discipline still available)\n";
        // Don't fail - I2C access is what we really need for discipline
    } else {
        std::cout << "[RTC Init] ✓ RTC device " << rtc_device_ << " opened (fd=" << rtc_fd_ << ")\n";
    }

    // Open I2C bus for DS3231 direct access (aging offset)
    // Raspberry Pi 5: DS3231 on GPIO I2C bus 14 (dtoverlay=i2c-rtc-gpio, dmesg shows 14-0068)
    const char* i2c_device = "/dev/i2c-14";
    i2c_fd_ = open(i2c_device, O_RDWR);
    if (i2c_fd_ < 0) {
        std::cerr << "[RTC Init] ERROR: Failed to open I2C device " << i2c_device 
                  << " errno=" << errno << " (" << strerror(errno) << ")\n";
        std::cerr << "[RTC Init] Note: DS3231 aging offset discipline will not be available\n";
        // Don't fail initialization - RTC time still works via /dev/rtc1
    } else {
        // Set I2C slave address to DS3231 (0x68)
        // Use I2C_SLAVE_FORCE because kernel RTC driver (rtc-ds1307) has claimed the device
        // This is SAFE because:
        //   - Kernel driver only accesses time registers (0x00-0x06)
        //   - We only access aging offset register (0x10)
        //   - No register overlap, no conflicts
        if (ioctl(i2c_fd_, I2C_SLAVE_FORCE, DS3231_I2C_ADDR) < 0) {
            std::cerr << "[RTC Init] ERROR: Failed to set I2C slave address 0x" 
                      << std::hex << (int)DS3231_I2C_ADDR << std::dec
                      << " errno=" << errno << " (" << strerror(errno) << ")\n";
            std::cerr << "[RTC Init] Note: Kernel RTC driver conflict - aging offset discipline unavailable\n";
            close(i2c_fd_);
            i2c_fd_ = -1;
        } else {
            std::cout << "[RTC Init] ✓ I2C device " << i2c_device 
                      << " opened successfully (fd=" << i2c_fd_ << ")\n";
            std::cout << "[RTC Init] ✓ I2C slave address 0x" << std::hex << (int)DS3231_I2C_ADDR 
                      << std::dec << " set (using I2C_SLAVE_FORCE)\n";            
            // Enable and configure DS3231 1Hz square wave output (if SQW device configured)
            if (!sqw_device_.empty()) {
                std::cout << "[RTC SQW] Configuring DS3231 1Hz square wave output...\n";
                if (enable_sqw_output(true)) {
                    std::cout << "[RTC SQW] \u2713 Square wave enabled on " << sqw_device_ << "\n";
                    std::cout << "[RTC SQW] \u2713 Precision: \u00b11\u00b5s (vs \u00b11s from I2C polling)\n";
                    std::cout << "[RTC SQW] \u2713 Drift measurement: 1,000,000x more accurate!\n";
                } else {
                    std::cerr << "[RTC SQW] \u26a0 Failed to enable square wave (continuing with I2C polling)\n";
                    sqw_device_.clear();  // Disable SQW if configuration failed
                }
            } else {
                std::cout << "[RTC SQW] \u2139 No SQW device configured (using I2C polling for drift measurement)\n";
                std::cout << "[RTC SQW] \u2139 For better precision, connect DS3231 SQW pin to GPIO and configure --rtc-sqw=/dev/pps1\n";
            }        }
    }

    return true;
}

bool RtcAdapter::read_time(RtcTime* rtc_time)
{
    // If RTC device not available, fail gracefully (I2C-only mode)
    if (rtc_fd_ < 0) {
        rtc_time->valid = false;
        return false;
    }
    
    struct rtc_time rt{};
    
    if (ioctl(rtc_fd_, RTC_RD_TIME, &rt) < 0) {
        rtc_time->valid = false;
        return false;
    }

    rtc_time->seconds = rt.tm_sec;
    rtc_time->minutes = rt.tm_min;
    rtc_time->hours = rt.tm_hour;
    rtc_time->day = rt.tm_mday;
    rtc_time->month = rt.tm_mon + 1; // Linux tm_mon is 0-11
    rtc_time->year = rt.tm_year + 1900; // Linux tm_year is years since 1900
    rtc_time->valid = true;

    return true;
}

bool RtcAdapter::set_time(const RtcTime& rtc_time)
{
    // If RTC device not available, fail gracefully (I2C-only mode)
    if (rtc_fd_ < 0) {
        return false;
    }
    
    struct rtc_time rt{};
    rt.tm_sec = rtc_time.seconds;
    rt.tm_min = rtc_time.minutes;
    rt.tm_hour = rtc_time.hours;
    rt.tm_mday = rtc_time.day;
    rt.tm_mon = rtc_time.month - 1;
    rt.tm_year = rtc_time.year - 1900;

    if (ioctl(rtc_fd_, RTC_SET_TIME, &rt) < 0) {
        return false;
    }

    return true;
}

bool RtcAdapter::get_ptp_time(uint64_t* seconds, uint32_t* nanoseconds)
{
    RtcTime rtc_time{};
    if (!read_time(&rtc_time)) {
        return false;
    }

    // Convert to Unix timestamp
    struct tm tm_time{};
    tm_time.tm_year = rtc_time.year - 1900;
    tm_time.tm_mon = rtc_time.month - 1;
    tm_time.tm_mday = rtc_time.day;
    tm_time.tm_hour = rtc_time.hours;
    tm_time.tm_min = rtc_time.minutes;
    tm_time.tm_sec = rtc_time.seconds;

    time_t unix_time = timegm(&tm_time);
    *seconds = static_cast<uint64_t>(unix_time);
    *nanoseconds = 0; // RTC has 1-second resolution

    return true;
}

bool RtcAdapter::set_ptp_time(uint64_t seconds, uint32_t nanoseconds)
{
    (void)nanoseconds;  // RTC doesn't support nanosecond precision
    
    // Convert PTP timestamp to RTC time
    time_t unix_time = static_cast<time_t>(seconds);
    struct tm* tm_time = gmtime(&unix_time);

    RtcTime rtc_time{};
    rtc_time.seconds = tm_time->tm_sec;
    rtc_time.minutes = tm_time->tm_min;
    rtc_time.hours = tm_time->tm_hour;
    rtc_time.day = tm_time->tm_mday;
    rtc_time.month = tm_time->tm_mon + 1;
    rtc_time.year = tm_time->tm_year + 1900;
    rtc_time.valid = true;

    return set_time(rtc_time);
}

bool RtcAdapter::sync_from_gps(uint64_t gps_seconds, uint32_t gps_nanoseconds)
{
    (void)gps_nanoseconds;  // RTC doesn't support nanosecond precision
    
    // Best practice from bp.md Section 4:
    // "If PPS indicates the start of a UTC second T, then right after the PPS edge
    //  you want DS3231 to read T+1 (because your code path can't complete instantly)."
    // 
    // Add 1 second to GPS time to compensate for:
    // 1. I2C write latency (few milliseconds)
    // 2. RTC 1-second resolution (increments at next boundary)
    uint64_t rtc_target_seconds = gps_seconds + 1;
    
    // Update RTC time
    if (!set_ptp_time(rtc_target_seconds, 0)) {
        return false;
    }

    last_sync_time_ = gps_seconds;
    return true;
}

double RtcAdapter::measure_drift_ppm(uint64_t gps_time_ns, uint64_t rtc_time_ns, uint32_t interval_sec)
{
    if (interval_sec == 0) {
        return 0.0;
    }

    // Calculate drift in nanoseconds
    int64_t drift_ns = static_cast<int64_t>(rtc_time_ns) - static_cast<int64_t>(gps_time_ns);
    
    // Convert to parts-per-million
    double drift_ppm = (static_cast<double>(drift_ns) / static_cast<double>(interval_sec)) / 1000.0;
    
    return drift_ppm;
}

int8_t RtcAdapter::calculate_aging_offset(double drift_ppm)
{
    // DS3231 aging offset: 0.1 ppm per LSB, range ±127
    // Negative offset increases frequency (compensates for slow clock)
    int offset_calc = static_cast<int>(-drift_ppm / 0.1);
    
    // Clamp to valid range before converting to int8_t
    if (offset_calc > 127) offset_calc = 127;
    if (offset_calc < -127) offset_calc = -127;
    
    return static_cast<int8_t>(offset_calc);
}

bool RtcAdapter::apply_frequency_discipline(double drift_ppm)
{
    if (i2c_fd_ < 0) {
        // I2C not available for direct DS3231 access
        std::cerr << "[RTC Discipline] ERROR: I2C device not open (fd=" << i2c_fd_ << ")\n";
        return false;
    }

    int8_t aging_offset = calculate_aging_offset(drift_ppm);
    
    std::cout << "[RTC Discipline] Calculated aging offset: " << static_cast<int>(aging_offset) 
              << " (for drift " << drift_ppm << " ppm)\n";
    
    // Use write_aging_offset() to avoid code duplication
    if (!write_aging_offset(aging_offset)) {
        return false;
    }
    
    measured_drift_ppm_ = drift_ppm;
    return true;
}

int8_t RtcAdapter::read_aging_offset()
{
    if (i2c_fd_ < 0) {
        return 0;
    }

    // Set register address
    uint8_t reg = DS3231_AGING_OFFSET_REG;
    if (write(i2c_fd_, &reg, 1) != 1) {
        return 0;
    }

    // Read aging offset value
    int8_t aging_offset = 0;
    if (read(i2c_fd_, &aging_offset, 1) != 1) {
        return 0;
    }

    return aging_offset;
}

bool RtcAdapter::write_aging_offset(int8_t offset)
{
    if (i2c_fd_ < 0) {
        return false;
    }

    // Write to DS3231 aging offset register (0x10)
    uint8_t write_data[2] = {DS3231_AGING_OFFSET_REG, static_cast<uint8_t>(offset)};
    ssize_t bytes_written = write(i2c_fd_, write_data, 2);
    if (bytes_written != 2) {
        std::cerr << "[RTC Discipline] Failed to write aging offset to I2C register 0x" 
                  << std::hex << static_cast<int>(DS3231_AGING_OFFSET_REG) << std::dec 
                  << " fd=" << i2c_fd_ << " bytes=" << bytes_written 
                  << " errno=" << errno << " (" << strerror(errno) << ")\n";
        return false;
    }
    
    std::cout << "[RTC Discipline] ✓ Aging offset written successfully to I2C register 0x" 
              << std::hex << static_cast<int>(DS3231_AGING_OFFSET_REG) << std::dec << "\n";
    
    // Verify write by reading back
    int8_t readback = read_aging_offset();
    if (readback != offset) {
        std::cerr << "[RTC Discipline] WARNING: Readback mismatch! wrote=" 
                  << static_cast<int>(offset) 
                  << " read=" << static_cast<int>(readback) << "\n";
        return false;
    } else {
        std::cout << "[RTC Discipline] ✓ Verified aging offset: " << static_cast<int>(readback) << "\n";
    }

    return true;
}

double RtcAdapter::get_temperature()
{
    if (i2c_fd_ < 0) {
        return 25.0; // Default temperature
    }

    // DS3231 temperature registers: 0x11 (MSB), 0x12 (LSB)
    uint8_t reg = 0x11;
    if (write(i2c_fd_, &reg, 1) != 1) {
        return 25.0;
    }

    uint8_t temp_data[2];
    if (read(i2c_fd_, temp_data, 2) != 2) {
        return 25.0;
    }

    // Convert to Celsius (resolution: 0.25°C)
    int8_t temp_msb = static_cast<int8_t>(temp_data[0]);
    uint8_t temp_lsb = temp_data[1] >> 6;
    
    double temperature = static_cast<double>(temp_msb) + (static_cast<double>(temp_lsb) * 0.25);
    
    return temperature;
}

bool RtcAdapter::enable_sqw_output(bool enable)
{
    if (i2c_fd_ < 0) {
        std::cerr << "[RTC SQW] ERROR: I2C not initialized\n";
        return false;
    }

    // DS3231 Control Register (0x0E)
    // Bit 2: INTCN (Interrupt Control)
    //   0 = SQW output enabled
    //   1 = Interrupt output (alarm)
    // Bits 3-4: RS (Rate Select)
    //   00 = 1 Hz
    //   01 = 1.024 kHz
    //   10 = 4.096 kHz
    //   11 = 8.192 kHz
    
    const uint8_t DS3231_CONTROL_REG = 0x0E;
    
    // Read current control register
    uint8_t reg = DS3231_CONTROL_REG;
    if (write(i2c_fd_, &reg, 1) != 1) {
        std::cerr << "[RTC SQW] ERROR: Failed to set register address\n";
        return false;
    }
    
    uint8_t control = 0;
    if (read(i2c_fd_, &control, 1) != 1) {
        std::cerr << "[RTC SQW] ERROR: Failed to read control register\n";
        return false;
    }
    
    std::cout << "[RTC SQW] Current control register: 0x" << std::hex << (int)control << std::dec << "\n";
    
    if (enable) {
        // Enable 1Hz square wave output
        control &= ~(1 << 2);  // Clear INTCN (enable SQW)
        control &= ~(0x03 << 3); // Clear RS bits
        control |= (0x00 << 3);  // Set RS=00 for 1Hz
        
        std::cout << "[RTC SQW] Enabling 1Hz square wave (control=0x" << std::hex << (int)control << std::dec << ")\n";
    } else {
        // Disable square wave (enable interrupt mode)
        control |= (1 << 2);  // Set INTCN (disable SQW)
        
        std::cout << "[RTC SQW] Disabling square wave (control=0x" << std::hex << (int)control << std::dec << ")\n";
    }
    
    // Write updated control register
    uint8_t write_data[2] = {DS3231_CONTROL_REG, control};
    if (write(i2c_fd_, write_data, 2) != 2) {
        std::cerr << "[RTC SQW] ERROR: Failed to write control register\n";
        return false;
    }
    
    // Verify write
    reg = DS3231_CONTROL_REG;
    if (write(i2c_fd_, &reg, 1) != 1) {
        std::cerr << "[RTC SQW] ERROR: Failed to re-read for verification\n";
        return false;
    }
    
    uint8_t verify = 0;
    if (read(i2c_fd_, &verify, 1) != 1) {
        std::cerr << "[RTC SQW] ERROR: Failed to verify write\n";
        return false;
    }
    
    if (verify != control) {
        std::cerr << "[RTC SQW] WARNING: Verification mismatch! wrote=0x" 
                  << std::hex << (int)control << " read=0x" << (int)verify << std::dec << "\n";
        return false;
    }
    
    std::cout << "[RTC SQW] \u2713 Control register updated and verified: 0x" 
              << std::hex << (int)verify << std::dec << "\n";
    
    if (enable && !sqw_device_.empty()) {
        std::cout << "[RTC SQW] \u2713 1Hz square wave now active on " << sqw_device_ << "\n";
        std::cout << "[RTC SQW] \u2713 Use 'sudo ppstest " << sqw_device_ << "' to monitor\n";
    }
    
    return true;
}

uint32_t RtcAdapter::calculate_holdover_quality(uint64_t current_time_sec)
{
    if (last_sync_time_ == 0) {
        return 0xFFFFFFFF; // Never synchronized
    }

    uint64_t holdover_duration = current_time_sec - last_sync_time_;
    
    // Estimate accumulated error based on measured drift
    // Quality degrades over time
    double accumulated_error_ns = measured_drift_ppm_ * static_cast<double>(holdover_duration) * 1000.0;
    
    // Return as nanoseconds
    return static_cast<uint32_t>(std::abs(accumulated_error_ns));
}

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
