/**
 * @file network_adapter.cpp
 * @brief Implementation of Network Adapter for IEEE 1588-2019 PTP
 * 
 * Extracted from LinuxPtpHal to create clean separation:
 *   - NetworkAdapter: Socket operations + timestamping
 *   - PhcAdapter: Clock operations only
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#include "network_adapter.hpp"
#include <cstring>
#include <cerrno>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {

// PTP Multicast addresses (IEEE 1588-2019)
static const char* PTP_EVENT_MULTICAST = "224.0.1.129";      // Primary domain
static const char* PTP_GENERAL_MULTICAST = "224.0.1.130";    // General messages
static const uint16_t PTP_EVENT_PORT = 319;
static const uint16_t PTP_GENERAL_PORT = 320;

NetworkAdapter::NetworkAdapter(const std::string& interface_name)
    : interface_name_(interface_name)
    , event_socket_(-1)
    , general_socket_(-1)
    , hw_timestamping_enabled_(false)
{
}

NetworkAdapter::~NetworkAdapter()
{
    if (event_socket_ >= 0) {
        close(event_socket_);
    }
    if (general_socket_ >= 0) {
        close(general_socket_);
    }
}

bool NetworkAdapter::initialize()
{
    // Create event socket (UDP port 319)
    event_socket_ = create_ptp_socket(PTP_EVENT_PORT);
    if (event_socket_ < 0) {
        std::cerr << "[NetworkAdapter] Failed to create event socket\n";
        return false;
    }

    // Create general socket (UDP port 320)
    general_socket_ = create_ptp_socket(PTP_GENERAL_PORT);
    if (general_socket_ < 0) {
        std::cerr << "[NetworkAdapter] Failed to create general socket\n";
        close(event_socket_);
        event_socket_ = -1;
        return false;
    }

    // Enable hardware timestamping on event socket
    if (!enable_hardware_timestamping(event_socket_)) {
        std::cerr << "[NetworkAdapter] Failed to enable HW timestamping on event socket\n";
        return false;
    }

    // Join PTP multicast groups
    if (!join_ptp_multicast(event_socket_)) {
        std::cerr << "[NetworkAdapter] Failed to join event multicast group\n";
        return false;
    }
    if (!join_ptp_multicast(general_socket_)) {
        std::cerr << "[NetworkAdapter] Failed to join general multicast group\n";
        return false;
    }

    hw_timestamping_enabled_ = true;
    std::cout << "[NetworkAdapter] Initialized on " << interface_name_ 
              << " (HW timestamping enabled)\n";
    
    return true;
}

int NetworkAdapter::create_ptp_socket(uint16_t port)
{
    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        return -1;
    }

    // Allow address reuse
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(sockfd);
        return -1;
    }

    // Bind to port
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

bool NetworkAdapter::enable_hardware_timestamping(int sockfd)
{
    // Configure hardware timestamping via SIOCSHWTSTAMP ioctl
    struct hwtstamp_config ts_config{};
    ts_config.tx_type = HWTSTAMP_TX_ON;
    ts_config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;

    struct ifreq ifr{};
    strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    ifr.ifr_data = reinterpret_cast<char*>(&ts_config);

    if (ioctl(sockfd, SIOCSHWTSTAMP, &ifr) < 0) {
        // Note: Some interfaces may already have HW timestamping enabled
        // Continue anyway, but timestamping may not be available
        return false;
    }

    // Enable SO_TIMESTAMPING socket option
    int flags = SOF_TIMESTAMPING_TX_HARDWARE |
                SOF_TIMESTAMPING_RX_HARDWARE |
                SOF_TIMESTAMPING_RAW_HARDWARE;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof(flags)) < 0) {
        return false;
    }

    return true;
}

bool NetworkAdapter::join_ptp_multicast(int sockfd)
{
    // Join both event and general multicast groups
    const char* multicast_addrs[] = {PTP_EVENT_MULTICAST, PTP_GENERAL_MULTICAST};
    
    for (const char* addr : multicast_addrs) {
        struct ip_mreqn mreq{};
        
        // Get interface index
        struct ifreq ifr{};
        strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
        if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
            return false;
        }
        
        mreq.imr_ifindex = ifr.ifr_ifindex;
        inet_pton(AF_INET, addr, &mreq.imr_multiaddr);
        mreq.imr_address.s_addr = INADDR_ANY;

        if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            // May already be a member - not necessarily an error
            continue;
        }
    }

    return true;
}

int NetworkAdapter::send_packet(const void* packet, size_t length,
                                NetworkTimestamp* tx_timestamp,
                                bool use_event_socket)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    int sockfd = use_event_socket ? event_socket_ : general_socket_;
    const char* multicast_addr = use_event_socket ? PTP_EVENT_MULTICAST : PTP_GENERAL_MULTICAST;
    uint16_t port = use_event_socket ? PTP_EVENT_PORT : PTP_GENERAL_PORT;
    
    // Setup destination address
    struct sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, multicast_addr, &dest_addr.sin_addr);

    // Send packet
    ssize_t sent = sendto(sockfd, packet, length, 0,
                         (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent < 0) {
        return -1;
    }

    // Retrieve TX timestamp if requested (asynchronous - from error queue)
    if (tx_timestamp && hw_timestamping_enabled_) {
        // TX timestamp retrieval is asynchronous - may not be immediately available
        // Caller should call get_tx_timestamp() separately if immediate timestamp needed
        tx_timestamp->seconds = 0;
        tx_timestamp->nanoseconds = 0;
        tx_timestamp->type = 0;
    }

    return static_cast<int>(sent);
}

int NetworkAdapter::receive_packet(void* buffer, size_t buffer_size,
                                   NetworkTimestamp* rx_timestamp,
                                   size_t* received_length,
                                   bool use_event_socket)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    int sockfd = use_event_socket ? event_socket_ : general_socket_;
    
    char control_buf[256];
    struct msghdr msg{};
    struct iovec iov{};
    struct sockaddr_in src_addr{};

    iov.iov_base = buffer;
    iov.iov_len = buffer_size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);
    msg.msg_name = &src_addr;
    msg.msg_namelen = sizeof(src_addr);

    // Receive message
    ssize_t len = recvmsg(sockfd, &msg, 0);
    if (len < 0) {
        return -1;
    }

    if (received_length) {
        *received_length = static_cast<size_t>(len);
    }

    // Extract RX timestamp from ancillary data
    if (rx_timestamp) {
        if (!extract_rx_timestamp(&msg, rx_timestamp)) {
            // No hardware timestamp available - zero it out
            rx_timestamp->seconds = 0;
            rx_timestamp->nanoseconds = 0;
            rx_timestamp->type = 0;
        }
    }

    return 0; // Success
}

bool NetworkAdapter::extract_rx_timestamp(struct msghdr* msg, NetworkTimestamp* timestamp)
{
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(msg); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(msg, cmsg)) {
        
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
            struct timespec* ts_array = reinterpret_cast<struct timespec*>(CMSG_DATA(cmsg));
            
            // Hardware timestamp is at index 2 (software at 0, deprecated at 1, hardware at 2)
            timestamp->seconds = ts_array[2].tv_sec;
            timestamp->nanoseconds = ts_array[2].tv_nsec;
            timestamp->type = SOF_TIMESTAMPING_RX_HARDWARE;
            return true;
        }
    }

    return false;
}

bool NetworkAdapter::get_tx_timestamp(NetworkTimestamp* tx_timestamp, uint32_t timeout_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Try to retrieve from event socket error queue
    return retrieve_tx_timestamp(event_socket_, tx_timestamp, timeout_ms);
}

bool NetworkAdapter::retrieve_tx_timestamp(int sockfd, NetworkTimestamp* timestamp, uint32_t timeout_ms)
{
    // Set timeout on socket if requested
    if (timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    
    char control_buf[256];
    struct msghdr msg{};
    struct iovec iov{};
    char dummy_buf[1];

    iov.iov_base = dummy_buf;
    iov.iov_len = sizeof(dummy_buf);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);

    // Receive from error queue (MSG_ERRQUEUE)
    ssize_t len = recvmsg(sockfd, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No timestamp available yet
            return false;
        }
        return false;
    }

    // Extract timestamp from control message
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; 
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
            struct timespec* ts_array = reinterpret_cast<struct timespec*>(CMSG_DATA(cmsg));
            
            // Hardware timestamp is at index 2
            timestamp->seconds = ts_array[2].tv_sec;
            timestamp->nanoseconds = ts_array[2].tv_nsec;
            timestamp->type = SOF_TIMESTAMPING_TX_HARDWARE;
            return true;
        }
    }

    return false;
}

bool NetworkAdapter::get_mac_address(uint8_t mac_address[6])
{
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        close(sock);
        return false;
    }
    
    std::memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
    close(sock);
    return true;
}

bool NetworkAdapter::join_multicast(const char* multicast_addr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Join on both sockets
    struct ip_mreqn mreq{};
    
    // Get interface index
    struct ifreq ifr{};
    strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(event_socket_, SIOCGIFINDEX, &ifr) < 0) {
        return false;
    }
    
    mreq.imr_ifindex = ifr.ifr_ifindex;
    inet_pton(AF_INET, multicast_addr, &mreq.imr_multiaddr);
    mreq.imr_address.s_addr = INADDR_ANY;

    // Join on event socket
    setsockopt(event_socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    
    // Join on general socket
    setsockopt(general_socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    return true;
}

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
