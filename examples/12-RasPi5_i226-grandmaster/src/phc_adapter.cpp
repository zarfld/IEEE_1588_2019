/**
 * @file phc_adapter.cpp
 * @brief Implementation of PHC hardware abstraction layer
 * 
 * Implements PhcAdapter using Linux PTP subsystem (clock_gettime, clock_settime,
 * clock_adjtime). Provides clean interface for protocol logic to access i226 PHC.
 * 
 * @see phc_adapter.hpp for interface documentation
 */

#include "phc_adapter.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/ptp_clock.h>
#include <time.h>
#include <sys/timex.h>

// PTP ioctl constants (if not defined in kernel headers)
#ifndef PTP_CLOCK_SETTIME
#define PTP_CLOCK_SETTIME  _IOW(PTP_CLK_MAGIC, 5, struct ptp_clock_time)
#endif
#ifndef PTP_CLOCK_GETTIME
#define PTP_CLOCK_GETTIME  _IOR(PTP_CLK_MAGIC, 6, struct ptp_clock_time)
#endif

PhcAdapter::PhcAdapter()
    : initialized_(false)
    , interface_name_()
    , device_path_()
    , phc_fd_(-1)
{
}

PhcAdapter::~PhcAdapter()
{
    close_phc_device();
}

bool PhcAdapter::initialize(const char* interface_name)
{
    if (initialized_) {
        std::cerr << "[PhcAdapter] Already initialized" << std::endl;
        return false;
    }
    
    if (!interface_name || strlen(interface_name) == 0) {
        std::cerr << "[PhcAdapter] Invalid interface name" << std::endl;
        return false;
    }
    
    interface_name_ = interface_name;
    
    // Discover PTP device for this interface
    if (!discover_phc_device()) {
        std::cerr << "[PhcAdapter] Failed to discover PHC device for " << interface_name << std::endl;
        return false;
    }
    
    // Open PHC device
    if (!open_phc_device()) {
        std::cerr << "[PhcAdapter] Failed to open PHC device " << device_path_ << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "[PhcAdapter] Initialized: " << interface_name_ << " → " << device_path_ << std::endl;
    return true;
}

bool PhcAdapter::is_initialized() const
{
    return initialized_;
}

bool PhcAdapter::get_time(uint64_t* sec, uint32_t* nsec)
{
    if (!initialized_ || phc_fd_ < 0) {
        std::cerr << "[PhcAdapter] Not initialized" << std::endl;
        return false;
    }
    
    if (!sec || !nsec) {
        std::cerr << "[PhcAdapter] Invalid output pointers" << std::endl;
        return false;
    }
    
    // LAYER 16 FIX: Read from PHC clockid, NOT CLOCK_REALTIME!
    // BUG: Was reading system clock instead of i226 PHC
    clockid_t clkid = ((~(clockid_t)phc_fd_) << 3) | 3;
    
    struct timespec ts;
    if (clock_gettime(clkid, &ts) != 0) {
        std::cerr << "[PhcAdapter] clock_gettime(PHC clkid=" << clkid << ") failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    *sec = static_cast<uint64_t>(ts.tv_sec);
    *nsec = static_cast<uint32_t>(ts.tv_nsec);
    
    return true;
}

bool PhcAdapter::set_time(uint64_t sec, uint32_t nsec)
{
    if (!initialized_ || phc_fd_ < 0) {
        std::cerr << "[PhcAdapter] Not initialized" << std::endl;
        return false;
    }
    
    if (nsec >= 1000000000) {
        std::cerr << "[PhcAdapter] Invalid nanoseconds: " << nsec << std::endl;
        return false;
    }
    
    // LAYER 13 FIX: Intel i226 doesn't support clock_settime() OR PTP_CLOCK_SETTIME ioctl!
    // Both return success/fail but don't actually change PHC time.
    // Use clock_adjtime with ADJ_SETOFFSET mode to set time by offset adjustment.
    
    // First get current PHC time
    clockid_t clkid = ((~(clockid_t)phc_fd_) << 3) | 3;
    struct timespec ts_current;
    if (clock_gettime(clkid, &ts_current) != 0) {
        std::cerr << "[PhcAdapter] clock_gettime(PHC clkid=" << clkid << ") failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "[PhcAdapter DEBUG] set_time() called:\n"
              << "  PHC device: " << device_path_ << " (fd=" << phc_fd_ << ", clkid=" << clkid << ")\n"
              << "  Target time: " << sec << "." << std::setfill('0') << std::setw(9) << nsec << " (GPS_UTC)\n"
              << "  Current PHC: " << ts_current.tv_sec << "." << std::setfill('0') << std::setw(9) << ts_current.tv_nsec << "\n";
    
    // Calculate offset delta (target - current)
    int64_t delta_sec = static_cast<int64_t>(sec) - ts_current.tv_sec;
    int64_t delta_nsec = static_cast<int64_t>(nsec) - ts_current.tv_nsec;
    
    std::cout << "  Raw delta: " << delta_sec << "." << std::setfill('0') << std::setw(9) 
              << (delta_nsec < 0 ? -delta_nsec : delta_nsec) << (delta_nsec < 0 ? " (negative nsec)\n" : "\n");
    
    // Normalize if nsec underflow/overflow
    if (delta_nsec < 0) {
        delta_sec -= 1;
        delta_nsec += 1000000000;
    } else if (delta_nsec >= 1000000000) {
        delta_sec += 1;
        delta_nsec -= 1000000000;
    }
    
    std::cout << "[PhcAdapter] Stepping PHC (clock_adjtime ADJ_SETOFFSET) delta=" 
              << delta_sec << "." << std::setfill('0') << std::setw(9) << delta_nsec << "s\n";
    
    // Apply offset using clock_adjtime ADJ_SETOFFSET
    struct timex tx;
    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_SETOFFSET | ADJ_NANO;
    tx.time.tv_sec = delta_sec;
    tx.time.tv_usec = delta_nsec;  // Actually nanoseconds when ADJ_NANO set
    
    if (clock_adjtime(clkid, &tx) != 0) {
        std::cerr << "[PhcAdapter] clock_adjtime ADJ_SETOFFSET failed: " 
                  << strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "[PhcAdapter] Step correction: " << sec << "." << nsec << "s" << std::endl;
    
    // Verify step worked
    struct timespec ts_verify;
    if (clock_gettime(clkid, &ts_verify) == 0) {
        std::cout << "[PhcAdapter] Verified: PHC now at " << ts_verify.tv_sec 
                  << "." << std::setfill('0') << std::setw(9) << ts_verify.tv_nsec << std::endl;
    }
    
    return true;
}

bool PhcAdapter::adjust_frequency(int32_t freq_ppb)
{
    if (!initialized_ || phc_fd_ < 0) {
        std::cerr << "[PhcAdapter] Not initialized" << std::endl;
        return false;
    }
    
    // Clamp to hardware limits
    if (freq_ppb > MAX_FREQUENCY_PPB) {
        std::cerr << "[PhcAdapter] ⚠ Clamping frequency " << freq_ppb << " → " << MAX_FREQUENCY_PPB << " ppb" << std::endl;
        freq_ppb = MAX_FREQUENCY_PPB;
    } else if (freq_ppb < -MAX_FREQUENCY_PPB) {
        std::cerr << "[PhcAdapter] ⚠ Clamping frequency " << freq_ppb << " → " << -MAX_FREQUENCY_PPB << " ppb" << std::endl;
        freq_ppb = -MAX_FREQUENCY_PPB;
    }
    
    // CRITICAL (Layer 8 fix): Convert PHC fd to clockid_t for clock_adjtime()
    // Modern Linux kernels expose PHC as POSIX clocks via special clock IDs.
    // Formula: clockid = ((~(clockid_t)(fd) << 3) | 3)
    // This converts the file descriptor into a valid POSIX clock ID.
    clockid_t clkid = ((~(clockid_t)phc_fd_) << 3) | 3;
    
    // Convert ppb to Linux timex units (1 ppb = 2^16 / 1e9 = 0.065536)
    struct timex tx;
    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_FREQUENCY;
    tx.freq = static_cast<long>(freq_ppb) * 65536L / 1000L;
    
    if (clock_adjtime(clkid, &tx) < 0) {
        std::cerr << "[PhcAdapter] clock_adjtime(clockid=" << clkid 
                  << ", freq=" << freq_ppb << " ppb) failed: " 
                  << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

int32_t PhcAdapter::get_max_frequency_ppb() const
{
    return MAX_FREQUENCY_PPB;
}

const char* PhcAdapter::get_interface_name() const
{
    return interface_name_.c_str();
}

const char* PhcAdapter::get_device_path() const
{
    return device_path_.c_str();
}

bool PhcAdapter::discover_phc_device()
{
    // Use sysfs to find PTP device for this interface
    // Path: /sys/class/net/{interface}/device/ptp/ptp*
    std::string sysfs_path = "/sys/class/net/" + interface_name_ + "/device/ptp";
    
    // List directory to find ptp* entry
    DIR* dir = opendir(sysfs_path.c_str());
    if (!dir) {
        std::cerr << "[PhcAdapter] opendir failed for " << sysfs_path << ": " << strerror(errno) << std::endl;
        return false;
    }
    
    struct dirent* entry;
    std::string ptp_name;
    
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name.find("ptp") == 0) {  // Starts with "ptp"
            ptp_name = name;
            break;
        }
    }
    
    closedir(dir);
    
    if (ptp_name.empty()) {
        std::cerr << "[PhcAdapter] No PTP device found in " << sysfs_path << std::endl;
        return false;
    }
    
    device_path_ = "/dev/" + ptp_name;
    
    std::cout << "[PhcAdapter] Discovered PHC: " << interface_name_ << " → " << device_path_ << std::endl;
    return true;
}

bool PhcAdapter::open_phc_device()
{
    phc_fd_ = open(device_path_.c_str(), O_RDWR);
    if (phc_fd_ < 0) {
        std::cerr << "[PhcAdapter] open failed for " << device_path_ << ": " << strerror(errno) << std::endl;
        return false;
    }
    
    // Query PHC capabilities (optional, for diagnostics)
    struct ptp_clock_caps caps;
    if (ioctl(phc_fd_, PTP_CLOCK_GETCAPS, &caps) == 0) {
        std::cout << "[PhcAdapter] PHC Capabilities:" << std::endl;
        std::cout << "  max_adj: " << caps.max_adj << " ppb" << std::endl;
        std::cout << "  n_alarm: " << caps.n_alarm << std::endl;
        std::cout << "  n_ext_ts: " << caps.n_ext_ts << std::endl;
        std::cout << "  n_per_out: " << caps.n_per_out << std::endl;
        std::cout << "  pps: " << caps.pps << std::endl;
    }
    
    return true;
}

void PhcAdapter::close_phc_device()
{
    if (phc_fd_ >= 0) {
        close(phc_fd_);
        phc_fd_ = -1;
    }
}
