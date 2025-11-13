/**
 * @file dcf77_adapter.hpp
 * @brief DCF77 Terrestrial Radio Time Source Adapter for IEEE 1588-2019 PTP
 * 
 * Adapter that decodes DCF77 time signals and updates PTP clock quality using
 * the library's Types::ClockQuality and Types::TimeSource types.
 * 
 * This example demonstrates:
 * - Using Types::TimeSource::Terrestrial_Radio (0x30) from the library
 * - Updating DefaultDataSet.clockQuality with library types
 * - Computing clock quality from DCF77 signal strength and decode success
 * 
 * IMPORTANT: This adapter USES the library's types, it does NOT duplicate them!
 * 
 * DCF77 Background:
 * - Frequency: 77.5 kHz longwave
 * - Location: Mainflingen, Germany (50°01'N, 9°00'E)
 * - Coverage: ~2000 km range (Central Europe)
 * - Accuracy: ±1 ms to PTB atomic clocks
 * - Modulation: Amplitude modulation (carrier reduction 25% or 10%)
 * - Bit encoding: Pulse width modulation (100ms = 0, 200ms = 1)
 * - Frame: 59 bits per minute, synchronized to atomic time (UTC+1/UTC+2)
 * 
 * Hardware Requirements:
 * - DCF77 receiver module (e.g., Pollin DCF1, Conrad DCF77, HKW DCF77)
 * - ESP32 or Arduino compatible microcontroller
 * - Pull-up resistor (10kΩ) on data pin if module has open-collector output
 * 
 * @see IEEE 1588-2019, Section 8.6.2.7 "timeSource"
 * @see IEEE 1588-2019, Table 6 "timeSource enumeration" (TERRESTRIAL_RADIO = 0x30)
 * @see include/IEEE/1588/PTP/2019/types.hpp for Types::ClockQuality
 * @see include/clocks.hpp for DefaultDataSet and TimePropertiesDataSet
 */

#ifndef DCF77_ADAPTER_HPP
#define DCF77_ADAPTER_HPP

#include "IEEE/1588/PTP/2019/types.hpp"  // Use library's Types::ClockQuality, Types::TimeSource
#include <cstdint>
#include <chrono>
#include <array>

namespace Examples {
namespace DCF77 {

/**
 * @brief DCF77 Bit Value
 */
enum class DCF77Bit {
    ZERO = 0,      ///< 100ms pulse
    ONE = 1,       ///< 200ms pulse
    INVALID = 2    ///< Invalid pulse width
};

/**
 * @brief DCF77 Frame (59 bits)
 */
struct DCF77Frame {
    bool valid{false};
    
    // Time components (BCD encoded in DCF77 protocol)
    uint8_t minute{0};       ///< Minutes (0-59)
    uint8_t hour{0};         ///< Hour (0-23)
    uint8_t day{0};          ///< Day of month (1-31)
    uint8_t weekday{0};      ///< Day of week (1-7, Monday=1)
    uint8_t month{0};        ///< Month (1-12)
    uint8_t year{0};         ///< Year (00-99, 20xx)
    
    // Status bits
    bool cet{false};         ///< Central European Time (UTC+1)
    bool cest{false};        ///< Central European Summer Time (UTC+2)
    bool leap_second{false}; ///< Leap second announcement
    
    // Quality indicators
    uint8_t signal_strength{0};  ///< Signal strength (0-100%)
    uint8_t decode_errors{0};    ///< Parity/consistency errors
    
    std::chrono::system_clock::time_point timestamp;  ///< When frame was received
};

/**
 * @brief DCF77 Signal Statistics
 */
struct DCF77Statistics {
    uint32_t frames_received{0};      ///< Total frames successfully decoded
    uint32_t frames_failed{0};        ///< Frames with parity errors
    uint32_t signal_losses{0};        ///< Number of times signal was lost
    uint8_t  avg_signal_strength{0};  ///< Average signal strength (0-100%)
    uint32_t seconds_since_sync{0};   ///< Seconds since last valid frame
};

/**
 * @brief DCF77 Terrestrial Radio Time Source Adapter
 * 
 * Decodes DCF77 time signals and computes IEEE 1588-2019 clock quality
 * using the library's Types::ClockQuality struct.
 * 
 * DCF77 Protocol:
 * - Second marks: 0.1 or 0.2 second carrier reduction
 * - Minute mark: No reduction at second 59
 * - Bit 0 (100ms pulse) = logic 0
 * - Bit 1 (200ms pulse) = logic 1
 * - 59 bits per minute frame
 * 
 * Example Usage:
 * @code
 * DCF77Adapter dcf77(GPIO_PIN_4);  // Data pin
 * dcf77.initialize();
 * 
 * // Call frequently to process incoming bits
 * while (running) {
 *     dcf77.update();
 *     
 *     if (dcf77.is_synchronized()) {
 *         // Get clock quality using LIBRARY's Types::ClockQuality
 *         Types::ClockQuality quality = dcf77.get_clock_quality();
 *         
 *         // Update PTP clock
 *         auto& ds = ptp_clock.get_default_data_set();
 *         ds.clockQuality = quality;  // Use library type!
 *         
 *         auto& tp = ptp_clock.get_time_properties_data_set();
 *         tp.timeSource = static_cast<uint8_t>(Types::TimeSource::Terrestrial_Radio);
 *     }
 * }
 * @endcode
 */
class DCF77Adapter {
public:
    /**
     * @brief Construct DCF77 adapter
     * 
     * @param data_pin GPIO pin connected to DCF77 receiver data output
     * @param invert_signal Invert signal (true if receiver has active-low output)
     */
    explicit DCF77Adapter(uint8_t data_pin, bool invert_signal = false);
    
    ~DCF77Adapter() = default;
    
    /**
     * @brief Initialize DCF77 receiver
     * 
     * Sets up GPIO pin and interrupt handlers.
     * 
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Process DCF77 signal and decode bits
     * 
     * Should be called frequently (e.g., every 50ms or in main loop).
     * Processes incoming pulses and builds complete minute frames.
     * 
     * @return true if new frame decoded
     */
    bool update();
    
    /**
     * @brief Get clock quality using LIBRARY's Types::ClockQuality
     * 
     * Computes clock quality from DCF77 signal strength and decode success:
     * - Good signal + recent sync: clockClass 6-13 (primary time source)
     * - Weak signal: clockClass 52-58 (degraded)
     * - No sync: clockClass 248 (unsynchronized)
     * 
     * Accuracy: ±1ms to atomic time (clockAccuracy 0x29)
     * 
     * @return Library's Types::ClockQuality struct (DO NOT RECREATE THIS TYPE!)
     */
    Types::ClockQuality get_clock_quality() const;
    
    /**
     * @brief Get time source type - always Terrestrial_Radio from library enum
     * 
     * @return Types::TimeSource::Terrestrial_Radio (0x30) from library
     */
    static constexpr Types::TimeSource get_time_source() {
        return Types::TimeSource::Terrestrial_Radio;
    }
    
    /**
     * @brief Check if synchronized to DCF77
     */
    bool is_synchronized() const;
    
    /**
     * @brief Get last decoded frame
     */
    const DCF77Frame& get_last_frame() const { return last_frame_; }
    
    /**
     * @brief Get DCF77 statistics
     */
    const DCF77Statistics& get_statistics() const { return statistics_; }
    
    /**
     * @brief Get current time from DCF77 (system_clock::time_point)
     * 
     * Returns the most recent DCF77 time adjusted by local clock drift.
     * 
     * @param[out] time Current time from DCF77 reference
     * @return true if time is valid and recent
     */
    bool get_time(std::chrono::system_clock::time_point& time) const;
    
    /**
     * @brief Get current time as PTP Timestamp (TAI)
     * 
     * Converts DCF77 time to PTP Timestamp format for direct use with
     * IEEE 1588-2019 library.
     * 
     * @param[out] seconds PTP seconds since epoch
     * @param[out] nanoseconds Nanoseconds within second
     * @return true if timestamp is valid
     */
    bool get_ptp_timestamp(uint64_t& seconds, uint32_t& nanoseconds) const;
    
    /**
     * @brief Get offset from local clock (nanoseconds)
     * 
     * Estimates offset between local clock and DCF77 reference.
     * Positive = local clock is ahead (subtract to correct)
     * Negative = local clock is behind (add to correct)
     * 
     * @return Estimated offset in nanoseconds, or 0 if not synchronized
     */
    int64_t get_offset_ns() const;
    
    /**
     * @brief Get seconds since last successful decode
     */
    int32_t get_seconds_since_sync() const;

private:
    uint8_t data_pin_;
    bool invert_signal_;
    
    // Current frame being decoded
    std::array<DCF77Bit, 59> current_frame_;
    uint8_t current_bit_index_{0};
    
    // Last successfully decoded frame
    DCF77Frame last_frame_;
    std::chrono::steady_clock::time_point last_sync_time_;
    
    // Signal timing
    std::chrono::steady_clock::time_point pulse_start_time_;
    std::chrono::steady_clock::time_point last_edge_time_;
    bool signal_high_{false};
    
    // Statistics
    DCF77Statistics statistics_;
    
    /**
     * @brief Process pulse edge (rising or falling)
     * 
     * Called by update() when signal edge is detected.
     */
    void process_edge(bool rising_edge);
    
    /**
     * @brief Decode pulse width to bit value
     * 
     * @param pulse_width_ms Pulse width in milliseconds
     * @return DCF77Bit value (ZERO, ONE, or INVALID)
     */
    DCF77Bit decode_pulse_width(uint32_t pulse_width_ms) const;
    
    /**
     * @brief Decode complete 59-bit frame to date/time
     * 
     * @param[out] frame Decoded frame with timestamp
     * @return true if frame valid (parity checks passed)
     */
    bool decode_frame(DCF77Frame& frame);
    
    /**
     * @brief Extract BCD value from bit array
     * 
     * @param start_bit Starting bit index
     * @param num_bits Number of bits (4 or 8)
     * @return Decimal value
     */
    uint8_t extract_bcd(uint8_t start_bit, uint8_t num_bits) const;
    
    /**
     * @brief Check parity bit
     * 
     * @param start_bit Starting bit index
     * @param end_bit Ending bit index (inclusive)
     * @param parity_bit Parity bit index
     * @return true if parity correct
     */
    bool check_parity(uint8_t start_bit, uint8_t end_bit, uint8_t parity_bit) const;
    
    /**
     * @brief Compute signal strength (0-100%)
     * 
     * Based on pulse regularity and decode success rate.
     */
    uint8_t compute_signal_strength() const;
    
    /**
     * @brief Convert DCF77 signal quality to IEEE 1588-2019 clockClass
     * 
     * Mapping based on IEEE 1588-2019 Table 5:
     * - Strong signal, recent sync: clockClass 6
     * - Weak signal: clockClass 52
     * - No sync > 1 hour: clockClass 187
     * - No sync > 24 hours: clockClass 248
     */
    uint8_t signal_to_clock_class() const;
};

} // namespace DCF77
} // namespace Examples

#endif // DCF77_ADAPTER_HPP
