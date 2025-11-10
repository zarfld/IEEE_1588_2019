/**
 * @file bmca_integration.hpp
 * @brief BMCA Runtime Integration Coordinator
 * 
 * Phase: 06-integration
 * Task: Task 1 - BMCA Integration
 * 
 * Coordinates Best Master Clock Algorithm execution with:
 * - Periodic BMCA execution via timer callbacks
 * - State machine transition coordination
 * - ParentDS/CurrentDS synchronization
 * - BMCA decision metrics and health monitoring
 * 
 * IEEE 1588-2019 References:
 * - Section 9.2: PTP state machine
 * - Section 9.3: Best Master Clock Algorithm
 * - Section 8.2.3: Parent data set (ParentDS)
 * 
 * @note This coordinator integrates existing BMCA implementation (PtpPort::run_bmca)
 *       with runtime system for operational deployment.
 */

#ifndef IEEE_1588_2019_BMCA_INTEGRATION_HPP
#define IEEE_1588_2019_BMCA_INTEGRATION_HPP

#include "types.hpp"
#include "clocks.hpp"
#include "../../../../Common/utils/health.hpp"
#include "../../../../Common/utils/metrics.hpp"
#include <functional>
#include <memory>
#include <string>
#include <cstdint>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Integration {

/**
 * @brief BMCA execution statistics and health indicators
 */
struct BMCAStatistics {
    // Execution counters
    std::uint64_t total_executions{0};        ///< Total BMCA runs
    std::uint64_t master_selections{0};       ///< Times local selected as master
    std::uint64_t slave_selections{0};        ///< Times foreign selected as master
    std::uint64_t passive_selections{0};      ///< Times passive due to tie
    
    // Decision change tracking
    std::uint64_t role_changes{0};            ///< State role transitions (M↔S)
    std::uint64_t parent_changes{0};          ///< Parent (GM) identity changes
    
    // Foreign master tracking
    std::uint8_t current_foreign_count{0};    ///< Active foreign masters
    std::uint8_t max_foreign_count{0};        ///< Peak foreign master count
    
    // Timing metrics
    std::uint64_t last_execution_duration_ns{0}; ///< Last BMCA execution time
    std::uint64_t max_execution_duration_ns{0};  ///< Peak BMCA execution time
    
    // Anomaly detection
    std::uint64_t oscillation_count{0};       ///< Rapid role changes (instability)
    std::uint64_t no_foreign_masters{0};      ///< BMCA runs with empty foreign list
    
    /**
     * @brief Reset all statistics to initial state
     */
    void reset() noexcept {
        *this = BMCAStatistics{};
    }
};

/**
 * @brief BMCA health indicators for monitoring
 */
struct BMCAHealthStatus {
    enum class Status {
        Healthy,        ///< BMCA operating normally
        Degraded,       ///< Minor issues detected (oscillation, slow execution)
        Critical        ///< Severe issues (no foreign masters, excessive oscillation)
    };
    
    Status status{Status::Healthy};
    std::string message{};              ///< Human-readable status description
    std::uint64_t timestamp_ns{0};      ///< Health check timestamp
    
    // Health indicators (thresholds can be configured)
    bool excessive_oscillation{false};  ///< >10 role changes per minute
    bool slow_execution{false};         ///< Execution time >100μs
    bool no_candidates{false};          ///< No foreign masters available
    bool stale_foreign_list{false};     ///< Foreign masters not updated in >10s
};

/**
 * @brief BMCA Integration Coordinator
 * 
 * Manages BMCA execution lifecycle, decision propagation, and monitoring.
 * Integrates with PtpPort for protocol operations and provides observability
 * through metrics and health reporting.
 * 
 * Usage:
 * @code
 * BMCAIntegration bmca_coordinator(port);
 * bmca_coordinator.configure(config);
 * bmca_coordinator.start();
 * 
 * // In main loop
 * bmca_coordinator.tick(current_time);
 * 
 * // Query health
 * auto health = bmca_coordinator.get_health_status();
 * auto stats = bmca_coordinator.get_statistics();
 * @endcode
 */
class BMCAIntegration {
public:
    /**
     * @brief Configuration for BMCA execution behavior
     */
    struct Configuration {
        // Execution timing (IEEE 1588-2019 default: 1 second)
        std::uint32_t execution_interval_ms{1000};  ///< BMCA execution period
        
        // Health monitoring thresholds
        std::uint32_t oscillation_threshold{10};    ///< Max role changes per minute
        std::uint32_t max_execution_time_us{100};   ///< Max acceptable execution time
        std::uint32_t stale_foreign_time_s{10};     ///< Foreign master staleness timeout
        
        // Behavior flags
        bool enable_periodic_execution{true};       ///< Run BMCA on timer
        bool enable_on_announce{true};              ///< Run BMCA on Announce reception
        bool enable_health_monitoring{true};        ///< Collect health metrics
    };
    
    /**
     * @brief Construct BMCA coordinator with port reference
     * 
     * @param port PTP port to coordinate BMCA for
     */
    explicit BMCAIntegration(Clocks::PtpPort& port) noexcept
        : port_(port)
        , config_{}
        , statistics_{}
        , health_{}
        , last_execution_time_{}
        , last_role_(Types::PortState::Initializing)
        , last_parent_identity_{}
        , is_running_(false)
    {}
    
    /**
     * @brief Configure BMCA execution behavior
     * 
     * @param config Configuration parameters
     * @return Success if configuration valid
     */
    Types::PTPResult<void> configure(const Configuration& config) noexcept {
        if (config.execution_interval_ms == 0) {
            return Types::PTPResult<void>::failure(Types::PTPError::Invalid_Parameter);
        }
        if (config.oscillation_threshold == 0) {
            return Types::PTPResult<void>::failure(Types::PTPError::Invalid_Parameter);
        }
        
        config_ = config;
        return Types::PTPResult<void>::success();
    }
    
    /**
     * @brief Start BMCA execution coordination
     * 
     * Resets statistics and enables periodic execution.
     * 
     * @return Success if started successfully
     */
    Types::PTPResult<void> start() noexcept {
        if (is_running_) {
            return Types::PTPResult<void>::failure(Types::PTPError::State_Error);
        }
        
        statistics_.reset();
        health_ = BMCAHealthStatus{};
        last_execution_time_ = Types::Timestamp{};
        is_running_ = true;
        
        return Types::PTPResult<void>::success();
    }
    
    /**
     * @brief Stop BMCA execution coordination
     * 
     * Disables periodic execution but preserves statistics.
     * 
     * @return Success if stopped successfully
     */
    Types::PTPResult<void> stop() noexcept {
        if (!is_running_) {
            return Types::PTPResult<void>::failure(Types::PTPError::State_Error);
        }
        
        is_running_ = false;
        return Types::PTPResult<void>::success();
    }
    
    /**
     * @brief Periodic tick for BMCA execution and health monitoring
     * 
     * Executes BMCA if interval expired and config permits.
     * Updates health status and metrics.
     * 
     * @param current_time Current timestamp
     * @return Success if tick processed successfully
     */
    Types::PTPResult<void> tick(const Types::Timestamp& current_time) noexcept;
    
    /**
     * @brief Force immediate BMCA execution
     * 
     * Useful when Announce message received or configuration changed.
     * 
     * @param current_time Current timestamp
     * @return Success if BMCA executed successfully
     */
    Types::PTPResult<void> execute_bmca(const Types::Timestamp& current_time) noexcept;
    
    /**
     * @brief Get current BMCA statistics
     * 
     * @return Reference to current statistics
     */
    const BMCAStatistics& get_statistics() const noexcept {
        return statistics_;
    }
    
    /**
     * @brief Get current health status
     * 
     * @return Current health indicators
     */
    BMCAHealthStatus get_health_status() const noexcept {
        return health_;
    }
    
    /**
     * @brief Reset all statistics and health indicators
     */
    void reset() noexcept {
        statistics_.reset();
        health_ = BMCAHealthStatus{};
    }
    
    /**
     * @brief Check if coordinator is running
     * 
     * @return true if started and operational
     */
    bool is_running() const noexcept {
        return is_running_;
    }

private:
    /**
     * @brief Execute BMCA and update statistics/health
     * 
     * @param current_time Execution timestamp
     * @return BMCA execution result
     */
    Types::PTPResult<void> execute_bmca_internal(const Types::Timestamp& current_time) noexcept;
    
    /**
     * @brief Update health status based on current statistics
     * 
     * @param current_time Health check timestamp
     */
    void update_health_status(const Types::Timestamp& current_time) noexcept;
    
    /**
     * @brief Detect role oscillation (rapid M↔S changes)
     * 
     * @return true if excessive oscillation detected
     */
    bool detect_oscillation() const noexcept;
    
    /**
     * @brief Check if foreign master list is stale
     * 
     * @param current_time Current timestamp
     * @return true if no foreign master updates in threshold period
     */
    bool is_foreign_list_stale(const Types::Timestamp& current_time) const noexcept;

    // Port reference for BMCA execution
    Clocks::PtpPort& port_;
    
    // Configuration
    Configuration config_;
    
    // Statistics and health
    BMCAStatistics statistics_;
    BMCAHealthStatus health_;
    
    // State tracking
    Types::Timestamp last_execution_time_;
    Types::PortState last_role_;
    std::uint8_t last_parent_identity_[8];
    
    // Runtime state
    bool is_running_;
};

} // namespace Integration
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE

#endif // IEEE_1588_2019_BMCA_INTEGRATION_HPP
