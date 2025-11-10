/**
 * @file servo_integration.cpp
 * @brief IEEE 1588-2019 Servo Integration Implementation
 * 
 * Implements PI controller for clock synchronization with stability features.
 */

#include "IEEE/1588/PTP/2019/servo_integration.hpp"
#include <cmath>
#include <algorithm>

namespace IEEE {
namespace _1588 {
namespace _2019 {
namespace servo {

//==============================================================================
// Lifecycle Management
//==============================================================================

bool ServoIntegration::start() {
    if (is_running_) return false;
    
    // Reset state machine
    health_.state = ServoState::Unlocked;
    health_.message = "Servo started, waiting for offset samples";
    health_.adjustments_active = false;
    health_.frequency_stable = false;
    health_.phase_locked = false;
    health_.within_spec = false;
    
    // Reset timing
    last_update_time_ns_ = 0;
    state_entry_time_ns_ = 0;
    consecutive_samples_in_threshold_ = 0;
    first_sample_done_ = false;
    
    // Reset PI controller state
    statistics_.integral_error = 0.0;
    last_freq_adjustment_ppb_ = 0.0;
    
    is_running_ = true;
    return true;
}

void ServoIntegration::stop() {
    is_running_ = false;
    health_.adjustments_active = false;
    health_.message = "Servo stopped";
}

void ServoIntegration::reset() {
    // Reset statistics (but preserve state if running)
    statistics_ = ServoStatistics{};
    
    // Reset PI controller
    statistics_.integral_error = 0.0;
    last_freq_adjustment_ppb_ = 0.0;
    consecutive_samples_in_threshold_ = 0;
    
    // If not running, reset state machine too
    if (!is_running_) {
        health_ = ServoHealthStatus{};
        first_sample_done_ = false;
    }
}

//==============================================================================
// Main Control Loop
//==============================================================================

bool ServoIntegration::adjust(double offset_ns, std::uint64_t current_time_ns) {
    if (!is_running_) return false;
    
    // Track timing for holdover detection
    if (!first_sample_done_) {
        last_update_time_ns_ = current_time_ns;
        state_entry_time_ns_ = current_time_ns;
        first_sample_done_ = true;
    }
    
    // Update state machine based on current offset
    update_state_machine(offset_ns, current_time_ns);
    
    // Calculate PI controller output (frequency adjustment)
    double freq_adjustment_ppb = calculate_pi_output(offset_ns);
    
    // Apply rate limiting for stability
    if (config_.enable_rate_limiting) {
        freq_adjustment_ppb = apply_rate_limiting(freq_adjustment_ppb);
    }
    
    // Apply anti-windup to prevent integral explosion
    if (config_.enable_anti_windup) {
        apply_anti_windup();
    }
    
    // Decide: Step (large offset) vs. Slew (small offset)
    bool should_step = std::abs(offset_ns) > config_.step_threshold_ns;
    
    if (should_step && health_.state != ServoState::Locked) {
        // Phase step for large offsets (when not locked)
        if (callbacks_.adjust_clock) {
            auto result = callbacks_.adjust_clock(static_cast<std::int64_t>(offset_ns));
            if (result == IEEE::_1588::PTP::_2019::Types::PTPError::Success) {
                statistics_.phase_adjustments++;
                statistics_.last_phase_adjustment_ns = offset_ns;
            }
        }
    } else {
        // Frequency slew for small offsets or when locked
        if (callbacks_.adjust_frequency) {
            auto result = callbacks_.adjust_frequency(freq_adjustment_ppb);
            if (result == IEEE::_1588::PTP::_2019::Types::PTPError::Success) {
                statistics_.frequency_adjustments++;
                statistics_.last_freq_adjustment_ppb = freq_adjustment_ppb;
                last_freq_adjustment_ppb_ = freq_adjustment_ppb;
            }
        }
    }
    
    // Update statistics and health
    update_statistics(offset_ns, freq_adjustment_ppb);
    update_health_status(current_time_ns);
    
    // Track timing
    last_update_time_ns_ = current_time_ns;
    statistics_.total_adjustments++;
    
    return true;
}

//==============================================================================
// State Machine
//==============================================================================

void ServoIntegration::update_state_machine(double offset_ns, std::uint64_t current_time_ns) {
    double abs_offset = std::abs(offset_ns);
    ServoState old_state = health_.state;
    ServoState new_state = old_state;
    
    // Check for holdover timeout
    if (config_.enable_holdover && first_sample_done_) {
        std::uint64_t time_since_update = (current_time_ns - last_update_time_ns_) / 1000000; // ns to ms
        if (time_since_update > config_.holdover_timeout_ms) {
            new_state = ServoState::Holdover;
        }
    }
    
    // State transitions based on offset magnitude (unless in holdover)
    if (new_state != ServoState::Holdover) {
        if (abs_offset < config_.lock_threshold_ns) {
            // Within lock threshold
            consecutive_samples_in_threshold_++;
            
            if (consecutive_samples_in_threshold_ >= config_.samples_for_lock) {
                new_state = ServoState::Locked;
            } else if (old_state != ServoState::Locked) {
                new_state = ServoState::Locking;
            }
        } else if (abs_offset < config_.locking_threshold_ns) {
            // Within locking threshold
            consecutive_samples_in_threshold_ = 0;
            new_state = ServoState::Locking;
        } else {
            // Outside thresholds
            consecutive_samples_in_threshold_ = 0;
            
            // Only unlock if significantly exceeds threshold
            if (abs_offset > config_.unlock_threshold_ns) {
                new_state = ServoState::Unlocked;
                if (old_state == ServoState::Locked) {
                    statistics_.lock_loss_count++;
                }
            }
        }
    }
    
    // Update state if changed
    if (new_state != old_state) {
        health_.state = new_state;
        state_entry_time_ns_ = current_time_ns;
        
        // Reset integral error on major state changes
        if (new_state == ServoState::Unlocked || new_state == ServoState::Holdover) {
            statistics_.integral_error = 0.0;
        }
    }
}

//==============================================================================
// PI Controller
//==============================================================================

double ServoIntegration::calculate_pi_output(double offset_ns) {
    // Proportional term: Kp * error
    double proportional = config_.kp * offset_ns;
    
    // Integral term: Ki * integral(error)
    // Accumulate error (convert ns to equivalent frequency error)
    // Heuristic: 1ns offset ≈ 1ppb frequency error over 1 second
    statistics_.integral_error += offset_ns;
    double integral = config_.ki * statistics_.integral_error;
    
    // Store terms for statistics
    statistics_.proportional_term = proportional;
    statistics_.integral_term = integral;
    
    // PI output: P + I
    double output_ppb = proportional + integral;
    
    // Clamp to maximum frequency adjustment
    output_ppb = std::max(-config_.max_freq_adjustment_ppb, 
                          std::min(config_.max_freq_adjustment_ppb, output_ppb));
    
    return output_ppb;
}

//==============================================================================
// Stability Features
//==============================================================================

double ServoIntegration::apply_rate_limiting(double requested_ppb) {
    // Calculate maximum allowed change per sample
    // Assume 1 sample per second (typical sync interval)
    double max_change = config_.max_rate_of_change_ppb_per_sec;
    
    double delta = requested_ppb - last_freq_adjustment_ppb_;
    
    if (std::abs(delta) > max_change) {
        // Limit rate of change
        double limited_ppb = last_freq_adjustment_ppb_ + 
                             (delta > 0 ? max_change : -max_change);
        
        statistics_.rate_limit_hits++;
        return limited_ppb;
    }
    
    return requested_ppb;
}

void ServoIntegration::apply_anti_windup() {
    // Clamp integral error to prevent runaway
    double max_integral = config_.integral_limit;
    
    if (std::abs(statistics_.integral_error) > max_integral) {
        statistics_.integral_error = (statistics_.integral_error > 0) 
                                      ? max_integral 
                                      : -max_integral;
        statistics_.anti_windup_activations++;
    }
}

//==============================================================================
// Statistics and Health
//==============================================================================

void ServoIntegration::update_statistics(double offset_ns, double freq_adj_ppb) {
    // Track last values
    statistics_.last_offset_ns = offset_ns;
    statistics_.last_freq_adjustment_ppb = freq_adj_ppb;
    
    // Track extremes
    if (!first_sample_done_ || offset_ns > statistics_.max_offset_seen_ns) {
        statistics_.max_offset_seen_ns = offset_ns;
    }
    if (!first_sample_done_ || offset_ns < statistics_.min_offset_seen_ns) {
        statistics_.min_offset_seen_ns = offset_ns;
    }
    
    // Track time in locked state
    if (health_.state == ServoState::Locked) {
        // Increment by typical sample interval (1000ms)
        statistics_.time_in_locked_ms += 1000;
    }
}

void ServoIntegration::update_health_status(std::uint64_t current_time_ns) {
    health_.timestamp_ns = current_time_ns;
    
    // Calculate time since last update
    if (first_sample_done_) {
        health_.time_since_last_update_ms = (current_time_ns - last_update_time_ns_) / 1000000;
    }
    
    // Calculate time in current state
    if (first_sample_done_) {
        health_.time_in_current_state_ms = (current_time_ns - state_entry_time_ns_) / 1000000;
    }
    
    // Update health indicators
    double abs_offset = std::abs(statistics_.last_offset_ns);
    
    health_.adjustments_active = (statistics_.total_adjustments > 0);
    health_.phase_locked = (abs_offset < config_.lock_threshold_ns);
    health_.within_spec = (abs_offset < config_.lock_threshold_ns); // <1µs spec
    health_.frequency_stable = (std::abs(last_freq_adjustment_ppb_) < 10.0); // <10ppb
    
    // Update status message
    switch (health_.state) {
        case ServoState::Uninitialized:
            health_.message = "Servo not initialized";
            break;
        case ServoState::Unlocked:
            health_.message = "Unlocked - large offset, converging";
            break;
        case ServoState::Locking:
            health_.message = "Locking - offset reducing, approaching lock";
            break;
        case ServoState::Locked:
            health_.message = "Locked - stable synchronization achieved";
            break;
        case ServoState::Holdover:
            health_.message = "Holdover - no recent updates, maintaining last frequency";
            break;
    }
}

} // namespace servo
} // namespace _2019
} // namespace _1588
} // namespace IEEE
