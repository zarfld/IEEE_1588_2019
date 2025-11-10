/**
 * @file servo_integration.hpp
 * @brief IEEE 1588-2019 Servo Integration - Clock Adjustment Control
 * @namespace IEEE::_1588::_2019::servo
 * 
 * Implements servo controller for clock synchronization according to 
 * IEEE 1588-2019 requirements. Uses PI (Proportional-Integral) controller
 * with stability features (anti-windup, rate limiting, state machine).
 * 
 * @see IEEE 1588-2019, Section 11 "Synchronization mechanisms"
 * @see IEEE 1588-2019, Section 7.6.3 "Clock correction"
 * 
 * IMPORTANT: This implementation is based on understanding of IEEE 1588-2019
 * specification. No copyrighted content from IEEE documents is reproduced.
 * Refer to original IEEE specification for authoritative requirements.
 */

#ifndef IEEE_1588_PTP_2019_SERVO_INTEGRATION_H
#define IEEE_1588_PTP_2019_SERVO_INTEGRATION_H

#include "clocks.hpp"
#include <cstdint>
#include <string>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace servo {

// Import types from Clocks namespace
using IEEE::_1588::PTP::_2019::Clocks::StateCallbacks;


//==============================================================================
// Servo State Machine (Based on IEEE 1588-2019 clock behavior requirements)
//==============================================================================

/**
 * @brief Servo controller states
 * @note Follows IEEE 1588-2019 clock synchronization phases
 */
enum class ServoState {
    Uninitialized = 0,  ///< Servo not configured
    Unlocked = 1,       ///< No sync, large offsets (>100µs)
    Locking = 2,        ///< Converging to target (<100µs)
    Locked = 3,         ///< Stable sync achieved (<1µs)
    Holdover = 4        ///< Lost sync, maintaining last freq
};

/**
 * @brief Convert ServoState to string for debugging
 */
inline const char* servo_state_to_string(ServoState state) {
    switch (state) {
        case ServoState::Uninitialized: return "Uninitialized";
        case ServoState::Unlocked: return "Unlocked";
        case ServoState::Locking: return "Locking";
        case ServoState::Locked: return "Locked";
        case ServoState::Holdover: return "Holdover";
        default: return "Unknown";
    }
}

//==============================================================================
// Servo Statistics (Performance Monitoring)
//==============================================================================

/**
 * @brief Servo controller performance statistics
 */
struct ServoStatistics {
    // Adjustment tracking
    std::uint64_t total_adjustments{0};      ///< Total clock adjustments made
    std::uint64_t frequency_adjustments{0};  ///< Total frequency adjustments
    std::uint64_t phase_adjustments{0};      ///< Total phase (time) adjustments
    
    // Error tracking
    double last_offset_ns{0.0};              ///< Most recent offset input
    double last_freq_adjustment_ppb{0.0};    ///< Last frequency adjustment (ppb)
    double last_phase_adjustment_ns{0.0};    ///< Last phase adjustment (ns)
    
    // PI controller state
    double integral_error{0.0};              ///< Accumulated integral error
    double proportional_term{0.0};           ///< Last proportional term
    double integral_term{0.0};               ///< Last integral term
    
    // Stability metrics
    double max_offset_seen_ns{0.0};          ///< Maximum offset ever seen
    double min_offset_seen_ns{0.0};          ///< Minimum offset ever seen
    std::uint64_t time_in_locked_ms{0};      ///< Time spent in Locked state
    std::uint64_t lock_loss_count{0};        ///< Number of times lock was lost
    
    // Adjustment limiting
    std::uint64_t rate_limit_hits{0};        ///< Times rate limit was applied
    std::uint64_t anti_windup_activations{0}; ///< Times anti-windup kicked in
};

//==============================================================================
// Servo Configuration (Tuning Parameters)
//==============================================================================

/**
 * @brief Servo controller configuration parameters
 * @note Tuning guide (IEEE 1588-2019 informative):
 *       - Higher Kp = faster response, more oscillation
 *       - Higher Ki = eliminates steady-state error, can cause overshoot
 *       - Increase damping for noisy networks
 *       - Decrease rate limits for critical systems
 */
struct ServoConfiguration {
    // PI controller gains
    double kp{0.7};                          ///< Proportional gain (unitless)
    double ki{0.3};                          ///< Integral gain (unitless)
    
    // State transition thresholds (nanoseconds)
    double lock_threshold_ns{1000.0};        ///< Locked if |offset| < 1µs
    double locking_threshold_ns{100000.0};   ///< Locking if |offset| < 100µs
    double unlock_threshold_ns{100000.0};    ///< Unlock if |offset| > 100µs
    
    // Holdover configuration
    std::uint32_t holdover_timeout_ms{5000}; ///< Enter holdover after 5s no updates
    
    // Adjustment limits (safety bounds per IEEE requirements)
    double max_freq_adjustment_ppb{500.0};   ///< Maximum ±500 ppb frequency change
    double max_phase_adjustment_ns{1000000.0}; ///< Maximum ±1ms phase step
    double max_rate_of_change_ppb_per_sec{100.0}; ///< Rate limit: 100 ppb/s
    
    // Anti-windup (prevent integral term explosion)
    bool enable_anti_windup{true};           ///< Enable integral clamping
    double integral_limit{1000000.0};        ///< Clamp integral to ±1ms equivalent
    
    // Stability features
    bool enable_rate_limiting{true};         ///< Enable rate-of-change limits
    bool enable_holdover{true};              ///< Enable holdover mode
    std::uint32_t samples_for_lock{10};      ///< Consecutive samples in threshold for lock
    
    // Step vs. slew threshold
    double step_threshold_ns{1000000.0};     ///< Step if |offset| > 1ms, else slew
};

//==============================================================================
// Servo Health Status (Monitoring)
//==============================================================================

/**
 * @brief Servo health and status information
 */
struct ServoHealthStatus {
    ServoState state{ServoState::Uninitialized}; ///< Current servo state
    std::string message;                     ///< Human-readable status message
    std::uint64_t timestamp_ns{0};           ///< Status update timestamp
    
    // Health indicators
    bool adjustments_active{false};          ///< Servo making adjustments
    bool frequency_stable{false};            ///< Frequency converged
    bool phase_locked{false};                ///< Phase within lock threshold
    bool within_spec{false};                 ///< Within IEEE accuracy spec
    
    // Timing info
    std::uint64_t time_since_last_update_ms{0}; ///< Time since last offset input
    std::uint64_t time_in_current_state_ms{0};  ///< Time in current state
};

//==============================================================================
// Servo Integration Class (Main Controller)
//==============================================================================

/**
 * @brief Servo controller for IEEE 1588-2019 clock synchronization
 * 
 * Implements PI controller with stability features:
 * - Proportional-Integral (PI) control algorithm
 * - Anti-windup for integral term
 * - Rate limiting for frequency changes
 * - State machine (Unlocked → Locking → Locked → Holdover)
 * - Step vs. slew decision logic
 * 
 * Usage pattern:
 * 1. Create ServoIntegration(clock_callbacks)
 * 2. configure(ServoConfiguration)
 * 3. start()
 * 4. Periodically call adjust(offset_ns) with sync offset
 * 5. Monitor get_health_status()
 * 6. stop() when done
 * 
 * @note Thread-safety: Caller must ensure serial access to adjust()
 */
class ServoIntegration {
public:
    /**
     * @brief Construct servo controller with clock callbacks
     * @param callbacks Clock adjustment interface from PtpPort
     * @note Callbacks must remain valid for lifetime of ServoIntegration
     */
    explicit ServoIntegration(const StateCallbacks& callbacks)
        : callbacks_(callbacks) {}
    
    // Lifecycle management
    
    /**
     * @brief Configure servo parameters
     * @param config Servo tuning configuration
     * @return true if configuration valid, false otherwise
     * @note Must be called before start()
     */
    bool configure(const ServoConfiguration& config) {
        // Validate configuration
        if (config.kp < 0.0 || config.ki < 0.0) return false;
        if (config.lock_threshold_ns <= 0.0) return false;
        if (config.max_freq_adjustment_ppb <= 0.0) return false;
        
        config_ = config;
        return true;
    }
    
    /**
     * @brief Start servo controller
     * @return true if started successfully
     * @note Resets state machine to Unlocked
     */
    bool start();
    
    /**
     * @brief Stop servo controller
     * @note Leaves clock at last adjusted frequency (no reset)
     */
    void stop();
    
    // Main control loop
    
    /**
     * @brief Adjust clock based on offset from master
     * @param offset_ns Current offset from master (nanoseconds)
     * @param current_time_ns Current system time (for holdover timeout)
     * @return true if adjustment successful
     * 
     * This is the core servo function. Call periodically with offset
     * from SyncIntegration. Servo will:
     * 1. Update state machine based on offset magnitude
     * 2. Calculate PI controller output (freq adjustment)
     * 3. Apply rate limiting and anti-windup
     * 4. Invoke clock adjustment callbacks
     * 5. Update statistics and health status
     * 
     * @note IEEE 1588-2019 recommends adjustment every sync interval
     */
    bool adjust(double offset_ns, std::uint64_t current_time_ns);
    
    // Status and monitoring
    
    /**
     * @brief Get current servo statistics
     * @return Copy of statistics structure
     */
    const ServoStatistics& get_statistics() const { return statistics_; }
    
    /**
     * @brief Get current servo health status
     * @return Copy of health status structure
     */
    const ServoHealthStatus& get_health_status() const { return health_; }
    
    /**
     * @brief Check if servo is running
     * @return true if started and not stopped
     */
    bool is_running() const { return is_running_; }
    
    /**
     * @brief Reset servo to initial state
     * @note Clears all statistics and integral error
     * @note Does NOT reset clock hardware
     */
    void reset();
    
private:
    // Internal control functions
    
    /**
     * @brief Update servo state machine based on offset
     * @param offset_ns Current offset from master
     * @param current_time_ns Current time for holdover
     */
    void update_state_machine(double offset_ns, std::uint64_t current_time_ns);
    
    /**
     * @brief Calculate PI controller output
     * @param offset_ns Current offset (error signal)
     * @return Frequency adjustment in ppb
     * @note Implements: output = Kp*error + Ki*integral(error)
     */
    double calculate_pi_output(double offset_ns);
    
    /**
     * @brief Apply rate limiting to frequency adjustment
     * @param requested_ppb Desired frequency adjustment
     * @return Rate-limited frequency adjustment
     */
    double apply_rate_limiting(double requested_ppb);
    
    /**
     * @brief Apply anti-windup to integral term
     * @note Clamps integral error to prevent runaway
     */
    void apply_anti_windup();
    
    /**
     * @brief Update servo statistics
     * @param offset_ns Current offset
     * @param freq_adj_ppb Applied frequency adjustment
     */
    void update_statistics(double offset_ns, double freq_adj_ppb);
    
    /**
     * @brief Update servo health status
     * @param current_time_ns Current system time
     */
    void update_health_status(std::uint64_t current_time_ns);
    
    // Member variables
    const StateCallbacks& callbacks_;     ///< Clock adjustment interface
    ServoConfiguration config_;           ///< Tuning parameters
    ServoStatistics statistics_;          ///< Performance tracking
    ServoHealthStatus health_;            ///< Current status
    
    // State tracking
    bool is_running_{false};              ///< Servo active flag
    bool first_sample_done_{false};       ///< First offset received
    std::uint64_t last_update_time_ns_{0}; ///< Timestamp of last adjust() call
    std::uint64_t state_entry_time_ns_{0}; ///< When current state entered
    std::uint32_t consecutive_samples_in_threshold_{0}; ///< For lock detection
    
    // PI controller state
    double last_freq_adjustment_ppb_{0.0}; ///< Previous frequency adjustment
};

} // namespace servo
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE

#endif // IEEE_1588_PTP_2019_SERVO_INTEGRATION_H
