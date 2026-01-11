/**
 * @file linux_ptp_hal.hpp
 * @brief Linux Hardware Abstraction Layer for IEEE 1588-2019 PTP
 * @details Implements hardware timestamping and PHC operations for Linux
 * 
 * Hardware Support:
 *   - Intel i226 NIC (hardware timestamping)
 *   - Linux PTP Hardware Clock (PHC)
 *   - SO_TIMESTAMPING socket option
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#pragma once

#include <cstdint>
#include <string>
#include <time.h>
#include <sys/socket.h>
#include <linux/net_tstamp.h>
#include <linux/ptp_clock.h>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {

/**
 * @brief Hardware timestamp from Linux kernel
 */
struct HardwareTimestamp {
    uint64_t seconds;       ///< Seconds since epoch
    uint32_t nanoseconds;   ///< Nanoseconds within second
    int32_t  type;          ///< Timestamp type (SOF_TIMESTAMPING_*)
};

/**
 * @brief Linux PTP Hardware Abstraction Layer
 * @details Provides interface to Linux PTP stack with hardware timestamping
 */
class LinuxPtpHal {
public:
    /**
     * @brief Construct HAL for specific network interface
     * @param interface Network interface name (e.g., "eth1")
     * @param phc_device PHC device path (e.g., "/dev/ptp0")
     */
    LinuxPtpHal(const std::string& interface, const std::string& phc_device);
    
    /**
     * @brief Destructor - cleanup sockets and resources
     */
    ~LinuxPtpHal();
    
    // Socket Operations
    
    /**
     * @brief Initialize PTP sockets with hardware timestamping
     * @return true on success, false on failure
     */
    bool initialize_sockets();
    
    /**
     * @brief Get MAC address of the network interface
     * @param mac_address Buffer to store 6-byte MAC address
     * @return true if successful
     */
    bool get_interface_mac(uint8_t mac_address[6]);
    
    /**
     * @brief Send PTP message with hardware TX timestamp
     * @param data Message buffer
     * @param length Message length in bytes
     * @param tx_timestamp Output: TX timestamp from hardware
     * @return Number of bytes sent, or -1 on error
     */
    int send_message(const void* data, size_t length, HardwareTimestamp* tx_timestamp);
    
    /**
     * @brief Receive PTP message with hardware RX timestamp
     * @param buffer Receive buffer
     * @param buffer_size Buffer size in bytes
     * @param rx_timestamp Output: RX timestamp from hardware
     * @return Number of bytes received, or -1 on error
     */
    int receive_message(void* buffer, size_t buffer_size, HardwareTimestamp* rx_timestamp);
    
    // PHC Operations
    
    /**
     * @brief Get current PHC time
     * @param seconds Output: Seconds
     * @param nanoseconds Output: Nanoseconds
     * @return true on success, false on failure
     */
    bool get_phc_time(uint64_t* seconds, uint32_t* nanoseconds);
    
    /**
     * @brief Set PHC time
     * @param seconds Seconds to set
     * @param nanoseconds Nanoseconds to set
     * @return true on success, false on failure
     */
    bool set_phc_time(uint64_t seconds, uint32_t nanoseconds);
    
    /**
     * @brief Get PHC frequency adjustment
     * @param ppb Output: Current frequency adjustment in parts-per-billion
     * @return true on success, false on failure
     */
    bool get_phc_frequency(int32_t* ppb);
    
    /**
     * @brief Adjust PHC frequency
     * @param ppb Parts-per-billion frequency adjustment (positive = faster)
     * @return true on success, false on failure
     */
    bool adjust_phc_frequency(int32_t ppb);
    
    /**
     * @brief Get PHC capabilities
     * @param caps Output: PHC capabilities structure
     * @return true on success, false on failure
     */
    bool get_phc_capabilities(struct ptp_clock_caps* caps);
    
    // Interface Information
    
    /**
     * @brief Get network interface name
     * @return Interface name (e.g., "eth1")
     */
    const std::string& get_interface_name() const { return interface_name_; }
    
    /**
     * @brief Get PHC device path
     * @return PHC device path (e.g., "/dev/ptp0")
     */
    const std::string& get_phc_device() const { return phc_device_; }
    
    /**
     * @brief Check if hardware timestamping is enabled
     * @return true if HW timestamping active, false otherwise
     */
    bool is_hw_timestamping_enabled() const { return hw_timestamping_enabled_; }

private:
    std::string interface_name_;     ///< Network interface name
    std::string phc_device_;         ///< PHC device path
    
    int event_socket_;               ///< PTP event socket (port 319)
    int general_socket_;             ///< PTP general socket (port 320)
    int phc_fd_;                     ///< PHC device file descriptor
    
    bool hw_timestamping_enabled_;   ///< Hardware timestamping status
    HardwareTimestamp last_tx_timestamp_; ///< Last TX timestamp
    HardwareTimestamp last_rx_timestamp_; ///< Last RX timestamp
    
    // Private helper methods
    bool enable_hardware_timestamping(int sockfd);
    bool join_ptp_multicast(int sockfd);
    bool join_multicast(int socket_fd, const char* multicast_addr);
    bool get_tx_timestamp(int socket_fd, HardwareTimestamp* timestamp);
    bool adjust_phc_offset(int64_t offset_ns);
    int retrieve_tx_timestamp(int sockfd, HardwareTimestamp* timestamp);
    int extract_rx_timestamp(struct msghdr* msg, HardwareTimestamp* timestamp);
};

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
