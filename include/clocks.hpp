/*
Module: include/clocks.hpp
Phase: 05-implementation
Traceability:
    Design: DES-C-010 (time sync), DES-I-007 (health)
    Requirements: REQ-F-003 (E2E offset), REQ-NF-REL-003 (observability)
    Tests: TEST-UNIT-SyncHeuristic, TEST-UNIT-HealthHeartbeat, TEST-UNIT-ForeignMasterOverflow, TEST-UNIT-TimestampOrdering
Notes: Public interface for PTP clocks and port state machines; aligns with IEEE 1588-2019 Sections 6, 8, 9, 11.

@req REQ-F-202 Deterministic BMCA per gPTP constraints (run_bmca, compare_announce_messages)
@req REQ-F-205 Dataset/MIB-Based Management (get_port_data_set, get_current_data_set, get_parent_data_set)
*/

/**
 * @file clocks.hpp
 * @brief IEEE 1588-2019 PTP Clock State Machines Implementation
 * @details Implements Ordinary Clock, Boundary Clock, and Transparent Clock
 *          state machines with deterministic design patterns as required by
 *          IEEE 1588-2019 Sections 9 and 10.
 * 
 * @note This implementation follows OpenAvnu deterministic design principles:
 *       - No dynamic memory allocation in critical paths
 *       - No blocking calls or exceptions
 *       - Bounded execution time for all operations
 *       - POD types for hardware compatibility
 * 
 * © 2024 OpenAvnu — IEEE 1588-2019 PTP v2.1
 */

#pragma once

#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"
#include <cstdint>
#include <array>
#include "Common/utils/logger.hpp"
#include "Common/utils/fault_injection.hpp"
#include "Common/utils/metrics.hpp"
#include "Common/utils/health.hpp"
#include "Common/utils/config.hpp"

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {

// Import types for convenience
using namespace Types;

/**
 * @brief IEEE 1588-2019 Clock State Machines
 * @details Implementation of clock state machines per IEEE 1588-2019
 *          Sections 9.2, 10.2, and 10.3 with time-sensitive design patterns.
 */
namespace Clocks {

// Forward declarations
class OrdinaryClock;
class BoundaryClock;
class TransparentClock;
class PtpPort;

// Message type aliases for convenience (actual messages defined in messages.hpp)
using AnnounceMessage = IEEE::_1588::PTP::_2019::AnnounceMessage;
using SyncMessage = IEEE::_1588::PTP::_2019::SyncMessage; 
using FollowUpMessage = IEEE::_1588::PTP::_2019::FollowUpMessage;
using DelayReqMessage = IEEE::_1588::PTP::_2019::DelayReqMessage;
using DelayRespMessage = IEEE::_1588::PTP::_2019::DelayRespMessage;

/**
 * @brief State Machine Events per IEEE 1588-2019 Section 9.2.6
 * @details Events that trigger state transitions in PTP port state machines
 */
enum class StateEvent : std::uint8_t {
    POWERUP                 = 0x00,  ///< Power-up or initialization
    INITIALIZE              = 0x01,  ///< Initialize event
    FAULT_DETECTED          = 0x02,  ///< Fault detected
    FAULT_CLEARED           = 0x03,  ///< Fault cleared
    DESIGNATED_ENABLED      = 0x04,  ///< Port designated and enabled
    DESIGNATED_DISABLED     = 0x05,  ///< Port designated but disabled
    RS_MASTER               = 0x06,  ///< Recommended State: Master
    RS_GRAND_MASTER         = 0x07,  ///< Recommended State: Grand Master
    RS_SLAVE                = 0x08,  ///< Recommended State: Slave
    RS_PASSIVE              = 0x09,  ///< Recommended State: Passive
    ANNOUNCE_RECEIPT_TIMEOUT = 0x0A, ///< Announce receipt timeout
    SYNCHRONIZATION_FAULT   = 0x0B,  ///< Synchronization fault
    QUALIFICATION_TIMEOUT   = 0x0C   ///< Master qualification timeout
};

/**
 * @brief Best Master Clock Algorithm Decision
 * @details Result of BMCA comparison per IEEE 1588-2019 Section 9.3
 */
enum class BMCADecision : std::uint8_t {
    BETTER_MASTER           = 0x00,  ///< Foreign master is better
    BETTER_BY_TOPOLOGY      = 0x01,  ///< Foreign master better by topology
    SAME_MASTER             = 0x02,  ///< Same master clock
    WORSE_BY_TOPOLOGY       = 0x03,  ///< Foreign master worse by topology
    WORSE_MASTER            = 0x04   ///< Foreign master is worse
};

/**
 * @brief Port Role Designation per IEEE 1588-2019 Section 9.3.3
 * @details Port role assignment after BMCA decision
 */
enum class PortRole : std::uint8_t {
    MASTER      = 0x00,  ///< Master port role
    SLAVE       = 0x01,  ///< Slave port role
    PASSIVE     = 0x02,  ///< Passive port role
    DISABLED    = 0x03   ///< Disabled port role
};

/**
 * @brief PTP Port Configuration
 * @details Deterministic configuration structure for PTP ports
 */
struct PortConfiguration {
    Types::PortNumber port_number{1};
    Types::DomainNumber domain_number{0};
    std::uint8_t announce_interval{1};        ///< Log message interval
    std::uint8_t sync_interval{0};            ///< Log message interval
    std::uint8_t delay_req_interval{0};       ///< Log message interval
    std::uint8_t announce_receipt_timeout{3}; ///< Timeout multiplier
    std::uint8_t sync_receipt_timeout{3};     ///< Timeout multiplier
    bool delay_mechanism_p2p{false};          ///< Use peer-to-peer delay
    std::uint8_t version_number{2};           ///< PTP version number
};

/**
 * @brief PTP Port Statistics
 * @details Performance and error counters with bounded memory usage
 */
struct PortStatistics {
    std::uint32_t state_transitions{0};
    std::uint32_t announce_messages_sent{0};
    std::uint32_t announce_messages_received{0};
    std::uint32_t sync_messages_sent{0};
    std::uint32_t sync_messages_received{0};
    std::uint32_t follow_up_messages_sent{0};
    std::uint32_t follow_up_messages_received{0};
    std::uint32_t delay_req_messages_sent{0};
    std::uint32_t delay_req_messages_received{0};
    std::uint32_t delay_resp_messages_sent{0};
    std::uint32_t delay_resp_messages_received{0};
    std::uint32_t announce_timeouts{0};
    std::uint32_t sync_timeouts{0};
    std::uint32_t qualification_timeouts{0};
    std::uint32_t fault_events{0};
    
    /** Reset all counters to zero */
    constexpr void reset() noexcept {
        *this = PortStatistics{};
    }
};

/**
 * @brief PTP Port Data Set per IEEE 1588-2019 Section 8.2.5
 * @details Complete port data set with deterministic layout
 */
struct PortDataSet {
    Types::PortIdentity port_identity;
    PortState port_state{PortState::Initializing};
    std::uint8_t log_min_delay_req_interval{0};
    Types::TimeInterval peer_mean_path_delay{0};
    std::uint8_t log_announce_interval{1};
    std::uint8_t announce_receipt_timeout{3};
    std::uint8_t log_sync_interval{0};
    bool delay_mechanism{false};  ///< false = E2E, true = P2P
    std::uint8_t log_min_pdelay_req_interval{0};
    std::uint8_t version_number{2};
};

/**
 * @brief Current Data Set per IEEE 1588-2019 Section 8.2.2
 * @details Dynamic state information with bounded precision
 */
struct CurrentDataSet {
    std::uint16_t steps_removed{0};
    Types::TimeInterval offset_from_master{0};
    Types::TimeInterval mean_path_delay{0};
};

/**
 * @brief Parent Data Set per IEEE 1588-2019 Section 8.2.3
 * @details Information about the master clock with deterministic structure
 */
struct ParentDataSet {
    Types::PortIdentity parent_port_identity;
    bool parent_stats{false};
    std::uint16_t observed_parent_offset_scaled_log_variance{0xFFFF};
    std::int32_t observed_parent_clock_phase_change_rate{0x7FFFFFFF};
    Types::ClockIdentity grandmaster_identity;
    Types::ClockQuality grandmaster_clock_quality;
    std::uint8_t grandmaster_priority1{128};
    std::uint8_t grandmaster_priority2{128};
};

/**
 * @brief Time Properties Data Set per IEEE 1588-2019 Section 8.2.4
 * @details Time properties from grandmaster clock with deterministic structure
 * 
 * Contains time metadata extracted from Announce message header flags and body.
 * All fields map directly to IEEE 1588-2019 specification requirements.
 * 
 * @see IEEE 1588-2019, Section 8.2.4 "timePropertiesDS data set member specifications"
 * @see IEEE 1588-2019, Section 13.5 "Announce message"
 * @see IEEE 1588-2019, Table 34 "Announce message fields"
 */
struct TimePropertiesDataSet {
    /** Current UTC offset in seconds (from AnnounceBody byte 44-45) */
    std::int16_t currentUtcOffset{0};
    
    /** True if currentUtcOffset is valid (from flagField bit 0x0004) */
    bool currentUtcOffsetValid{false};
    
    /** True if last minute of current day has 59 seconds (from flagField bit 0x0002) */
    bool leap59{false};
    
    /** True if last minute of current day has 61 seconds (from flagField bit 0x0001) */
    bool leap61{false};
    
    /** True if timescale is PTP (from flagField bit 0x0008) */
    bool ptpTimescale{false};
    
    /** True if time is traceable to primary reference (from flagField bit 0x0010) */
    bool timeTraceable{false};
    
    /** True if frequency is traceable to primary reference (from flagField bit 0x0020) */
    bool frequencyTraceable{false};
    
    /** Time source (from AnnounceBody byte 63) per IEEE 1588-2019 Table 6 */
    std::uint8_t timeSource{0};
};


/**
 * @brief Clock synchronization information
 */
struct SynchronizationData {
    Types::Timestamp masterTimeStamp;       ///< Master timestamp from Sync message
    Types::Timestamp slaveTimeStamp;        ///< Slave timestamp when Sync received
    Types::CorrectionField correction;      ///< Correction field from messages
    Types::TimeInterval offsetFromMaster;   ///< Calculated offset from master
    Types::TimeInterval meanPathDelay;      ///< Mean path delay to master
    
    SynchronizationData() noexcept = default;
    
    /**
     * @brief Calculate offset from master using timestamps
     * @param sync_timestamp Master timestamp from Sync message
     * @param sync_reception Local timestamp when Sync was received
     * @param delay_req_timestamp Local timestamp when DelayReq was sent
     * @param delay_resp_timestamp Master timestamp from DelayResp message
     * @return Result containing calculated offset or error
     */
    Types::PTPResult<Types::TimeInterval> calculateOffset(
        const Types::Timestamp& sync_timestamp,
        const Types::Timestamp& sync_reception,
        const Types::Timestamp& delay_req_timestamp,
        const Types::Timestamp& delay_resp_timestamp
    ) noexcept {
        // Ordering assertions (FM-001): T2>=T1 and T4>=T3 must hold
        if (sync_reception < sync_timestamp) {
            Common::utils::logging::warn("Timestamps", 0x0204, "Sync RX timestamp earlier than origin (T2 < T1)");
            Common::utils::metrics::increment(Common::utils::metrics::CounterId::ValidationsFailed, 1);
            Common::utils::health::emit();
        }
        if (delay_resp_timestamp < delay_req_timestamp) {
            Common::utils::logging::warn("Timestamps", 0x0205, "Delay response RX earlier than request TX (T4 < T3)");
            Common::utils::metrics::increment(Common::utils::metrics::CounterId::ValidationsFailed, 1);
            Common::utils::health::emit();
        }
        // IEEE 1588-2019 E2E algorithm:
        // offset_from_master = ((T2 - T1) - (T4 - T3)) / 2
        const Types::TimeInterval t2_minus_t1 = (sync_reception - sync_timestamp);
        const Types::TimeInterval t4_minus_t3 = (delay_resp_timestamp - delay_req_timestamp);
        // Work directly on scaled nanoseconds (2^-16 ns units) to avoid float rounding
        const Types::Integer64 diff_scaled = (t2_minus_t1.scaled_nanoseconds - t4_minus_t3.scaled_nanoseconds);
        Types::Integer64 scaled;
        // Optional FM-014 mitigation: unbiased half-to-even division by 2 to remove rounding bias when odd values appear in scaled domain.
        if (Common::utils::config::is_rounding_compensation_enabled()) {
            // Banker's rounding for division by 2
            const Types::Integer64 n = diff_scaled / 2;      // trunc towards 0
            const Types::Integer64 r = diff_scaled % 2;      // remainder, same sign as numerator
            if (r == 0) {
                scaled = n;
            } else {
                // Tie at .5: round to even result
                if ((n & 1) != 0) {
                    scaled = n + (diff_scaled > 0 ? 1 : -1);
                } else {
                    scaled = n;
                }
            }
        } else {
            scaled = diff_scaled / 2;
        }
        Types::Integer64 adjusted = scaled;
        if (Common::utils::fi::is_offset_jitter_enabled()) {
            adjusted += (Common::utils::fi::get_offset_jitter_ns() << 16); // convert ns to scaled (2^-16 ns)
        }
        // Range validation & clamp (mitigation FM-002/FM-013)
        constexpr Types::Integer64 MAX_ABS_SCALED = static_cast<Types::Integer64>(1ull << 46); // ~ 2^30 ns after division margin
        if (adjusted > MAX_ABS_SCALED) {
            adjusted = MAX_ABS_SCALED;
            Common::utils::logging::warn("Offset", 0x0202, "Offset clamped positive upper bound");
            Common::utils::metrics::increment(Common::utils::metrics::CounterId::ValidationsFailed, 1);
        } else if (adjusted < -MAX_ABS_SCALED) {
            adjusted = -MAX_ABS_SCALED;
            Common::utils::logging::warn("Offset", 0x0203, "Offset clamped negative lower bound");
            Common::utils::metrics::increment(Common::utils::metrics::CounterId::ValidationsFailed, 1);
        } else {
            Common::utils::metrics::increment(Common::utils::metrics::CounterId::ValidationsPassed, 1);
        }
        offsetFromMaster = Types::TimeInterval{adjusted};
        Common::utils::logging::debug("Offset", 0x0200, "Offset from master calculated");
        Common::utils::metrics::increment(Common::utils::metrics::CounterId::OffsetsComputed, 1);
        Common::utils::health::record_offset_ns(static_cast<long long>(offsetFromMaster.toNanoseconds()));
        Common::utils::health::emit();
        return Types::PTPResult<Types::TimeInterval>::success(offsetFromMaster);
    }
};


// Compile-time size validation for deterministic data structures
static_assert(sizeof(PortDataSet) <= 128, 
              "PortDataSet must be compact for deterministic access");
static_assert(sizeof(CurrentDataSet) <= 32, 
              "CurrentDataSet must be compact for real-time access");  
static_assert(sizeof(ParentDataSet) <= 64, 
              "ParentDataSet must be compact for BMCA operations");

/**
 * @brief State Machine Callbacks
 * @details Function pointers for hardware abstraction with bounded execution
 */
struct StateCallbacks {
    // Message transmission callbacks (must be non-blocking)
    Types::PTPError (*send_announce)(const AnnounceMessage& msg);
    Types::PTPError (*send_sync)(const SyncMessage& msg);
    Types::PTPError (*send_follow_up)(const FollowUpMessage& msg);
    Types::PTPError (*send_delay_req)(const DelayReqMessage& msg);
    Types::PTPError (*send_delay_resp)(const DelayRespMessage& msg);
    
    // Timestamping callbacks (must be deterministic)
    Types::Timestamp (*get_timestamp)();
    Types::PTPError (*get_tx_timestamp)(std::uint16_t sequence_id, Types::Timestamp* timestamp);
    
    // Hardware control callbacks (bounded execution time)
    Types::PTPError (*adjust_clock)(std::int64_t adjustment_ns);
    Types::PTPError (*adjust_frequency)(double ppb_adjustment);
    
    // Event notification callbacks
    void (*on_state_change)(PortState old_state, PortState new_state);
    void (*on_fault)(const char* fault_description);
};

//==============================================================================
// TLV Parsing Helper Functions (IEEE 1588-2019 Section 14)
//==============================================================================

/**
 * @brief Parse TLV header from buffer
 * @param buffer Input buffer containing TLV data
 * @param buffer_size Size of input buffer
 * @param header Output TLV header structure
 * @return Success/failure result
 * @note IEEE 1588-2019 Section 14 - TLV format
 */
inline Types::PTPResult<void> parse_tlv_header(const std::uint8_t* buffer,
                                               std::size_t buffer_size,
                                               TLVHeader& header) noexcept {
    if (buffer == nullptr || buffer_size < sizeof(TLVHeader)) {
        return Types::PTPResult<void>::failure(Types::PTPError::INVALID_LENGTH);
    }
    
    // Copy TLV header (already in network byte order)
    std::memcpy(&header, buffer, sizeof(TLVHeader));
    
    // Validate header
    return header.validate();
}

/**
 * @brief Parse Management TLV from buffer
 * @param buffer Input buffer containing Management TLV payload (after TLV header)
 * @param buffer_size Size of input buffer
 * @param mgmt_tlv Output Management TLV structure
 * @return Success/failure result
 * @note IEEE 1588-2019 Section 15.5.4.1 - MANAGEMENT TLV format
 */
inline Types::PTPResult<void> parse_management_tlv(const std::uint8_t* buffer,
                                                   std::size_t buffer_size,
                                                   ManagementTLV& mgmt_tlv) noexcept {
    if (buffer == nullptr || buffer_size < sizeof(ManagementTLV)) {
        return Types::PTPResult<void>::failure(Types::PTPError::INVALID_LENGTH);
    }
    
    // Copy Management TLV header (managementId is in network byte order)
    std::memcpy(&mgmt_tlv, buffer, sizeof(ManagementTLV));
    
    return Types::PTPResult<void>::success();
}

/**
 * @brief Validate TLV length field against buffer bounds
 * @param tlv_length Length field from TLV header (host byte order)
 * @param available_size Available buffer size for TLV payload
 * @return Success/failure result
 * @note IEEE 1588-2019 Section 14.2 - lengthField validation
 */
inline Types::PTPResult<void> validate_tlv_length(std::uint16_t tlv_length,
                                                  std::size_t available_size) noexcept {
    if (tlv_length > available_size) {
        return Types::PTPResult<void>::failure(Types::PTPError::INVALID_LENGTH);
    }
    
    // Additional sanity check: TLV length should not exceed practical limits
    if (tlv_length > 1500) {  // Ethernet MTU limit
        return Types::PTPResult<void>::failure(Types::PTPError::INVALID_LENGTH);
    }
    
    return Types::PTPResult<void>::success();
}

/**
 * @brief PTP Port State Machine
 * @details Implementation of IEEE 1588-2019 port state machine with
 *          deterministic state transitions and bounded execution time.
 * 
 * Implements state machine per IEEE 1588-2019 Figure 9-1 with:
 * - Deterministic state transitions
 * - Non-blocking message processing
 * - Bounded memory usage
 * - Hardware timestamping integration
 */
class PtpPort {
public:
    /** Default constructor for container initialization (non-operational) */
    PtpPort() noexcept;
    /**
     * @brief Construct PTP port with deterministic configuration
     * @param config Port configuration (must be valid)
     * @param callbacks Hardware abstraction callbacks
     */
    explicit PtpPort(const PortConfiguration& config, 
                     const StateCallbacks& callbacks) noexcept;
    
    /** Non-copyable for deterministic resource management */
    PtpPort(const PtpPort&) = delete;
    PtpPort& operator=(const PtpPort&) = delete;
    
    /** Movable for efficient initialization */
    PtpPort(PtpPort&&) noexcept = default;
    PtpPort& operator=(PtpPort&&) noexcept = default;
    
    /** Destructor ensures clean resource release */
    ~PtpPort() noexcept = default;
    
    // State machine control (deterministic operations)
    
    /**
     * @brief Initialize port state machine
     * @return Success/failure result
     * @post Port state is INITIALIZING
     */
    Types::PTPResult<void> initialize() noexcept;
    
    /**
     * @brief Start port operation
     * @return Success/failure result
     * @pre Port must be initialized
     */
    Types::PTPResult<void> start() noexcept;
    
    /**
     * @brief Stop port operation
     * @return Success/failure result
     * @post Port enters appropriate quiescent state
     */
    Types::PTPResult<void> stop() noexcept;
    
    /**
     * @brief Process state machine event
     * @param event State machine event to process
     * @return Success/failure result
     * @note Bounded execution time, non-blocking
     */
    Types::PTPResult<void> process_event(StateEvent event) noexcept;
    
    // Message processing (non-blocking, bounded execution time)
    
    /**
     * @brief Process received Announce message
     * @param message Validated Announce message
     * @return Success/failure result
     */
    Types::PTPResult<void> process_announce(const AnnounceMessage& message) noexcept;
    
    /**
     * @brief Process received Sync message
     * @param message Validated Sync message
     * @param rx_timestamp Receive timestamp from hardware
     * @return Success/failure result
     */
    Types::PTPResult<void> process_sync(const SyncMessage& message,
                                       const Types::Timestamp& rx_timestamp) noexcept;
    
    /**
     * @brief Process received Follow_Up message
     * @param message Validated Follow_Up message
     * @return Success/failure result
     */
    Types::PTPResult<void> process_follow_up(const FollowUpMessage& message) noexcept;
    
    /**
     * @brief Process received Delay_Req message
     * @param message Validated Delay_Req message
     * @param rx_timestamp Receive timestamp from hardware
     * @return Success/failure result
     */
    Types::PTPResult<void> process_delay_req(const DelayReqMessage& message,
                                            const Types::Timestamp& rx_timestamp) noexcept;
    
    /**
     * @brief Process received Delay_Resp message
     * @param message Validated Delay_Resp message
     * @return Success/failure result
     */
    Types::PTPResult<void> process_delay_resp(const DelayRespMessage& message) noexcept;
    
    /**
     * @brief Process received Pdelay_Req message (peer delay request)
     * @param message Validated Pdelay_Req message
     * @param rx_timestamp Receive timestamp from hardware (t2)
     * @return Success/failure result
     * @note IEEE 1588-2019 Section 11.4 - Peer delay mechanism
     */
    Types::PTPResult<void> process_pdelay_req(const PdelayReqMessage& message,
                                              const Types::Timestamp& rx_timestamp) noexcept;
    
    /**
     * @brief Process received Pdelay_Resp message (peer delay response)
     * @param message Validated Pdelay_Resp message (contains t2)
     * @param rx_timestamp Receive timestamp from hardware (t4)
     * @return Success/failure result
     * @note IEEE 1588-2019 Section 11.4 - Peer delay mechanism
     */
    Types::PTPResult<void> process_pdelay_resp(const PdelayRespMessage& message,
                                               const Types::Timestamp& rx_timestamp) noexcept;
    
    /**
     * @brief Process received Pdelay_Resp_Follow_Up message (precise t3 timestamp)
     * @param message Validated Pdelay_Resp_Follow_Up message (contains t3)
     * @return Success/failure result
     * @note IEEE 1588-2019 Section 11.4.3 - Two-step peer delay
     */
    Types::PTPResult<void> process_pdelay_resp_follow_up(const PdelayRespFollowUpMessage& message) noexcept;
    
    /**
     * @brief Process Management message (dataset GET/SET operations)
     * @param message Management message to process
     * @param response_buffer Buffer to store response (if GET operation)
     * @param response_size Size of response buffer / bytes written on success
     * @return Success/failure result
     * @note IEEE 1588-2019 Section 15 - Management messages
     * @note This is a minimal implementation supporting basic GET operations
     */
    Types::PTPResult<void> process_management(const ManagementMessage& message,
                                              std::uint8_t* response_buffer,
                                              std::size_t& response_size) noexcept;
    
    // Periodic processing (deterministic timing)
    
    /**
     * @brief Execute periodic state machine tasks
     * @param current_time Current system time
     * @return Success/failure result
     * @note Must be called at regular intervals (e.g., every 125ms)
     */
    Types::PTPResult<void> tick(const Types::Timestamp& current_time) noexcept;
    
    // State queries (deterministic, read-only)
    
    /** Get current port state */
    constexpr PortState get_state() const noexcept { return port_data_set_.port_state; }
    
    /** Get port identity */
    constexpr const Types::PortIdentity& get_identity() const noexcept { 
        return port_data_set_.port_identity; 
    }
    
    /** Get current statistics */
    constexpr const PortStatistics& get_statistics() const noexcept { return statistics_; }
    
    /** Get port configuration */
    constexpr const PortConfiguration& get_configuration() const noexcept { return config_; }
    
    /** Get current data set */
    constexpr const CurrentDataSet& get_current_data_set() const noexcept { return current_data_set_; }
    
    /** Get parent data set */
    constexpr const ParentDataSet& get_parent_data_set() const noexcept { return parent_data_set_; }
    
    /** Get time properties data set per IEEE 1588-2019 Section 8.2.4 */
    constexpr const TimePropertiesDataSet& get_time_properties_data_set() const noexcept { 
        return time_properties_data_set_; 
    }

    /** Get port data set (for dataset/read observability tests) */
    constexpr const PortDataSet& get_port_data_set() const noexcept { return port_data_set_; }
    
    /** Check if port is in master role */
    constexpr bool is_master() const noexcept { 
        return port_data_set_.port_state == PortState::Master; 
    }
    
    /** Check if port is in slave role */
    constexpr bool is_slave() const noexcept { 
        return port_data_set_.port_state == PortState::Slave || 
               port_data_set_.port_state == PortState::Uncalibrated; 
    }
    
    /** Check if port is synchronized */
    constexpr bool is_synchronized() const noexcept { 
        return port_data_set_.port_state == PortState::Slave; 
    }
    
    // Configuration updates (deterministic)
    
    /**
     * @brief Update announce interval
     * @param log_interval Log base 2 interval value
     * @return Success/failure result
     */
    Types::PTPResult<void> set_announce_interval(std::uint8_t log_interval) noexcept;
    
    /**
     * @brief Update sync interval
     * @param log_interval Log base 2 interval value
     * @return Success/failure result
     */
    Types::PTPResult<void> set_sync_interval(std::uint8_t log_interval) noexcept;
    
    /**
     * @brief Clear all statistics counters
     */
    void clear_statistics() noexcept { statistics_.reset(); }

private:
    // Configuration and state (POD for deterministic access)
    PortConfiguration config_;
    StateCallbacks callbacks_;
    PortDataSet port_data_set_;
    CurrentDataSet current_data_set_;
    ParentDataSet parent_data_set_;
    TimePropertiesDataSet time_properties_data_set_;
    PortStatistics statistics_;
    
    // Timing state (bounded precision)
    Types::Timestamp last_announce_time_{};
    Types::Timestamp last_sync_time_{};
    Types::Timestamp last_delay_req_time_{};
    Types::Timestamp announce_timeout_time_{};
    Types::Timestamp sync_timeout_time_{};
    // Health heartbeat throttling
    Types::Timestamp last_health_emit_time_{};
    std::uint16_t announce_sequence_id_{0};
    std::uint16_t sync_sequence_id_{0};
    std::uint16_t delay_req_sequence_id_{0};

    // Offset/delay calculation timestamps (T1..T4 per IEEE 1588-2019 Section 11.3)
    Types::Timestamp sync_origin_timestamp_{};      // T1 precise origin timestamp (from Follow_Up)
    Types::Timestamp sync_rx_timestamp_{};          // T2 local receive timestamp of Sync
    Types::Timestamp delay_req_tx_timestamp_{};     // T3 local transmit timestamp of Delay_Req
    Types::Timestamp delay_resp_rx_timestamp_{};    // T4 master receive timestamp of Delay_Req (from Delay_Resp)
    
    // CorrectionField accumulation per IEEE 1588-2019 Section 11.3.2
    Types::TimeInterval sync_correction_{};         // Correction from Sync message
    Types::TimeInterval follow_up_correction_{};    // Correction from Follow_Up message
    Types::TimeInterval delay_resp_correction_{};   // Correction from Delay_Resp message
    
    bool have_sync_{false};
    bool have_follow_up_{false};
    bool have_delay_req_{false};
    bool have_delay_resp_{false};
    // Local heuristic counter: successful offsets computed while in UNCALIBRATED
    std::uint32_t successful_offsets_in_window_{0};
    
    // Peer delay mechanism timestamps (per IEEE 1588-2019 Section 11.4)
    Types::Timestamp pdelay_req_tx_timestamp_{};       // t1 local transmit timestamp of Pdelay_Req
    Types::Timestamp pdelay_req_rx_timestamp_{};       // t2 peer receive timestamp of Pdelay_Req (from Pdelay_Resp)
    Types::Timestamp pdelay_resp_tx_timestamp_{};      // t3 peer transmit timestamp of Pdelay_Resp (from Follow_Up)
    Types::Timestamp pdelay_resp_rx_timestamp_{};      // t4 local receive timestamp of Pdelay_Resp
    
    // Peer delay correctionField accumulation per IEEE 1588-2019 Section 11.4.2
    Types::TimeInterval pdelay_resp_correction_{};     // Correction from Pdelay_Resp message
    Types::TimeInterval pdelay_resp_follow_up_correction_{}; // Correction from Pdelay_Resp_Follow_Up message
    
    bool have_pdelay_req_{false};                      // Sent Pdelay_Req, waiting for response
    bool have_pdelay_resp_{false};                     // Received Pdelay_Resp (has t2, t4)
    bool have_pdelay_resp_follow_up_{false};           // Received Pdelay_Resp_Follow_Up (has t3)
    std::uint16_t pdelay_req_sequence_id_{0};          // Sequence ID for Pdelay_Req messages
    
    // BMCA state (limited storage for deterministic operation)
    static constexpr size_t MAX_FOREIGN_MASTERS = 16;
    std::array<AnnounceMessage, MAX_FOREIGN_MASTERS> foreign_masters_;
    std::array<Types::Timestamp, MAX_FOREIGN_MASTERS> foreign_master_timestamps_;
    std::uint8_t foreign_master_count_{0};
    
    // Internal state machine operations (deterministic)
    Types::PTPResult<void> transition_to_state(PortState new_state) noexcept;
    Types::PTPResult<void> execute_state_actions() noexcept;
    Types::PTPResult<void> send_announce_message() noexcept;
    Types::PTPResult<void> send_sync_message() noexcept;
    Types::PTPResult<void> send_delay_req_message() noexcept;
    Types::PTPResult<void> check_timeouts(const Types::Timestamp& current_time) noexcept;
    Types::PTPResult<void> run_bmca() noexcept;
    BMCADecision compare_announce_messages(const AnnounceMessage& local,
                                         const AnnounceMessage& foreign) const noexcept;
    Types::PTPResult<void> update_foreign_master_list(const AnnounceMessage& message) noexcept;
    Types::PTPResult<void> prune_expired_foreign_masters(const Types::Timestamp& current_time) noexcept;
    Types::PTPResult<void> calculate_offset_and_delay() noexcept;
    Types::PTPResult<void> calculate_peer_delay() noexcept;
    
    // Time interval calculations (bounded execution time)
    constexpr Types::TimeInterval time_interval_for_log_interval(std::uint8_t log_interval,
                                                                 std::uint8_t multiplier = 1) const noexcept {
        // Duration = (2^log_interval) seconds * multiplier
        // Convert seconds to nanoseconds then to scaled TimeInterval
        const std::uint64_t seconds = (1ULL << log_interval) * static_cast<std::uint64_t>(multiplier);
        const double ns = static_cast<double>(seconds) * 1'000'000'000.0;
        return Types::TimeInterval::fromNanoseconds(ns);
    }
    
    inline bool is_timeout_expired(const Types::Timestamp& last_time,
                                   const Types::Timestamp& current_time,
                                   const Types::TimeInterval& timeout_interval) const noexcept {
        // Calculate elapsed time and compare with timeout
        Types::TimeInterval elapsed = current_time - last_time;
        return elapsed.toNanoseconds() >= timeout_interval.toNanoseconds();
    }
};

/**
 * @brief Ordinary Clock State Machine
 * @details IEEE 1588-2019 Ordinary Clock implementation with single port
 *          and deterministic state management per Section 6.5.2.
 */
class OrdinaryClock {
public:
    /**
     * @brief Construct Ordinary Clock
     * @param port_config Configuration for the single PTP port
     * @param callbacks Hardware abstraction callbacks
     */
    explicit OrdinaryClock(const PortConfiguration& port_config,
                          const StateCallbacks& callbacks) noexcept;
    
    /** Non-copyable for deterministic resource management */
    OrdinaryClock(const OrdinaryClock&) = delete;
    OrdinaryClock& operator=(const OrdinaryClock&) = delete;
    
    /** Movable for efficient initialization */
    OrdinaryClock(OrdinaryClock&&) noexcept = default;
    OrdinaryClock& operator=(OrdinaryClock&&) noexcept = default;
    
    /** Destructor ensures clean resource release */
    ~OrdinaryClock() noexcept = default;
    
    // Clock control operations
    
    /**
     * @brief Initialize ordinary clock
     * @return Success/failure result
     */
    Types::PTPResult<void> initialize() noexcept;
    
    /**
     * @brief Start clock operation
     * @return Success/failure result
     */
    Types::PTPResult<void> start() noexcept;
    
    /**
     * @brief Stop clock operation
     * @return Success/failure result
     */
    Types::PTPResult<void> stop() noexcept;
    
    // Message processing delegation
    
    /**
     * @brief Process received PTP message
     * @param message_type Type of PTP message
     * @param message_data Message data buffer
     * @param message_size Size of message data
     * @param rx_timestamp Hardware receive timestamp
     * @return Success/failure result
     */
    Types::PTPResult<void> process_message(std::uint8_t message_type,
                                          const void* message_data,
                                          std::size_t message_size,
                                          const Types::Timestamp& rx_timestamp) noexcept;
    
    // Periodic processing
    
    /**
     * @brief Execute periodic clock tasks
     * @param current_time Current system time
     * @return Success/failure result
     */
    Types::PTPResult<void> tick(const Types::Timestamp& current_time) noexcept;
    
    // State queries
    
    /** Get the single PTP port */
    constexpr const PtpPort& get_port() const noexcept { return port_; }
    
    /** Get clock type */
    constexpr ClockType get_clock_type() const noexcept { return ClockType::Ordinary; }
    
    /** Check if clock is master */
    constexpr bool is_master() const noexcept { return port_.is_master(); }
    
    /** Check if clock is slave */
    constexpr bool is_slave() const noexcept { return port_.is_slave(); }
    
    /** Check if clock is synchronized */
    constexpr bool is_synchronized() const noexcept { return port_.is_synchronized(); }
    
    /** Get time properties data set per IEEE 1588-2019 Section 8.2.4 */
    constexpr const TimePropertiesDataSet& get_time_properties_data_set() const noexcept {
        return port_.get_time_properties_data_set();
    }

private:
    PtpPort port_;  ///< Single port for Ordinary Clock
};

/**
 * @brief Boundary Clock State Machine
 * @details IEEE 1588-2019 Boundary Clock implementation with multiple ports
 *          and deterministic state coordination per Section 6.5.3.
 */
class BoundaryClock {
public:
    static constexpr std::size_t MAX_PORTS = 8;  ///< Maximum ports for deterministic arrays
    
    /**
     * @brief Construct Boundary Clock
     * @param port_configs Array of port configurations
     * @param port_count Number of active ports
     * @param callbacks Hardware abstraction callbacks
     */
    explicit BoundaryClock(const std::array<PortConfiguration, MAX_PORTS>& port_configs,
                          std::size_t port_count,
                          const StateCallbacks& callbacks) noexcept;
    
    /** Non-copyable for deterministic resource management */
    BoundaryClock(const BoundaryClock&) = delete;
    BoundaryClock& operator=(const BoundaryClock&) = delete;
    
    /** Movable for efficient initialization */
    BoundaryClock(BoundaryClock&&) noexcept = default;
    BoundaryClock& operator=(BoundaryClock&&) noexcept = default;
    
    /** Destructor ensures clean resource release */
    ~BoundaryClock() noexcept = default;
    
    // Clock control operations
    
    /**
     * @brief Initialize boundary clock
     * @return Success/failure result
     */
    Types::PTPResult<void> initialize() noexcept;
    
    /**
     * @brief Start clock operation
     * @return Success/failure result
     */
    Types::PTPResult<void> start() noexcept;
    
    /**
     * @brief Stop clock operation
     * @return Success/failure result
     */
    Types::PTPResult<void> stop() noexcept;
    
    // Message processing
    
    /**
     * @brief Process received PTP message on specific port
     * @param port_number Port that received the message
     * @param message_type Type of PTP message
     * @param message_data Message data buffer
     * @param message_size Size of message data
     * @param rx_timestamp Hardware receive timestamp
     * @return Success/failure result
     */
    Types::PTPResult<void> process_message(Types::PortNumber port_number,
                                          std::uint8_t message_type,
                                          const void* message_data,
                                          std::size_t message_size,
                                          const Types::Timestamp& rx_timestamp) noexcept;
    
    // Periodic processing
    
    /**
     * @brief Execute periodic clock tasks
     * @param current_time Current system time
     * @return Success/failure result
     */
    Types::PTPResult<void> tick(const Types::Timestamp& current_time) noexcept;
    
    // State queries
    
    /** Get number of active ports */
    constexpr std::size_t get_port_count() const noexcept { return port_count_; }
    
    /** Get specific port by number */
    const PtpPort* get_port(Types::PortNumber port_number) const noexcept;
    
    /** Get clock type */
    constexpr ClockType get_clock_type() const noexcept { return ClockType::Boundary; }
    
    /** Check if any port is master */
    bool has_master_port() const noexcept;
    
    /** Check if any port is slave */
    bool has_slave_port() const noexcept;
    
    /** Check if clock is synchronized (has synchronized slave port) */
    bool is_synchronized() const noexcept;

private:
    std::array<PtpPort, MAX_PORTS> ports_;
    std::size_t port_count_;
    
    // Find port by number (deterministic search)
    PtpPort* find_port(Types::PortNumber port_number) noexcept;
    const PtpPort* find_port(Types::PortNumber port_number) const noexcept;
};

/**
 * @brief Transparent Clock State Machine
 * @details IEEE 1588-2019 Transparent Clock implementation with residence
 *          time correction per Section 6.5.4 and 6.5.5.
 */
class TransparentClock {
public:
    static constexpr std::size_t MAX_PORTS = 16;  ///< Maximum ports for transparent clock
    
    /**
     * @brief Transparent Clock Types
     */
    enum class TransparentType : std::uint8_t {
        END_TO_END   = 0x00,  ///< End-to-End Transparent Clock (E2E TC)
        PEER_TO_PEER = 0x01   ///< Peer-to-Peer Transparent Clock (P2P TC)
    };
    
    /**
     * @brief Construct Transparent Clock
     * @param type Type of transparent clock (E2E or P2P)
     * @param port_configs Array of port configurations
     * @param port_count Number of active ports
     * @param callbacks Hardware abstraction callbacks
     */
    explicit TransparentClock(TransparentType type,
                             const std::array<PortConfiguration, MAX_PORTS>& port_configs,
                             std::size_t port_count,
                             const StateCallbacks& callbacks) noexcept;
    
    /** Non-copyable for deterministic resource management */
    TransparentClock(const TransparentClock&) = delete;
    TransparentClock& operator=(const TransparentClock&) = delete;
    
    /** Movable for efficient initialization */
    TransparentClock(TransparentClock&&) noexcept = default;
    TransparentClock& operator=(TransparentClock&&) noexcept = default;
    
    /** Destructor ensures clean resource release */
    ~TransparentClock() noexcept = default;
    
    // Clock control operations
    
    /**
     * @brief Initialize transparent clock
     * @return Success/failure result
     */
    Types::PTPResult<void> initialize() noexcept;
    
    /**
     * @brief Start clock operation
     * @return Success/failure result
     */
    Types::PTPResult<void> start() noexcept;
    
    /**
     * @brief Stop clock operation
     * @return Success/failure result
     */
    Types::PTPResult<void> stop() noexcept;
    
    // Message forwarding with residence time correction
    
    /**
     * @brief Forward PTP message with residence time correction
     * @param ingress_port Port that received the message
     * @param egress_port Port to forward the message to
     * @param message_data Message data buffer (will be modified)
     * @param message_size Size of message data
     * @param ingress_timestamp Message ingress timestamp
     * @param egress_timestamp Message egress timestamp
     * @return Success/failure result
     */
    Types::PTPResult<void> forward_message(Types::PortNumber ingress_port,
                                          Types::PortNumber egress_port,
                                          void* message_data,
                                          std::size_t message_size,
                                          const Types::Timestamp& ingress_timestamp,
                                          const Types::Timestamp& egress_timestamp) noexcept;
    
    // State queries
    
    /** Get transparent clock type */
    constexpr TransparentType get_transparent_type() const noexcept { return type_; }
    
    /** Get clock type */
    constexpr ClockType get_clock_type() const noexcept { 
        return (type_ == TransparentType::END_TO_END) ? 
               ClockType::E2E_Transparent : 
               ClockType::P2P_Transparent; 
    }
    
    /** Get number of active ports */
    constexpr std::size_t get_port_count() const noexcept { return port_count_; }

private:
    TransparentType type_;
    std::array<PortConfiguration, MAX_PORTS> port_configs_;
    std::size_t port_count_;
    StateCallbacks callbacks_;
    
    // Residence time calculation (bounded execution time)
    Types::PTPResult<Types::TimeInterval> calculate_residence_time(
        const Types::Timestamp& ingress_timestamp,
        const Types::Timestamp& egress_timestamp) const noexcept;
    
    // Correction field update (deterministic operation)
    Types::PTPResult<void> update_correction_field(void* message_data,
                                                   std::size_t message_size,
                                                   Types::TimeInterval residence_time) const noexcept;
};

} // namespace Clocks
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE