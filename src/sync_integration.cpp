/**
 * @file sync_integration.cpp
 * @brief IEEE 1588-2019 Synchronization Integration Implementation
 */

#include "IEEE/1588/PTP/2019/sync_integration.hpp"
#include <cmath>
#include <algorithm>

using namespace IEEE::_1588::PTP::_2019;

Types::PTPResult<void> SyncIntegration::tick(const Types::Timestamp& current_time) noexcept {
    if (!is_running_) {
        return Types::PTPResult<void>::failure(Types::PTPError::State_Error);
    }
    
    // Check if sampling interval has elapsed
    bool should_sample = false;
    if (!first_sample_done_) {
        should_sample = true; // First sample - use flag, not timestamp==0
    } else {
        auto elapsed = current_time - last_sample_time_;
        std::uint64_t elapsed_ms = static_cast<std::uint64_t>(elapsed.toNanoseconds() / 1'000'000.0);
        should_sample = (elapsed_ms >= config_.sampling_interval_ms);
    }
    
    if (should_sample) {
        auto result = collect_sample(current_time);
        if (!result.is_success()) {
            return result;
        }
        last_sample_time_ = current_time;
        first_sample_done_ = true;  // Mark first sample as complete
    }
    
    // Update health status periodically
    if (config_.enable_health_monitoring) {
        update_health_status(current_time);
    }
    
    return Types::PTPResult<void>::success();
}

Types::PTPResult<void> SyncIntegration::sample_now(const Types::Timestamp& current_time) noexcept {
    if (!is_running_) {
        return Types::PTPResult<void>::failure(Types::PTPError::State_Error);
    }
    
    return collect_sample(current_time);
}

Types::PTPResult<void> SyncIntegration::collect_sample(const Types::Timestamp& current_time) noexcept {
    // Get current data set from port
    auto current_ds = port_.get_current_data_set();
    auto config = port_.get_configuration();
    
    // Extract offset and delay values
    double offset_ns = current_ds.offset_from_master.toNanoseconds();
    double delay_ns = current_ds.mean_path_delay.toNanoseconds();
    
    // Track which mechanism is being used
    statistics_.using_p2p_delay = config.delay_mechanism_p2p;
    
    // Update statistics
    update_statistics(offset_ns, delay_ns);
    
    // Update mechanism counters
    if (statistics_.using_p2p_delay) {
        statistics_.p2p_measurements++;
    } else {
        statistics_.e2e_measurements++;
    }
    
    // TODO: Task 3 - Trigger servo adjustment with offset_ns
    // if (config_.enable_servo) {
    //     servo_.adjust(offset_ns);
    // }
    
    return Types::PTPResult<void>::success();
}

void SyncIntegration::update_statistics(double offset_ns, double delay_ns) noexcept {
    // Update offset statistics
    statistics_.total_offset_samples++;
    statistics_.current_offset_ns = offset_ns;
    
    // Track min/max offset
    if (statistics_.total_offset_samples == 1) {
        statistics_.min_offset_ns = offset_ns;
        statistics_.max_offset_ns = offset_ns;
        statistics_.avg_offset_ns = offset_ns;
    } else {
        statistics_.min_offset_ns = std::min(statistics_.min_offset_ns, offset_ns);
        statistics_.max_offset_ns = std::max(statistics_.max_offset_ns, offset_ns);
        
        // Running average (incremental calculation)
        double delta = offset_ns - statistics_.avg_offset_ns;
        statistics_.avg_offset_ns += delta / statistics_.total_offset_samples;
    }
    
    // Track sub-microsecond accuracy
    if (std::abs(offset_ns) < 1000.0) { // < 1µs
        statistics_.sub_microsecond_samples++;
    }
    
    // Update delay statistics
    if (delay_ns >= 0.0) { // Valid delay
        statistics_.total_delay_samples++;
        statistics_.current_delay_ns = delay_ns;
        
        if (statistics_.total_delay_samples == 1) {
            statistics_.min_delay_ns = delay_ns;
            statistics_.max_delay_ns = delay_ns;
            statistics_.avg_delay_ns = delay_ns;
        } else {
            statistics_.min_delay_ns = std::min(statistics_.min_delay_ns, delay_ns);
            statistics_.max_delay_ns = std::max(statistics_.max_delay_ns, delay_ns);
            
            double delta_delay = delay_ns - statistics_.avg_delay_ns;
            statistics_.avg_delay_ns += delta_delay / statistics_.total_delay_samples;
        }
    } else {
        statistics_.negative_delay_count++;
    }
    
    // Update variance calculation (rolling window)
    offset_samples_.push_back(offset_ns);
    
    // Keep only recent samples for variance
    if (offset_samples_.size() > config_.variance_window_samples) {
        offset_samples_.erase(offset_samples_.begin());
    }
    
    // Calculate variance if we have enough samples
    if (offset_samples_.size() >= 2) {
        calculate_variance();
    }
}

void SyncIntegration::calculate_variance() noexcept {
    // Calculate mean of samples
    double sum = 0.0;
    for (double sample : offset_samples_) {
        sum += sample;
    }
    double mean = sum / offset_samples_.size();
    
    // Calculate variance
    double variance_sum = 0.0;
    for (double sample : offset_samples_) {
        double diff = sample - mean;
        variance_sum += diff * diff;
    }
    
    statistics_.offset_variance_ns2 = variance_sum / offset_samples_.size();
    statistics_.offset_std_dev_ns = std::sqrt(statistics_.offset_variance_ns2);
}

void SyncIntegration::update_health_status(const Types::Timestamp& current_time) noexcept {
    // Update timestamp
    health_.timestamp_ns = current_time.getTotalSeconds() * 1'000'000'000ULL + current_time.nanoseconds;
    
    // Start with critical status, improve based on conditions
    health_.status = SyncHealthStatus::Status::Critical;
    health_.message.clear();
    
    // Check if we have any samples
    if (statistics_.total_offset_samples == 0) {
        health_.message = "No synchronization samples yet";
        health_.offset_within_spec = false;
        health_.delay_stable = false;
        health_.measurements_valid = false;
        return;
    }
    
    // Check offset accuracy
    double abs_offset = std::abs(statistics_.current_offset_ns);
    
    if (abs_offset < config_.synchronized_threshold_ns) {
        health_.offset_within_spec = true;
        health_.status = SyncHealthStatus::Status::Synchronized;
        health_.message = "Synchronized (< 1µs)";
    } else if (abs_offset < config_.degraded_threshold_ns) {
        health_.offset_within_spec = false;
        health_.status = SyncHealthStatus::Status::Converging;
        health_.message = "Converging (offset < 10µs)";
    } else if (abs_offset < config_.critical_threshold_ns) {
        health_.offset_within_spec = false;
        health_.status = SyncHealthStatus::Status::Degraded;
        health_.message = "Degraded (offset < 100µs)";
    } else {
        health_.offset_within_spec = false;
        health_.status = SyncHealthStatus::Status::Critical;
        health_.message = "Critical (offset > 100µs)";
    }
    
    // Check delay stability (low variance = stable)
    if (statistics_.offset_std_dev_ns < 500.0) { // < 500ns std dev
        health_.delay_stable = true;
    } else {
        health_.delay_stable = false;
        if (health_.status == SyncHealthStatus::Status::Synchronized) {
            health_.status = SyncHealthStatus::Status::Converging;
            health_.message = "Converging (high variance)";
        }
    }
    
    // Check measurement validity
    double error_rate = 0.0;
    if (statistics_.total_offset_samples > 0) {
        error_rate = static_cast<double>(statistics_.negative_delay_count + 
                                        statistics_.timestamp_order_violations) / 
                     statistics_.total_offset_samples;
    }
    
    if (error_rate < 0.01) { // < 1% error rate
        health_.measurements_valid = true;
    } else {
        health_.measurements_valid = false;
        if (health_.status != SyncHealthStatus::Status::Critical) {
            health_.status = SyncHealthStatus::Status::Degraded;
            health_.message = "Degraded (measurement errors)";
        }
    }
    
    // TODO: Task 3 - Check servo lock status
    health_.servo_locked = false; // Not yet implemented
}
