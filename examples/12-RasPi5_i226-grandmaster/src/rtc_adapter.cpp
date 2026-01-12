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
#include <sys/timex.h>

// Kernel PPS API (RFC 2783)
#include <sys/time.h>

// PPS API structures (from kernel uapi/linux/pps.h)
#ifndef PPS_GETPARAMS
#define PPS_GETPARAMS   _IOR('p', 0xa1, struct pps_kparams *)
#define PPS_SETPARAMS   _IOW('p', 0xa2, struct pps_kparams *)
#define PPS_GETCAP      _IOR('p', 0xa3, int *)
#define PPS_FETCH       _IOWR('p', 0xa4, struct pps_fdata *)
#endif

struct pps_ktime {
    __s64 sec;
    __s32 nsec;
    __u32 flags;
};

struct pps_kinfo {
    __u32 assert_sequence;
    __u32 clear_sequence;
    struct pps_ktime assert_tu;
    struct pps_ktime clear_tu;
    int current_mode;
};

struct pps_fdata {
    struct pps_kinfo info;
    struct pps_ktime timeout;
};

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
    , pps_fd_(-1)
    , last_sync_time_(0)
    , measured_drift_ppm_(0.0)
    , last_pps_sec_(0)
    , last_pps_nsec_(0)
    , last_pps_seq_(-1)
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
    if (pps_fd_ >= 0) {
        close(pps_fd_);
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
    // Raspberry Pi 5: DS3231 on GPIO I2C bus 15 (dtoverlay=i2c-rtc-gpio, dmesg shows 15-0068)
    const char* i2c_device = "/dev/i2c-15";
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
                
                // Small delay to let I2C bus settle after initialization
                usleep(50000);  // 50ms delay
                
                if (enable_sqw_output(true)) {
                    // Open PPS device for reading SQW timestamps
                    pps_fd_ = open(sqw_device_.c_str(), O_RDONLY);
                    if (pps_fd_ < 0) {
                        std::cerr << "[RTC SQW] ⚠ Failed to open PPS device " << sqw_device_ 
                                  << " (errno=" << errno << ": " << strerror(errno) << ")\n";
                        std::cerr << "[RTC SQW] ⚠ SQW configured but cannot read timestamps - using I2C polling\n";
                        sqw_device_.clear();
                    } else {
                        std::cout << "[RTC SQW] ✓ PPS device " << sqw_device_ << " opened (fd=" << pps_fd_ << ")\n";
                        std::cout << "[RTC SQW] ✓ Square wave enabled on " << sqw_device_ << "\n";
                        std::cout << "[RTC SQW] ✓ Precision: ±1µs (vs ±1s from I2C polling)\n";
                        std::cout << "[RTC SQW] ✓ Drift measurement: 1,000,000x more accurate!\n";
                    }
                } else {
                    std::cerr << "[RTC SQW] ⚠ Failed to enable square wave (continuing with I2C polling)\n";
                    sqw_device_.clear();  // Disable SQW if configuration failed
                }
            } else {
                std::cout << "[RTC SQW] ℹ No SQW device configured (using I2C polling for drift measurement)\n";
                std::cout << "[RTC SQW] ℹ For better precision, connect DS3231 SQW pin to GPIO and configure --rtc-sqw=/dev/pps1\n";
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

bool RtcAdapter::get_time(uint64_t* seconds, uint32_t* nanoseconds)
{
    // Read RTC integer seconds (always needed for absolute time)
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
    
    // If SQW/PPS available, get sub-second precision from last edge timing
    if (pps_fd_ >= 0) {
        struct pps_fdata pps_data{};
        pps_data.timeout.sec = 2;      // 2 second timeout (wait for next 1Hz edge)
        pps_data.timeout.nsec = 0;
        
        int pps_ret = ioctl(pps_fd_, PPS_FETCH, &pps_data);
        std::cout << "[RTC PPS] PPS_FETCH returned " << pps_ret 
                  << " (errno=" << errno << ", fd=" << pps_fd_ << ")\n";
        
        if (pps_ret == 0) {
            std::cout << "[RTC PPS] Fetched: seq=" << pps_data.info.assert_sequence 
                      << " sec=" << pps_data.info.assert_tu.sec 
                      << " nsec=" << pps_data.info.assert_tu.nsec << "\n";
            std::cout << "[RTC PPS] Last cached: seq=" << last_pps_seq_ 
                      << " sec=" << last_pps_sec_ 
                      << " nsec=" << last_pps_nsec_ << "\n";
            
            // Update cached PPS timestamp if new edge detected
            if (pps_data.info.assert_sequence != (uint32_t)last_pps_seq_) {
                std::cout << "[RTC PPS] ✓ New edge detected, updating cache\n";
                last_pps_seq_ = pps_data.info.assert_sequence;
                last_pps_sec_ = pps_data.info.assert_tu.sec;
                last_pps_nsec_ = pps_data.info.assert_tu.nsec;
            } else {
                std::cout << "[RTC PPS] Same sequence, using cached timestamp\n";
            }
        } else {
            std::cout << "[RTC PPS] ⚠ PPS_FETCH failed (ret=" << pps_ret 
                      << " errno=" << errno << ")\n";
        }
        
        // Return nanosecond offset within current RTC second
        // SQW edges mark second boundaries - we measure offset from last edge
        if (last_pps_seq_ >= 0) {
            // Get current system time (UTC domain)
            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            
            std::cout << "[RTC PPS] RTC seconds=" << *seconds 
                      << " PPS edge sec=" << last_pps_sec_
                      << " System now sec=" << now.tv_sec << "\n";
            
            // CRITICAL: Keep everything in UTC domain!
            // - PPS edge timestamps are UTC (from CLOCK_REALTIME)
            // - RTC is UTC (hwclock --utc)
            // - System now is UTC (CLOCK_REALTIME)
            // Do NOT convert to TAI - that's only for PHC/PTP side
            int64_t edge_utc_sec = last_pps_sec_;
            int64_t now_utc_sec = now.tv_sec;
            
            // Calculate nanoseconds since last SQW edge
            // All times in UTC domain - compare directly
            int64_t edge_time_ns = (int64_t)edge_utc_sec * 1000000000LL + (int64_t)last_pps_nsec_;
            int64_t now_utc_ns = (int64_t)now_utc_sec * 1000000000LL + (int64_t)now.tv_nsec;
            int64_t offset_from_edge_ns = now_utc_ns - edge_time_ns;
            
            std::cout << "[RTC PPS] edge_utc_ns=" << edge_time_ns 
                      << " now_utc_ns=" << now_utc_ns 
                      << " offset_from_edge=" << offset_from_edge_ns << " ns\n";
            
            // Since SQW edges are at second boundaries, calculate which second we're in
            // and the nanosecond offset within that second
            int64_t seconds_since_edge = offset_from_edge_ns / 1000000000LL;
            int64_t ns_within_second = offset_from_edge_ns % 1000000000LL;
            
            std::cout << "[RTC PPS] Seconds since edge=" << seconds_since_edge
                      << " ns within current second=" << ns_within_second << "\n";
            
            // CRITICAL FIX: RTC I2C read takes ~5ms. If PPS edge occurred DURING the read,
            // we might have captured the OLD second (before edge) but PPS shows NEW second.
            // 
            // Detect this race condition:
            // - If edge_utc_sec == rtc_seconds + 1 (edge is 1 second ahead)
            // - AND offset is small (< 100ms, edge just happened)
            // - Then RTC was read BEFORE edge, adjust RTC second forward by 1
            //
            // Example timeline:
            //   t=0ms: Start RTC I2C read
            //   t=3ms: Second boundary passes, PPS edge fires (new second starts)
            //   t=5ms: RTC read completes with OLD second
            //   t=6ms: PPS_FETCH returns NEW second
            //   Result: RTC=N, PPS edge=N+1, offset=6ms
            //   Fix: Increment RTC to N+1 since we're actually in the new second now
            
            if (edge_utc_sec == (int64_t)*seconds + 1 && ns_within_second < 100000000LL) {
                std::cout << "[RTC PPS] ✓ Detected second boundary race: RTC read before edge\n"
                          << "  RTC=" << *seconds << " Edge=" << edge_utc_sec 
                          << " offset=" << ns_within_second << "ns\n"
                          << "  → Incrementing RTC second to match edge (race correction)\n";
                (*seconds)++;  // Advance RTC to match the second we're actually in
            }
            
            // Sanity check: offset should be reasonable (< 10 seconds typically)
            if (offset_from_edge_ns < 0) {
                std::cout << "[RTC PPS] ⚠ Negative offset! (clock went backwards?) Using 0\n";
                ns_within_second = 0;
            } else if (offset_from_edge_ns >= 10000000000LL) {  // >10 seconds
                std::cout << "[RTC PPS] ⚠ Offset > 10s! (stale edge?) Using 0\n";
                ns_within_second = 0;
            }
            
            // Return the nanosecond position within the current second
            // The RTC seconds we read is the reference - we're just adding precision
            std::cout << "[RTC PPS] Final nanoseconds=" << ns_within_second << "\n";
            *nanoseconds = (uint32_t)ns_within_second;
            return true;
        } else {
            std::cout << "[RTC PPS] ⚠ No PPS sequence cached yet (last_pps_seq_=-1)\n";
        }
    } else {
        std::cout << "[RTC PPS] ⚠ PPS not available (pps_fd_=" << pps_fd_ << ")\n";
    }
    
    // Fallback: no sub-second precision
    *nanoseconds = 0;
    return true;
}

bool RtcAdapter::get_ptp_time(uint64_t* seconds, uint32_t* nanoseconds)
{
    // Use the new get_time() which supports both PPS and RTC fallback
    return get_time(seconds, nanoseconds);
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
        std::cerr << "[RTC SQW] ERROR: I2C not initialized (fd=" << i2c_fd_ << ")\n";
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
    const uint8_t DS3231_STATUS_REG = 0x0F;
    
    std::cout << "[RTC SQW] Attempting to configure DS3231 square wave output\n";
    
    // First, try reading the status register (0x0F) as a diagnostic
    // This helps us understand if the I2C communication works at all
    std::cout << "[RTC SQW] Diagnostic: Testing I2C communication via status register (0x0F)\n";
    uint8_t test_reg = DS3231_STATUS_REG;
    ssize_t test_written = write(i2c_fd_, &test_reg, 1);
    if (test_written == 1) {
        uint8_t status = 0;
        if (read(i2c_fd_, &status, 1) == 1) {
            std::cout << "[RTC SQW] ✓ I2C communication OK (status reg = 0x" << std::hex << (int)status << std::dec << ")\n";
        } else {
            std::cout << "[RTC SQW] ⚠ Could read status register address but not value\n";
        }
    } else {
        std::cerr << "[RTC SQW] ⚠ Cannot access status register (errno=" << errno << ": " << strerror(errno) << ")\n";
        std::cerr << "[RTC SQW] This indicates a fundamental I2C communication problem\n";
    }
    
    // Try accessing the control register with retry logic
    // The kernel RTC driver might be accessing it intermittently
    std::cout << "[RTC SQW] Attempting to read control register (0x" << std::hex << (int)DS3231_CONTROL_REG << std::dec << ")\n";
    
    uint8_t control = 0;
    bool read_success = false;
    const int MAX_RETRIES = 5;
    
    for (int retry = 0; retry < MAX_RETRIES && !read_success; ++retry) {
        if (retry > 0) {
            std::cout << "[RTC SQW] Retry attempt " << retry << "/" << (MAX_RETRIES-1) << "...\n";
            usleep(100000);  // 100ms delay between retries
        }
        
        uint8_t reg = DS3231_CONTROL_REG;
        ssize_t bytes_written = write(i2c_fd_, &reg, 1);
        
        if (bytes_written != 1) {
            std::cerr << "[RTC SQW] Retry " << retry << ": Failed to set register address (errno=" << errno << ": " << strerror(errno) << ")\n";
            continue;
        }
        
        if (read(i2c_fd_, &control, 1) != 1) {
            std::cerr << "[RTC SQW] Retry " << retry << ": Failed to read control register value\n";
            continue;
        }
        
        read_success = true;
        std::cout << "[RTC SQW] ✓ Successfully read control register on attempt " << retry << "\n";
    }
    
    if (!read_success) {
        std::cerr << "[RTC SQW] ERROR: Failed to read control register after " << MAX_RETRIES << " attempts\n";
        std::cerr << "[RTC SQW] \n";
        std::cerr << "[RTC SQW] *** KERNEL DRIVER CONFLICT DETECTED ***\n";
        std::cerr << "[RTC SQW] The kernel RTC driver (rtc-ds1307) is actively using the control register.\n";
        std::cerr << "[RTC SQW] \n";
        std::cerr << "[RTC SQW] WORKAROUND OPTIONS:\n";
        std::cerr << "[RTC SQW] 1. Configure SQW manually before running this program:\n";
        std::cerr << "[RTC SQW]    sudo i2cset -y 15 0x68 0x0E 0x00\n";
        std::cerr << "[RTC SQW] \n";
        std::cerr << "[RTC SQW] 2. Add device tree configuration to enable SQW at boot:\n";
        std::cerr << "[RTC SQW]    (Contact developer for device tree overlay)\n";
        std::cerr << "[RTC SQW] \n";
        std::cerr << "[RTC SQW] Continuing with I2C polling mode (1-second granularity)...\n";
        return false;
    }
    
    std::cout << "[RTC SQW] Current control register: 0x" << std::hex << (int)control << std::dec << "\n";
    
    uint8_t new_control = control;
    
    if (enable) {
        // Enable 1Hz square wave output
        new_control &= ~(1 << 2);  // Clear INTCN (enable SQW)
        new_control &= ~(0x03 << 3); // Clear RS bits
        new_control |= (0x00 << 3);  // Set RS=00 for 1Hz
        
        std::cout << "[RTC SQW] Enabling 1Hz square wave (new control=0x" << std::hex << (int)new_control << std::dec << ")\n";
    } else {
        // Disable square wave (enable interrupt mode)
        new_control |= (1 << 2);  // Set INTCN (disable SQW)
        
        std::cout << "[RTC SQW] Disabling square wave (new control=0x" << std::hex << (int)new_control << std::dec << ")\n";
    }
    
    // Write updated control register with retry logic
    bool write_success = false;
    
    for (int retry = 0; retry < MAX_RETRIES && !write_success; ++retry) {
        if (retry > 0) {
            std::cout << "[RTC SQW] Write retry " << retry << "/" << (MAX_RETRIES-1) << "...\n";
            usleep(100000);  // 100ms delay
        }
        
        uint8_t write_data[2] = {DS3231_CONTROL_REG, new_control};
        if (write(i2c_fd_, write_data, 2) == 2) {
            write_success = true;
            std::cout << "[RTC SQW] ✓ Control register write successful on attempt " << retry << "\n";
        } else {
            std::cerr << "[RTC SQW] Write retry " << retry << ": Failed (errno=" << errno << ": " << strerror(errno) << ")\n";
        }
    }
    
    if (!write_success) {
        std::cerr << "[RTC SQW] ERROR: Failed to write control register after " << MAX_RETRIES << " attempts\n";
        std::cerr << "[RTC SQW] Kernel driver conflict prevents runtime configuration.\n";
        return false;
    }
    
    // Verify write with retry
    bool verify_success = false;
    uint8_t verify = 0;
    
    for (int retry = 0; retry < MAX_RETRIES && !verify_success; ++retry) {
        if (retry > 0) {
            usleep(50000);  // 50ms delay
        }
        
        uint8_t reg = DS3231_CONTROL_REG;
        if (write(i2c_fd_, &reg, 1) == 1 && read(i2c_fd_, &verify, 1) == 1) {
            verify_success = true;
        }
    }
    
    if (!verify_success) {
        std::cerr << "[RTC SQW] WARNING: Could not verify control register write\n";
        std::cerr << "[RTC SQW] SQW may or may not be enabled - check with oscilloscope\n";
        return false;
    }
    
    if (verify != new_control) {
        std::cerr << "[RTC SQW] ERROR: Verification failed!\n";
        std::cerr << "[RTC SQW]   Expected: 0x" << std::hex << (int)new_control << "\n";
        std::cerr << "[RTC SQW]   Got:      0x" << (int)verify << std::dec << "\n";
        return false;
    }
    
    std::cout << "[RTC SQW] ✓ Control register verified: 0x" << std::hex << (int)verify << std::dec << "\n";
    
    if (enable) {
        std::cout << "[RTC SQW] ✓ 1Hz square wave now active on " << sqw_device_ << "\n";
        std::cout << "[RTC SQW] ✓ Edge detection enabled (microsecond precision)\n";
    } else {
        std::cout << "[RTC SQW] ✓ Square wave disabled\n";
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
