/**
 * @file linux_ptp_hal.cpp
 * @brief Implementation of Linux PTP Hardware Abstraction Layer
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#include "linux_ptp_hal.hpp"
#include <cstring>
#include <cerrno>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {

// PTP Multicast addresses
static const char* PTP_EVENT_MULTICAST = "224.0.1.129";
static const char* PTP_GENERAL_MULTICAST = "224.0.1.130";
static const uint16_t PTP_EVENT_PORT = 319;
static const uint16_t PTP_GENERAL_PORT = 320;

LinuxPtpHal::LinuxPtpHal(const std::string& interface, const std::string& phc_device)
    : interface_name_(interface)
    , phc_device_(phc_device)
    , event_socket_(-1)
    , general_socket_(-1)
    , phc_fd_(-1)
{
}

LinuxPtpHal::~LinuxPtpHal()
{
    if (event_socket_ >= 0) {
        close(event_socket_);
    }
    if (general_socket_ >= 0) {
        close(general_socket_);
    }
    if (phc_fd_ >= 0) {
        close(phc_fd_);
    }
}

bool LinuxPtpHal::get_interface_mac(uint8_t mac_address[6])
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

bool LinuxPtpHal::initialize_sockets()
{
    // Open PHC device
    phc_fd_ = open(phc_device_.c_str(), O_RDWR);
    if (phc_fd_ < 0) {
        // Error: Failed to open PHC device
        return false;
    }

    // Create event socket (UDP port 319)
    event_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (event_socket_ < 0) {
        return false;
    }

    // Create general socket (UDP port 320)
    general_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (general_socket_ < 0) {
        close(event_socket_);
        event_socket_ = -1;
        return false;
    }

    // Enable hardware timestamping on event socket
    if (!enable_hardware_timestamping(event_socket_)) {
        return false;
    }

    // Bind event socket to port 319
    struct sockaddr_in event_addr{};
    event_addr.sin_family = AF_INET;
    event_addr.sin_port = htons(PTP_EVENT_PORT);
    event_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(event_socket_, (struct sockaddr*)&event_addr, sizeof(event_addr)) < 0) {
        return false;
    }

    // Bind general socket to port 320
    struct sockaddr_in general_addr{};
    general_addr.sin_family = AF_INET;
    general_addr.sin_port = htons(PTP_GENERAL_PORT);
    general_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(general_socket_, (struct sockaddr*)&general_addr, sizeof(general_addr)) < 0) {
        return false;
    }

    // Join PTP multicast groups
    if (!join_multicast(event_socket_, PTP_EVENT_MULTICAST)) {
        return false;
    }
    if (!join_multicast(general_socket_, PTP_GENERAL_MULTICAST)) {
        return false;
    }

    return true;
}

bool LinuxPtpHal::enable_hardware_timestamping(int socket_fd)
{
    // Configure hardware timestamping via SIOCSHWTSTAMP ioctl
    struct hwtstamp_config ts_config{};
    ts_config.tx_type = HWTSTAMP_TX_ON;
    ts_config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;

    struct ifreq ifr{};
    strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    ifr.ifr_data = reinterpret_cast<char*>(&ts_config);

    if (ioctl(socket_fd, SIOCSHWTSTAMP, &ifr) < 0) {
        return false;
    }

    // Enable SO_TIMESTAMPING socket option
    int flags = SOF_TIMESTAMPING_TX_HARDWARE |
                SOF_TIMESTAMPING_RX_HARDWARE |
                SOF_TIMESTAMPING_RAW_HARDWARE;
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof(flags)) < 0) {
        return false;
    }

    return true;
}

bool LinuxPtpHal::join_multicast(int socket_fd, const char* multicast_addr)
{
    struct ip_mreqn mreq{};
    
    // Get interface index
    struct ifreq ifr{};
    strncpy(ifr.ifr_name, interface_name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        return false;
    }
    
    mreq.imr_ifindex = ifr.ifr_ifindex;
    inet_pton(AF_INET, multicast_addr, &mreq.imr_multiaddr);
    mreq.imr_address.s_addr = INADDR_ANY;

    if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return false;
    }

    return true;
}

int LinuxPtpHal::send_message(const void* data, size_t length, HardwareTimestamp* tx_timestamp)
{
    struct sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PTP_EVENT_PORT);
    inet_pton(AF_INET, PTP_EVENT_MULTICAST, &dest_addr.sin_addr);

    // Send message
    ssize_t sent = sendto(event_socket_, data, length, 0,
                         (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent < 0) {
        return -1;
    }

    // Retrieve TX timestamp from error queue
    if (tx_timestamp) {
        if (!get_tx_timestamp(event_socket_, tx_timestamp)) {
            return -1;
        }
    }

    return static_cast<int>(sent);
}

bool LinuxPtpHal::get_tx_timestamp(int socket_fd, HardwareTimestamp* timestamp)
{
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

    // Receive from error queue
    ssize_t len = recvmsg(socket_fd, &msg, MSG_ERRQUEUE);
    if (len < 0) {
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

int LinuxPtpHal::receive_message(void* buffer, size_t buffer_size, HardwareTimestamp* rx_timestamp)
{
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
    ssize_t len = recvmsg(event_socket_, &msg, 0);
    if (len < 0) {
        return -1;
    }

    // Extract RX timestamp
    if (rx_timestamp) {
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
                struct timespec* ts_array = reinterpret_cast<struct timespec*>(CMSG_DATA(cmsg));
                
                // Hardware timestamp is at index 2
                rx_timestamp->seconds = ts_array[2].tv_sec;
                rx_timestamp->nanoseconds = ts_array[2].tv_nsec;
                rx_timestamp->type = SOF_TIMESTAMPING_RX_HARDWARE;
                break;
            }
        }
    }

    return static_cast<int>(len);
}

bool LinuxPtpHal::get_phc_time(uint64_t* seconds, uint32_t* nanoseconds)
{
    // Convert PHC file descriptor to clockid (same as set_phc_time)
    clockid_t clkid = ((~(clockid_t)(phc_fd_) << 3) | 3);
    
    struct timespec ts{};
    if (clock_gettime(clkid, &ts) < 0) {
        return false;
    }

    *seconds = ts.tv_sec;
    *nanoseconds = ts.tv_nsec;
    return true;
}

bool LinuxPtpHal::set_phc_time(uint64_t seconds, uint32_t nanoseconds)
{
    // Use POSIX clock_settime() instead of PTP_CLOCK_SETTIME ioctl
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = nanoseconds;

    // Convert PHC file descriptor to clockid
    clockid_t clkid = ((~(clockid_t)(phc_fd_) << 3) | 3);

    if (clock_settime(clkid, &ts) < 0) {
        std::cerr << "[PHC ERROR] set_phc_time() FAILED: " << strerror(errno) 
                  << " (errno=" << errno << ", clkid=" << clkid << ", phc_fd=" << phc_fd_ << ")\n";
        return false;
    }

    // Verify the time was actually set
    struct timespec verify_ts;
    if (clock_gettime(clkid, &verify_ts) == 0) {
        int64_t set_ns = (int64_t)seconds * 1000000000LL + nanoseconds;
        int64_t verify_ns = (int64_t)verify_ts.tv_sec * 1000000000LL + verify_ts.tv_nsec;
        int64_t diff_ns = verify_ns - set_ns;
        if (std::abs(diff_ns) > 1000000) {  // >1ms difference
            std::cerr << "[PHC WARNING] Time set but verification shows " << (diff_ns / 1000000.0) 
                      << " ms difference!\n";
        }
    }

    return true;
}

bool LinuxPtpHal::adjust_phc_frequency(int32_t ppb)
{
    // Use POSIX clock_adjtime() instead of PTP_CLOCK_ADJTIME ioctl
    struct timex tx;
    memset(&tx, 0, sizeof(tx));

    tx.modes = ADJ_FREQUENCY;
    // Convert ppb to scaled PPM: freq in units of 2^-16 ppm
    tx.freq = (long)(((long long)ppb * 65536LL) / 1000LL);

    // Convert PHC file descriptor to clockid
    clockid_t clkid = ((~(clockid_t)(phc_fd_) << 3) | 3);

    if (clock_adjtime(clkid, &tx) < 0) {
        std::cerr << "[PHC ERROR] adjust_phc_frequency(" << ppb << " ppb) FAILED: " 
                  << strerror(errno) << " (errno=" << errno << ")\n";
        return false;
    }

    return true;
}

bool LinuxPtpHal::adjust_phc_offset(int64_t offset_ns)
{
    // Use POSIX clock_adjtime() instead of PTP_CLOCK_ADJTIME ioctl
    struct timex tx;
    memset(&tx, 0, sizeof(tx));

    // For small offsets use ADJ_OFFSET, for large ones use ADJ_SETOFFSET
    if (offset_ns < 500000000LL && offset_ns > -500000000LL) {
        // Small offset: slew the clock
        tx.modes = ADJ_OFFSET | ADJ_NANO;
        tx.offset = offset_ns;
    } else {
        // Large offset: step the clock immediately
        tx.modes = ADJ_SETOFFSET | ADJ_NANO;
        tx.time.tv_sec = offset_ns / 1000000000LL;
        tx.time.tv_usec = offset_ns % 1000000000LL;  // tv_usec used for nanoseconds when ADJ_NANO set
    }

    // Convert PHC file descriptor to clockid
    clockid_t clkid = ((~(clockid_t)(phc_fd_) << 3) | 3);

    if (clock_adjtime(clkid, &tx) < 0) {
        return false;
    }

    return true;
}

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
