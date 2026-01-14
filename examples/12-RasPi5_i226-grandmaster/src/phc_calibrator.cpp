/**
 * @file phc_calibrator.cpp
 * @brief PHC frequency calibration implementation
 */

#include "phc_calibrator.hpp"
#include "phc_adapter.hpp"
#include "gps_adapter.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

PhcCalibrator::PhcCalibrator(const PhcCalibratorConfig& config)
    : config_(config)
    , phc_(nullptr)
    , gps_(nullptr)
    , calibrated_(false)
    , baseline_pps_seq_(0)
    , baseline_phc_ns_(0)
    , cumulative_freq_ppb_(0)
    , measured_drift_ppb_(0)
    , iterations_(0)
    , correlation_failures_(0)
    , last_drift_ppm_(0.0)
    , last_phc_delta_ns_(0)
    , last_ref_delta_ns_(0)
{
    // Validate configuration
    if (config_.interval_pulses == 0) {
        std::cerr << "[PhcCalibrator] Warning: interval_pulses is 0, using default (20)\n";
        config_.interval_pulses = 20;
    }
    if (config_.max_correction_ppb == 0) {
        std::cerr << "[PhcCalibrator] Warning: max_correction_ppb is 0, using default (500000)\n";
        config_.max_correction_ppb = 500000;
    }
    if (config_.drift_threshold_ppm <= 0.0) {
        std::cerr << "[PhcCalibrator] Warning: drift_threshold_ppm invalid, using default (100.0)\n";
        config_.drift_threshold_ppm = 100.0;
    }
    if (config_.sanity_threshold_ppm <= 0.0) {
        std::cerr << "[PhcCalibrator] Warning: sanity_threshold_ppm invalid, using default (2000.0)\n";
        config_.sanity_threshold_ppm = 2000.0;
    }
    if (config_.max_iterations == 0) {
        std::cerr << "[PhcCalibrator] Warning: max_iterations is 0, using default (5)\n";
        config_.max_iterations = 5;
    }
}

int PhcCalibrator::initialize(PhcAdapter* phc, IEEE::_1588::PTP::_2019::Linux::GpsAdapter* gps)
{
    if (!phc) {
        std::cerr << "[PhcCalibrator] Error: NULL PHC adapter pointer\n";
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    phc_ = phc;
    gps_ = gps;  // GPS is optional (nullptr allowed)
    
    std::cout << "[PhcCalibrator] Initialized (interval=" << config_.interval_pulses 
              << " pulses, threshold=" << config_.drift_threshold_ppm << " ppm)\n";
    
    return 0;
}

int PhcCalibrator::start_calibration(uint32_t pps_sequence, int64_t phc_timestamp_ns)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!phc_) {
        std::cerr << "[PhcCalibrator] Error: Not initialized\n";
        return -1;
    }
    
    baseline_pps_seq_ = pps_sequence;
    baseline_phc_ns_ = phc_timestamp_ns;
    iterations_ = 0;
    correlation_failures_ = 0;
    
    std::cout << "[PhcCalibrator] Baseline set at PPS #" << pps_sequence 
              << " (PHC: " << phc_timestamp_ns << " ns)\n"
              << "  Will measure over " << config_.interval_pulses << " pulses...\n";
    
    return 0;
}

int PhcCalibrator::update_calibration(uint32_t pps_sequence, int64_t phc_timestamp_ns)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!phc_) {
        std::cerr << "[PhcCalibrator] Error: Not initialized\n";
        return -1;
    }
    
    if (calibrated_) {
        return 1;  // Already calibrated
    }
    
    if (baseline_pps_seq_ == 0) {
        std::cerr << "[PhcCalibrator] Error: Baseline not set\n";
        return -2;
    }
    
    // Calculate elapsed PPS pulses
    uint32_t elapsed_pulses = pps_sequence - baseline_pps_seq_;
    
    // Progress logging every 5 pulses
    static uint32_t last_progress = 0;
    if (elapsed_pulses > 0 && elapsed_pulses % 5 == 0 && elapsed_pulses != last_progress) {
        std::cout << "[PhcCalibrator] Progress: " << elapsed_pulses << "/" 
                  << config_.interval_pulses << " pulses (PPS #" << pps_sequence << ")...\n";
        last_progress = elapsed_pulses;
    }
    
    if (elapsed_pulses < config_.interval_pulses) {
        return 0;  // Not enough pulses yet
    }
    
    // Perform measurement
    // PURE INTEGER NANOSECOND DELTAS (no floats until final ratio)
    int64_t phc_delta_ns = phc_timestamp_ns - baseline_phc_ns_;
    int64_t ref_delta_ns = static_cast<int64_t>(elapsed_pulses) * 1000000000LL;  // N pulses × 1 sec/pulse
    
    // Calculate drift
    double drift_ppm = calculate_drift_ppm(phc_delta_ns, ref_delta_ns);
    
    // Store for diagnostics
    last_drift_ppm_ = drift_ppm;
    last_phc_delta_ns_ = phc_delta_ns;
    last_ref_delta_ns_ = ref_delta_ns;
    
    // Sanity check: reject unrealistic drift
    if (std::abs(drift_ppm) > config_.sanity_threshold_ppm) {
        std::cerr << "[PhcCalibrator] ❌ INVALID MEASUREMENT: " << std::fixed << std::setprecision(1)
                  << drift_ppm << " ppm (exceeds ±" << config_.sanity_threshold_ppm << " ppm threshold)\n"
                  << "  PHC delta: " << phc_delta_ns << " ns, Ref delta: " << ref_delta_ns << " ns\n"
                  << "  LIKELY CAUSES:\n"
                  << "    1. Wrong PHC device\n"
                  << "    2. PHC time discontinuity (clock step during measurement)\n"
                  << "  Resetting baseline and retrying...\n";
        
        // Reset baseline and try again
        baseline_pps_seq_ = pps_sequence;
        baseline_phc_ns_ = phc_timestamp_ns;
        return 0;
    }
    
    // Increment iteration counter
    iterations_++;
    
    // Check if still needs calibration
    if (std::abs(drift_ppm) > config_.drift_threshold_ppm && iterations_ < config_.max_iterations) {
        // Calculate correction (negate drift to compensate)
        int32_t correction_ppb = static_cast<int32_t>(-drift_ppm * 1000.0);  // ppm → ppb
        
        std::cout << "[PhcCalibrator] Iteration " << iterations_ << " (" << elapsed_pulses << " pulses): Measured " 
                  << std::fixed << std::setprecision(1) << drift_ppm << " ppm drift\n"
                  << "  PHC delta: " << phc_delta_ns << " ns, Ref delta: " << ref_delta_ns << " ns\n";
        
        // Apply correction
        int result = apply_frequency_correction(correction_ppb);
        if (result < 0) {
            std::cerr << "[PhcCalibrator] Error: Failed to apply correction\n";
            return result;
        }
        
        std::cout << "  Current total: " << cumulative_freq_ppb_ - correction_ppb << " ppb, "
                  << "Correction: " << correction_ppb << " ppb, "
                  << "New total: " << cumulative_freq_ppb_ << " ppb\n";
        
        // Reset baseline for next measurement
        baseline_pps_seq_ = pps_sequence;
        baseline_phc_ns_ = phc_timestamp_ns;
        
        return 0;  // Continue calibration
    }
    
    // Calibration complete!
    const char* reason = (iterations_ >= config_.max_iterations) 
        ? "max iterations reached" : "drift acceptable";
    
    // Apply final correction
    int32_t final_correction_ppb = static_cast<int32_t>(-drift_ppm * 1000.0);
    int result = apply_frequency_correction(final_correction_ppb);
    if (result < 0) {
        std::cerr << "[PhcCalibrator] Error: Failed to apply final correction\n";
        return result;
    }
    
    // CRITICAL (Layer 6 fix): Store actual measured drift, NOT clamped cumulative
    // The cumulative may have been clamped to ±500000 ppb during iterations,
    // but we need to remember the actual measured drift for step corrections
    measured_drift_ppb_ = final_correction_ppb;
    
    std::cout << "[PhcCalibrator] ✓ Complete (" << reason << ")! Final drift: " 
              << std::fixed << std::setprecision(1) << drift_ppm << " ppm\n"
              << "  Final correction applied: " << final_correction_ppb << " ppb\n"
              << "  Measured drift (for step restore): " << measured_drift_ppb_ << " ppb\n"
              << "  Final cumulative freq: " << cumulative_freq_ppb_ << " ppb\n";
    
    calibrated_ = true;
    return 1;  // Calibration complete
}

bool PhcCalibrator::is_calibrated() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return calibrated_;
}

void PhcCalibrator::get_state(PhcCalibrationState* state) const
{
    if (!state) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    state->calibrated = calibrated_;
    state->cumulative_freq_ppb = cumulative_freq_ppb_;
    state->iterations = iterations_;
    state->last_drift_ppm = last_drift_ppm_;
    state->last_phc_delta_ns = last_phc_delta_ns_;
    state->last_ref_delta_ns = last_ref_delta_ns_;
}

void PhcCalibrator::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    calibrated_ = false;
    baseline_pps_seq_ = 0;
    baseline_phc_ns_ = 0;
    cumulative_freq_ppb_ = 0;
    measured_drift_ppb_ = 0;
    iterations_ = 0;
    correlation_failures_ = 0;
    last_drift_ppm_ = 0.0;
    last_phc_delta_ns_ = 0;
    last_ref_delta_ns_ = 0;
    
    std::cout << "[PhcCalibrator] Reset (ready for recalibration)\n";
}

int32_t PhcCalibrator::get_cumulative_frequency() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    // CRITICAL (Layer 6 fix): Return measured drift, NOT clamped cumulative
    // Step corrections need the actual drift, not the hardware-clamped value
    return measured_drift_ppb_;
}

double PhcCalibrator::calculate_drift_ppm(int64_t phc_delta_ns, int64_t ref_delta_ns) const
{
    // drift_ppm = ((PHC_measured - reference) / reference) × 10^6
    return (static_cast<double>(phc_delta_ns - ref_delta_ns) / ref_delta_ns) * 1e6;
}

int PhcCalibrator::apply_frequency_correction(int32_t correction_ppb)
{
    // Clamp correction per iteration
    if (correction_ppb > config_.max_correction_ppb) {
        correction_ppb = config_.max_correction_ppb;
    } else if (correction_ppb < -config_.max_correction_ppb) {
        correction_ppb = -config_.max_correction_ppb;
    }
    
    // Calculate new total frequency (cumulative in software)
    int32_t new_freq_ppb = cumulative_freq_ppb_ + correction_ppb;
    
    // Clamp to hardware limits (±500,000 ppb = ±500 ppm)
    const int32_t max_total_freq = 500000;
    if (new_freq_ppb > max_total_freq) {
        new_freq_ppb = max_total_freq;
    }
    if (new_freq_ppb < -max_total_freq) {
        new_freq_ppb = -max_total_freq;
    }
    
    // Apply to hardware
    int result = phc_->adjust_frequency(new_freq_ppb);
    if (result < 0) {
        return result;
    }
    
    // Update software tracker
    cumulative_freq_ppb_ = new_freq_ppb;
    
    return 0;
}
