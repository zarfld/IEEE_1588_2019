/**
 * @file rtc_drift_discipline.cpp
 * @brief Implementation of RTC drift discipline
 */

#include "rtc_drift_discipline.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

RtcDriftDiscipline::RtcDriftDiscipline(const RtcDriftDisciplineConfig& config)
    : config_(config)
    , last_adjustment_time_(0)
    , last_sample_time_(0)
{
    samples_.reserve(config_.buffer_size);
}

void RtcDriftDiscipline::add_sample(double drift_ppm, uint64_t timestamp_sec)
{
    // Circular buffer: Remove oldest if full
    if (samples_.size() >= config_.buffer_size) {
        samples_.erase(samples_.begin());
    }
    
    samples_.push_back(drift_ppm);
    last_sample_time_ = timestamp_sec;
}

bool RtcDriftDiscipline::should_adjust(uint64_t current_time_sec) const
{
    // Need minimum samples
    if (samples_.size() < config_.min_samples) {
        return false;
    }
    
    // Check minimum interval since last adjustment
    if (last_adjustment_time_ > 0) {
        uint64_t elapsed = current_time_sec - last_adjustment_time_;
        if (elapsed < config_.min_interval_sec) {
            return false;
        }
    } else {
        // First adjustment: check total elapsed time
        if (current_time_sec < config_.min_interval_sec) {
            return false;
        }
    }
    
    // Check stability gate
    double stddev = get_stddev();
    return stddev < config_.stability_threshold;
}

int RtcDriftDiscipline::calculate_lsb_adjustment() const
{
    double avg = get_average_drift();
    
    // Proportional control law: delta_lsb = round(drift / ppm_per_lsb)
    int lsb = static_cast<int>(std::round(avg / config_.ppm_per_lsb));
    
    // Clamp to maximum range
    if (lsb > config_.max_lsb_delta) {
        lsb = config_.max_lsb_delta;
    } else if (lsb < -config_.max_lsb_delta) {
        lsb = -config_.max_lsb_delta;
    }
    
    return lsb;
}

double RtcDriftDiscipline::get_average_drift() const
{
    if (samples_.empty()) {
        return 0.0;
    }
    
    double sum = std::accumulate(samples_.begin(), samples_.end(), 0.0);
    return sum / samples_.size();
}

double RtcDriftDiscipline::get_stddev() const
{
    if (samples_.size() < 2) {
        return 0.0;
    }
    
    double avg = get_average_drift();
    double sum_sq = 0.0;
    
    for (double sample : samples_) {
        double diff = sample - avg;
        sum_sq += diff * diff;
    }
    
    return std::sqrt(sum_sq / samples_.size());
}

size_t RtcDriftDiscipline::get_sample_count() const
{
    return samples_.size();
}

void RtcDriftDiscipline::record_adjustment(uint64_t timestamp_sec)
{
    last_adjustment_time_ = timestamp_sec;
}
