/**
 * @file pi_servo.cpp
 * @brief Implementation of Proportional-Integral servo
 * 
 * Extracted from ptp_grandmaster.cpp (lines 1350-1450) to create clean,
 * testable, hardware-independent servo engine.
 * 
 * @see pi_servo.hpp for interface documentation
 */

#include "pi_servo.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

PI_Servo::PI_Servo(const PIServoConfig& config)
    : config_(config)
    , integral_(0.0)
    , last_correction_ppb_(0)
    , locked_(false)
    , consecutive_locked_(0)
    , sample_count_(0)
{
    // Validate configuration
    if (config_.kp <= 0.0 || config_.ki <= 0.0) {
        std::cerr << "[PI_Servo] WARNING: Invalid gains (Kp=" << config_.kp 
                  << ", Ki=" << config_.ki << "), using defaults\n";
        config_.kp = 0.7;
        config_.ki = 0.00003;
    }
    
    if (config_.integral_max_ns <= 0.0) {
        std::cerr << "[PI_Servo] WARNING: Invalid integral max (" << config_.integral_max_ns 
                  << "ns), using 50ms\n";
        config_.integral_max_ns = 50000000.0;  // 50ms
    }
    
    if (config_.freq_max_ppb <= 0) {
        std::cerr << "[PI_Servo] WARNING: Invalid freq max (" << config_.freq_max_ppb 
                  << "ppb), using 100000ppb\n";
        config_.freq_max_ppb = 100000;  // ±100 ppm
    }
}

int32_t PI_Servo::calculate_correction(int64_t offset_ns)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    sample_count_++;
    
    // Update integral with new offset
    integral_ += offset_ns;
    
    // Anti-windup protection: Clamp integral to prevent runaway
    // This is the CRITICAL fix for oscillation bug - prevents integral
    // from growing to massive values (100ms+) that cause large corrections
    if (integral_ > config_.integral_max_ns) {
        integral_ = config_.integral_max_ns;
    } else if (integral_ < -config_.integral_max_ns) {
        integral_ = -config_.integral_max_ns;
    }
    
    // Calculate PI correction
    // adjustment = Kp * offset + Ki * integral
    // Units: (ns/s) / 1 = ppb by definition (1 ns/s = 1 ppb)
    double adjustment = config_.kp * offset_ns + config_.ki * integral_;
    int32_t correction_ppb = static_cast<int32_t>(adjustment);
    
    // Clamp correction to safe per-sample limit
    // Note: This is correction DELTA, not cumulative frequency
    // Controller will add calibration drift separately
    if (correction_ppb > config_.freq_max_ppb) {
        correction_ppb = config_.freq_max_ppb;
    } else if (correction_ppb < -config_.freq_max_ppb) {
        correction_ppb = -config_.freq_max_ppb;
    }
    
    last_correction_ppb_ = correction_ppb;
    
    // Check lock criteria
    bool lock_achieved = check_lock_criteria(offset_ns, correction_ppb);
    
    if (lock_achieved) {
        consecutive_locked_++;
        if (consecutive_locked_ >= config_.lock_stability_samples && !locked_) {
            locked_ = true;
            std::cout << "[PI_Servo] ✓ LOCKED (phase=" << offset_ns << "ns < ±" 
                     << config_.phase_lock_threshold_ns << "ns, freq=" << correction_ppb 
                     << "ppb < ±" << config_.freq_lock_threshold_ppb << "ppb)\n";
        }
    } else {
        // Reset consecutive counter if criteria not met
        if (consecutive_locked_ > 0) {
            consecutive_locked_ = 0;
        }
        // Lost lock if was previously locked
        if (locked_) {
            locked_ = false;
            std::cout << "[PI_Servo] ⚠ LOST LOCK (phase=" << offset_ns << "ns, freq=" 
                     << correction_ppb << "ppb)\n";
        }
    }
    
    return correction_ppb;
}

void PI_Servo::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    integral_ = 0.0;
    last_correction_ppb_ = 0;
    locked_ = false;
    consecutive_locked_ = 0;
    // Note: Don't reset sample_count_ - it's a lifetime statistic
    
    std::cout << "[PI_Servo] Reset (integral=0, lock=false)\n";
}

void PI_Servo::get_state(ServoDiagnostics* state) const {
    if (!state) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    state->integral_ns = integral_;
    state->last_correction_ppb = last_correction_ppb_;
    state->locked = locked_;
    state->samples = sample_count_;
}

bool PI_Servo::is_locked() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return locked_;
}

double PI_Servo::get_integral() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return integral_;
}

int PI_Servo::get_consecutive_locked() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return consecutive_locked_;
}

bool PI_Servo::check_lock_criteria(int64_t offset_ns, int32_t correction_ppb)
{
    // Lock criteria (from deb.md specification):
    // - Phase offset within ±100ns
    // - Frequency correction within ±5ppb
    bool phase_locked = std::abs(offset_ns) < config_.phase_lock_threshold_ns;
    bool freq_locked = std::abs(correction_ppb) < config_.freq_lock_threshold_ppb;
    
    bool criteria_met = phase_locked && freq_locked;
    
    // Debug output for first few calls on each servo instance
    if (sample_count_ < 3) {
        std::cout << "[check_lock sample_" << sample_count_ << "] offset=" << offset_ns 
                  << "ns, correction=" << correction_ppb 
                  << "ppb, phase_locked=" << phase_locked << ", freq_locked=" << freq_locked 
                  << ", criteria_met=" << criteria_met << "\n";
    }
    
    // Just return whether criteria are met - caller handles state updates
    return criteria_met;
}
