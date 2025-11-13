/**
 * @file dcf77_adapter.cpp
 * @brief DCF77 Terrestrial Radio Time Source Adapter Implementation
 */

#include "dcf77_adapter.hpp"
#include <cstring>
#include <algorithm>

// Platform-specific GPIO access
// NOTE: This is a simplified example - real implementation would use
// ESP32 GPIO HAL, Arduino digitalRead(), or platform-specific APIs
namespace GPIO {
    void pinMode(uint8_t pin, uint8_t mode) {
        // Platform-specific implementation
        // ESP32: gpio_set_direction()
        // Arduino: pinMode(pin, INPUT_PULLUP)
    }
    
    bool digitalRead(uint8_t pin) {
        // Platform-specific implementation
        // ESP32: gpio_get_level()
        // Arduino: digitalRead()
        return false;  // Placeholder
    }
}

namespace Examples {
namespace DCF77 {

// Namespace alias for IEEE 1588-2019 Types (inside our namespace)
namespace Types = IEEE::_1588::PTP::_2019::Types;

// DCF77 timing constants (milliseconds)
constexpr uint32_t PULSE_SHORT_MIN = 50;   // 100ms pulse minimum
constexpr uint32_t PULSE_SHORT_MAX = 150;  // 100ms pulse maximum
constexpr uint32_t PULSE_LONG_MIN = 150;   // 200ms pulse minimum
constexpr uint32_t PULSE_LONG_MAX = 250;   // 200ms pulse maximum
constexpr uint32_t MINUTE_MARK_MIN = 1500; // No pulse for ~2 seconds
constexpr uint32_t SYNC_TIMEOUT_S = 3600;  // 1 hour holdover

// DCF77 bit positions in frame
constexpr uint8_t BIT_MINUTE_START = 21;
constexpr uint8_t BIT_MINUTE_PARITY = 28;
constexpr uint8_t BIT_HOUR_START = 29;
constexpr uint8_t BIT_HOUR_PARITY = 35;
constexpr uint8_t BIT_DATE_START = 36;
constexpr uint8_t BIT_DATE_PARITY = 58;
constexpr uint8_t BIT_CET_CEST = 17;
constexpr uint8_t BIT_LEAP_SECOND = 19;

DCF77Adapter::DCF77Adapter(uint8_t data_pin, bool invert_signal)
    : data_pin_(data_pin)
    , invert_signal_(invert_signal)
    , current_bit_index_(0)
    , signal_high_(false)
{
    current_frame_.fill(DCF77Bit::INVALID);
}

bool DCF77Adapter::initialize() {
    // Configure GPIO pin as input with pull-up
    GPIO::pinMode(data_pin_, 0);  // INPUT mode
    
    // Initialize timing
    last_edge_time_ = std::chrono::steady_clock::now();
    pulse_start_time_ = last_edge_time_;
    
    return true;
}

bool DCF77Adapter::update() {
    // Read current signal state
    bool raw_signal = GPIO::digitalRead(data_pin_);
    bool signal = invert_signal_ ? !raw_signal : raw_signal;
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_edge = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_edge_time_).count();
    
    // Detect edge
    if (signal != signal_high_) {
        process_edge(signal);  // Rising edge if signal=true, falling if false
        signal_high_ = signal;
        last_edge_time_ = now;
    }
    
    // Check for minute mark (no pulse for ~2 seconds)
    if (!signal && time_since_edge > MINUTE_MARK_MIN) {
        // Minute mark detected - try to decode frame
        if (current_bit_index_ >= 58) {  // Need at least 58 bits
            if (decode_frame(last_frame_)) {
                last_sync_time_ = std::chrono::steady_clock::now();
                statistics_.frames_received++;
                current_bit_index_ = 0;
                current_frame_.fill(DCF77Bit::INVALID);
                return true;  // New frame decoded
            } else {
                statistics_.frames_failed++;
            }
        }
        
        // Reset for next frame
        current_bit_index_ = 0;
        current_frame_.fill(DCF77Bit::INVALID);
    }
    
    return false;
}

Types::ClockQuality DCF77Adapter::get_clock_quality() const {
    Types::ClockQuality quality;
    
    if (!is_synchronized()) {
        // Not synchronized - use default values
        quality.clock_class = 248;  // Default, not synchronized
        quality.clock_accuracy = 0xFE;  // Unknown
        quality.offset_scaled_log_variance = 0xFFFF;  // Max variance
        return quality;
    }
    
    // DCF77 is synchronized to PTB atomic clocks with ±1ms accuracy
    quality.clock_class = signal_to_clock_class();
    quality.clock_accuracy = 0x29;  // ±1ms (IEEE 1588-2019 Table 6)
    
    // Compute variance from signal quality
    uint8_t signal_strength = compute_signal_strength();
    if (signal_strength > 80) {
        quality.offset_scaled_log_variance = 0x4E20;  // Good signal
    } else if (signal_strength > 50) {
        quality.offset_scaled_log_variance = 0x6000;  // Moderate signal
    } else {
        quality.offset_scaled_log_variance = 0x8000;  // Weak signal
    }
    
    return quality;
}

bool DCF77Adapter::is_synchronized() const {
    if (!last_frame_.valid) {
        return false;
    }
    
    // Check if sync is recent enough
    auto now = std::chrono::steady_clock::now();
    auto time_since_sync = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_sync_time_).count();
    
    return time_since_sync < SYNC_TIMEOUT_S;
}

int32_t DCF77Adapter::get_seconds_since_sync() const {
    if (!last_frame_.valid) {
        return -1;  // Never synchronized
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_sync_time_);
    return static_cast<int32_t>(duration.count());
}

void DCF77Adapter::process_edge(bool rising_edge) {
    if (rising_edge) {
        // Rising edge - start of pulse
        pulse_start_time_ = std::chrono::steady_clock::now();
    } else {
        // Falling edge - end of pulse, measure width
        auto now = std::chrono::steady_clock::now();
        auto pulse_width = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - pulse_start_time_).count();
        
        // Decode pulse width to bit value
        DCF77Bit bit = decode_pulse_width(static_cast<uint32_t>(pulse_width));
        
        if (bit != DCF77Bit::INVALID && current_bit_index_ < 59) {
            current_frame_[current_bit_index_] = bit;
            current_bit_index_++;
        }
    }
}

DCF77Bit DCF77Adapter::decode_pulse_width(uint32_t pulse_width_ms) const {
    if (pulse_width_ms >= PULSE_SHORT_MIN && pulse_width_ms <= PULSE_SHORT_MAX) {
        return DCF77Bit::ZERO;  // 100ms pulse
    } else if (pulse_width_ms >= PULSE_LONG_MIN && pulse_width_ms <= PULSE_LONG_MAX) {
        return DCF77Bit::ONE;   // 200ms pulse
    } else {
        return DCF77Bit::INVALID;  // Invalid pulse width
    }
}

bool DCF77Adapter::decode_frame(DCF77Frame& frame) {
    // Check if we have enough bits
    if (current_bit_index_ < 58) {
        return false;
    }
    
    // Bit 0 must always be 0 (start marker)
    if (current_frame_[0] != DCF77Bit::ZERO) {
        frame.decode_errors++;
        return false;
    }
    
    // Check parity bits
    if (!check_parity(BIT_MINUTE_START, BIT_MINUTE_PARITY - 1, BIT_MINUTE_PARITY)) {
        frame.decode_errors++;
        return false;
    }
    if (!check_parity(BIT_HOUR_START, BIT_HOUR_PARITY - 1, BIT_HOUR_PARITY)) {
        frame.decode_errors++;
        return false;
    }
    if (!check_parity(BIT_DATE_START, BIT_DATE_PARITY - 1, BIT_DATE_PARITY)) {
        frame.decode_errors++;
        return false;
    }
    
    // Extract time components (BCD format)
    frame.minute = extract_bcd(BIT_MINUTE_START, 7);
    frame.hour = extract_bcd(BIT_HOUR_START, 6);
    frame.day = extract_bcd(36, 6);
    frame.weekday = extract_bcd(42, 3);
    frame.month = extract_bcd(45, 5);
    frame.year = extract_bcd(50, 8);
    
    // Extract status bits
    frame.cet = (current_frame_[BIT_CET_CEST] == DCF77Bit::ONE);
    frame.cest = (current_frame_[BIT_CET_CEST + 1] == DCF77Bit::ONE);
    frame.leap_second = (current_frame_[BIT_LEAP_SECOND] == DCF77Bit::ONE);
    
    // Validate ranges
    if (frame.minute > 59 || frame.hour > 23 || frame.day < 1 || frame.day > 31 ||
        frame.month < 1 || frame.month > 12) {
        frame.decode_errors++;
        return false;
    }
    
    // Convert to system time
    std::tm tm{};
    tm.tm_year = frame.year + 100;  // Years since 1900 (2000-2099)
    tm.tm_mon = frame.month - 1;    // 0-11
    tm.tm_mday = frame.day;
    tm.tm_hour = frame.hour;
    tm.tm_min = frame.minute;
    tm.tm_sec = 0;
    
    auto time_t_val = std::mktime(&tm);
    frame.timestamp = std::chrono::system_clock::from_time_t(time_t_val);
    
    frame.signal_strength = compute_signal_strength();
    frame.valid = true;
    
    return true;
}

uint8_t DCF77Adapter::extract_bcd(uint8_t start_bit, uint8_t num_bits) const {
    uint8_t value = 0;
    uint8_t weight = 1;
    
    for (uint8_t i = 0; i < num_bits; i++) {
        if (current_frame_[start_bit + i] == DCF77Bit::ONE) {
            value += weight;
        }
        
        // BCD weights: 1, 2, 4, 8, then 10, 20, 40, 80
        if ((i + 1) % 4 == 0) {
            weight = (i + 1) / 4 * 10;
        } else {
            weight *= 2;
        }
    }
    
    return value;
}

bool DCF77Adapter::check_parity(uint8_t start_bit, uint8_t end_bit, uint8_t parity_bit) const {
    uint8_t count = 0;
    
    for (uint8_t i = start_bit; i <= end_bit; i++) {
        if (current_frame_[i] == DCF77Bit::ONE) {
            count++;
        }
    }
    
    // Even parity: total number of 1s (including parity bit) should be even
    bool parity_is_one = (current_frame_[parity_bit] == DCF77Bit::ONE);
    return ((count + (parity_is_one ? 1 : 0)) % 2) == 0;
}

uint8_t DCF77Adapter::compute_signal_strength() const {
    // Compute signal strength from decode success rate
    uint32_t total_frames = statistics_.frames_received + statistics_.frames_failed;
    
    if (total_frames == 0) {
        return 0;
    }
    
    uint32_t success_rate = (statistics_.frames_received * 100) / total_frames;
    return std::min(static_cast<uint8_t>(success_rate), static_cast<uint8_t>(100));
}

uint8_t DCF77Adapter::signal_to_clock_class() const {
    int32_t seconds_since_sync = get_seconds_since_sync();
    
    if (seconds_since_sync < 0) {
        return 248;  // Never synchronized
    }
    
    uint8_t signal_strength = compute_signal_strength();
    
    // Map to IEEE 1588-2019 clockClass (Table 5)
    if (seconds_since_sync < 60 && signal_strength > 80) {
        return 6;  // Primary time source, excellent signal
    } else if (seconds_since_sync < 300 && signal_strength > 50) {
        return 13;  // Application-specific time source
    } else if (seconds_since_sync < 3600) {
        return 52;  // Degraded by symmetric path
    } else if (seconds_since_sync < 86400) {
        return 187;  // Degraded accuracy
    } else {
        return 248;  // Default, not synchronized
    }
}

bool DCF77Adapter::get_time(std::chrono::system_clock::time_point& time) const {
    if (!last_frame_.valid) {
        return false;
    }
    
    // Get time from last decoded frame
    time = last_frame_.timestamp;
    
    // Adjust for time elapsed since frame was received
    // (DCF77 frames arrive once per minute at the start of each minute)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - last_sync_time_;
    time = last_frame_.timestamp + std::chrono::duration_cast<std::chrono::system_clock::duration>(elapsed);
    
    return true;
}

bool DCF77Adapter::get_ptp_timestamp(uint64_t& seconds, uint32_t& nanoseconds) const {
    std::chrono::system_clock::time_point time;
    if (!get_time(time)) {
        return false;
    }
    
    // Convert to nanoseconds since Unix epoch
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
    
    // PTP epoch is same as Unix epoch for our purposes
    // Note: DCF77 broadcasts UTC+1 (CET) or UTC+2 (CEST)
    // You may need to adjust for timezone offset
    seconds = static_cast<uint64_t>(ns / 1000000000LL);
    nanoseconds = static_cast<uint32_t>(ns % 1000000000LL);
    
    return true;
}

int64_t DCF77Adapter::get_offset_ns() const {
    if (!last_frame_.valid) {
        return 0;
    }
    
    // Get DCF77 time
    std::chrono::system_clock::time_point dcf77_time;
    if (!get_time(dcf77_time)) {
        return 0;
    }
    
    // Get local system time
    auto local_time = std::chrono::system_clock::now();
    
    // Calculate offset (positive = local ahead of DCF77)
    auto offset = std::chrono::duration_cast<std::chrono::nanoseconds>(
        local_time - dcf77_time).count();
    
    return offset;
}

Types::Timestamp DCF77Adapter::get_current_time() const {
    // Get DCF77 time adjusted to current moment
    std::chrono::system_clock::time_point dcf77_time;
    if (!get_time(dcf77_time)) {
        // Return zero timestamp if not valid
        return Types::Timestamp{0, 0};
    }
    
    // Convert to nanoseconds since Unix epoch
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        dcf77_time.time_since_epoch()).count();
    
    // DCF77 broadcasts Central European Time (CET/CEST)
    // CET = UTC+1, CEST = UTC+2 (summer time)
    // Note: The last_frame_.dst_active flag indicates CEST
    
    // Adjust to UTC (PTP uses TAI which is close to UTC)
    // For simplicity, subtract 1 or 2 hours based on DST flag
    int64_t utc_offset_hours = last_frame_.dst_active ? 2 : 1;
    int64_t utc_offset_ns = utc_offset_hours * 3600LL * 1000000000LL;
    
    int64_t utc_ns = ns - utc_offset_ns;
    
    // PTP Timestamp: seconds + nanoseconds
    uint64_t seconds = static_cast<uint64_t>(utc_ns / 1000000000LL);
    uint32_t nanoseconds = static_cast<uint32_t>(utc_ns % 1000000000LL);
    
    return Types::Timestamp{seconds, nanoseconds};
}

} // namespace DCF77
} // namespace Examples
