/**
 * @file intel_avb_hal.hpp
 * @brief Hardware Abstraction Layer for Intel AVB Filter Driver
 * 
 * Implements IEEE 1588-2019 HAL interface using Intel AVB Filter Driver
 * IOCTLs for Windows platform with Intel I210/I226 Ethernet controllers.
 * 
 * Provides:
 * - Hardware timestamping via IOCTL_AVB_GET_TIMESTAMP
 * - Register access for PTP configuration
 * - Clock adjustment capabilities
 * - Multi-adapter support
 * 
 * @copyright IEEE 1588-2019 compliant implementation
 * @version 1.0.0
 * @date 2025-12-05
 * 
 * @note Requires Intel AVB Filter Driver installed
 * @see ../../../external/IntelAvbFilter/include/avb_ioctl.h
 */

#ifndef EXAMPLES_INTEL_AVB_HAL_HPP
#define EXAMPLES_INTEL_AVB_HAL_HPP

#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>
#include <array>

// Intel AVB Filter Driver IOCTL interface
// Path should be adjusted based on your IntelAvbFilter location
extern "C" {
#include "../../../IntelAvbFilter/include/avb_ioctl.h"
}

namespace Examples {
namespace IntelAVB {

/**
 * @brief Intel adapter identification
 */
struct AdapterInfo {
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t capabilities;
    std::string description;
    
    bool supports_ptp() const {
        return (capabilities & 0x00000001) != 0; // INTEL_CAP_BASIC_1588
    }
    
    bool supports_tsn() const {
        return (capabilities & 0x00000004) != 0; // INTEL_CAP_TSN_TAS
    }
};

/**
 * @brief PTP Timestamp structure (IEEE 1588-2019 compliant)
 */
struct PTPTimestamp {
    uint64_t seconds;      // 48-bit seconds (stored in 64-bit)
    uint32_t nanoseconds;  // 32-bit nanoseconds
    
    PTPTimestamp() : seconds(0), nanoseconds(0) {}
    PTPTimestamp(uint64_t sec, uint32_t nsec) : seconds(sec), nanoseconds(nsec) {}
    
    // Convert to/from nanoseconds total
    uint64_t to_nanoseconds() const {
        return seconds * 1000000000ULL + nanoseconds;
    }
    
    static PTPTimestamp from_nanoseconds(uint64_t total_ns) {
        return PTPTimestamp(total_ns / 1000000000ULL, 
                           static_cast<uint32_t>(total_ns % 1000000000ULL));
    }
};

/**
 * @brief Intel AVB Hardware Abstraction Layer
 * 
 * Provides IEEE 1588-2019 compliant HAL using Intel AVB Filter Driver.
 * 
 * CRITICAL FEATURES:
 * - Hardware timestamping (<100ns accuracy)
 * - Direct register access for PTP configuration
 * - Multi-adapter support (I210, I219, I225, I226)
 * - Windows-native implementation
 */
class IntelAVBHAL {
public:
    /**
     * @brief Constructor - does not open device
     */
    IntelAVBHAL();
    
    /**
     * @brief Destructor - closes device handle
     */
    ~IntelAVBHAL();
    
    // Non-copyable
    IntelAVBHAL(const IntelAVBHAL&) = delete;
    IntelAVBHAL& operator=(const IntelAVBHAL&) = delete;
    
    // Movable
    IntelAVBHAL(IntelAVBHAL&&) noexcept;
    IntelAVBHAL& operator=(IntelAVBHAL&&) noexcept;
    
    //========================================================================
    // Device Management
    //========================================================================
    
    /**
     * @brief Open Intel AVB Filter Driver device
     * 
     * Opens device handle to "\\.\IntelAvbFilter"
     * 
     * @return true on success, false on failure
     */
    bool open_device();
    
    /**
     * @brief Close device handle
     */
    void close_device();
    
    /**
     * @brief Check if device is open
     */
    bool is_open() const { return device_handle_ != INVALID_HANDLE_VALUE; }
    
    /**
     * @brief Initialize device subsystem
     * 
     * Calls IOCTL_AVB_INIT_DEVICE to trigger hardware initialization.
     * Optional - driver performs lazy initialization.
     * 
     * @return true on success
     */
    bool initialize_device();
    
    /**
     * @brief Get device information string
     * 
     * @return Device description or empty string on failure
     */
    std::string get_device_info();
    
    //========================================================================
    // Multi-Adapter Support
    //========================================================================
    
    /**
     * @brief Enumerate available Intel adapters
     * 
     * @param adapters Output array of adapter information
     * @param max_adapters Maximum adapters to enumerate
     * @return Number of adapters found
     */
    size_t enumerate_adapters(AdapterInfo* adapters, size_t max_adapters);
    
    /**
     * @brief Open specific adapter by vendor/device ID
     * 
     * Switches driver context to specified adapter.
     * All subsequent IOCTLs target this adapter.
     * 
     * @param vendor_id PCI vendor ID (0x8086 for Intel)
     * @param device_id PCI device ID (e.g., 0x125C for I226-V)
     * @return true on success
     */
    bool open_adapter(uint16_t vendor_id, uint16_t device_id);
    
    /**
     * @brief Initialize PTP clock hardware
     * 
     * Configures PTP clock registers for I226 controllers:
     * - Sets TSAUXC bit 31 (enable timestamp)
     * - Programs TIMINCA for proper increment
     * - Initializes SYSTIML/SYSTIMH to current time
     * 
     * @return true on success
     * 
     * @note Must be called after open_adapter() for I226
     */
    bool initialize_ptp_clock();
    
    //========================================================================
    // IEEE 1588 PTP Hardware Clock
    //========================================================================
    
    /**
     * @brief Get current PTP hardware timestamp
     * 
     * Reads IEEE 1588 timestamp from hardware via IOCTL_AVB_GET_TIMESTAMP.
     * Uses SYSTIML/SYSTIMH registers on Intel controllers.
     * 
     * @param timestamp Output timestamp
     * @return true on success
     * 
     * @note Accuracy: ~8ns resolution on I210, ~1ns on I226
     */
    bool get_timestamp(PTPTimestamp& timestamp);
    
    /**
     * @brief Set PTP hardware timestamp
     * 
     * Writes IEEE 1588 timestamp to hardware via IOCTL_AVB_SET_TIMESTAMP.
     * 
     * @param timestamp Timestamp to set
     * @return true on success
     */
    bool set_timestamp(const PTPTimestamp& timestamp);
    
    /**
     * @brief Adjust clock by offset
     * 
     * Apply step adjustment to PTP hardware clock.
     * 
     * @param offset_ns Offset in nanoseconds (can be negative)
     * @return true on success
     * 
     * @note Use for large corrections (>128ms recommended)
     */
    bool adjust_clock_offset(int64_t offset_ns);
    
    /**
     * @brief Adjust clock frequency
     * 
     * Apply frequency correction for continuous discipline via IOCTL_AVB_ADJUST_FREQUENCY.
     * Production-safe approach without hardcoded register addresses.
     * 
     * @param ppb Parts-per-billion adjustment
     * @return true on success
     * 
     * @note Use for continuous servo control (hardware frequency adjustment)
     */
    bool adjust_clock_frequency(double ppb);
    
    /**
     * @brief Get PTP clock configuration
     * 
     * Query current PTP hardware state via IOCTL_AVB_GET_CLOCK_CONFIG.
     * Production-safe approach without hardcoded register addresses.
     * 
     * @param systim Output: Current SYSTIM counter value (nanoseconds)
     * @param timinca Output: Current clock increment configuration
     * @param tsauxc Output: Auxiliary clock control register state
     * @param clock_rate_mhz Output: Base clock rate (125/156/200/250 MHz)
     * @return true on success
     * 
     * @note Replaces raw register reads of SYSTIM, TIMINCA, TSAUXC
     */
    bool get_clock_config(uint64_t& systim, uint32_t& timinca, 
                         uint32_t& tsauxc, uint32_t& clock_rate_mhz);
    
    //========================================================================
    // Register Access (Debug-Only - Use High-Level IOCTLs in Production)
    //========================================================================
    
#ifndef NDEBUG
    /**
     * @brief Read 32-bit MMIO register (DEBUG ONLY)
     * 
     * Direct register access via IOCTL_AVB_READ_REGISTER.
     * 
     * @warning DEBUG BUILDS ONLY - disabled in release for security.
     *          In production, use:
     *          - get_clock_config() for PTP register queries
     *          - adjust_clock_frequency() for clock adjustments
     * 
     * @param offset Register offset from BAR0
     * @param value Output register value
     * @return true on success
     */
    bool read_register(uint32_t offset, uint32_t& value);
    
    /**
     * @brief Write 32-bit MMIO register (DEBUG ONLY)
     * 
     * Direct register access via IOCTL_AVB_WRITE_REGISTER.
     * 
     * @warning DEBUG BUILDS ONLY - disabled in release for security.
     *          In production, use:
     *          - adjust_clock_frequency() for frequency control
     *          - set_timestamp() for clock initialization
     * 
     * @param offset Register offset from BAR0
     * @param value Register value to write
     * @return true on success
     */
    bool write_register(uint32_t offset, uint32_t value);
#endif // NDEBUG
    
    //========================================================================
    // Error Handling
    //========================================================================
    
    /**
     * @brief Get last Windows error code
     */
    DWORD get_last_error() const { return last_error_; }
    
    /**
     * @brief Get last NTSTATUS code from driver
     */
    uint32_t get_last_status() const { return last_status_; }
    
    /**
     * @brief Get error description
     */
    std::string get_error_string() const;

private:
    HANDLE device_handle_;
    DWORD last_error_;
    uint32_t last_status_;
    
    // Helper: Execute DeviceIoControl with error handling
    bool execute_ioctl(DWORD ioctl_code, 
                      void* input_buffer, 
                      DWORD input_size,
                      void* output_buffer, 
                      DWORD output_size);
};

/**
 * @brief IEEE 1588-2019 HAL Adapter
 * 
 * Adapts IntelAVBHAL to IEEE 1588-2019 StateCallbacks interface.
 * Provides function pointers for PTP clock state machines.
 */
class IEEE1588HALAdapter {
public:
    /**
     * @brief Constructor with Intel AVB HAL reference
     */
    explicit IEEE1588HALAdapter(std::shared_ptr<IntelAVBHAL> hal);
    
    /**
     * @brief Get timestamp callback for IEEE 1588 stack
     * 
     * @note Compatible with Types::Timestamp (*get_timestamp)()
     */
    static uint64_t get_timestamp_callback(void* context);
    
    /**
     * @brief Get TX timestamp callback for IEEE 1588 stack
     * 
     * @param sequence_id Sequence ID of transmitted packet
     * @param timestamp Output timestamp
     * @return 0 on success, negative on error
     */
    static int get_tx_timestamp_callback(void* context, 
                                        uint16_t sequence_id, 
                                        uint64_t* timestamp);
    
    /**
     * @brief Adjust clock callback for IEEE 1588 stack
     * 
     * @param adjustment_ns Clock offset adjustment in nanoseconds
     * @return 0 on success, negative on error
     */
    static int adjust_clock_callback(void* context, int64_t adjustment_ns);
    
    /**
     * @brief Adjust frequency callback for IEEE 1588 stack
     * 
     * @param ppb_adjustment Frequency adjustment in parts-per-billion
     * @return 0 on success, negative on error
     */
    static int adjust_frequency_callback(void* context, double ppb_adjustment);
    
    /**
     * @brief Get context pointer for callbacks
     */
    void* get_context() { return this; }

private:
    std::shared_ptr<IntelAVBHAL> hal_;
};

} // namespace IntelAVB
} // namespace Examples

#endif // EXAMPLES_INTEL_AVB_HAL_HPP
