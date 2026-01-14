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
    
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        std::cerr << "[PhcAdapter] clock_gettime failed: " << strerror(errno) << std::endl;
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
    
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(sec);
    ts.tv_nsec = static_cast<long>(nsec);
    
    if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
        std::cerr << "[PhcAdapter] clock_settime failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "[PhcAdapter] Step correction: " << sec << "." << nsec << "s" << std::endl;
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
    
    // Convert ppb to Linux timex units (1 ppb = 2^16 / 1e9 = 0.065536)
    // freq (timex units) = freq_ppb * 65536 / 1000
    struct timex tx;
    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_FREQUENCY;
    tx.freq = static_cast<long>(freq_ppb) * 65536L / 1000L;
    
    // CRITICAL (Layer 7 fix): Use PHC file descriptor, NOT CLOCK_REALTIME!
    // CLOCK_REALTIME adjusts system clock, we need to adjust the hardware PHC
    if (clock_adjtime(phc_fd_, &tx) < 0) {
        std::cerr << "[PhcAdapter] clock_adjtime(phc_fd=" << phc_fd_ << ") failed: " << strerror(errno) << std::endl;
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
