/**
 * @file rtc_drift_discipline.hpp
 * @brief RTC Drift Discipline for DS3231 Aging Offset Compensation
 * 
 * Implements deb.md Recommendations A + E:
 * - 120-sample averaging window (20 minutes @ 10s intervals)
 * - Stability gate: stddev < 0.3 ppm threshold
 * - Proportional control: delta_lsb = round(drift_avg_ppm / 0.1)
 * - LSB clamping: [-3, +3] range
 * - Minimum 1200s interval between adjustments
 * 
 * @see REFACTORED_VALIDATION_PLAN.md Priority #1
 * @see deb.md Recommendations A and E
 */

#ifndef RTC_DRIFT_DISCIPLINE_HPP
#define RTC_DRIFT_DISCIPLINE_HPP

#include <cstdint>
#include <cstddef>
#include <vector>

/**
 * @brief Configuration for RTC drift discipline
 */
struct RtcDriftDisciplineConfig {
    size_t buffer_size = 120;            ///< Drift sample buffer size (120 samples = 20 min)
    double stability_threshold = 0.3;    ///< Stddev threshold in ppm (reject if >= 0.3)
    size_t min_samples = 60;             ///< Minimum samples before first adjustment
    uint32_t min_interval_sec = 1200;    ///< Minimum seconds between adjustments (20 min)
    double ppm_per_lsb = 0.1;            ///< DS3231: 0.1 ppm per LSB
    int max_lsb_delta = 3;               ///< Maximum LSB adjustment per cycle (±3)
};

/**
 * @brief RTC Drift Discipline Engine
 * 
 * Manages DS3231 aging offset compensation via drift measurement and
 * proportional control law. Accumulates drift samples, applies stability
 * gate, and calculates aging offset adjustments.
 */
class RtcDriftDiscipline {
public:
    /**
     * @brief Construct discipline engine with configuration
     * @param config Discipline parameters
     */
    explicit RtcDriftDiscipline(const RtcDriftDisciplineConfig& config);
    
    /**
     * @brief Add drift measurement sample
     * @param drift_ppm Measured drift in parts-per-million
     * @param timestamp_sec Timestamp when sample was measured
     */
    void add_sample(double drift_ppm, uint64_t timestamp_sec);
    
    /**
     * @brief Check if adjustment should be applied now
     * @param current_time_sec Current UTC time in seconds
     * @return true if adjustment criteria met (samples, interval, stability)
     */
    bool should_adjust(uint64_t current_time_sec) const;
    
    /**
     * @brief Calculate aging offset adjustment (LSB units)
     * @return LSB delta clamped to [-max_lsb_delta, +max_lsb_delta]
     * 
     * Formula: delta_lsb = round(drift_avg_ppm / ppm_per_lsb)
     * DS3231: Positive LSB = slower clock, negative LSB = faster clock
     */
    int calculate_lsb_adjustment() const;
    
    /**
     * @brief Get average drift over buffered samples
     * @return Average drift in ppm
     */
    double get_average_drift() const;
    
    /**
     * @brief Get standard deviation of drift measurements
     * @return Stddev in ppm (used for stability gate)
     */
    double get_stddev() const;
    
    /**
     * @brief Get number of samples in buffer
     * @return Sample count (0 to buffer_size)
     */
    size_t get_sample_count() const;
    
    /**
     * @brief Record that adjustment was applied (resets interval timer)
     * @param timestamp_sec Time when adjustment was applied
     */
    void record_adjustment(uint64_t timestamp_sec);

private:
    RtcDriftDisciplineConfig config_;
    std::vector<double> samples_;
    uint64_t last_adjustment_time_;
    uint64_t last_sample_time_;
};

#endif // RTC_DRIFT_DISCIPLINE_HPP
