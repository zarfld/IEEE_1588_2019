/**
 * @file minimal_hal.hpp
 * @brief Minimal Hardware Abstraction Layer for PTP Slave Example
 * 
 * This header defines a minimal HAL interface for demonstrating PTP concepts.
 * In production, you would implement these functions using:
 * - Real network sockets (UDP/Ethernet)
 * - Hardware timestamping capabilities
 * - System clock adjustment APIs
 * 
 * @note This is a SIMPLIFIED interface for educational purposes.
 *       See integration-guide.md for production HAL implementation.
 * 
 * @copyright Based on IEEE 1588-2019 concepts
 * @version 1.0.0
 * @date 2025-11-11
 */

#ifndef EXAMPLES_MINIMAL_HAL_HPP
#define EXAMPLES_MINIMAL_HAL_HPP

#include <cstdint>
#include <cstddef>
#include <functional>

namespace Examples {
namespace MinimalHAL {

/**
 * @brief PTP message types (subset for this example)
 * 
 * Per IEEE 1588-2019 Section 13.3.2.2 Table 19
 */
enum class MessageType : std::uint8_t {
    SYNC = 0x0,
    DELAY_REQ = 0x1,
    FOLLOW_UP = 0x8,
    DELAY_RESP = 0x9,
    ANNOUNCE = 0xB
};

/**
 * @brief Simulated PTP message structure
 * 
 * Contains essential fields for demonstration.
 * Real implementation would parse full IEEE 1588-2019 message format.
 */
struct PTPMessage {
    MessageType message_type;
    std::uint64_t timestamp_seconds;     // 48-bit seconds (we use 64 for simplicity)
    std::uint32_t timestamp_nanoseconds; // 32-bit nanoseconds
    std::uint8_t clock_identity[8];      // 8-octet clock identity
    std::uint16_t port_number;           // Port number
    std::uint8_t priority1;              // Priority1 (for BMCA)
    std::uint8_t priority2;              // Priority2 (for BMCA)
    std::uint8_t clock_class;            // Clock class (for BMCA)
    std::uint8_t clock_accuracy;         // Clock accuracy
    std::uint16_t offset_scaled_log_variance; // Variance
};

/**
 * @brief Network HAL Interface
 * 
 * Provides simulated network send/receive for PTP messages.
 * In production: Use actual UDP/Ethernet sockets.
 */
class NetworkHAL {
public:
    NetworkHAL();
    ~NetworkHAL();

    /**
     * @brief Send PTP packet over network
     * 
     * @param data Packet data to send
     * @param length Length of packet in bytes
     * @return 0 on success, negative on error
     * 
     * @note Production: Would use sendto() or raw socket transmission
     */
    int send_packet(const std::uint8_t* data, std::size_t length);

    /**
     * @brief Receive PTP packet from network
     * 
     * @param buffer Buffer to store received packet
     * @param length [in/out] Buffer size on input, actual received size on output
     * @return 0 on success, negative on error
     * 
     * @note Production: Would use recvfrom() with hardware timestamps
     */
    int receive_packet(std::uint8_t* buffer, std::size_t* length);

    /**
     * @brief Simulate receiving a specific PTP message
     * 
     * For testing/demo purposes - preloads a message into receive queue
     * 
     * @param message Message to simulate receiving
     */
    void simulate_receive(const PTPMessage& message);

private:
    struct Impl;
    Impl* pImpl;
};

/**
 * @brief Timestamp HAL Interface
 * 
 * Provides time capture capabilities.
 * In production: Use hardware timestamping or high-precision system time.
 */
class TimestampHAL {
public:
    TimestampHAL();
    ~TimestampHAL();

    /**
     * @brief Get current timestamp in nanoseconds
     * 
     * @return Current time as 64-bit nanosecond count
     * 
     * @note Production: Would use:
     *       - Linux: clock_gettime(CLOCK_REALTIME, ...)
     *       - Windows: QueryPerformanceCounter()
     *       - Hardware: NIC timestamp register
     */
    std::uint64_t get_time_ns();

    /**
     * @brief Set simulated time (for testing)
     * 
     * @param time_ns Time in nanoseconds since epoch
     */
    void set_simulated_time(std::uint64_t time_ns);

private:
    std::uint64_t simulated_time_ns_;
};

/**
 * @brief Clock Adjustment HAL Interface
 * 
 * Provides local clock discipline capabilities.
 * In production: Use system time adjustment APIs.
 */
class ClockHAL {
public:
    ClockHAL();
    ~ClockHAL();

    /**
     * @brief Adjust local clock by given offset
     * 
     * @param offset_ns Offset in nanoseconds (positive = clock ahead, negative = clock behind)
     * @return 0 on success, negative on error
     * 
     * @note Production: Would use:
     *       - Linux: adjtimex(), clock_adjtime()
     *       - Windows: SetSystemTime(), SetSystemTimeAdjustment()
     *       - Embedded: Direct register manipulation
     */
    int adjust_clock(std::int64_t offset_ns);

    /**
     * @brief Get total accumulated adjustment
     * 
     * @return Total adjustment applied (for demonstration)
     */
    std::int64_t get_total_adjustment() const;

private:
    std::int64_t total_adjustment_ns_;
};

/**
 * @brief Complete minimal HAL combining all interfaces
 * 
 * Convenient wrapper for all HAL components
 */
class MinimalHALSystem {
public:
    MinimalHALSystem();
    ~MinimalHALSystem();

    // Access individual HAL components
    NetworkHAL& network() { return network_; }
    TimestampHAL& timestamp() { return timestamp_; }
    ClockHAL& clock() { return clock_; }

    /**
     * @brief Initialize HAL system
     * 
     * @return 0 on success, negative on error
     */
    int initialize();

    /**
     * @brief Shutdown HAL system
     */
    void shutdown();

private:
    NetworkHAL network_;
    TimestampHAL timestamp_;
    ClockHAL clock_;
    bool initialized_;
};

} // namespace MinimalHAL
} // namespace Examples

#endif // EXAMPLES_MINIMAL_HAL_HPP
