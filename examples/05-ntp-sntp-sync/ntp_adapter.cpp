/**
 * @file ntp_adapter.cpp
 * @brief NTP/SNTP Time Source Adapter Implementation
 */

#include "ntp_adapter.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
#endif

namespace Examples {
namespace NTP {

// Namespace alias for IEEE 1588-2019 Types (inside our namespace)
namespace Types = IEEE::_1588::PTP::_2019::Types;

// NTP packet structure (RFC 4330)
#pragma pack(push, 1)
struct NTPPacket {
    uint8_t li_vn_mode;       // Leap Indicator (2), Version (3), Mode (3)
    uint8_t stratum;
    int8_t poll;
    int8_t precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t reference_id;
    uint64_t reference_timestamp;
    uint64_t originate_timestamp;
    uint64_t receive_timestamp;
    uint64_t transmit_timestamp;
};
#pragma pack(pop)

// NTP epoch offset (1900-01-01 to 1970-01-01)
constexpr uint64_t NTP_EPOCH_OFFSET = 2208988800ULL;

NTPAdapter::NTPAdapter(
    const std::string& server,
    uint16_t port,
    uint32_t poll_interval_s)
    : server_(server)
    , port_(port)
    , poll_interval_s_(poll_interval_s)
    , socket_fd_(-1)
{
}

bool NTPAdapter::initialize() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }
#endif
    
    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ < 0) {
        return false;
    }
    
    // Set socket timeout (5 seconds)
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
#ifdef _WIN32
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, 
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    
    return true;
}

bool NTPAdapter::update() {
    return query_ntp_server(last_query_result_);
}

Types::ClockQuality NTPAdapter::get_clock_quality() const {
    Types::ClockQuality quality;
    
    if (!last_query_result_.valid) {
        // Not synchronized - use default values
        quality.clock_class = 248;  // Default, not synchronized
        quality.clock_accuracy = 0xFE;  // Unknown
        quality.offset_scaled_log_variance = 0xFFFF;  // Max variance
        return quality;
    }
    
    // Convert NTP parameters to IEEE 1588-2019 clock quality
    quality.clock_class = stratum_to_clock_class(last_query_result_.stratum);
    quality.clock_accuracy = precision_to_clock_accuracy(
        last_query_result_.precision,
        last_query_result_.round_trip_ns);
    quality.offset_scaled_log_variance = compute_offset_scaled_log_variance(
        last_query_result_.round_trip_ns / 2);  // Estimate jitter as half RTT
    
    return quality;
}

int32_t NTPAdapter::get_seconds_since_sync() const {
    if (!last_query_result_.valid) {
        return -1;  // Never synchronized
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_query_time_);
    return static_cast<int32_t>(duration.count());
}

bool NTPAdapter::get_time(std::chrono::system_clock::time_point& time) const {
    if (!last_query_result_.valid) {
        return false;
    }
    
    // Get time from last NTP query
    time = last_query_result_.timestamp;
    
    // Adjust for time elapsed since query
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - last_query_time_;
    time += elapsed;
    
    return true;
}

bool NTPAdapter::get_ptp_timestamp(uint64_t& seconds, uint32_t& nanoseconds) const {
    std::chrono::system_clock::time_point time;
    if (!get_time(time)) {
        return false;
    }
    
    // Convert to nanoseconds since Unix epoch
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
    
    // PTP epoch is same as Unix epoch for our purposes
    // (In real implementation, you'd convert Unix → TAI by adding leap seconds)
    seconds = static_cast<uint64_t>(ns / 1000000000LL);
    nanoseconds = static_cast<uint32_t>(ns % 1000000000LL);
    
    return true;
}

bool NTPAdapter::query_ntp_server(NTPQueryResult& result) {
    if (socket_fd_ < 0) {
        return false;
    }
    
    // Resolve server address
    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    struct addrinfo* server_info = nullptr;
    if (getaddrinfo(server_.c_str(), std::to_string(port_).c_str(), 
                    &hints, &server_info) != 0) {
        return false;
    }
    
    // Build NTP request packet
    NTPPacket request{};
    request.li_vn_mode = 0x1B;  // LI=0, VN=3 (NTPv3), Mode=3 (client)
    
    // Record transmit time
    auto t1 = std::chrono::system_clock::now();
    
    // Send request
    ssize_t sent = sendto(socket_fd_, reinterpret_cast<const char*>(&request),
                          sizeof(request), 0, 
                          server_info->ai_addr, server_info->ai_addrlen);
    
    if (sent != sizeof(request)) {
        freeaddrinfo(server_info);
        return false;
    }
    
    // Receive response
    NTPPacket response{};
    struct sockaddr_in from{};
    socklen_t from_len = sizeof(from);
    
    ssize_t received = recvfrom(socket_fd_, reinterpret_cast<char*>(&response),
                                sizeof(response), 0,
                                reinterpret_cast<struct sockaddr*>(&from), &from_len);
    
    auto t4 = std::chrono::system_clock::now();
    
    freeaddrinfo(server_info);
    
    if (received != sizeof(response)) {
        return false;
    }
    
    // Parse NTP timestamps (network byte order to host byte order)
    auto ntoh64 = [](uint64_t net) -> uint64_t {
        return (static_cast<uint64_t>(ntohl(static_cast<uint32_t>(net >> 32))) << 32) |
               ntohl(static_cast<uint32_t>(net & 0xFFFFFFFF));
    };
    
    uint64_t t2_ntp = ntoh64(response.receive_timestamp);
    uint64_t t3_ntp = ntoh64(response.transmit_timestamp);
    
    // Convert to nanoseconds since Unix epoch
    auto ntp_to_ns = [](uint64_t ntp_ts) -> int64_t {
        uint32_t seconds = static_cast<uint32_t>(ntp_ts >> 32);
        uint32_t fraction = static_cast<uint32_t>(ntp_ts & 0xFFFFFFFF);
        
        // Convert to Unix epoch
        int64_t unix_seconds = static_cast<int64_t>(seconds) - NTP_EPOCH_OFFSET;
        int64_t nanoseconds = (static_cast<int64_t>(fraction) * 1000000000LL) >> 32;
        
        return unix_seconds * 1000000000LL + nanoseconds;
    };
    
    auto chrono_to_ns = [](const std::chrono::system_clock::time_point& tp) -> int64_t {
        auto duration = tp.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    };
    
    int64_t t1_ns = chrono_to_ns(t1);
    int64_t t2_ns = ntp_to_ns(t2_ntp);
    int64_t t3_ns = ntp_to_ns(t3_ntp);
    int64_t t4_ns = chrono_to_ns(t4);
    
    // Calculate offset and delay (RFC 4330)
    int64_t offset = ((t2_ns - t1_ns) + (t3_ns - t4_ns)) / 2;
    int64_t delay = (t4_ns - t1_ns) - (t3_ns - t2_ns);
    
    // Fill result
    result.valid = true;
    result.timestamp = t4;
    result.offset_ns = offset;
    result.round_trip_ns = delay;
    result.stratum = response.stratum;
    result.precision = response.precision;
    result.root_delay_ns = ntohl(response.root_delay) * 1000000000ULL / 65536;
    result.root_dispersion_ns = ntohl(response.root_dispersion) * 1000000000ULL / 65536;
    
    last_query_time_ = std::chrono::steady_clock::now();
    
    return true;
}

uint8_t NTPAdapter::stratum_to_clock_class(uint8_t stratum) const {
    // Map NTP stratum to IEEE 1588-2019 clockClass (Table 5)
    if (stratum == 0 || stratum >= 16) {
        return 248;  // Default, not synchronized
    } else if (stratum == 1) {
        return 6;  // Primary time source synchronized to external reference
    } else if (stratum == 2) {
        return 52;  // Degraded by symmetric path without boundary clocks
    } else if (stratum == 3) {
        return 58;  // Degraded by packet-based method (NTP)
    } else {
        return 187;  // Degraded accuracy for stratum > 3
    }
}

uint8_t NTPAdapter::precision_to_clock_accuracy(int8_t precision, int64_t round_trip_ns) const {
    // Convert NTP precision (log2 seconds) to nanoseconds
    double precision_s = std::pow(2.0, static_cast<double>(precision));
    int64_t precision_ns = static_cast<int64_t>(precision_s * 1e9);
    
    // Use worst case of precision and half of round-trip delay
    int64_t accuracy_ns = std::max(precision_ns, round_trip_ns / 2);
    
    // Map to IEEE 1588-2019 clockAccuracy (Table 6)
    if (accuracy_ns < 25) return 0x20;
    if (accuracy_ns < 100) return 0x21;
    if (accuracy_ns < 250) return 0x22;
    if (accuracy_ns < 1000) return 0x23;          // 1µs
    if (accuracy_ns < 2500) return 0x24;          // 2.5µs
    if (accuracy_ns < 10000) return 0x25;         // 10µs
    if (accuracy_ns < 25000) return 0x26;         // 25µs
    if (accuracy_ns < 100000) return 0x27;        // 100µs
    if (accuracy_ns < 250000) return 0x28;        // 250µs
    if (accuracy_ns < 1000000) return 0x29;       // 1ms
    if (accuracy_ns < 2500000) return 0x2A;       // 2.5ms
    if (accuracy_ns < 10000000) return 0x2B;      // 10ms
    if (accuracy_ns < 25000000) return 0x2C;      // 25ms
    if (accuracy_ns < 100000000) return 0x2D;     // 100ms
    if (accuracy_ns < 250000000) return 0x2E;     // 250ms
    if (accuracy_ns < 1000000000) return 0x2F;    // 1s
    if (accuracy_ns < 10000000000LL) return 0x30; // 10s
    return 0xFE;  // Unknown / > 10s
}

uint16_t NTPAdapter::compute_offset_scaled_log_variance(int64_t jitter_ns) const {
    // Convert jitter to variance in seconds^2
    double jitter_s = static_cast<double>(jitter_ns) / 1e9;
    double variance_s2 = jitter_s * jitter_s;
    
    // Compute scaled log variance: 16384 * log2(variance)
    if (variance_s2 <= 0) {
        return 0;  // Perfect stability (unrealistic)
    }
    
    double log_variance = std::log2(variance_s2);
    int32_t scaled = static_cast<int32_t>(16384.0 * log_variance);
    
    // Clamp to uint16 range
    if (scaled < 0) return 0;
    if (scaled > 0xFFFF) return 0xFFFF;
    
    return static_cast<uint16_t>(scaled);
}

} // namespace NTP
} // namespace Examples
