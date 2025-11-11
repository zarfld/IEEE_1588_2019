/**
 * @file hal_template.hpp
 * @brief HAL Implementation Template - Interface Definitions
 * 
 * This file provides a complete template for implementing the Hardware
 * Abstraction Layer (HAL) required by the IEEE 1588-2019 PTP library.
 * 
 * Copy this file to your platform-specific directory and implement each
 * function according to your platform's capabilities.
 * 
 * INSTRUCTIONS:
 * 1. Copy this file: cp hal_template.hpp my_platform_hal.hpp
 * 2. Replace "Template" with your platform name (e.g., "Linux", "Windows", "FreeRTOS")
 * 3. Implement each function in corresponding .cpp file
 * 4. Test each function independently before integration
 * 5. Use platform-specific APIs (see comments for examples)
 * 
 * @copyright IEEE 1588-2019 PTP Library
 * @version 1.0.0
 * @date 2025-11-11
 */

#ifndef HAL_TEMPLATE_HPP
#define HAL_TEMPLATE_HPP

#include <cstdint>
#include <cstddef>

namespace PlatformHAL {

//============================================================================
// Network HAL - Network Communication Interface
//============================================================================

/**
 * @brief Network Hardware Abstraction Layer
 * 
 * Provides network packet send/receive functionality for PTP messages.
 * Must support either:
 * - Ethernet Layer 2 (Ethertype 0x88F7) - Recommended for local network
 * - UDP/IPv4 (ports 319/320) - For routed networks
 * 
 * KEY REQUIREMENTS:
 * - Must support multicast (PTP uses multicast addresses)
 * - Should support hardware timestamping if available
 * - Must be non-blocking or provide timeout mechanism
 * - Must handle packet loss gracefully
 */
class NetworkHAL {
public:
    /**
     * @brief Constructor - Initialize network interface
     * 
     * TODO: Implement constructor to:
     * - Open network socket (raw or UDP)
     * - Configure for multicast reception
     * - Enable hardware timestamping if available
     * - Set non-blocking mode or timeouts
     * 
     * PLATFORM EXAMPLES:
     * - Linux: socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))
     * - Windows: socket(AF_INET, SOCK_RAW, IPPROTO_UDP) with Winsock2
     * - FreeRTOS: lwIP socket() or raw API
     * - Bare-metal: Initialize Ethernet controller registers
     */
    NetworkHAL();
    
    /**
     * @brief Destructor - Clean up network resources
     * 
     * TODO: Implement destructor to:
     * - Close sockets
     * - Release buffers
     * - Disable hardware features
     */
    ~NetworkHAL();
    
    /**
     * @brief Send PTP packet over network
     * 
     * @param data Packet data (IEEE 1588-2019 format, already formed)
     * @param length Packet length in bytes
     * @return 0 on success, negative error code on failure
     * 
     * TODO: Implement using:
     * - Linux: sendto() with destination address
     * - Windows: sendto() with Winsock2
     * - FreeRTOS: lwIP sendto() or raw API
     * - Bare-metal: Write to TX buffer, trigger transmission
     * 
     * ERROR CODES:
     * - -1: Network error (ENETDOWN, EHOSTUNREACH)
     * - -2: Buffer full (would block)
     * - -3: Invalid parameters
     * 
     * PERFORMANCE NOTE: This function is called frequently.
     * Avoid dynamic allocation and minimize latency.
     */
    int send_packet(const std::uint8_t* data, std::size_t length);
    
    /**
     * @brief Receive PTP packet from network
     * 
     * @param buffer Destination buffer for received packet
     * @param length [in] Buffer size, [out] Received packet size
     * @param timestamp_ns [out] Hardware timestamp of reception (nanoseconds)
     * @return 0 on success, -1 on timeout, negative error code on failure
     * 
     * TODO: Implement using:
     * - Linux: recvmsg() with SO_TIMESTAMPING to get hardware timestamp
     * - Windows: recvfrom() + QueryPerformanceCounter() for software timestamp
     * - FreeRTOS: lwIP recvfrom() + hardware timer read
     * - Bare-metal: Check RX buffer, read timestamp register
     * 
     * TIMESTAMP EXTRACTION (Linux hardware timestamping):
     * ```c
     * struct msghdr msg;
     * struct cmsghdr *cmsg;
     * recvmsg(sock, &msg, 0);
     * for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
     *     if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
     *         struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
     *         *timestamp_ns = ts[2].tv_sec * 1000000000ULL + ts[2].tv_nsec;
     *     }
     * }
     * ```
     * 
     * ERROR CODES:
     * - 0: Packet received successfully
     * - -1: Timeout (no packet available)
     * - -2: Network error
     * - -3: Buffer too small
     */
    int receive_packet(std::uint8_t* buffer, std::size_t* length, std::uint64_t* timestamp_ns);
    
    /**
     * @brief Check if packet is available (non-blocking)
     * 
     * @return true if packet ready to receive, false otherwise
     * 
     * TODO: Implement using:
     * - Linux: select() or poll() with timeout=0
     * - Windows: select() with timeout=0
     * - FreeRTOS: Check lwIP queue or hardware RX flag
     * - Bare-metal: Read RX status register
     */
    bool has_packet();
    
private:
    // TODO: Add platform-specific members
    // Examples:
    // - int socket_fd;              // Linux/Windows socket descriptor
    // - void* lwip_pcb;            // FreeRTOS lwIP protocol control block
    // - volatile uint32_t* eth_regs; // Bare-metal register base
};

//============================================================================
// Timestamp HAL - High-Resolution Time Capture
//============================================================================

/**
 * @brief Timestamp Hardware Abstraction Layer
 * 
 * Provides high-resolution time measurement for PTP synchronization.
 * 
 * KEY REQUIREMENTS:
 * - Sub-microsecond resolution (ideally nanoseconds)
 * - Monotonic (never goes backwards)
 * - Low latency (<1μs to capture)
 * - Hardware timestamping preferred for best accuracy
 */
class TimestampHAL {
public:
    /**
     * @brief Constructor - Initialize timestamp hardware
     * 
     * TODO: Implement to:
     * - Initialize hardware timer if needed
     * - Configure for maximum resolution
     * - Calibrate if necessary
     */
    TimestampHAL();
    
    /**
     * @brief Destructor - Clean up timestamp resources
     */
    ~TimestampHAL();
    
    /**
     * @brief Get current time in nanoseconds
     * 
     * @return Current time in nanoseconds since epoch (Unix: Jan 1, 1970)
     * 
     * TODO: Implement using:
     * - Linux: clock_gettime(CLOCK_REALTIME) or CLOCK_TAI
     * - Windows: QueryPerformanceCounter() converted to nanoseconds
     * - FreeRTOS: xTaskGetTickCount() + high-resolution hardware timer
     * - Bare-metal: Read hardware timer counter, convert to nanoseconds
     * 
     * LINUX EXAMPLE:
     * ```c
     * struct timespec ts;
     * clock_gettime(CLOCK_REALTIME, &ts);
     * return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
     * ```
     * 
     * WINDOWS EXAMPLE:
     * ```c
     * LARGE_INTEGER frequency, counter;
     * QueryPerformanceFrequency(&frequency);
     * QueryPerformanceCounter(&counter);
     * return (counter.QuadPart * 1000000000ULL) / frequency.QuadPart;
     * ```
     * 
     * FREERTOS EXAMPLE:
     * ```c
     * uint32_t ticks = xTaskGetTickCount();
     * uint32_t timer_count = READ_HARDWARE_TIMER();
     * uint64_t coarse_ns = ticks * (1000000000ULL / configTICK_RATE_HZ);
     * uint64_t fine_ns = timer_count * TIMER_NS_PER_TICK;
     * return coarse_ns + fine_ns;
     * ```
     * 
     * PERFORMANCE CRITICAL: This is called frequently (every PTP message).
     * Optimize for minimum latency (<1μs).
     */
    std::uint64_t get_time_ns();
    
    /**
     * @brief Get timestamp resolution in nanoseconds
     * 
     * @return Resolution in nanoseconds (e.g., 1 for 1ns, 8 for 8ns)
     * 
     * TODO: Return actual hardware resolution
     * - 1ns: Hardware PTP clock
     * - 8ns: 125MHz oscillator
     * - 100ns: Typical Windows QPC
     * - 1000ns: 1MHz timer
     */
    std::uint32_t get_resolution_ns();
    
private:
    // TODO: Add platform-specific members
    // Examples:
    // - uint64_t frequency_hz;     // Timer frequency
    // - uint64_t epoch_offset_ns;  // Offset to Unix epoch
};

//============================================================================
// Clock HAL - Clock Discipline Interface
//============================================================================

/**
 * @brief Adjustment mode for clock discipline
 */
enum class AdjustMode {
    STEP,   ///< Immediate jump (for large offsets >128ms)
    SLEW    ///< Gradual adjustment (for small offsets <128ms)
};

/**
 * @brief Clock Hardware Abstraction Layer
 * 
 * Provides clock adjustment capabilities for PTP synchronization.
 * 
 * KEY REQUIREMENTS:
 * - Ability to adjust system time (requires elevated privileges)
 * - Support both step and slew adjustments
 * - Frequency adjustment for continuous discipline
 * - Sub-microsecond adjustment resolution preferred
 */
class ClockHAL {
public:
    /**
     * @brief Constructor - Initialize clock control
     * 
     * TODO: Implement to:
     * - Verify permission to adjust clock
     * - Initialize clock discipline state
     */
    ClockHAL();
    
    /**
     * @brief Destructor - Clean up clock control
     */
    ~ClockHAL();
    
    /**
     * @brief Adjust system clock by offset
     * 
     * @param offset_ns Offset in nanoseconds (positive = speed up, negative = slow down)
     * @param mode STEP for immediate jump, SLEW for gradual adjustment
     * @return 0 on success, negative error code on failure
     * 
     * TODO: Implement using:
     * - Linux: adjtimex() with ADJ_OFFSET_SINGLESHOT (step) or ADJ_OFFSET (slew)
     * - Windows: SetSystemTime() for step (limited to 1ms resolution)
     * - FreeRTOS: Software PLL adjusting timer reload values
     * - Bare-metal: Direct RTC register manipulation
     * 
     * LINUX EXAMPLE (STEP):
     * ```c
     * struct timex tx;
     * memset(&tx, 0, sizeof(tx));
     * tx.modes = ADJ_OFFSET_SINGLESHOT | ADJ_NANO;
     * tx.offset = offset_ns;
     * return adjtimex(&tx);
     * ```
     * 
     * LINUX EXAMPLE (SLEW):
     * ```c
     * struct timex tx;
     * memset(&tx, 0, sizeof(tx));
     * tx.modes = ADJ_OFFSET | ADJ_NANO;
     * tx.offset = offset_ns;
     * return adjtimex(&tx);  // Slew rate: ~0.5ms per second
     * ```
     * 
     * WINDOWS EXAMPLE (STEP only, 1ms resolution):
     * ```c
     * SYSTEMTIME st;
     * GetSystemTime(&st);
     * // Adjust st by offset_ns (complex conversion)
     * // Windows only supports millisecond resolution
     * return SetSystemTime(&st) ? 0 : -1;
     * ```
     * 
     * EMBEDDED EXAMPLE (RTC adjustment):
     * ```c
     * uint32_t rtc_value = READ_RTC_REGISTER();
     * rtc_value += (offset_ns / RTC_NS_PER_TICK);
     * WRITE_RTC_REGISTER(rtc_value);
     * ```
     * 
     * ERROR CODES:
     * - 0: Success
     * - -1: Permission denied (need root/CAP_SYS_TIME)
     * - -2: Invalid offset (too large)
     * - -3: Platform doesn't support clock adjustment
     */
    int adjust_clock(std::int64_t offset_ns, AdjustMode mode);
    
    /**
     * @brief Adjust clock frequency for continuous discipline
     * 
     * @param ppb Parts-per-billion adjustment (positive = speed up, negative = slow down)
     * @return 0 on success, negative error code on failure
     * 
     * TODO: Implement using:
     * - Linux: adjtimex() with ADJ_FREQUENCY
     * - Windows: Not supported (use software PLL)
     * - FreeRTOS: Adjust timer frequency
     * - Bare-metal: Adjust oscillator trim register
     * 
     * LINUX EXAMPLE:
     * ```c
     * struct timex tx;
     * memset(&tx, 0, sizeof(tx));
     * tx.modes = ADJ_FREQUENCY;
     * tx.freq = ppb * 65536 / 1000;  // Convert PPB to scaled PPM
     * return adjtimex(&tx);
     * ```
     * 
     * PPB RANGE: Typically ±500 PPB (±500 microseconds per second)
     */
    int adjust_frequency(std::int32_t ppb);
    
private:
    // TODO: Add platform-specific members
    // Examples:
    // - int32_t current_frequency_ppb;  // Current frequency adjustment
    // - bool slew_active;               // Is slew in progress?
};

//============================================================================
// Complete HAL System - Combines All HAL Components
//============================================================================

/**
 * @brief Complete HAL System combining all interfaces
 * 
 * Provides unified access to all HAL components.
 * Use this in your application code.
 */
class PlatformHALSystem {
public:
    /**
     * @brief Constructor - Create HAL system
     */
    PlatformHALSystem();
    
    /**
     * @brief Destructor - Clean up HAL system
     */
    ~PlatformHALSystem();
    
    /**
     * @brief Initialize all HAL components
     * 
     * @return 0 on success, negative error code on failure
     * 
     * TODO: Implement to initialize all HAL components in correct order:
     * 1. Timestamp HAL (needed for accurate time)
     * 2. Network HAL (may need timestamps)
     * 3. Clock HAL (requires time reference)
     */
    int initialize();
    
    /**
     * @brief Shutdown all HAL components
     * 
     * TODO: Implement to shutdown in reverse order
     */
    void shutdown();
    
    /**
     * @brief Get network HAL interface
     * @return Reference to network HAL
     */
    NetworkHAL& network() { return network_hal_; }
    
    /**
     * @brief Get timestamp HAL interface
     * @return Reference to timestamp HAL
     */
    TimestampHAL& timestamp() { return timestamp_hal_; }
    
    /**
     * @brief Get clock HAL interface
     * @return Reference to clock HAL
     */
    ClockHAL& clock() { return clock_hal_; }
    
private:
    NetworkHAL network_hal_;
    TimestampHAL timestamp_hal_;
    ClockHAL clock_hal_;
    bool initialized_;
};

} // namespace PlatformHAL

#endif // HAL_TEMPLATE_HPP
