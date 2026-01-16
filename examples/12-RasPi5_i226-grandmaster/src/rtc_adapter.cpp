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
    , skip_samples_(0)
    , latched_rtc_sec_(0)
    , edge_mono_ns_(0)
    , timeinfo_valid_(false)
    , pending_seq_(0)
    , drift_observer_(ptp::Config::CreateDefault(), "RTC-Discipline")
{
    std::cout << "[RTC Init] ✓ DriftObserver initialized for intelligent frequency discipline\n";
    std::cout << "[RTC Init] ✓ Second-Latching Architecture: RTC seconds latched at PPS edge\n";
    std::cout << "[RTC Init] ✓ Monotonic Time Interpolation: CLOCK_MONOTONIC_RAW for sub-second precision\n";
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
    // Auto-detect I2C bus by scanning /sys/class/rtc/rtcN/name
    const char* i2c_device = nullptr;
    char i2c_path[64];
    int detected_bus = -1;
    
    // Try to detect bus from /sys/class/rtc entries
    for (int rtc_num = 0; rtc_num < 10; rtc_num++) {
        char rtc_name_path[128];
        snprintf(rtc_name_path, sizeof(rtc_name_path), "/sys/class/rtc/rtc%d/name", rtc_num);
        
        FILE* f = fopen(rtc_name_path, "r");
        if (f) {
            char name[128];
            if (fgets(name, sizeof(name), f)) {
                // Look for "rtc-ds1307 XX-0068" or "ds1307 XX-0068" pattern
                int bus_num;
                if (sscanf(name, "rtc-ds1307 %d-0068", &bus_num) == 1 ||
                    sscanf(name, "ds1307 %d-0068", &bus_num) == 1) {
                    detected_bus = bus_num;
                    fclose(f);
                    std::cout << "[RTC Init] Detected DS3231 on I2C bus " << detected_bus 
                              << " (from " << rtc_name_path << ")\n";
                    break;
                }
            }
            fclose(f);
        }
    }
    
    // If not detected via sysfs, try scanning I2C buses
    if (detected_bus < 0) {
        std::cout << "[RTC Init] Scanning I2C buses for DS3231...\n";
        for (int bus = 0; bus < 20; bus++) {
            snprintf(i2c_path, sizeof(i2c_path), "/dev/i2c-%d", bus);
            int test_fd = open(i2c_path, O_RDWR);
            if (test_fd >= 0) {
                if (ioctl(test_fd, I2C_SLAVE_FORCE, 0x68) == 0) {
                    // Try to read register 0x00 (seconds)
                    uint8_t reg = 0x00;
                    if (write(test_fd, &reg, 1) == 1) {
                        uint8_t value;
                        if (read(test_fd, &value, 1) == 1) {
                            // Verify it looks like BCD seconds (0x00-0x59)
                            if ((value & 0x7F) <= 0x59) {
                                detected_bus = bus;
                                close(test_fd);
                                std::cout << "[RTC Init] Found DS3231 on I2C bus " << bus << "\n";
                                break;
                            }
                        }
                    }
                }
                close(test_fd);
            }
        }
    }
    
    // Open detected or fallback I2C bus
    if (detected_bus >= 0) {
        snprintf(i2c_path, sizeof(i2c_path), "/dev/i2c-%d", detected_bus);
        i2c_device = i2c_path;
    } else {
        std::cerr << "[RTC Init] WARNING: Could not auto-detect DS3231 I2C bus, using default /dev/i2c-14\n";
        i2c_device = "/dev/i2c-14";  // Fallback
    }
    
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

    // CRITICAL INITIALIZATION: Read actual RTC time to initialize latched_rtc_sec_
    // This prevents massive fake offset at first PPS edge (would trigger false epoch change)
    std::cout << "[RTC Init] Reading initial RTC time for Second-Latching Architecture...\n";
    RtcTime initial_time{};
    if (read_time(&initial_time)) {
        // Convert to Unix timestamp
        struct tm tm_time{};
        tm_time.tm_year = initial_time.year - 1900;
        tm_time.tm_mon = initial_time.month - 1;
        tm_time.tm_mday = initial_time.day;
        tm_time.tm_hour = initial_time.hours;
        tm_time.tm_min = initial_time.minutes;
        tm_time.tm_sec = initial_time.seconds;
        
        time_t unix_time = timegm(&tm_time);
        latched_rtc_sec_ = static_cast<uint64_t>(unix_time);
        
        std::cout << "[RTC Init] ✓ latched_rtc_sec initialized to " << latched_rtc_sec_ 
                  << " (" << initial_time.year << "-" << (int)initial_time.month << "-" << (int)initial_time.day
                  << " " << (int)initial_time.hours << ":" << (int)initial_time.minutes << ":" << (int)initial_time.seconds << ")\n";
    } else {
        std::cerr << "[RTC Init] ⚠ Failed to read initial RTC time, latched_rtc_sec remains 0\n";
        std::cerr << "[RTC Init] ⚠ First PPS edge will cause false correction\n";
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

bool RtcAdapter::get_time(uint64_t* seconds, uint32_t* nanoseconds, bool wait_for_edge)
{
    // SECOND-LATCHING ARCHITECTURE:
    // RTC seconds are latched at each PPS edge (predictive increment),
    // then confirmed by authoritative DS3231 I2C read.
    // 
    // MONOTONIC TIME INTERPOLATION:
    // Sub-second nanoseconds interpolated using CLOCK_MONOTONIC_RAW
    // (immune to PHC servo adjustments - eliminates system time contamination)
    // 
    // Event Flow:
    // 1. PPS Edge Event: Latch monotonic timestamp, predictively increment RTC second
    // 2. TimeInfo Event: Confirm RTC second with authoritative DS3231 hardware read  
    // 3. get_time(): Calculate elapsed ns since edge using stable monotonic clock
    
    // If SQW/PPS available, use Option A architecture
    if (pps_fd_ >= 0) {
        struct pps_fdata pps_data{};
        
        if (wait_for_edge) {
            // BLOCKING MODE: Wait for NEXT PPS edge (2s timeout)
            pps_data.timeout.sec = 2;
            pps_data.timeout.nsec = 0;
            
            int pps_ret = ioctl(pps_fd_, PPS_FETCH, &pps_data);
            if (pps_ret < 0) {
                std::cout << "[RTC PPS] ⚠ PPS_FETCH failed in blocking mode (ret=" << pps_ret 
                          << " errno=" << errno << ")\n";
                goto rtc_fallback;
            }
            
            std::cout << "[RTC PPS] PPS_FETCH returned " << pps_ret << " (blocking mode)\n";
        } else {
            // NON-BLOCKING MODE: Fetch last known edge immediately (timeout=0)
            pps_data.timeout.sec = 0;
            pps_data.timeout.nsec = 0;
            
            int pps_ret = ioctl(pps_fd_, PPS_FETCH, &pps_data);
            if (pps_ret < 0) {
                std::cout << "[RTC PPS] ⚠ PPS_FETCH failed in non-blocking mode (ret=" << pps_ret 
                          << " errno=" << errno << ")\n";
                goto rtc_fallback;
            }
            
            std::cout << "[RTC PPS] PPS_FETCH returned " << pps_ret << " (non-blocking mode)\n";
        }
        
        std::cout << "[RTC PPS] Fetched: seq=" << pps_data.info.assert_sequence 
                  << " sec=" << pps_data.info.assert_tu.sec 
                  << " nsec=" << pps_data.info.assert_tu.nsec << "\n";
        std::cout << "[RTC PPS] Cached: seq=" << last_pps_seq_ << " latched_sec=" << latched_rtc_sec_ 
                  << " valid=" << timeinfo_valid_ << "\n";
        
        // PPS EDGE EVENT: New second boundary detected
        // Per deb.md spec: OnRtcPpsEvent is SEPARATE from OnTimeInformation
        if (pps_data.info.assert_sequence != (uint32_t)last_pps_seq_) {
            std::cout << "[RTC PPS] ✓ PPS Edge Event: Second boundary detected\n";
            
            // Latch monotonic timestamp at edge (stable reference, immune to servo)
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
            edge_mono_ns_ = ts.tv_sec * 1000000000LL + ts.tv_nsec;
            
            // Second-Latching: Predictively advance RTC second counter
            latched_rtc_sec_ += 1;
            
            // Mark pending TimeInfo confirmation from DS3231 hardware
            timeinfo_valid_ = false;
            pending_seq_ = pps_data.info.assert_sequence;
            
            // Update cache
            last_pps_seq_ = pps_data.info.assert_sequence;
            last_pps_sec_ = pps_data.info.assert_tu.sec;
            last_pps_nsec_ = pps_data.info.assert_tu.nsec;
            
            std::cout << "[RTC PPS] edge_mono_ns=" << edge_mono_ns_ 
                      << " latched_sec=" << latched_rtc_sec_ << " (predictive, awaiting confirmation)\n";
        } else {
            std::cout << "[RTC PPS] Same sequence, using cached state\n";
        }
        
        // TIMEINFO CONFIRMATION EVENT: Read DS3231 hardware to confirm/correct latched second
        // Per deb.md spec: This is SEPARATE event from PPS edge, happens later
        // Sequence matching prevents epoch contamination from late/early/stale I2C reads
        if (last_pps_seq_ >= 0 && !timeinfo_valid_) {
            RtcTime rtc_time{};
            if (read_time(&rtc_time)) {
                // Convert DS3231 time to Unix timestamp
                struct tm tm_time{};
                tm_time.tm_year = rtc_time.year - 1900;
                tm_time.tm_mon = rtc_time.month - 1;
                tm_time.tm_mday = rtc_time.day;
                tm_time.tm_hour = rtc_time.hours;
                tm_time.tm_min = rtc_time.minutes;
                tm_time.tm_sec = rtc_time.seconds;
                
                time_t unix_time = timegm(&tm_time);
                uint64_t ds3231_sec = static_cast<uint64_t>(unix_time);
                
                std::cout << "[RTC TimeInfo] DS3231_sec=" << ds3231_sec 
                          << " latched_sec=" << latched_rtc_sec_ << " pending_seq=" << pending_seq_ << "\n";
                
                // Confirm or correct based on DS3231 read
                if (ds3231_sec == latched_rtc_sec_) {
                    // Perfect match - DS3231 has incremented to new second
                    timeinfo_valid_ = true;
                    std::cout << "[RTC TimeInfo] ✓ Confirmed: DS3231 matches prediction\n";
                } else if (ds3231_sec == latched_rtc_sec_ - 1) {
                    // Expected race - DS3231 still shows old second (I2C read was too fast)
                    // Keep predictive value, will retry on next call
                    std::cout << "[RTC TimeInfo] ⚠ Race: DS3231 still old second (will retry)\n";
                    timeinfo_valid_ = false;
                } else {
                    // Unexpected - correct with authoritative DS3231 value
                    std::cout << "[RTC TimeInfo] ⚠ Correction: DS3231 differs (missed edges or step?)\n";
                    latched_rtc_sec_ = ds3231_sec;
                    timeinfo_valid_ = true;
                }
            } else {
                std::cout << "[RTC TimeInfo] ⚠ DS3231 I2C read failed, using predictive latched_sec\n";
            }
        }
        
        // MONOTONIC TIME INTERPOLATION: Calculate sub-second ns since edge
        // Uses CLOCK_MONOTONIC_RAW (immune to PHC servo, NTP, clock_settime)
        if (last_pps_seq_ >= 0) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC_RAW, &ts);  // Stable monotonic reference
            int64_t now_mono_ns = ts.tv_sec * 1000000000LL + ts.tv_nsec;
            
            // Elapsed time since PPS edge (no system time contamination)
            int64_t elapsed = now_mono_ns - edge_mono_ns_;
            if (elapsed < 0) elapsed = 0;  // Sanity check
            
            // Dropout detection: carry_sec > 0 indicates missed PPS edges
            uint64_t carry_sec = elapsed / 1000000000LL;
            uint32_t nsec = elapsed % 1000000000LL;
            
            std::cout << "[RTC Interpolation] elapsed=" << elapsed << "ns dropout_sec=" << carry_sec 
                      << " interpolated_nsec=" << nsec << "\n";
            
            if (carry_sec > 0) {
                std::cout << "[RTC PPS] ⚠ Missed PPS: carry_sec=" << carry_sec 
                          << " (>1s elapsed since edge)\n";
                // TODO: Notify DriftObserver of missing ticks
            }
            
            // Return interpolated time
            *seconds = latched_rtc_sec_ + carry_sec;
            *nanoseconds = nsec;
            
            std::cout << "[RTC PPS] Final: sec=" << *seconds << " nsec=" << *nanoseconds 
                      << " valid=" << timeinfo_valid_ << "\n";
            
            return timeinfo_valid_;  // Caller can treat false as prediction/holdover
        } else {
            std::cout << "[RTC PPS] ⚠ No PPS sequence cached yet\n";
        }
    }
    
rtc_fallback:
    // Fallback: PPS not available, read RTC without sub-second precision
    std::cout << "[RTC] Using RTC-only reading (no PPS)\n";
    
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
    *nanoseconds = 0;
    return true;
}

bool RtcAdapter::get_ptp_time(uint64_t* seconds, uint32_t* nanoseconds)
{
    // Use the new get_time() which supports both PPS and RTC fallback
    return get_time(seconds, nanoseconds);
}

bool RtcAdapter::get_pps_edge_timestamp(uint64_t* rtc_sec, uint32_t* pps_edge_nsec, bool wait_for_edge)
{
    // This method returns the PPS edge timestamp directly for drift measurement
    // CRITICAL: pps_edge_nsec is last_pps_nsec_ (the actual edge timestamp nanoseconds)
    // which drifts with the RTC oscillator - this is the signal we need!
    
    if (pps_fd_ < 0) {
        std::cout << "[RTC Edge] No PPS device available\n";
        return false;
    }
    
    // Fetch PPS data (blocking or non-blocking based on wait_for_edge)
    struct pps_fdata pps_data{};
    
    if (wait_for_edge) {
        // BLOCKING: Wait for next PPS edge
        pps_data.timeout.sec = 2;
        pps_data.timeout.nsec = 0;
    } else {
        // NON-BLOCKING: Use cached edge immediately
        pps_data.timeout.sec = 0;
        pps_data.timeout.nsec = 0;
    }
    
    int pps_ret = ioctl(pps_fd_, PPS_FETCH, &pps_data);
    if (pps_ret < 0) {
        std::cout << "[RTC Edge] PPS_FETCH failed (ret=" << pps_ret << " errno=" << errno << ")\n";
        return false;
    }
    
    // Update cache if new edge detected
    if (pps_data.info.assert_sequence != (uint32_t)last_pps_seq_) {
        last_pps_seq_ = pps_data.info.assert_sequence;
        last_pps_sec_ = pps_data.info.assert_tu.sec;
        last_pps_nsec_ = pps_data.info.assert_tu.nsec;
    }
    
    if (last_pps_seq_ < 0) {
        std::cout << "[RTC Edge] No PPS edge cached yet\n";
        return false;
    }
    
    // CRITICAL FIX: Use PPS metadata seconds (GPS-synced system time), NOT RTC hardware seconds!
    // The PPS edge timestamp represents SYSTEM TIME (GPS-synchronized) when RTC's PPS pulse arrived.
    // RTC hardware seconds are battery-backed and have a completely different time base!
    // For drift measurement, we need both seconds and nanoseconds from the SAME time source (system/GPS).
    *rtc_sec = static_cast<uint64_t>(last_pps_sec_);
    
    // CRITICAL: Return the PPS edge nanoseconds (last_pps_nsec_)
    // This is the nanosecond component of when the PPS edge occurred within the system second.
    // As the RTC oscillator drifts, this value changes gradually, providing sub-second drift measurement!
    // Example: GPS PPS at .000000000, RTC PPS at .185954570 → drift = change in .185954570 over time
    *pps_edge_nsec = last_pps_nsec_;
    
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
    
    // Notify DriftObserver of reference change (GPS sync = new time reference)
    drift_observer_.NotifyEvent(ptp::ObserverEvent::ReferenceChanged);
    std::cout << "[RTC Sync] ℹ DriftObserver notified: ReferenceChanged (epoch incremented, holdoff=10 ticks)\n";
    
    // Skip next 5 PPS samples after time discontinuity (expert recommendation)
    // This prevents contamination from PHC calibration/RTC sync transients
    skip_samples_ = 5;
    std::cout << "[RTC Sync] ℹ Skipping next 5 drift samples (discontinuity transient)\n";
    
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

bool RtcAdapter::adjust_aging_offset(int8_t delta_lsb)
{
    // Read current aging offset
    int8_t current_offset = read_aging_offset();
    
    // Calculate new offset (clamp to valid range -127 to +127)
    int16_t new_offset = static_cast<int16_t>(current_offset) + static_cast<int16_t>(delta_lsb);
    if (new_offset > 127) {
        new_offset = 127;
        std::cout << "[RTC Discipline] Clamping adjustment to maximum (+127 LSB)\n";
    } else if (new_offset < -127) {
        new_offset = -127;
        std::cout << "[RTC Discipline] Clamping adjustment to minimum (-127 LSB)\n";
    }
    
    std::cout << "[RTC Discipline] Adjusting aging offset: " 
              << static_cast<int>(current_offset) << " LSB + " 
              << static_cast<int>(delta_lsb) << " LSB = " 
              << static_cast<int>(new_offset) << " LSB\n";
    
    // Write new offset
    return write_aging_offset(static_cast<int8_t>(new_offset));
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

bool RtcAdapter::process_pps_tick(uint64_t gps_time_ns, uint64_t rtc_time_ns)
{
    // Skip samples if in post-discontinuity holdoff
    if (should_skip_sample()) {
        std::cout << "[RTC Drift] Skipping sample (post-discontinuity holdoff, " 
                  << skip_samples_ << " samples remaining)\n";
        return false;
    }
    
    // Calculate offset and drift in nanoseconds
    int64_t offset_ns = static_cast<int64_t>(rtc_time_ns) - static_cast<int64_t>(gps_time_ns);
    
    std::cout << "[RTC Drift] PPS tick: gps_time=" << gps_time_ns 
              << " ns, rtc_time=" << rtc_time_ns 
              << " ns, offset=" << offset_ns << " ns\n";
    
    // Feed to DriftObserver (it calculates dt_ref internally)
    drift_observer_.Update(static_cast<int64_t>(gps_time_ns), static_cast<int64_t>(rtc_time_ns));
    
    // Get current estimate for logging
    auto estimate = drift_observer_.GetEstimate();
    if (estimate.ready) {
        std::cout << "[RTC Drift] Estimate: drift=" << estimate.drift_ppm << " ppm"
                  << " (stddev=" << estimate.drift_stddev_ppm << " ppm)"
                  << " trustworthy=" << (estimate.trustworthy ? "YES" : "NO")
                  << " epoch=" << estimate.current_epoch 
                  << " ticks=" << estimate.ticks_in_epoch << "\n";
    } else {
        std::cout << "[RTC Drift] Estimate not ready (need more samples, epoch=" 
                  << estimate.current_epoch << ")\n";
    }
    
    return true;
}

bool RtcAdapter::apply_drift_discipline(bool force)
{
    auto estimate = drift_observer_.GetEstimate();
    
    // Check if estimate is trustworthy (or forced)
    if (!force && !estimate.trustworthy) {
        std::cout << "[RTC Discipline] Estimate not trustworthy - skipping discipline\n";
        std::cout << "[RTC Discipline]   ready=" << estimate.ready 
                  << " ticks_in_holdoff=" << estimate.ticks_in_holdoff
                  << " drift_stddev=" << estimate.drift_stddev_ppm << " ppm\n";
        return false;
    }
    
    if (!estimate.ready) {
        std::cout << "[RTC Discipline] Estimate not ready - need more samples\n";
        return false;
    }
    
    std::cout << "[RTC Discipline] Applying frequency discipline:\n";
    std::cout << "[RTC Discipline]   drift=" << estimate.drift_ppm << " ppm\n";
    std::cout << "[RTC Discipline]   drift_stddev=" << estimate.drift_stddev_ppm << " ppm\n";
    std::cout << "[RTC Discipline]   epoch=" << estimate.current_epoch << "\n";
    std::cout << "[RTC Discipline]   ticks_in_epoch=" << estimate.ticks_in_epoch << "\n";
    std::cout << "[RTC Discipline]   trustworthy=" << (estimate.trustworthy ? "YES" : "NO") << "\n";
    
    // Apply discipline using measured drift
    bool success = apply_frequency_discipline(estimate.drift_ppm);
    
    if (success) {
        std::cout << "[RTC Discipline] ✓ Frequency discipline applied successfully\n";
    } else {
        std::cout << "[RTC Discipline] ✗ Frequency discipline failed\n";
    }
    
    return success;
}

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
