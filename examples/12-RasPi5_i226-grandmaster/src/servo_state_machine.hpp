/**
 * @file servo_state_machine.hpp
 * @brief Servo state machine for GPS/RTC holdover management
 * 
 * Manages transitions between LOCKED_GPS, HOLDOVER_RTC, and RECOVERY_GPS states
 * based on GPS signal quality, phase error, and frequency stability.
 * 
 * @see ARCHITECTURE.md Section 4: State Machine
 * @see ptp_grandmaster.cpp lines 589-642 (original implementation)
 */

#ifndef SERVO_STATE_MACHINE_HPP
#define SERVO_STATE_MACHINE_HPP

#include <cstdint>
#include <mutex>

/**
 * @brief Servo operating states
 * 
 * State transitions follow IEEE 1588-2019 disciplining requirements with
 * GPS/RTC holdover support per AVnu Milan specification.
 */
enum class ServoState {
    LOCKED_GPS,      ///< Normal operation: PHC disciplined to GPS PPS + GPS ToD
    HOLDOVER_RTC,    ///< GPS lost: PHC frequency stabilized via RTC PPS (frozen anchors)
    RECOVERY_GPS     ///< GPS returning: Reacquisition window before LOCKED_GPS
};

/**
 * @brief Configuration for servo state machine
 */
struct ServoStateMachineConfig {
    uint32_t recovery_samples;          ///< Consecutive good GPS samples needed for RECOVERY→LOCKED (default: 10)
    int64_t phase_lock_threshold_ns;    ///< Phase error threshold for lock detection (default: ±100ns)
    double freq_lock_threshold_ppb;     ///< Frequency error threshold for lock detection (default: ±5ppb)
    uint32_t lock_stability_samples;    ///< Consecutive locked samples to declare stable (default: 10)
    int64_t holdover_phase_limit_ns;    ///< Maximum phase error in HOLDOVER before forcing resync (default: 100ms)
};

/**
 * @brief State machine diagnostic information
 */
struct ServoStateMachineState {
    ServoState current_state;           ///< Current servo state
    uint32_t consecutive_gps_good;      ///< Count of consecutive good GPS samples
    uint32_t consecutive_locked;        ///< Count of consecutive locked samples
    uint64_t last_state_change_time;    ///< UTC seconds when state last changed
    uint64_t time_in_current_state;     ///< Seconds spent in current state
    bool gps_pps_valid;                 ///< Last GPS PPS validity status
    bool gps_tod_valid;                 ///< Last GPS ToD validity status
    int64_t last_phase_error_ns;        ///< Last phase error measurement
    double last_freq_error_ppb;         ///< Last frequency error measurement
};

/**
 * @brief Servo state machine for GPS/RTC holdover management
 * 
 * Implements three-state machine:
 * - LOCKED_GPS: Normal operation with GPS disciplining
 * - HOLDOVER_RTC: GPS lost, using RTC for holdover
 * - RECOVERY_GPS: GPS recovering, waiting for stability
 * 
 * Thread-safe for multi-threaded access.
 */
class ServoStateMachine {
public:
    /**
     * @brief Construct state machine with default configuration
     */
    ServoStateMachine();
    
    /**
     * @brief Construct state machine with custom configuration
     * 
     * @param config State machine configuration parameters
     */
    explicit ServoStateMachine(const ServoStateMachineConfig& config);
    
    /**
     * @brief Destructor
     */
    ~ServoStateMachine();
    
    /**
     * @brief Update state machine with current GPS and servo measurements
     * 
     * Called periodically (typically 1Hz) with latest measurements. State machine
     * evaluates GPS validity, phase error, and frequency error to determine
     * appropriate state transitions.
     * 
     * @param pps_valid GPS PPS signal is valid (no dropout detected)
     * @param tod_valid GPS time-of-day is valid (NMEA fix available)
     * @param phase_error_ns Phase error between GPS and PHC (nanoseconds)
     * @param freq_error_ppb Frequency error between GPS and PHC (parts-per-billion)
     * @param current_utc_sec Current UTC time in seconds (for state duration tracking)
     */
    void update(bool pps_valid, bool tod_valid, int64_t phase_error_ns, 
                double freq_error_ppb, uint64_t current_utc_sec);
    
    /**
     * @brief Get current servo state
     * 
     * @return Current state (LOCKED_GPS, HOLDOVER_RTC, or RECOVERY_GPS)
     */
    ServoState get_state() const;
    
    /**
     * @brief Get detailed state machine diagnostic information
     * 
     * @param state Output structure to receive state information
     */
    void get_state_info(ServoStateMachineState* state) const;
    
    /**
     * @brief Check if servo is locked to GPS
     * 
     * @return true if in LOCKED_GPS state with stable lock
     */
    bool is_locked() const;
    
    /**
     * @brief Check if servo is in holdover mode
     * 
     * @return true if in HOLDOVER_RTC state
     */
    bool is_holdover() const;
    
    /**
     * @brief Check if servo is recovering from GPS loss
     * 
     * @return true if in RECOVERY_GPS state
     */
    bool is_recovering() const;
    
    /**
     * @brief Reset state machine to initial RECOVERY_GPS state
     * 
     * Clears all counters and forces state back to RECOVERY_GPS.
     * Use when restarting synchronization or after major configuration change.
     */
    void reset();
    
    /**
     * @brief Get time spent in current state (seconds)
     * 
     * @param current_utc_sec Current UTC time in seconds
     * @return Seconds elapsed since last state transition
     */
    uint64_t get_time_in_state(uint64_t current_utc_sec) const;

private:
    ServoStateMachineConfig config_;
    
    // State machine state
    ServoState state_;
    uint32_t consecutive_gps_good_;
    uint32_t consecutive_locked_;
    uint64_t last_state_change_time_;
    
    // Last measurements (for diagnostics)
    bool last_pps_valid_;
    bool last_tod_valid_;
    int64_t last_phase_error_ns_;
    double last_freq_error_ppb_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // State transition methods
    void transition_to(ServoState new_state, uint64_t current_utc_sec);
    void update_locked_gps(bool pps_valid, bool tod_valid, uint64_t current_utc_sec);
    void update_holdover_rtc(bool pps_valid, bool tod_valid, uint64_t current_utc_sec);
    void update_recovery_gps(bool pps_valid, bool tod_valid, int64_t phase_error_ns,
                             double freq_error_ppb, uint64_t current_utc_sec);
    
    // Lock detection
    bool is_phase_locked(int64_t phase_error_ns) const;
    bool is_freq_locked(double freq_error_ppb) const;
};

#endif // SERVO_STATE_MACHINE_HPP
