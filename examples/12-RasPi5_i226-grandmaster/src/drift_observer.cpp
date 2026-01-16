/**
 * @file drift_observer.cpp
 * @brief Drift Observer implementation
 * 
 * TDD Status: RED phase - Minimal stubs to make tests compile and fail
 * 
 * All methods are intentionally incomplete to produce failing tests.
 * This is correct TDD behavior - tests should fail in RED phase.
 * 
 * @date 2026-01-16
 */

#include "drift_observer.hpp"
#include <iostream>
#include <cmath>
#include <cstdlib>  // for std::abs
#include <vector>   // for std::vector

namespace ptp {

// Constructor
DriftObserver::DriftObserver(const Config& config, const std::string& name)
    : config_(config)
    , name_(name)
    , write_index_(0)
    , sample_count_(0)
    , current_epoch_(0)
    , sample_seq_(0)
    , holdoff_ticks_remaining_(0)
    , prev_t_ref_ns_(0)
    , prev_t_clk_ns_(0)
    , prev_offset_ns_(0)
    , first_sample_(true)
    , stats_dirty_(true)
{
    samples_.resize(config_.window_size);
}

// Update observer with new PPS tick
void DriftObserver::Update(int64_t t_ref_ns, int64_t t_clk_ns) {
    // Create new sample
    DriftSample sample = {};
    sample.seq = sample_seq_++;
    sample.epoch_id = current_epoch_;
    sample.t_ref_ns = t_ref_ns;
    sample.t_clk_ns = t_clk_ns;
    sample.offset_ns = t_clk_ns - t_ref_ns;
    sample.valid = true;  // Assume valid initially
    sample.flags = 0;
    
    // Compute deltas from previous sample
    if (!first_sample_) {
        sample.dt_ref_ns = t_ref_ns - prev_t_ref_ns_;
        sample.dt_clk_ns = t_clk_ns - prev_t_clk_ns_;
        
        // Compute drift: change in offset = offset[k] - offset[k-1]
        sample.drift_ns_per_s = sample.offset_ns - prev_offset_ns_;
        
        // Automatic step detection - check for large offset jump
        int64_t offset_change = std::abs(sample.offset_ns - prev_offset_ns_);
        if (offset_change > config_.max_offset_step_ns) {
            // Clock step detected - increment epoch automatically
            IncrementEpoch();
            sample.epoch_id = current_epoch_;  // Update sample to new epoch
        }
        
        // Spike detection
        DetectOutliers(sample);
    }
    
    // Decrement holdoff timer if active
    if (holdoff_ticks_remaining_ > 0) {
        holdoff_ticks_remaining_--;
    }
    
    // Store sample in ring buffer
    samples_[write_index_] = sample;
    write_index_ = (write_index_ + 1) % config_.window_size;
    if (sample_count_ < config_.window_size) {
        sample_count_++;
    }
    
    // Update previous values for next iteration
    prev_t_ref_ns_ = t_ref_ns;
    prev_t_clk_ns_ = t_clk_ns;
    prev_offset_ns_ = sample.offset_ns;
    first_sample_ = false;
    
    // Mark statistics as dirty
    stats_dirty_ = true;
}

// Get current estimate
Estimate DriftObserver::GetEstimate() const {
    Estimate est = {};
    est.ready = false;
    est.trustworthy = false;
    est.health_flags = HF_NOT_READY;
    est.ticks_in_holdoff = holdoff_ticks_remaining_;
    
    // Count valid samples from current epoch only
    size_t valid_count = 0;
    for (size_t i = 0; i < sample_count_; i++) {
        if (samples_[i].epoch_id == current_epoch_ && samples_[i].valid) {
            valid_count++;
        }
    }
    
    // Return actual sample counts
    est.total_samples = sample_count_;
    est.valid_samples = valid_count;
    est.current_epoch = current_epoch_;
    est.ticks_in_epoch = valid_count;  // Number of valid samples in current epoch
    
    // Evaluate ready flag based on minimum sample threshold
    est.ready = (valid_count >= config_.min_valid_samples);
    if (!est.ready) {
        est.health_flags |= HF_NOT_READY;
    } else {
        // Clear NOT_READY if we have enough samples
        est.health_flags &= ~HF_NOT_READY;
    }
    
    // Set holdoff flag if still in holdoff period
    if (holdoff_ticks_remaining_ > 0) {
        est.health_flags |= HF_IN_HOLDOFF;
    }
    
    // Compute statistics if we have enough valid samples from current epoch
    if (valid_count >= config_.min_valid_samples) {
        ComputeStatistics();
        
        // Copy cached statistics to estimate
        est.offset_mean_ns = cached_estimate_.offset_mean_ns;
        est.offset_stddev_ns = cached_estimate_.offset_stddev_ns;
        est.drift_ppm = cached_estimate_.drift_ppm;
        est.drift_stddev_ppm = cached_estimate_.drift_stddev_ppm;
        
        // Evaluate trustworthy flag: ready + out of holdoff + low jitter
        est.trustworthy = est.ready && 
                          (holdoff_ticks_remaining_ == 0) && 
                          (est.drift_stddev_ppm <= config_.max_drift_stddev_ppm);
    }
    
    return est;
}

// Get all samples
std::vector<DriftSample> DriftObserver::GetSamples() const {
    std::vector<DriftSample> result;
    result.reserve(sample_count_);
    
    // Return samples in chronological order (oldest to newest)
    for (size_t i = 0; i < sample_count_; i++) {
        result.push_back(samples_[i]);
    }
    
    return result;
}

// Get most recent sample
const DriftSample& DriftObserver::Latest() const {
    // Return the most recently written sample
    if (sample_count_ == 0) {
        return samples_[0];  // Return first (empty) if no samples yet
    }
    
    // Most recent is at (write_index_ - 1), wrapping around
    size_t latest_index = (write_index_ == 0) ? (config_.window_size - 1) : (write_index_ - 1);
    return samples_[latest_index];
}

// Notify observer of contamination event
void DriftObserver::NotifyEvent(ObserverEvent event, int64_t magnitude_ns) {
    (void)magnitude_ns;  // Unused for now
    
    // Handle event-specific state changes and holdoff timers
    switch (event) {
        case ObserverEvent::ClockStepped:
            // Increment epoch and set holdoff for clock step
            IncrementEpoch();  // This also sets holdoff_ticks_remaining_
            break;
            
        case ObserverEvent::FrequencyAdjusted:
            // Invalidate current window, set holdoff for freq adjust
            holdoff_ticks_remaining_ = config_.holdoff_after_freq_ticks;
            stats_dirty_ = true;
            break;
            
        case ObserverEvent::ReferenceChanged:
            // Increment epoch and set holdoff for reference change
            current_epoch_++;
            holdoff_ticks_remaining_ = config_.holdoff_after_ref_ticks;
            stats_dirty_ = true;
            break;
            
        case ObserverEvent::WarmStartRequested:
            // Full reset
            ClearWindow();
            current_epoch_ = 0;
            holdoff_ticks_remaining_ = config_.holdoff_after_step_ticks;
            break;
            
        // Other events that don't affect drift estimation
        case ObserverEvent::ReferenceLost:
        case ObserverEvent::ReferenceRecovered:
        case ObserverEvent::ClockSlewed:
        case ObserverEvent::ServoModeChanged:
            // No action needed for these events
            break;
    }
}

// Reset observer
void DriftObserver::Reset() {
    // Clear window and reset all state
    ClearWindow();
    
    // Reset epoch and holdoff
    current_epoch_ = 0;
    holdoff_ticks_remaining_ = 0;
    
    // Reset previous sample tracking
    prev_t_ref_ns_ = 0;
    prev_t_clk_ns_ = 0;
    prev_offset_ns_ = 0;
    first_sample_ = true;
    
    // Mark statistics as dirty
    stats_dirty_ = true;
}

// ============================================================================
// Private methods - Statistics and outlier detection
// ============================================================================

void DriftObserver::ComputeStatistics() const {
    if (stats_dirty_) {
        // Collect valid offsets from current epoch only
        std::vector<int64_t> valid_offsets;
        for (size_t i = 0; i < sample_count_; i++) {
            // Only include samples from current epoch
            if (samples_[i].epoch_id == current_epoch_ && samples_[i].valid) {
                valid_offsets.push_back(samples_[i].offset_ns);
            }
        }
        
        if (valid_offsets.empty()) {
            return;
        }
        
        // Compute offset mean in single pass
        int64_t sum = 0;
        for (int64_t val : valid_offsets) {
            sum += val;
        }
        cached_estimate_.offset_mean_ns = sum / static_cast<int64_t>(valid_offsets.size());
        
        // Compute offset standard deviation
        double variance = 0.0;
        for (int64_t val : valid_offsets) {
            double diff = static_cast<double>(val - cached_estimate_.offset_mean_ns);
            variance += diff * diff;
        }
        cached_estimate_.offset_stddev_ns = static_cast<int64_t>(
            std::sqrt(variance / static_cast<double>(valid_offsets.size()))
        );
        
        // Compute drift estimate using configured method
        double drift_ppm;
        if (config_.use_linear_regression) {
            drift_ppm = ComputeDriftLinearRegression();
        } else {
            drift_ppm = ComputeDriftMean();
        }
        cached_estimate_.drift_ppm = drift_ppm;
        
        // Compute drift standard deviation (jitter metric)
        std::vector<double> valid_drifts_ppm;
        for (size_t i = 0; i < sample_count_; i++) {
            if (samples_[i].epoch_id == current_epoch_ && samples_[i].valid && i > 0) {
                double drift_ppm_val = static_cast<double>(samples_[i].drift_ns_per_s) / 1000.0;
                valid_drifts_ppm.push_back(drift_ppm_val);
            }
        }
        
        if (!valid_drifts_ppm.empty()) {
            double drift_variance = 0.0;
            for (double drift : valid_drifts_ppm) {
                double diff = drift - drift_ppm;
                drift_variance += diff * diff;
            }
            cached_estimate_.drift_stddev_ppm = std::sqrt(
                drift_variance / static_cast<double>(valid_drifts_ppm.size())
            );
        } else {
            cached_estimate_.drift_stddev_ppm = 0.0;
        }
        
        stats_dirty_ = false;
    }
}

void DriftObserver::DetectOutliers(DriftSample& sample) {
    // Check for offset spike (large jump in offset)
    if (IsOffsetSpike(sample.drift_ns_per_s)) {
        sample.valid = false;
        sample.flags |= DriftSample::FLAG_OFFSET_SPIKE;
    }
    
    // Check for drift spike (drift out of plausible range)
    if (IsDriftSpike(sample.drift_ns_per_s)) {
        sample.valid = false;
        sample.flags |= DriftSample::FLAG_DRIFT_SPIKE;
    }
    
    // Check for dt_ref deviation (reference clock issue)
    int64_t dt_ref_deviation = std::abs(sample.dt_ref_ns - 1000000000LL);
    if (dt_ref_deviation > config_.max_dt_ref_deviation_ns) {
        sample.valid = false;
        sample.flags |= DriftSample::FLAG_DT_REF_INVALID;
    }
}

bool DriftObserver::IsOffsetSpike(int64_t offset_ns) const {
    // Offset spike: large jump in offset (drift_ns_per_s is the delta)
    return std::abs(offset_ns) > config_.max_offset_step_ns;
}

bool DriftObserver::IsDriftSpike(int64_t drift_ns_per_s) const {
    // Convert drift to ppm: (drift_ns / 1e9 ns) * 1e6 = drift_ns / 1e3
    double drift_ppm = static_cast<double>(drift_ns_per_s) / 1000.0;
    return std::abs(drift_ppm) > config_.max_drift_ppm;
}

void DriftObserver::IncrementEpoch() {
    current_epoch_++;
    stats_dirty_ = true;  // Statistics need recomputation
    
    // Phase 6: Set holdoff after clock step
    holdoff_ticks_remaining_ = config_.holdoff_after_step_ticks;
}

void DriftObserver::ClearWindow() {
    write_index_ = 0;
    sample_count_ = 0;
    first_sample_ = true;
    stats_dirty_ = true;
}

double DriftObserver::ComputeDriftLinearRegression() const {
    // Linear regression: fit offset(t) = a + b*t where b is drift
    // Using least squares with t = sample index in seconds
    
    std::vector<double> x_vals;  // Time (sample index)
    std::vector<int64_t> y_vals;  // Offset
    
    // Only use samples from current epoch
    for (size_t i = 0; i < sample_count_; i++) {
        if (samples_[i].epoch_id == current_epoch_ && samples_[i].valid) {
            x_vals.push_back(static_cast<double>(i));
            y_vals.push_back(samples_[i].offset_ns);
        }
    }
    
    size_t n = x_vals.size();
    if (n < 2) {
        return 0.0;
    }
    
    // Compute means
    double x_mean = 0.0;
    double y_mean = 0.0;
    for (size_t i = 0; i < n; i++) {
        x_mean += x_vals[i];
        y_mean += static_cast<double>(y_vals[i]);
    }
    x_mean /= static_cast<double>(n);
    y_mean /= static_cast<double>(n);
    
    // Compute slope: b = sum((x - x_mean)(y - y_mean)) / sum((x - x_mean)^2)
    double numerator = 0.0;
    double denominator = 0.0;
    for (size_t i = 0; i < n; i++) {
        double x_diff = x_vals[i] - x_mean;
        double y_diff = static_cast<double>(y_vals[i]) - y_mean;
        numerator += x_diff * y_diff;
        denominator += x_diff * x_diff;
    }
    
    if (denominator < 1e-9) {
        return 0.0;
    }
    
    double slope_ns_per_sample = numerator / denominator;
    
    // Convert to ppm: slope is ns/sample, samples are 1 second apart
    // drift_ppm = (slope_ns / 1e9 ns) * 1e6 = slope_ns / 1e3
    double drift_ppm = slope_ns_per_sample / 1000.0;
    
    if (std::isnan(drift_ppm) || std::isinf(drift_ppm)) {
        return 0.0;
    }
    
    return drift_ppm;
}

double DriftObserver::ComputeDriftMean() const {
    // Mean of drift deltas: average of drift_ns_per_s for valid samples
    
    std::vector<int64_t> valid_drifts;
    // Only use samples from current epoch
    for (size_t i = 0; i < sample_count_; i++) {
        if (samples_[i].epoch_id == current_epoch_ && samples_[i].valid && i > 0) {  // Skip first sample (no drift computed)
            valid_drifts.push_back(samples_[i].drift_ns_per_s);
        }
    }
    
    if (valid_drifts.empty()) {
        return 0.0;
    }
    
    int64_t sum = 0;
    for (int64_t drift : valid_drifts) {
        sum += drift;
    }
    
    double mean_drift_ns = static_cast<double>(sum) / static_cast<double>(valid_drifts.size());
    
    // Convert to ppm: (drift_ns / 1e9 ns) * 1e6 = drift_ns / 1e3
    return mean_drift_ns / 1000.0;
}

double DriftObserver::ComputeMAD(const std::vector<int64_t>& values) const {
    (void)values;
    // RED phase: Returns 0.0
    return 0.0;
}

} // namespace ptp
