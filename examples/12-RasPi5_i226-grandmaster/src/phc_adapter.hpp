/**
 * @file phc_adapter.hpp
 * @brief Hardware abstraction for PTP Hardware Clock (PHC)
 * 
 * Provides clean interface for i226 NIC PHC access, following IEEE 1588-2019
 * dependency injection pattern. Isolates hardware-specific Linux PTP HAL calls
 * from protocol logic.
 * 
 * @see ARCHITECTURE.md Section 1: Hardware Adapters
 * @see .github/instructions/copilot-instructions.md (Hardware Agnostic principle)
 */

#ifndef PHC_ADAPTER_HPP
#define PHC_ADAPTER_HPP

#include <cstdint>
#include <string>

/**
 * @brief Hardware abstraction for PTP Hardware Clock
 * 
 * This adapter wraps Linux-specific PHC operations (clock_gettime, clock_settime,
 * clock_adjtime) to enable:
 * - Hardware-independent protocol testing (mock adapters)
 * - Clean separation of concerns (hardware vs. servo logic)
 * - IEEE 1588-2019 compliance (dependency injection pattern)
 */
class PhcAdapter {
public:
    /**
     * @brief Default constructor
     */
    PhcAdapter();
    
    /**
     * @brief Destructor
     */
    virtual ~PhcAdapter();
    
    /**
     * @brief Initialize PHC for specified network interface
     * 
     * @param interface_name Network interface name (e.g., "eth1")
     * @return true if successful, false otherwise
     * 
     * @note Must be called before any other operations
     * @note Stores interface name for /dev/ptp* device discovery
     */
    virtual bool initialize(const char* interface_name);
    
    /**
     * @brief Check if adapter is initialized
     * 
     * @return true if initialize() was successful
     */
    bool is_initialized() const;
    
    /**
     * @brief Get current PHC timestamp
     * 
     * @param[out] sec Seconds since Unix epoch
     * @param[out] nsec Nanoseconds within current second [0, 999999999]
     * @return true if read successful, false otherwise
     * 
     * @note Uses clock_gettime(CLOCK_REALTIME) on PHC device
     * @note Precision: Nanosecond resolution (hardware-dependent accuracy)
     */
    virtual bool get_time(uint64_t* sec, uint32_t* nsec);
    
    /**
     * @brief Set PHC to specific timestamp (step correction)
     * 
     * @param sec Seconds since Unix epoch
     * @param nsec Nanoseconds within second [0, 999999999]
     * @return true if set successful, false otherwise
     * 
     * @note Uses clock_settime(CLOCK_REALTIME) on PHC device
     * @warning Causes discontinuity in PHC timeline (use for large offsets only)
     * @warning Caller should reset servo integral after step
     */
    virtual bool set_time(uint64_t sec, uint32_t nsec);
    
    /**
     * @brief Adjust PHC frequency (slew correction)
     * 
     * @param freq_ppb Frequency adjustment in parts-per-billion
     *                 Positive: PHC runs faster
     *                 Negative: PHC runs slower
     *                 Range: ±500000 ppb (±500 ppm) for i226
     * @return true if adjustment successful, false otherwise
     * 
     * @note Uses clock_adjtime(ADJ_FREQUENCY) on PHC device
     * @note Adjustment is ABSOLUTE, not incremental (caller manages accumulation)
     * @note Hardware enforces ±500000 ppb limit (i226 specification)
     * 
     * CRITICAL: This sets the TOTAL frequency, not an increment:
     *   Example: adjust_frequency(10000) → PHC runs at +10000 ppb
     *            adjust_frequency(15000) → PHC runs at +15000 ppb (not +25000)
     * 
     * Caller must accumulate corrections:
     *   cumulative_freq = calibration_drift + servo_correction
     *   adjust_frequency(cumulative_freq)
     */
    virtual bool adjust_frequency(int32_t freq_ppb);
    
    /**
     * @brief Get maximum supported frequency adjustment
     * 
     * @return Maximum frequency adjustment in ppb (500000 for i226)
     * 
     * @note Hardware limit enforced by NIC (i226: ±500 ppm)
     */
    int32_t get_max_frequency_ppb() const;
    
    /**
     * @brief Get network interface name
     * 
     * @return Interface name (e.g., "eth1"), or empty if not initialized
     */
    const char* get_interface_name() const;
    
    /**
     * @brief Get PHC device path
     * 
     * @return Device path (e.g., "/dev/ptp0"), or empty if not initialized
     */
    const char* get_device_path() const;

private:
    // Linux PTP HAL integration (will be replaced with abstraction layer)
    bool initialized_;
    std::string interface_name_;
    std::string device_path_;
    int phc_fd_;  // File descriptor for /dev/ptp* device
    
    // Hardware capabilities
    static constexpr int32_t MAX_FREQUENCY_PPB = 500000;  // i226 limit: ±500 ppm
    
    // Helper methods
    bool discover_phc_device();
    bool open_phc_device();
    void close_phc_device();
};

#endif // PHC_ADAPTER_HPP
