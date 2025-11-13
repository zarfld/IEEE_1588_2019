/**
 * @file ntp_adapter.hpp
 * @brief NTP/SNTP Time Source Adapter for IEEE 1588-2019 PTP
 * 
 * Adapter that queries NTP servers and updates PTP clock quality using
 * the library's Types::ClockQuality and Types::TimeSource types.
 * 
 * This example demonstrates:
 * - Using Types::TimeSource::NTP (0x50) from the library
 * - Updating DefaultDataSet.clockQuality with library types
 * - Computing clock quality from NTP stratum and accuracy
 * 
 * IMPORTANT: This adapter USES the library's types, it does NOT duplicate them!
 * 
 * @see IEEE 1588-2019, Section 8.6.2.7 "timeSource"
 * @see IEEE 1588-2019, Table 6 "timeSource enumeration" (NTP = 0x50)
 * @see include/IEEE/1588/PTP/2019/types.hpp for Types::ClockQuality
 * @see include/clocks.hpp for DefaultDataSet and TimePropertiesDataSet
 */

#ifndef NTP_ADAPTER_HPP
#define NTP_ADAPTER_HPP

#include "IEEE/1588/PTP/2019/types.hpp"  // Use library's Types::ClockQuality, Types::TimeSource
#include <cstdint>
#include <string>
#include <chrono>

namespace Examples {
namespace NTP {

/**
 * @brief NTP Query Result
 */
struct NTPQueryResult {
    bool valid{false};
    std::chrono::system_clock::time_point timestamp;
    int64_t offset_ns{0};         ///< Offset from local clock (nanoseconds)
    int64_t round_trip_ns{0};     ///< Round-trip delay (nanoseconds)
    uint8_t stratum{16};          ///< NTP stratum (1-16)
    uint8_t precision{-10};       ///< Log2 of precision in seconds
    uint32_t root_delay_ns{0};    ///< Root delay (nanoseconds)
    uint32_t root_dispersion_ns{0}; ///< Root dispersion (nanoseconds)
};

/**
 * @brief NTP/SNTP Time Source Adapter
 * 
 * Queries NTP servers and computes IEEE 1588-2019 clock quality
 * using the library's Types::ClockQuality struct.
 * 
 * Example Usage:
 * @code
 * NTPAdapter ntp("pool.ntp.org");
 * ntp.initialize();
 * 
 * // Poll NTP server
 * if (ntp.update()) {
 *     // Get clock quality using LIBRARY's Types::ClockQuality
 *     Types::ClockQuality quality = ntp.get_clock_quality();
 *     
 *     // Update PTP clock
 *     auto& ds = ptp_clock.get_default_data_set();
 *     ds.clockQuality = quality;  // Use library type!
 *     
 *     auto& tp = ptp_clock.get_time_properties_data_set();
 *     tp.timeSource = static_cast<uint8_t>(Types::TimeSource::NTP);
 * }
 * @endcode
 */
class NTPAdapter {
public:
    /**
     * @brief Construct NTP adapter
     * 
     * @param server NTP server address (e.g., "pool.ntp.org", "time.google.com")
     * @param port NTP port (default 123)
     * @param poll_interval_s Polling interval (default 64 seconds)
     */
    explicit NTPAdapter(
        const std::string& server,
        uint16_t port = 123,
        uint32_t poll_interval_s = 64);
    
    ~NTPAdapter() = default;
    
    /**
     * @brief Initialize NTP client
     * 
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Query NTP server and update quality
     * 
     * Should be called periodically (e.g., every 64 seconds).
     * 
     * @return true if NTP query successful
     */
    bool update();
    
    /**
     * @brief Get clock quality using LIBRARY's Types::ClockQuality
     * 
     * Computes clock quality from NTP stratum and accuracy:
     * - Stratum 1: clockClass 6-13 (primary time source)
     * - Stratum 2: clockClass 52 (degraded by symmetric path)
     * - Stratum 3+: clockClass 187-193 (degraded)
     * - Stratum 16: clockClass 248 (unsynchronized)
     * 
     * @return Library's Types::ClockQuality struct (DO NOT RECREATE THIS TYPE!)
     */
    Types::ClockQuality get_clock_quality() const;
    
    /**
     * @brief Get time source type - always NTP from library enum
     * 
     * @return Types::TimeSource::NTP (0x50) from library
     */
    static constexpr Types::TimeSource get_time_source() {
        return Types::TimeSource::NTP;
    }
    
    /**
     * @brief Get NTP server address
     */
    std::string get_server() const { return server_; }
    
    /**
     * @brief Check if synchronized to NTP
     */
    bool is_synchronized() const { return last_query_result_.valid; }
    
    /**
     * @brief Get last NTP query result
     */
    const NTPQueryResult& get_last_result() const { return last_query_result_; }
    
    /**
     * @brief Get current time from NTP (system_clock::time_point)
     * 
     * Returns the most recent NTP time adjusted by local clock drift.
     * 
     * @param[out] time Current time from NTP reference
     * @return true if time is valid and recent
     */
    bool get_time(std::chrono::system_clock::time_point& time) const;
    
    /**
     * @brief Get current time as PTP Timestamp (TAI)
     * 
     * Converts NTP time to PTP Timestamp format for direct use with
     * IEEE 1588-2019 library.
     * 
     * @param[out] timestamp PTP timestamp (seconds + nanoseconds)
     * @return true if timestamp is valid
     */
    bool get_ptp_timestamp(uint64_t& seconds, uint32_t& nanoseconds) const;
    
    /**
     * @brief Get offset from local clock (nanoseconds)
     * 
     * Positive = local clock is ahead of NTP (subtract to correct)
     * Negative = local clock is behind NTP (add to correct)
     * 
     * @return Offset in nanoseconds, or 0 if not synchronized
     */
    int64_t get_offset_ns() const {
        return last_query_result_.valid ? last_query_result_.offset_ns : 0;
    }
    
    /**
     * @brief Get seconds since last successful query
     */
    int32_t get_seconds_since_sync() const;

private:
    std::string server_;
    uint16_t port_;
    uint32_t poll_interval_s_;
    
    NTPQueryResult last_query_result_;
    std::chrono::steady_clock::time_point last_query_time_;
    
    // NTP state
    int socket_fd_{-1};
    
    /**
     * @brief Perform SNTP query (RFC 4330)
     * 
     * @param[out] result Query result
     * @return true if query successful
     */
    bool query_ntp_server(NTPQueryResult& result);
    
    /**
     * @brief Convert NTP stratum to IEEE 1588-2019 clockClass
     * 
     * Mapping based on IEEE 1588-2019 Table 5:
     * - Stratum 1: clockClass 6 (primary time source)
     * - Stratum 2-3: clockClass 52-58 (degraded by symmetric path)
     * - Stratum 4+: clockClass 187 (degraded accuracy)
     * - Stratum 16: clockClass 248 (default, not synchronized)
     */
    uint8_t stratum_to_clock_class(uint8_t stratum) const;
    
    /**
     * @brief Convert NTP precision to IEEE 1588-2019 clockAccuracy
     * 
     * Mapping based on IEEE 1588-2019 Table 6:
     * - <25ns: 0x20
     * - <100ns: 0x21
     * - <250ns: 0x22
     * - <1µs: 0x23
     * - <2.5µs: 0x24
     * - <10µs: 0x25
     * - <25µs: 0x26
     * - <100µs: 0x27
     * - <250µs: 0x28
     * - <1ms: 0x29
     * - <2.5ms: 0x2A
     * - <10ms: 0x2B
     * - >10ms: 0xFE (unknown)
     */
    uint8_t precision_to_clock_accuracy(int8_t precision, int64_t round_trip_ns) const;
    
    /**
     * @brief Compute offset scaled log variance from NTP statistics
     */
    uint16_t compute_offset_scaled_log_variance(int64_t jitter_ns) const;
};

} // namespace NTP
} // namespace Examples

#endif // NTP_ADAPTER_HPP
