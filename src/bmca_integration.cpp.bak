/**
 * @file bmca_integration.cpp
 * @brief BMCA Runtime Integration Coordinator Implementation
 * 
 * Phase: 06-integration
 * Task: Task 1 - BMCA Integration
 * 
 * Implements BMCA execution coordination, metrics collection, and health monitoring.
 * 
 * @see bmca_integration.hpp for interface documentation
 */

#include "IEEE/1588/PTP/2019/bmca_integration.hpp"
#include "Common/utils/logger.hpp"
#include "Common/utils/metrics.hpp"
#include <cstring>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Integration {

Types::PTPResult<void> BMCAIntegration::tick(const Types::Timestamp& current_time) noexcept {
    if (!is_running_) {
        return Types::PTPResult<void>::failure(Types::PTPError::State_Error);
    }
    
    // Check if execution interval expired
    if (!config_.enable_periodic_execution) {
        return Types::PTPResult<void>::success(); // Periodic execution disabled
    }
    
    // Calculate elapsed time since last execution
    auto elapsed = current_time - last_execution_time_;
    std::uint64_t elapsed_ms = static_cast<std::uint64_t>(elapsed.toNanoseconds() / 1'000'000.0);
    
    if (elapsed_ms >= config_.execution_interval_ms) {
        auto result = execute_bmca_internal(current_time);
        if (!result.is_success()) {
            return result;
        }
        
        last_execution_time_ = current_time;
    }
    
    // Update health status periodically
    if (config_.enable_health_monitoring) {
        update_health_status(current_time);
    }
    
    return Types::PTPResult<void>::success();
}

Types::PTPResult<void> BMCAIntegration::execute_bmca(const Types::Timestamp& current_time) noexcept {
    if (!is_running_) {
        return Types::PTPResult<void>::failure(Types::PTPError::State_Error);
    }
    
    return execute_bmca_internal(current_time);
}

Types::PTPResult<void> BMCAIntegration::execute_bmca_internal(const Types::Timestamp& current_time) noexcept {
    // Measure execution time
    auto start_time = current_time;
    
    // Capture state before BMCA
    auto role_before = port_.get_port_state();
    Types::ClockIdentity parent_before{};
    std::memcpy(parent_before, port_.get_parent_data_set().grandmasterIdentity, 8);
    
    // Execute BMCA via port
    auto bmca_result = port_.run_bmca();
    
    // Measure execution duration (note: in real system, would use high-res timer)
    auto end_time = current_time; // Simplified for now
    std::uint64_t duration_ns = 0; // Would calculate from start_time to end_time
    
    // Update execution statistics
    statistics_.total_executions++;
    statistics_.last_execution_duration_ns = duration_ns;
    if (duration_ns > statistics_.max_execution_duration_ns) {
        statistics_.max_execution_duration_ns = duration_ns;
    }
    
    // Track foreign master count
    auto foreign_count = port_.get_foreign_master_count();
    statistics_.current_foreign_count = foreign_count;
    if (foreign_count > statistics_.max_foreign_count) {
        statistics_.max_foreign_count = foreign_count;
    }
    if (foreign_count == 0) {
        statistics_.no_foreign_masters++;
    }
    
    // Capture state after BMCA
    auto role_after = port_.get_port_state();
    Types::ClockIdentity parent_after{};
    std::memcpy(parent_after, port_.get_parent_data_set().grandmasterIdentity, 8);
    
    // Detect role changes
    if (role_before != role_after) {
        statistics_.role_changes++;
        
        // Track specific role selections
        if (role_after == Clocks::PortState::Master || 
            role_after == Clocks::PortState::PreMaster) {
            statistics_.master_selections++;
            Common::utils::metrics::increment(Common::utils::metrics::CounterId::BMCA_LocalWins, 1);
        } else if (role_after == Clocks::PortState::Slave ||
                   role_after == Clocks::PortState::Uncalibrated) {
            statistics_.slave_selections++;
            Common::utils::metrics::increment(Common::utils::metrics::CounterId::BMCA_ForeignWins, 1);
        } else if (role_after == Clocks::PortState::Passive) {
            statistics_.passive_selections++;
            Common::utils::metrics::increment(Common::utils::metrics::CounterId::BMCA_PassiveWins, 1);
        }
        
        Common::utils::logging::info("BMCA", 0x0601, 
            "Role changed: %d -> %d", 
            static_cast<int>(role_before), 
            static_cast<int>(role_after));
    }
    
    // Detect parent (GM) changes
    bool parent_changed = false;
    for (int i = 0; i < 8; ++i) {
        if (parent_before[i] != parent_after[i]) {
            parent_changed = true;
            break;
        }
    }
    if (parent_changed) {
        statistics_.parent_changes++;
        std::memcpy(last_parent_identity_, parent_after, 8);
        
        Common::utils::logging::info("BMCA", 0x0602, 
            "Parent (GM) changed - new selections: %llu", 
            static_cast<unsigned long long>(statistics_.parent_changes));
    }
    
    // Update last known state
    last_role_ = role_after;
    
    // Update metrics
    Common::utils::metrics::set_gauge(Common::utils::metrics::GaugeId::BMCA_ExecutionCount, 
                                      static_cast<double>(statistics_.total_executions));
    Common::utils::metrics::set_gauge(Common::utils::metrics::GaugeId::ForeignMasterCount, 
                                      static_cast<double>(foreign_count));
    
    return bmca_result;
}

void BMCAIntegration::update_health_status(const Types::Timestamp& current_time) noexcept {
    health_.timestamp_ns = current_time.getTotalSeconds() * 1'000'000'000ULL + 
                           current_time.nanoseconds;
    
    // Check for oscillation
    health_.excessive_oscillation = detect_oscillation();
    
    // Check execution time
    health_.slow_execution = (statistics_.last_execution_duration_ns > 
                              config_.max_execution_time_us * 1000ULL);
    
    // Check for no foreign masters
    health_.no_candidates = (statistics_.current_foreign_count == 0);
    
    // Check for stale foreign list
    health_.stale_foreign_list = is_foreign_list_stale(current_time);
    
    // Determine overall status
    if (health_.no_candidates || health_.excessive_oscillation) {
        health_.status = BMCAHealthStatus::Status::Critical;
        health_.message = "BMCA critical: ";
        if (health_.no_candidates) health_.message += "no foreign masters; ";
        if (health_.excessive_oscillation) health_.message += "excessive oscillation; ";
    } else if (health_.slow_execution || health_.stale_foreign_list) {
        health_.status = BMCAHealthStatus::Status::Degraded;
        health_.message = "BMCA degraded: ";
        if (health_.slow_execution) health_.message += "slow execution; ";
        if (health_.stale_foreign_list) health_.message += "stale foreign list; ";
    } else {
        health_.status = BMCAHealthStatus::Status::Healthy;
        health_.message = "BMCA operating normally";
    }
    
    // Emit health report if not healthy
    if (health_.status != BMCAHealthStatus::Status::Healthy) {
        Common::utils::health::emit(Common::utils::health::SelfTestReport{
            .test_name = "BMCA Health",
            .passed = (health_.status == BMCAHealthStatus::Status::Degraded),
            .message = health_.message
        });
    }
}

bool BMCAIntegration::detect_oscillation() const noexcept {
    // Detect rapid role changes (>10 per minute is considered oscillation)
    // This is a simplified check - real implementation would track time window
    
    // For now, check if role changes are excessive relative to total executions
    if (statistics_.total_executions < 60) {
        return false; // Not enough data yet
    }
    
    // Calculate changes per execution (normalized to 60 executions = 1 minute at 1Hz)
    double changes_per_minute = static_cast<double>(statistics_.role_changes) / 
                                 static_cast<double>(statistics_.total_executions) * 60.0;
    
    return (changes_per_minute > config_.oscillation_threshold);
}

bool BMCAIntegration::is_foreign_list_stale(const Types::Timestamp& current_time) const noexcept {
    // Check if foreign master list hasn't been updated recently
    // This would require tracking last_foreign_master_update_time
    // For now, return false (requires additional port API)
    
    // TODO: Add last_foreign_master_update_time tracking to PtpPort
    // and implement staleness detection here
    
    return false;
}

} // namespace Integration
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
