/**
 * @file network_adapter.hpp
 * @brief Network Adapter for IEEE 1588-2019 PTP
 * @details Implements repository's NetworkInterface HAL for Linux
 * 
 * Aligns with:
 *   - IEEE 1588-2019 Hardware Abstraction Layer design
 *   - Repository: 04-design/components/ieee-1588-2019-hal-interface-design.md
 * 
 * Responsibilities:
 *   - PTP socket creation and management (event/general)
 *   - Hardware timestamping (SO_TIMESTAMPING)
 *   - Multicast group membership
 *   - Packet transmission with TX timestamps
 *   - Packet reception with RX timestamps
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#pragma once

#include <cstdint>
#include <string>
#include <mutex>

// Forward declaration for msghdr
struct msghdr;

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {

/**
 * @brief Hardware timestamp structure
 * @details Compatible with Linux SO_TIMESTAMPING
 */
struct NetworkTimestamp {
    uint64_t seconds;      ///< Seconds since epoch
    uint32_t nanoseconds;  ///< Nanoseconds (0-999999999)
    uint32_t type;         ///< Timestamp type (SOF_TIMESTAMPING_*)
    
    NetworkTimestamp() : seconds(0), nanoseconds(0), type(0) {}
};

/**
 * @brief Network Adapter for PTP Communication
 * @details Implements IEEE 1588-2019 HAL NetworkInterface
 * 
 * Features:
 *   - Dual socket architecture (event port 319, general port 320)
 *   - Hardware timestamping via SO_TIMESTAMPING
 *   - PTP multicast group membership (01:1B:19:00:00:00)
 *   - TX timestamp retrieval from error queue
 *   - RX timestamp extraction from ancillary data
 * 
 * Hardware Support:
 *   - Intel i226/i225/i210 NICs
 *   - Any Linux network interface with PTP support
 */
class NetworkAdapter {
public:
    /**
     * @brief Construct network adapter for specific interface
     * @param interface_name Network interface (e.g., "eth1")
     */
    explicit NetworkAdapter(const std::string& interface_name);
    
    /**
     * @brief Destructor - cleanup sockets
     */
    ~NetworkAdapter();
    
    // Initialization
    
    /**
     * @brief Initialize PTP sockets with hardware timestamping
     * @return true on success, false on failure
     * 
     * Actions:
     *   - Creates event socket (UDP port 319)
     *   - Creates general socket (UDP port 320)
     *   - Enables SO_TIMESTAMPING on both sockets
     *   - Joins PTP multicast groups
     */
    bool initialize();
    
    // Network Operations (IEEE 1588-2019 HAL Interface)
    
    /**
     * @brief Send packet with hardware TX timestamp
     * @param packet Packet data buffer
     * @param length Packet length in bytes
     * @param tx_timestamp Output: TX timestamp (if available)
     * @param use_event_socket true for event socket (319), false for general (320)
     * @return Number of bytes sent, or -1 on error
     * 
     * @note TX timestamp retrieval is asynchronous
     * @see get_tx_timestamp() to retrieve pending timestamp
     */
    int send_packet(const void* packet, size_t length, 
                   NetworkTimestamp* tx_timestamp,
                   bool use_event_socket = true);
    
    /**
     * @brief Receive packet with hardware RX timestamp
     * @param buffer Receive buffer
     * @param buffer_size Buffer size in bytes
     * @param rx_timestamp Output: RX timestamp (if available)
     * @param received_length Output: Actual bytes received
     * @param use_event_socket true for event socket (319), false for general (320)
     * @return 0 on success, -1 on error
     */
    int receive_packet(void* buffer, size_t buffer_size,
                      NetworkTimestamp* rx_timestamp,
                      size_t* received_length,
                      bool use_event_socket = true);
    
    /**
     * @brief Get pending TX timestamp from error queue
     * @param tx_timestamp Output: Retrieved TX timestamp
     * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
     * @return true if timestamp retrieved, false if unavailable
     */
    bool get_tx_timestamp(NetworkTimestamp* tx_timestamp, uint32_t timeout_ms = 0);
    
    // Capabilities
    
    /**
     * @brief Check if hardware timestamping is supported
     * @return true if HW timestamping enabled
     */
    bool supports_hardware_timestamping() const { return hw_timestamping_enabled_; }
    
    /**
     * @brief Get timestamp precision in nanoseconds
     * @return Timestamp precision (typically 8ns for Intel NICs)
     */
    uint32_t get_timestamp_precision_ns() const { return 8; /* Intel i226 spec */ }
    
    // Configuration
    
    /**
     * @brief Get interface MAC address
     * @param mac_address Output: 6-byte MAC address
     * @return true on success, false on failure
     */
    bool get_mac_address(uint8_t mac_address[6]);
    
    /**
     * @brief Join multicast group
     * @param multicast_addr Multicast IP address (e.g., "224.0.1.129")
     * @return true on success, false on failure
     */
    bool join_multicast(const char* multicast_addr);
    
    /**
     * @brief Get network interface name
     * @return Interface name (e.g., "eth1")
     */
    const std::string& get_interface_name() const { return interface_name_; }
    
    /**
     * @brief Get event socket file descriptor
     * @return Event socket FD (for select/poll)
     */
    int get_event_socket() const { return event_socket_; }
    
    /**
     * @brief Get general socket file descriptor
     * @return General socket FD (for select/poll)
     */
    int get_general_socket() const { return general_socket_; }

private:
    std::string interface_name_;     ///< Network interface name
    
    int event_socket_;               ///< PTP event socket (port 319)
    int general_socket_;             ///< PTP general socket (port 320)
    
    bool hw_timestamping_enabled_;   ///< Hardware timestamping status
    
    mutable std::mutex mutex_;       ///< Thread safety
    
    // Private helpers
    
    /**
     * @brief Create and bind PTP socket
     * @param port UDP port number (319 or 320)
     * @return Socket file descriptor, or -1 on error
     */
    int create_ptp_socket(uint16_t port);
    
    /**
     * @brief Enable hardware timestamping on socket
     * @param sockfd Socket file descriptor
     * @return true on success, false on failure
     */
    bool enable_hardware_timestamping(int sockfd);
    
    /**
     * @brief Join PTP multicast groups on socket
     * @param sockfd Socket file descriptor
     * @return true on success, false on failure
     */
    bool join_ptp_multicast(int sockfd);
    
    /**
     * @brief Extract RX timestamp from message ancillary data
     * @param msg Message header with control data
     * @param timestamp Output: Extracted timestamp
     * @return true if timestamp found, false otherwise
     */
    bool extract_rx_timestamp(struct msghdr* msg, NetworkTimestamp* timestamp);
    
    /**
     * @brief Retrieve TX timestamp from socket error queue
     * @param sockfd Socket file descriptor
     * @param timestamp Output: Retrieved timestamp
     * @param timeout_ms Timeout in milliseconds
     * @return true if timestamp retrieved, false if unavailable
     */
    bool retrieve_tx_timestamp(int sockfd, NetworkTimestamp* timestamp, uint32_t timeout_ms);
};

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
