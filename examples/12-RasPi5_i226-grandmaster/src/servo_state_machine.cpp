/**
 * @file servo_state_machine.cpp
 * @brief Servo state machine implementation
 */

#include "servo_state_machine.hpp"
#include <iostream>
#include <cmath>

ServoStateMachine::ServoStateMachine()
    : state_(ServoState::RECOVERY_GPS)
    , consecutive_gps_good_(0)
    , consecutive_locked_(0)
    , last_state_change_time_(0)
    , last_pps_valid_(false)
    , last_tod_valid_(false)
    , last_phase_error_ns_(0)
    , last_freq_error_ppb_(0.0)
{
    // Default configuration
    config_.recovery_samples = 10;
    config_.phase_lock_threshold_ns = 100;         // ±100ns
    config_.freq_lock_threshold_ppb = 5.0;         // ±5ppb
    config_.lock_stability_samples = 10;
    config_.holdover_phase_limit_ns = 100000000;  // 100ms
}

ServoStateMachine::ServoStateMachine(const ServoStateMachineConfig& config)
    : config_(config)
    , state_(ServoState::RECOVERY_GPS)
    , consecutive_gps_good_(0)
    , consecutive_locked_(0)
    , last_state_change_time_(0)
    , last_pps_valid_(false)
    , last_tod_valid_(false)
    , last_phase_error_ns_(0)
    , last_freq_error_ppb_(0.0)
{
}

ServoStateMachine::~ServoStateMachine() {
}

void ServoStateMachine::update(bool pps_valid, bool tod_valid, int64_t phase_error_ns,
                               double freq_error_ppb, uint64_t current_utc_sec)
{
    std::lock_guard<std::mutex> lock(mutex_);
        // Initialize state change time on first update
    if (last_state_change_time_ == 0) {
        last_state_change_time_ = current_utc_sec;
    }
        // Store measurements for diagnostics
    last_pps_valid_ = pps_valid;
    last_tod_valid_ = tod_valid;
    last_phase_error_ns_ = phase_error_ns;
    last_freq_error_ppb_ = freq_error_ppb;
    
    // State-specific update logic
    switch (state_) {
        case ServoState::LOCKED_GPS:
            update_locked_gps(pps_valid, tod_valid, current_utc_sec);
            break;
            
        case ServoState::HOLDOVER_RTC:
            update_holdover_rtc(pps_valid, tod_valid, current_utc_sec);
            break;
            
        case ServoState::RECOVERY_GPS:
            update_recovery_gps(pps_valid, tod_valid, phase_error_ns, freq_error_ppb, current_utc_sec);
            break;
    }
}

ServoState ServoStateMachine::get_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void ServoStateMachine::get_state_info(ServoStateMachineState* state) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    state->current_state = state_;
    state->consecutive_gps_good = consecutive_gps_good_;
    state->consecutive_locked = consecutive_locked_;
    state->last_state_change_time = last_state_change_time_;
    state->time_in_current_state = (last_state_change_time_ > 0) ? 
        (state->last_state_change_time - last_state_change_time_) : 0;
    state->gps_pps_valid = last_pps_valid_;
    state->gps_tod_valid = last_tod_valid_;
    state->last_phase_error_ns = last_phase_error_ns_;
    state->last_freq_error_ppb = last_freq_error_ppb_;
}

bool ServoStateMachine::is_locked() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == ServoState::LOCKED_GPS && 
           consecutive_locked_ >= config_.lock_stability_samples;
}

bool ServoStateMachine::is_holdover() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == ServoState::HOLDOVER_RTC;
}

bool ServoStateMachine::is_recovering() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == ServoState::RECOVERY_GPS;
}

void ServoStateMachine::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "[ServoStateMachine] Reset to RECOVERY_GPS\n";
    
    state_ = ServoState::RECOVERY_GPS;
    consecutive_gps_good_ = 0;
    consecutive_locked_ = 0;
    last_state_change_time_ = 0;
    last_pps_valid_ = false;
    last_tod_valid_ = false;
    last_phase_error_ns_ = 0;
    last_freq_error_ppb_ = 0.0;
}

uint64_t ServoStateMachine::get_time_in_state(uint64_t current_utc_sec) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (last_state_change_time_ == 0) {
        return 0;
    }
    
    return current_utc_sec - last_state_change_time_;
}

// Private methods

void ServoStateMachine::transition_to(ServoState new_state, uint64_t current_utc_sec) {
    if (new_state == state_) {
        return;  // No change
    }
    
    // Log transition
    const char* old_state_name = 
        (state_ == ServoState::LOCKED_GPS) ? "LOCKED_GPS" :
        (state_ == ServoState::HOLDOVER_RTC) ? "HOLDOVER_RTC" : "RECOVERY_GPS";
    const char* new_state_name = 
        (new_state == ServoState::LOCKED_GPS) ? "LOCKED_GPS" :
        (new_state == ServoState::HOLDOVER_RTC) ? "HOLDOVER_RTC" : "RECOVERY_GPS";
    
    std::cout << "[ServoStateMachine] " << old_state_name << " → " << new_state_name << "\n";
    
    state_ = new_state;
    last_state_change_time_ = current_utc_sec;
    
    // Reset state-specific counters on transition
    if (new_state == ServoState::RECOVERY_GPS) {
        consecutive_gps_good_ = 0;
    }
    if (new_state == ServoState::LOCKED_GPS) {
        consecutive_locked_ = 0;
    }
}

void ServoStateMachine::update_locked_gps(bool pps_valid, bool tod_valid, uint64_t current_utc_sec) {
    // Check for GPS loss
    if (!pps_valid || !tod_valid) {
        std::cout << "[ServoStateMachine] GPS lost (PPS=" << (pps_valid ? "OK" : "FAIL") 
                 << ", ToD=" << (tod_valid ? "OK" : "FAIL") << ")\n";
        transition_to(ServoState::HOLDOVER_RTC, current_utc_sec);
        return;
    }
    
    // Track lock stability
    if (is_phase_locked(last_phase_error_ns_) && is_freq_locked(last_freq_error_ppb_)) {
        consecutive_locked_++;
    } else {
        consecutive_locked_ = 0;
    }
}

void ServoStateMachine::update_holdover_rtc(bool pps_valid, bool tod_valid, uint64_t current_utc_sec) {
    // Check for GPS recovery
    if (pps_valid && tod_valid) {
        std::cout << "[ServoStateMachine] GPS returning\n";
        transition_to(ServoState::RECOVERY_GPS, current_utc_sec);
    }
    
    // Could add: Check if phase error exceeds holdover_phase_limit_ns and force resync
}

void ServoStateMachine::update_recovery_gps(bool pps_valid, bool tod_valid, int64_t phase_error_ns,
                                            double freq_error_ppb, uint64_t current_utc_sec)
{
    // Waiting for GPS to stabilize before declaring lock
    if (pps_valid && tod_valid) {
        consecutive_gps_good_++;
        
        if (consecutive_gps_good_ >= config_.recovery_samples) {
            std::cout << "[ServoStateMachine] GPS stable for " << consecutive_gps_good_ 
                     << " samples (threshold: " << config_.recovery_samples << ")\n";
            transition_to(ServoState::LOCKED_GPS, current_utc_sec);
        }
    } else {
        // Reset counter on any invalid sample
        consecutive_gps_good_ = 0;
    }
}

bool ServoStateMachine::is_phase_locked(int64_t phase_error_ns) const {
    return std::abs(phase_error_ns) <= config_.phase_lock_threshold_ns;
}

bool ServoStateMachine::is_freq_locked(double freq_error_ppb) const {
    return std::fabs(freq_error_ppb) <= config_.freq_lock_threshold_ppb;
}
