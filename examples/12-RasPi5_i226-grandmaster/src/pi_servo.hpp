/**
 * @file pi_servo.hpp
 * @brief Proportional-Integral (PI) servo implementation
 * 
 * Classical PI control loop for frequency disciplining. Uses phase offset
 * measurements to calculate frequency corrections via proportional and
 * integral terms.
 * 
 * @see servo_interface.hpp for interface documentation
 * @see ARCHITECTURE.md Section 2: Servo Engines
 */

#ifndef PI_SERVO_HPP
#define PI_SERVO_HPP

#include "servo_interface.hpp"
#include <mutex>

/**
 * @brief PI Servo configuration parameters
 */
struct PIServoConfig {
    double kp;                         ///< Proportional gain (typically 0.7)
    double ki;                         ///< Integral gain (typically 0.00003)
    double integral_max_ns;            ///< Maximum integral value (anti-windup, typically 50ms)
    int32_t freq_max_ppb;              ///< Maximum frequency correction per sample (typically 100000)
    int64_t phase_lock_threshold_ns;   ///< Phase lock threshold (typically 100ns)
    int32_t freq_lock_threshold_ppb;   ///< Frequency lock threshold (typically 5ppb)
    int lock_stability_samples;        ///< Samples needed for lock detection (typically 10)
};

/**
 * @brief Proportional-Integral servo for PTP synchronization
 * 
 * Implements classical PI control:
 *   integral += offset
 *   correction = Kp * offset + Ki * integral
 * 
 * Features:
 * - Anti-windup protection (integral clamping)
 * - Lock detection (phase AND frequency criteria)
 * - Thread-safe operation
 * 
 * CRITICAL DESIGN: Outputs correction DELTA, not cumulative frequency!
 * This prevents the limit cycle bug that plagued the original implementation.
 */
class PI_Servo : public ServoInterface {
public:
    /**
     * @brief Construct PI servo with configuration
     * 
     * @param config Servo parameters (gains, limits, thresholds)
     */
    explicit PI_Servo(const PIServoConfig& config);
    
    /**
     * @brief Destructor
     */
    virtual ~PI_Servo() = default;
    
    // ServoInterface implementation
    int32_t calculate_correction(int64_t offset_ns) override;
    void reset() override;
    void get_state(ServoDiagnostics* state) const override;
    bool is_locked() const override;
    
    /**
     * @brief Get integral value (for debugging)
     * @return Current integral accumulator value in nanoseconds
     */
    double get_integral() const;
    
    /**
     * @brief Get consecutive locked sample count
     * @return Number of consecutive samples meeting lock criteria
     */
    int get_consecutive_locked() const;

private:
    // Configuration
    PIServoConfig config_;
    
    // State
    double integral_;                  ///< Integral accumulator (nanoseconds)
    int32_t last_correction_ppb_;      ///< Last correction output
    bool locked_;                      ///< Lock status
    int consecutive_locked_;           ///< Consecutive samples meeting lock criteria
    uint64_t sample_count_;            ///< Total samples processed
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Helper methods
    bool check_lock_criteria(int64_t offset_ns, int32_t correction_ppb);
};

#endif // PI_SERVO_HPP
