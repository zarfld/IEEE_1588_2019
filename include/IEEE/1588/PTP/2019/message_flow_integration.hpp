/**
 * @file message_flow_integration.hpp
 * @brief IEEE 1588-2019 Message Flow Integration Coordinator
 * 
 * Coordinates end-to-end message processing pipeline:
 * - Announce → BMCA → State transitions
 * - Sync → Offset calculation → Servo adjustment
 * 
 * Integrates Tasks 1-3 (BMCA, Sync, Servo) into cohesive message handling.
 * 
 * @see IEEE 1588-2019, Section 13 "Message formats"
 * @see IEEE 1588-2019, Section 9.2 "PTP state machine"
 * @see IEEE 1588-2019, Section 11 "Synchronization mechanisms"
 * 
 * Phase: 06-integration
 * Task: Task 4 - Message Flow Integration
 * Dependencies: Tasks 1, 2, 3 (BMCA, Sync, Servo coordinators)
 */

#ifndef IEEE_1588_PTP_2019_MESSAGE_FLOW_INTEGRATION_HPP
#define IEEE_1588_PTP_2019_MESSAGE_FLOW_INTEGRATION_HPP

#include "types.hpp"
#include "messages.hpp"
#include "clocks.hpp"
#include <cstdint>
#include <string>
#include <functional>
#include <memory>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {

// Forward declarations
namespace servo {
    class ServoIntegration;
}

namespace Integration {
    class BMCACoordinator;
    class SyncCoordinator;
}

//==============================================================================
// Message Flow Statistics
//==============================================================================

/**
 * @brief Message flow processing statistics
 * 
 * Tracks message processing through complete pipeline with error detection.
 */
struct MessageFlowStatistics {
    // Message reception counters
    std::uint64_t announce_received{0};       ///< Total Announce messages received
    std::uint64_t sync_received{0};           ///< Total Sync messages received
    std::uint64_t follow_up_received{0};      ///< Total Follow_Up messages received
    std::uint64_t delay_resp_received{0};     ///< Total Delay_Resp messages received
    
    // Processing success counters
    std::uint64_t announce_processed{0};      ///< Announce → BMCA successful
    std::uint64_t sync_processed{0};          ///< Sync → Offset → Servo successful
    std::uint64_t bmca_triggered{0};          ///< BMCA executions triggered by Announce
    std::uint64_t servo_adjustments{0};       ///< Servo adjustments triggered by Sync
    
    // Error counters
    std::uint64_t announce_errors{0};         ///< Announce processing failures
    std::uint64_t sync_errors{0};             ///< Sync processing failures
    std::uint64_t invalid_messages{0};        ///< Messages failing validation
    std::uint64_t out_of_order{0};            ///< Messages received out of expected order
    std::uint64_t domain_mismatches{0};       ///< Domain number mismatches
    
    // Timing metrics
    std::uint64_t last_announce_time_ns{0};   ///< Timestamp of last Announce
    std::uint64_t last_sync_time_ns{0};       ///< Timestamp of last Sync
    std::uint64_t announce_interval_ns{0};    ///< Average Announce interval
    std::uint64_t sync_interval_ns{0};        ///< Average Sync interval
    
    // State transitions
    std::uint64_t state_transitions{0};       ///< Port state changes triggered
    std::uint64_t parent_changes{0};          ///< Grand Master changes detected
    
    /**
     * @brief Reset all statistics to initial state
     */
    void reset() noexcept {
        *this = MessageFlowStatistics{};
    }
};

/**
 * @brief Message flow health status
 */
struct MessageFlowHealthStatus {
    enum class Status {
        Healthy,        ///< All message flows operating normally
        Degraded,       ///< Some message flows experiencing issues
        Critical        ///< Major message flow failures
    };
    
    Status status{Status::Critical};
    std::string message{};              ///< Human-readable status description
    std::uint64_t timestamp_ns{0};      ///< Health check timestamp
    
    // Health indicators
    bool announce_flow_active{false};   ///< Announce messages being received
    bool sync_flow_active{false};       ///< Sync messages being received
    bool bmca_operational{false};       ///< BMCA executing successfully
    bool servo_operational{false};      ///< Servo making adjustments
    bool within_timing_spec{false};     ///< Message intervals within spec
    
    // Component health rollup
    bool bmca_healthy{false};           ///< BMCA coordinator health
    bool sync_healthy{false};           ///< Sync coordinator health
    bool servo_healthy{false};          ///< Servo controller health
};

/**
 * @brief Message Flow Configuration
 */
struct MessageFlowConfiguration {
    // Domain filtering
    std::uint8_t expected_domain{0};            ///< Expected PTP domain number
    bool strict_domain_checking{true};          ///< Reject messages from other domains
    
    // Timing thresholds (nanoseconds)
    std::uint64_t announce_timeout_ns{3000000000ULL};  ///< 3 seconds
    std::uint64_t sync_timeout_ns{1000000000ULL};      ///< 1 second
    std::uint64_t max_message_age_ns{10000000000ULL};  ///< 10 seconds
    
    // Processing options
    bool enable_bmca_on_announce{true};         ///< Trigger BMCA on Announce
    bool enable_servo_on_sync{true};            ///< Trigger servo on Sync
    bool validate_message_order{true};          ///< Check message sequencing
    bool log_message_flows{false};              ///< Enable detailed logging
    
    /**
     * @brief Create default configuration
     */
    static MessageFlowConfiguration create_default() noexcept {
        return MessageFlowConfiguration{};
    }
};

//==============================================================================
// Message Flow Integration Coordinator
//==============================================================================

/**
 * @brief Message Flow Integration Coordinator
 * 
 * Orchestrates end-to-end message processing:
 * 1. Announce messages → BMCA coordinator → State transitions
 * 2. Sync messages → Sync coordinator → Servo controller
 * 
 * Provides unified interface for message handling with health monitoring
 * and error detection across the complete synchronization pipeline.
 * 
 * Thread Safety: Not thread-safe. Caller must ensure serialized access.
 * Real-Time: All operations use bounded execution time (no dynamic allocation).
 */
class MessageFlowCoordinator {
public:
    /**
     * @brief Construct message flow coordinator
     * 
     * @param bmca BMCA integration coordinator (must outlive this object)
     * @param sync Sync integration coordinator (must outlive this object)
     * @param servo Servo integration controller (must outlive this object)
     * @param port PTP port for state management (must outlive this object)
     */
    explicit MessageFlowCoordinator(
        BMCACoordinator& bmca,
        SyncCoordinator& sync,
        servo::ServoIntegration& servo,
        Clocks::PtpPort& port
    ) noexcept;
    
    /**
     * @brief Configure message flow coordinator
     * 
     * @param config Configuration parameters
     * @return Success if configuration valid
     */
    Types::PTPError configure(const MessageFlowConfiguration& config) noexcept;
    
    /**
     * @brief Start message flow processing
     * 
     * Enables message handling and component coordination.
     * 
     * @return Success if started successfully
     */
    Types::PTPError start() noexcept;
    
    /**
     * @brief Stop message flow processing
     * 
     * Disables message handling. Does not stop underlying components.
     */
    void stop() noexcept;
    
    /**
     * @brief Process received Announce message
     * 
     * Flow: Validate → Extract foreign master info → Trigger BMCA
     * 
     * @param message Announce message to process
     * @param reception_timestamp_ns Time message was received (local clock)
     * @return Success if processed successfully
     * 
     * @note Triggers BMCA execution if enabled in configuration
     * @see IEEE 1588-2019, Section 13.5 "Announce message"
     * @see IEEE 1588-2019, Section 9.3 "Best master clock algorithm"
     */
    Types::PTPError process_announce_message(
        const AnnounceMessage& message,
        std::uint64_t reception_timestamp_ns
    ) noexcept;
    
    /**
     * @brief Process received Sync message
     * 
     * Flow: Validate → Record timestamp → Calculate offset → Servo adjust
     * 
     * @param message Sync message to process
     * @param reception_timestamp_ns Time message was received (local clock)
     * @return Success if processed successfully
     * 
     * @note Triggers servo adjustment if enabled in configuration
     * @see IEEE 1588-2019, Section 13.3 "Sync message"
     * @see IEEE 1588-2019, Section 11 "Synchronization mechanisms"
     */
    Types::PTPError process_sync_message(
        const SyncMessage& message,
        std::uint64_t reception_timestamp_ns
    ) noexcept;
    
    /**
     * @brief Process received Follow_Up message
     * 
     * Flow: Validate → Pair with Sync → Calculate precise offset
     * 
     * @param message Follow_Up message to process
     * @return Success if processed successfully
     * 
     * @note Must follow corresponding Sync message
     * @see IEEE 1588-2019, Section 13.4 "Follow_Up message"
     */
    Types::PTPError process_follow_up_message(
        const FollowUpMessage& message
    ) noexcept;
    
    /**
     * @brief Process received Delay_Resp message
     * 
     * Flow: Validate → Calculate path delay → Update sync coordinator
     * 
     * @param message Delay_Resp message to process
     * @return Success if processed successfully
     * 
     * @see IEEE 1588-2019, Section 13.7 "Delay_Resp message"
     */
    Types::PTPError process_delay_resp_message(
        const DelayRespMessage& message
    ) noexcept;
    
    /**
     * @brief Get message flow statistics
     * 
     * @return Current statistics snapshot
     */
    MessageFlowStatistics get_statistics() const noexcept {
        return statistics_;
    }
    
    /**
     * @brief Get message flow health status
     * 
     * @return Current health status with component rollup
     */
    MessageFlowHealthStatus get_health_status() const noexcept;
    
    /**
     * @brief Check if message flow is running
     * 
     * @return true if started and processing messages
     */
    bool is_running() const noexcept {
        return is_running_;
    }
    
    /**
     * @brief Reset all statistics
     * 
     * Clears message flow statistics. Does not reset component statistics.
     */
    void reset() noexcept;
    
private:
    // Component references
    BMCACoordinator& bmca_;
    SyncCoordinator& sync_;
    servo::ServoIntegration& servo_;
    Clocks::PtpPort& port_;
    
    // Configuration and state
    MessageFlowConfiguration config_;
    MessageFlowStatistics statistics_;
    bool is_running_{false};
    bool first_announce_{true};
    bool first_sync_{true};
    
    // Message sequencing state
    std::uint16_t last_announce_sequence_{0};
    std::uint16_t last_sync_sequence_{0};
    
    // Helper methods
    
    /**
     * @brief Validate common message requirements
     * 
     * @param header Message header to validate
     * @return Success if valid
     */
    Types::PTPError validate_message_header(
        const CommonHeader& header
    ) noexcept;
    
    /**
     * @brief Check if message domain matches configuration
     * 
     * @param domain Domain number from message
     * @return Success if domain matches or checking disabled
     */
    Types::PTPError check_domain(std::uint8_t domain) noexcept;
    
    /**
     * @brief Check if message timestamp is recent
     * 
     * @param timestamp_ns Message timestamp
     * @param current_time_ns Current time
     * @return Success if timestamp is recent enough
     */
    Types::PTPError check_message_age(
        std::uint64_t timestamp_ns,
        std::uint64_t current_time_ns
    ) noexcept;
    
    /**
     * @brief Update message flow health status
     */
    void update_health_status() noexcept;
    
    /**
     * @brief Update message timing statistics
     * 
     * @param is_announce true for Announce, false for Sync
     * @param timestamp_ns Current message timestamp
     */
    void update_timing_statistics(
        bool is_announce,
        std::uint64_t timestamp_ns
    ) noexcept;
};

} // namespace Integration
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE

#endif // IEEE_1588_PTP_2019_MESSAGE_FLOW_INTEGRATION_HPP
