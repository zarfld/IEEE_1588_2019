/**
 * @file servo_interface.hpp
 * @brief Abstract interface for PTP servo algorithms
 * 
 * Defines the contract for servo engines that calculate frequency corrections
 * from phase offset measurements. Enables swappable servo algorithms without
 * modifying the controller (Open/Closed Principle).
 * 
 * @see ARCHITECTURE.md Section 2: Servo Engines
 * @see .github/instructions/copilot-instructions.md (Hardware Agnostic principle)
 */

#ifndef SERVO_INTERFACE_HPP
#define SERVO_INTERFACE_HPP

#include <cstdint>

/**
 * @brief Servo diagnostics data (renamed from ServoState to avoid collision with enum class)
 */
struct ServoDiagnostics {
    double integral_ns;           ///< Integral accumulator (nanoseconds)
    int32_t last_correction_ppb;  ///< Last frequency correction output (ppb)
    bool locked;                  ///< True when servo has achieved lock
    uint64_t samples;             ///< Number of samples processed
};

/**
 * @brief Abstract interface for servo algorithms
 * 
 * A servo converts phase offset measurements into frequency corrections.
 * The controller accumulates corrections with calibration drift:
 *   total_freq = calibration_drift + servo_correction
 * 
 * CRITICAL: Servo outputs DELTA (correction), NOT cumulative frequency!
 * This prevents the limit cycle bug (500000 + -500000 = 0) that plagued
 * the original monolithic implementation.
 */
class ServoInterface {
public:
    virtual ~ServoInterface() = default;
    
    /**
     * @brief Calculate frequency correction from phase offset
     * 
     * @param offset_ns Phase offset in nanoseconds
     *                  Positive: Local clock ahead of reference
     *                  Negative: Local clock behind reference
     * @return Frequency correction in ppb (parts-per-billion)
     *         Positive: Speed up local clock
     *         Negative: Slow down local clock
     * 
     * @note Output is correction DELTA, not cumulative value
     * @note Controller will add calibration drift before applying to hardware
     * @note Thread-safe: Can be called from multiple threads
     * 
     * Example:
     *   offset = -1000ns (clock 1µs behind)
     *   correction = servo->calculate_correction(-1000)
     *   total_freq = calibration_drift + correction
     *   phc->adjust_frequency(total_freq)
     */
    virtual int32_t calculate_correction(int64_t offset_ns) = 0;
    
    /**
     * @brief Reset servo state (after step correction)
     * 
     * Called when PHC is stepped (large offset correction via set_time()).
     * Servo should reset integral accumulators and lock status.
     * 
     * @note Thread-safe: Can be called from multiple threads
     * 
     * When to call:
     *   - After step correction (offset > 100ms)
     *   - GPS dropout recovery (state transition to RECOVERY)
     *   - Manual intervention
     */
    virtual void reset() = 0;
    
    /**
     * @brief Get current servo state for diagnostics
     * 
     * @param[out] state Servo diagnostics structure to fill
     * 
     * @note Thread-safe: Can be called from multiple threads
     * @note State is snapshot at time of call (may change immediately after)
     * 
     * Used for:
     *   - Debug logging (integral value, lock status)
     *   - Servo comparison reports
     *   - Performance monitoring
     */
    virtual void get_state(ServoDiagnostics* state) const = 0;
    
    /**
     * @brief Check if servo has achieved lock
     * 
     * @return true if servo is locked to reference, false otherwise
     * 
     * Lock criteria (servo-specific):
     *   - PI Servo: abs(offset) < threshold AND abs(correction) < threshold
     *   - Freq-Error Servo: Convergence flag set
     * 
     * @note Thread-safe: Can be called from multiple threads
     */
    virtual bool is_locked() const = 0;
};

#endif // SERVO_INTERFACE_HPP
