/**
 * @file intel_avb_hal.cpp
 * @brief Implementation of Intel AVB Hardware Abstraction Layer
 * 
 * @copyright IEEE 1588-2019 compliant implementation
 * @version 1.0.0
 * @date 2025-12-05
 */

#include "intel_avb_hal.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

namespace Examples {
namespace IntelAVB {

//============================================================================
// IntelAVBHAL Implementation
//============================================================================

IntelAVBHAL::IntelAVBHAL() 
    : device_handle_(INVALID_HANDLE_VALUE)
    , last_error_(ERROR_SUCCESS)
    , last_status_(0) {
}

IntelAVBHAL::~IntelAVBHAL() {
    close_device();
}

IntelAVBHAL::IntelAVBHAL(IntelAVBHAL&& other) noexcept
    : device_handle_(other.device_handle_)
    , last_error_(other.last_error_)
    , last_status_(other.last_status_) {
    other.device_handle_ = INVALID_HANDLE_VALUE;
}

IntelAVBHAL& IntelAVBHAL::operator=(IntelAVBHAL&& other) noexcept {
    if (this != &other) {
        close_device();
        device_handle_ = other.device_handle_;
        last_error_ = other.last_error_;
        last_status_ = other.last_status_;
        other.device_handle_ = INVALID_HANDLE_VALUE;
    }
    return *this;
}

bool IntelAVBHAL::open_device() {
    if (is_open()) {
        close_device();
    }
    
    // Open Intel AVB Filter Driver device
    device_handle_ = CreateFileW(
        L"\\\\.\\IntelAvbFilter",
        GENERIC_READ | GENERIC_WRITE,
        0, // No sharing
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (device_handle_ == INVALID_HANDLE_VALUE) {
        last_error_ = GetLastError();
        return false;
    }
    
    // AUTO-INITIALIZE: Call IOCTL_AVB_INIT_DEVICE after opening device
    // This matches the working tsauxc_toggle_test.c behavior from IntelAvbFilter driver
    // Required for driver to grant write access to PTP registers
    DWORD bytes_returned = 0;
    BOOL init_success = DeviceIoControl(
        device_handle_,
        IOCTL_AVB_INIT_DEVICE,
        nullptr, 0,
        nullptr, 0,
        &bytes_returned,
        nullptr
    );
    
    if (!init_success) {
        DWORD init_error = GetLastError();
        std::cerr << "WARNING: IOCTL_AVB_INIT_DEVICE failed (error " << init_error << ")\n";
        std::cerr << "         Some write operations may not work properly\n";
        // Don't return false - continue even if init fails (some drivers may not need it)
    }
    
    last_error_ = ERROR_SUCCESS;
    return true;
}

void IntelAVBHAL::close_device() {
    if (device_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(device_handle_);
        device_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool IntelAVBHAL::initialize_device() {
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        return false;
    }
    
    DWORD bytes_returned = 0;
    BOOL success = DeviceIoControl(
        device_handle_,
        IOCTL_AVB_INIT_DEVICE,
        nullptr, 0,
        nullptr, 0,
        &bytes_returned,
        nullptr
    );
    
    if (!success) {
        last_error_ = GetLastError();
        return false;
    }
    
    last_error_ = ERROR_SUCCESS;
    return true;
}

std::string IntelAVBHAL::get_device_info() {
    if (!is_open()) {
        return "";
    }
    
    AVB_DEVICE_INFO_REQUEST info_req;
    ZeroMemory(&info_req, sizeof(info_req));
    info_req.buffer_size = sizeof(info_req.device_info);
    
    if (!execute_ioctl(IOCTL_AVB_GET_DEVICE_INFO,
                      &info_req, sizeof(info_req),
                      &info_req, sizeof(info_req))) {
        return "";
    }
    
    last_status_ = info_req.status;
    return std::string(info_req.device_info);
}

size_t IntelAVBHAL::enumerate_adapters(AdapterInfo* adapters, size_t max_adapters) {
    if (!is_open() || !adapters || max_adapters == 0) {
        return 0;
    }
    
    // Get total count first
    AVB_ENUM_REQUEST enum_req;
    ZeroMemory(&enum_req, sizeof(enum_req));
    enum_req.index = 0;
    
    if (!execute_ioctl(IOCTL_AVB_ENUM_ADAPTERS,
                      &enum_req, sizeof(enum_req),
                      &enum_req, sizeof(enum_req))) {
        return 0;
    }
    
    size_t total_count = enum_req.count;
    size_t adapters_to_query = (total_count < max_adapters) ? total_count : max_adapters;
    
    // Query each adapter
    for (size_t i = 0; i < adapters_to_query; i++) {
        ZeroMemory(&enum_req, sizeof(enum_req));
        enum_req.index = static_cast<uint32_t>(i);
        
        if (execute_ioctl(IOCTL_AVB_ENUM_ADAPTERS,
                         &enum_req, sizeof(enum_req),
                         &enum_req, sizeof(enum_req))) {
            adapters[i].vendor_id = enum_req.vendor_id;
            adapters[i].device_id = enum_req.device_id;
            adapters[i].capabilities = enum_req.capabilities;
            
            // Build description
            std::ostringstream desc;
            desc << "Intel ";
            switch (enum_req.device_id) {
                case 0x1533: desc << "I210"; break;
                case 0x125C: desc << "I226-V"; break;
                case 0x125B: desc << "I226-IT"; break;
                case 0x15F2: desc << "I225-V"; break;
                case 0x15B7: 
                case 0x15B8: 
                case 0x15B9: desc << "I219"; break;
                default: desc << "0x" << std::hex << enum_req.device_id; break;
            }
            adapters[i].description = desc.str();
        }
    }
    
    return adapters_to_query;
}

bool IntelAVBHAL::open_adapter(uint16_t vendor_id, uint16_t device_id) {
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        return false;
    }
    
    AVB_OPEN_REQUEST open_req;
    ZeroMemory(&open_req, sizeof(open_req));
    open_req.vendor_id = vendor_id;
    open_req.device_id = device_id;
    
    if (!execute_ioctl(IOCTL_AVB_OPEN_ADAPTER,
                      &open_req, sizeof(open_req),
                      &open_req, sizeof(open_req))) {
        return false;
    }
    
    last_status_ = open_req.status;
    return (open_req.status == 0);
}

bool IntelAVBHAL::initialize_ptp_clock() {
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        std::cerr << "PTP Init: Device not open" << std::endl;
        return false;
    }
    
    std::cout << "PTP Init: Starting I226 PTP clock initialization..." << std::endl;
    
    // I226 PTP Clock Initialization Sequence
    // Based on Intel I226 datasheet Section 7.13 - IEEE 1588
    
    // ⚠️ WARNING: Direct register access should be avoided in production code!
    // This initialization method is for debugging/development only.
    // Proper approach is to use high-level IOCTLs (IOCTL_AVB_SET_TIMESTAMP, etc.)
    
    // Read current register values before initialization
    uint32_t old_tsauxc = 0, old_timinca = 0, old_systiml = 0, old_systimh = 0;
    read_register(0x0B640, old_tsauxc);  // TSAUXC: Time Sync Auxiliary Control
    read_register(0x0B608, old_timinca); // TIMINCA: Time Increment Attributes
    read_register(0x0B600, old_systiml); // SYSTIML: System Time Low
    read_register(0x0B604, old_systimh); // SYSTIMH: System Time High
    
    std::cout << "PTP Init: Before - TSAUXC=0x" << std::hex << old_tsauxc
              << ", TIMINCA=0x" << old_timinca
              << ", SYSTIML=0x" << old_systiml
              << ", SYSTIMH=0x" << old_systimh << std::dec << std::endl;
    
    // Step 1: Enable timestamp in TSAUXC register
    // TSAUXC offset: 0x0B640 - Time Sync Auxiliary Control register
    // Bit 31: DisableSystime - INVERTED BIT (0=enabled, 1=disabled)
    // Note: Setting bit 31 DISABLES PTP, clearing enables it
    std::cout << "PTP Init: Step 1 - Clearing TSAUXC bit 31 (enable PTP)..." << std::endl;
    uint32_t tsauxc = 0x78000000; // Bit 31 clear = PTP enabled, other config bits set
    if (!write_register(0x0B640, tsauxc)) {
        std::cerr << "PTP Init: Failed to write TSAUXC (status=" << last_status_ << ")" << std::endl;
        return false;
    }
    std::cout << "PTP Init: TSAUXC write completed" << std::endl;
    
    // Step 2: Program TIMINCA register
    // TIMINCA offset: 0x0B608 - Time Increment Attributes
    // For I226 hardware:
    // - Increment = 24 ns per clock cycle (0x18 in bits [31:24])
    // - Period = 0 (no fractional adjustment initially)
    std::cout << "PTP Init: Step 2 - Programming TIMINCA..." << std::endl;
    uint32_t timinca = 0x18000000; // 24 ns increment (matches hardware default)
    if (!write_register(0x0B608, timinca)) {
        std::cerr << "PTP Init: Failed to write TIMINCA (status=" << last_status_ << ")" << std::endl;
        return false;
    }
    std::cout << "PTP Init: TIMINCA write completed" << std::endl;
    
    // Step 3: Initialize SYSTIML and SYSTIMH to current time
    // Get current Unix timestamp
    std::cout << "PTP Init: Step 3 - Initializing SYSTIM to current time..." << std::endl;
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000LL;
    
    std::cout << "PTP Init: Current time: " << seconds << "s + " << nanoseconds << "ns" << std::endl;
    
    // Write SYSTIML (nanoseconds)
    if (!write_register(0x0B600, static_cast<uint32_t>(nanoseconds))) {
        std::cerr << "PTP Init: Failed to write SYSTIML (status=" << last_status_ << ")" << std::endl;
        return false;
    }
    std::cout << "PTP Init: SYSTIML write completed" << std::endl;
    
    // Write SYSTIMH (seconds)
    if (!write_register(0x0B604, static_cast<uint32_t>(seconds & 0xFFFFFFFF))) {
        std::cerr << "PTP Init: Failed to write SYSTIMH (status=" << last_status_ << ")" << std::endl;
        return false;
    }
    std::cout << "PTP Init: SYSTIMH write completed" << std::endl;
    
    // Verify initialization by reading back registers
    std::cout << "PTP Init: Verifying initialization..." << std::endl;
    uint32_t verify_tsauxc = 0, verify_timinca = 0, verify_systiml = 0, verify_systimh = 0;
    if (!read_register(0x0B640, verify_tsauxc)) {
        std::cerr << "PTP Init: Failed to read TSAUXC for verification" << std::endl;
        return false;
    }
    read_register(0x0B608, verify_timinca);
    read_register(0x0B600, verify_systiml);
    read_register(0x0B604, verify_systimh);
    
    std::cout << "PTP Init: After - TSAUXC=0x" << std::hex << verify_tsauxc
              << ", TIMINCA=0x" << verify_timinca
              << ", SYSTIML=0x" << verify_systiml
              << ", SYSTIMH=0x" << verify_systimh << std::dec << std::endl;
    
    if ((verify_tsauxc & 0x80000000) != 0) {
        std::cerr << "PTP Init: TSAUXC bit 31 is SET (PTP disabled) after initialization!" << std::endl;
        std::cerr << "           Expected bit 31 CLEAR for enabled PTP clock" << std::endl;
        last_error_ = ERROR_BAD_CONFIGURATION;
        return false;
    }
    
    std::cout << "PTP Init: Initialization successful!" << std::endl;
    return true;
}

bool IntelAVBHAL::get_timestamp(PTPTimestamp& timestamp) {
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        return false;
    }
    
    AVB_TIMESTAMP_REQUEST ts_req;
    ZeroMemory(&ts_req, sizeof(ts_req));
    ts_req.clock_id = 0; // Default hardware clock
    
    if (!execute_ioctl(IOCTL_AVB_GET_TIMESTAMP,
                      &ts_req, sizeof(ts_req),
                      &ts_req, sizeof(ts_req))) {
        return false;
    }
    
    last_status_ = ts_req.status;
    
    // Convert from nanoseconds total to seconds + nanoseconds
    timestamp = PTPTimestamp::from_nanoseconds(ts_req.timestamp);
    return (ts_req.status == 0);
}

bool IntelAVBHAL::set_timestamp(const PTPTimestamp& timestamp) {
    // Use production IOCTL_AVB_SET_TIMESTAMP approach
    // Direct testing confirms this IOCTL works correctly
    
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        return false;
    }
    
    AVB_TIMESTAMP_REQUEST ts_req;
    ZeroMemory(&ts_req, sizeof(ts_req));
    ts_req.timestamp = timestamp.to_nanoseconds();
    ts_req.clock_id = 0;
    
    if (!execute_ioctl(IOCTL_AVB_SET_TIMESTAMP,
                      &ts_req, sizeof(ts_req),
                      &ts_req, sizeof(ts_req))) {
        return false;
    }
    
    last_status_ = ts_req.status;
    return (ts_req.status == 0);
}

bool IntelAVBHAL::adjust_clock_offset(int64_t offset_ns) {
    // Get current time
    PTPTimestamp current;
    if (!get_timestamp(current)) {
        return false;
    }
    
    // Calculate new time with offset
    int64_t new_ns = static_cast<int64_t>(current.to_nanoseconds()) + offset_ns;
    if (new_ns < 0) {
        new_ns = 0; // Clamp to zero
    }
    
    PTPTimestamp new_time = PTPTimestamp::from_nanoseconds(
        static_cast<uint64_t>(new_ns));
    
    return set_timestamp(new_time);
}

bool IntelAVBHAL::adjust_clock_frequency(double ppb) {
    // Use production IOCTL_AVB_ADJUST_FREQUENCY approach  
    // Direct testing confirms this IOCTL works correctly
    
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        return false;
    }
    
    // For I226 @ 125MHz: base clock period = 8ns, increment = 24ns (3 cycles)
    // Frequency adjustment modifies fractional part of increment
    // Formula: increment_frac = (ppb * 2^32) / 10^9
    
    AVB_FREQUENCY_REQUEST freq_req = {};
    freq_req.increment_ns = 24;  // I226 base increment
    
    // Calculate fractional adjustment
    // ppb range: ±1000 ppm = ±1,000,000 ppb
    // Fractional resolution: 2^32 per nanosecond
    int64_t frac_adjustment = (static_cast<int64_t>(ppb * 1000.0) * (1ULL << 32)) / 1000000000LL;
    freq_req.increment_frac = static_cast<uint32_t>(frac_adjustment & 0xFFFFFFFF);
    
    DWORD bytes_returned = 0;
    BOOL success = DeviceIoControl(
        device_handle_,
        IOCTL_AVB_ADJUST_FREQUENCY,
        &freq_req,
        sizeof(freq_req),
        &freq_req,  // Driver returns current_increment
        sizeof(freq_req),
        &bytes_returned,
        nullptr
    );
    
    if (!success) {
        last_error_ = GetLastError();
        return false;
    }
    
    last_status_ = freq_req.status;
    return (freq_req.status == 0);
}

#ifndef NDEBUG
// Debug-only register access - matches driver's conditional compilation
// In production builds, use high-level IOCTLs:
// - IOCTL_AVB_GET_CLOCK_CONFIG for PTP register queries
// - IOCTL_AVB_ADJUST_FREQUENCY for clock adjustments
bool IntelAVBHAL::read_register(uint32_t offset, uint32_t& value) {
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        return false;
    }
    
    AVB_REGISTER_REQUEST reg_req;
    ZeroMemory(&reg_req, sizeof(reg_req));
    reg_req.offset = offset;
    
    if (!execute_ioctl(IOCTL_AVB_READ_REGISTER,
                      &reg_req, sizeof(reg_req),
                      &reg_req, sizeof(reg_req))) {
        return false;
    }
    
    last_status_ = reg_req.status;
    value = reg_req.value;
    return (reg_req.status == 0);
}

bool IntelAVBHAL::write_register(uint32_t offset, uint32_t value) {
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        return false;
    }
    
    AVB_REGISTER_REQUEST reg_req;
    ZeroMemory(&reg_req, sizeof(reg_req));
    reg_req.offset = offset;
    reg_req.value = value;
    
    if (!execute_ioctl(IOCTL_AVB_WRITE_REGISTER,
                      &reg_req, sizeof(reg_req),
                      &reg_req, sizeof(reg_req))) {
        return false;
    }
    
    last_status_ = reg_req.status;
    return (reg_req.status == 0);
}
#endif // NDEBUG

bool IntelAVBHAL::get_clock_config(uint64_t& systim, uint32_t& timinca, 
                                   uint32_t& tsauxc, uint32_t& clock_rate_mhz) {
    // Production approach: Use IOCTL_AVB_GET_CLOCK_CONFIG (no register addresses)
    // This replaces raw register reads of SYSTIM, TIMINCA, TSAUXC
    
    if (!is_open()) {
        last_error_ = ERROR_INVALID_HANDLE;
        return false;
    }
    
    AVB_CLOCK_CONFIG config = {};
    
    DWORD bytes_returned = 0;
    BOOL success = DeviceIoControl(
        device_handle_,
        IOCTL_AVB_GET_CLOCK_CONFIG,
        nullptr,
        0,
        &config,
        sizeof(config),
        &bytes_returned,
        nullptr
    );
    
    if (!success) {
        last_error_ = GetLastError();
        return false;
    }
    
    if (config.status != 0) {
        last_status_ = config.status;
        return false;
    }
    
    // Return queried values
    systim = config.systim;
    timinca = config.timinca;
    tsauxc = config.tsauxc;
    clock_rate_mhz = config.clock_rate_mhz;
    
    return true;
}

std::string IntelAVBHAL::get_error_string() const {
    if (last_error_ == ERROR_SUCCESS) {
        return "Success";
    }
    
    LPWSTR buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        last_error_,
        0,
        (LPWSTR)&buffer,
        0,
        nullptr
    );
    
    std::wstring wstr(buffer ? buffer : L"Unknown error");
    LocalFree(buffer);
    
    // Convert to narrow string
    std::string result;
    for (wchar_t wc : wstr) {
        result += static_cast<char>(wc);
    }
    return result;
}

bool IntelAVBHAL::execute_ioctl(DWORD ioctl_code,
                               void* input_buffer,
                               DWORD input_size,
                               void* output_buffer,
                               DWORD output_size) {
    DWORD bytes_returned = 0;
    BOOL success = DeviceIoControl(
        device_handle_,
        ioctl_code,
        input_buffer,
        input_size,
        output_buffer,
        output_size,
        &bytes_returned,
        nullptr
    );
    
    if (!success) {
        last_error_ = GetLastError();
        return false;
    }
    
    last_error_ = ERROR_SUCCESS;
    return true;
}

//============================================================================
// IEEE1588HALAdapter Implementation
//============================================================================

IEEE1588HALAdapter::IEEE1588HALAdapter(std::shared_ptr<IntelAVBHAL> hal)
    : hal_(hal) {
}

uint64_t IEEE1588HALAdapter::get_timestamp_callback(void* context) {
    auto* adapter = static_cast<IEEE1588HALAdapter*>(context);
    if (!adapter || !adapter->hal_) {
        return 0;
    }
    
    PTPTimestamp timestamp;
    if (!adapter->hal_->get_timestamp(timestamp)) {
        return 0;
    }
    
    return timestamp.to_nanoseconds();
}

int IEEE1588HALAdapter::get_tx_timestamp_callback(void* context,
                                                 uint16_t sequence_id,
                                                 uint64_t* timestamp) {
    auto* adapter = static_cast<IEEE1588HALAdapter*>(context);
    if (!adapter || !adapter->hal_ || !timestamp) {
        return -1;
    }
    
    // For now, use current timestamp
    // TODO: Implement TX timestamp capture via hardware
    PTPTimestamp ts;
    if (!adapter->hal_->get_timestamp(ts)) {
        return -1;
    }
    
    *timestamp = ts.to_nanoseconds();
    return 0;
}

int IEEE1588HALAdapter::adjust_clock_callback(void* context, int64_t adjustment_ns) {
    auto* adapter = static_cast<IEEE1588HALAdapter*>(context);
    if (!adapter || !adapter->hal_) {
        return -1;
    }
    
    return adapter->hal_->adjust_clock_offset(adjustment_ns) ? 0 : -1;
}

int IEEE1588HALAdapter::adjust_frequency_callback(void* context, double ppb_adjustment) {
    auto* adapter = static_cast<IEEE1588HALAdapter*>(context);
    if (!adapter || !adapter->hal_) {
        return -1;
    }
    
    return adapter->hal_->adjust_clock_frequency(ppb_adjustment) ? 0 : -1;
}

} // namespace IntelAVB
} // namespace Examples
