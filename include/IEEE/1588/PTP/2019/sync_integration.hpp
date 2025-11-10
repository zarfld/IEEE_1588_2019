/**
 * @file sync_integration.hpp
 * @brief IEEE 1588-2019 Synchronization Integration Coordinator
 * 
 * Coordinates offset calculation, delay measurement, and servo control
 * for IEEE 1588-2019 PTP time synchronization. Integrates E2E and P2P
 * delay mechanisms with clock servo for sub-microsecond accuracy.
 * 
 * @see IEEE 1588-2019, Section 11 "Synchronization and delay measurement"
 * @see IEEE 1588-2019, Section 11.3 "Delay request-response mechanism (E2E)"
 * @see IEEE 1588-2019, Section 11.4 "Peer delay mechanism (P2P)"
 */

#ifndef IEEE_1588_PTP_2019_SYNC_INTEGRATION_HPP
#define IEEE_1588_PTP_2019_SYNC_INTEGRATION_HPP

#include "clocks.hpp"
#include "types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {

// Forward declaration
namespace Clocks { class PtpPort; }

/**
 * @brief Synchronization accuracy statistics
 * 
 * Tracks timing metrics for sync accuracy validation and monitoring.
 * All time values in nanoseconds for precision measurement.
 */
struct SyncStatistics {
    // Offset tracking (IEEE 1588-2019 Section 11.2)
    std::uint64_t total_offset_samples{0};     ///< Total offset calculations performed
    double current_offset_ns{0.0};             ///< Current offset from master (nanoseconds)
    double min_offset_ns{0.0};                 ///< Minimum offset observed (nanoseconds)
    double max_offset_ns{0.0};                 ///< Maximum offset observed (nanoseconds)
    double avg_offset_ns{0.0};                 ///< Average offset (nanoseconds)
    
    // Delay tracking (IEEE 1588-2019 Section 11.3/11.4)
    std::uint64_t total_delay_samples{0};      ///< Total delay measurements
    double current_delay_ns{0.0};              ///< Current mean path delay (nanoseconds)
    double min_delay_ns{0.0};                  ///< Minimum delay observed (nanoseconds)
    double max_delay_ns{0.0};                  ///< Maximum delay observed (nanoseconds)
    double avg_delay_ns{0.0};                  ///< Average delay (nanoseconds)
    
    // Accuracy metrics
    double offset_variance_ns2{0.0};           ///< Offset variance (nanoseconds²)
    double offset_std_dev_ns{0.0};             ///< Offset standard deviation (nanoseconds)
    std::uint64_t sub_microsecond_samples{0};  ///< Samples with |offset| < 1µs
    
    // Mechanism tracking
    bool using_p2p_delay{false};               ///< P2P (true) or E2E (false)
    std::uint64_t p2p_measurements{0};         ///< Peer delay measurements
    std::uint64_t e2e_measurements{0};         ///< End-to-end measurements
    
    // Error detection
    std::uint64_t negative_delay_count{0};     ///< Invalid negative delay measurements
    std::uint64_t timestamp_order_violations{0}; ///< Timestamp ordering issues
    
    /**
     * @brief Reset all statistics to initial state
     */
    void reset() noexcept {
        *this = SyncStatistics{};
    }
};

/**
 * @brief Synchronization health indicators
 */
struct SyncHealthStatus {
    enum class Status {
        Synchronized,   ///< Offset < 1µs, stable
        Converging,     ///< Offset reducing, not yet < 1µs
        Degraded,       ///< Offset > 10µs or variance high
        Critical        ///< Offset > 100µs or persistent errors
    };
    
    Status status{Status::Critical};
    std::string message{};              ///< Human-readable status description
    std::uint64_t timestamp_ns{0};      ///< Health check timestamp
    
    // Health indicators
    bool offset_within_spec{false};     ///< |offset| < 1µs (sub-microsecond)
    bool delay_stable{false};           ///< Delay variance low
    bool measurements_valid{false};     ///< No timestamp violations
    bool servo_locked{false};           ///< Servo has converged (TODO: Task 3)
};

/**
 * @brief Synchronization Integration Coordinator
 * 
 * Manages sync/offset/delay measurement integration with clock servo.
 * Coordinates IEEE 1588-2019 synchronization mechanisms (E2E or P2P)
 * and provides observability through metrics and health monitoring.
 * 
 * Usage:
 * @code
 * SyncIntegration sync_coordinator(port);
 * sync_coordinator.configure(config);
 * sync_coordinator.start();
 * 
 * // In main loop
 * sync_coordinator.tick(current_time);
 * 
 * // Query sync quality
 * auto health = sync_coordinator.get_health_status();
 * auto stats = sync_coordinator.get_statistics();
 * @endcode
 */
class SyncIntegration {
public:
    /**
     * @brief Configuration for synchronization behavior
     */
    struct Configuration {
        // Monitoring timing
        std::uint32_t sampling_interval_ms{1000};  ///< Statistics update period
        
        // Accuracy thresholds (nanoseconds)
        double synchronized_threshold_ns{1000.0};  ///< Sub-microsecond target
        double degraded_threshold_ns{10000.0};     ///< 10µs warning level
        double critical_threshold_ns{100000.0};    ///< 100µs critical level
        
        // Health monitoring
        bool enable_health_monitoring{true};       ///< Enable health checks
        std::uint32_t variance_window_samples{10}; ///< Samples for variance calc
        
        // Servo integration (TODO: Task 3)
        bool enable_servo{false};                  ///< Enable servo control
        
        /**
         * @brief Validate configuration parameters
         */
        bool validate() const noexcept {
            return sampling_interval_ms > 0 &&
                   synchronized_threshold_ns > 0.0 &&
                   degraded_threshold_ns > synchronized_threshold_ns &&
                   critical_threshold_ns > degraded_threshold_ns &&
                   variance_window_samples > 0;
        }
    };
    
    /**
     * @brief Construct synchronization coordinator
     * @param port Reference to PtpPort being coordinated
     */
    explicit SyncIntegration(Clocks::PtpPort& port) noexcept
        : port_(port) {
        // Initialize with default configuration
        config_ = Configuration{};
    }
    
    /**
     * @brief Configure synchronization behavior
     * @param config Configuration parameters
     * @return Success if valid, failure otherwise
     */
    Types::PTPResult<void> configure(const Configuration& config) noexcept {
        if (!config.validate()) {
            return Types::PTPResult<void>::failure(Types::PTPError::Invalid_Parameter);
        }
        config_ = config;
        return Types::PTPResult<void>::success();
    }
    
    /**
     * @brief Start synchronization monitoring
     * @return Success if started, failure if already running
     */
    Types::PTPResult<void> start() noexcept {
        if (is_running_) {
            return Types::PTPResult<void>::failure(Types::PTPError::State_Error);
        }
        
        is_running_ = true;
        statistics_.reset();
        
        // Initialize health to Critical (no data yet)
        health_.status = SyncHealthStatus::Status::Critical;
        health_.message = "Not yet synchronized";
        health_.offset_within_spec = false;
        health_.delay_stable = false;
        health_.measurements_valid = false;
        health_.servo_locked = false;
        
        return Types::PTPResult<void>::success();
    }
    
    /**
     * @brief Stop synchronization monitoring
     * @return Success if stopped
     */
    Types::PTPResult<void> stop() noexcept {
        if (!is_running_) {
            return Types::PTPResult<void>::failure(Types::PTPError::State_Error);
        }
        
        is_running_ = false;
        return Types::PTPResult<void>::success();
    }
    
    /**
     * @brief Periodic tick for sync monitoring and servo control
     * 
     * Samples offset and delay, updates statistics, triggers servo.
     * Must be called regularly for accurate sync monitoring.
     * 
     * @param current_time Current timestamp
     * @return Success if tick processed successfully
     */
    Types::PTPResult<void> tick(const Types::Timestamp& current_time) noexcept;
    
    /**
     * @brief Force immediate synchronization sample
     * 
     * Useful when Sync/Follow_Up/Delay_Resp received.
     * 
     * @param current_time Current timestamp
     * @return Success if sample collected
     */
    Types::PTPResult<void> sample_now(const Types::Timestamp& current_time) noexcept;
    
    /**
     * @brief Get current synchronization statistics
     * @return Reference to current statistics
     */
    const SyncStatistics& get_statistics() const noexcept {
        return statistics_;
    }
    
    /**
     * @brief Get current health status
     * @return Current health indicators
     */
    SyncHealthStatus get_health_status() const noexcept {
        return health_;
    }
    
    /**
     * @brief Reset all statistics and health indicators
     */
    void reset() noexcept {
        statistics_.reset();
        health_ = SyncHealthStatus{};
        last_sample_time_ = Types::Timestamp{};
        offset_samples_.clear();
    }
    
    /**
     * @brief Check if coordinator is running
     */
    bool is_running() const noexcept {
        return is_running_;
    }

private:
    // Internal methods
    Types::PTPResult<void> collect_sample(const Types::Timestamp& current_time) noexcept;
    void update_statistics(double offset_ns, double delay_ns) noexcept;
    void update_health_status(const Types::Timestamp& current_time) noexcept;
    void calculate_variance() noexcept;
    
    // Port reference
    Clocks::PtpPort& port_;
    
    // Configuration
    Configuration config_;
    
    // Statistics and health
    SyncStatistics statistics_;
    SyncHealthStatus health_;
    
    // State tracking
    Types::Timestamp last_sample_time_;
    bool first_sample_done_{false};  ///< Track if first sample completed
    bool is_running_{false};
    
    // Variance calculation (rolling window)
    std::vector<double> offset_samples_;  ///< Recent offset samples for variance
};

} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE

#endif // IEEE_1588_PTP_2019_SYNC_INTEGRATION_HPP
