/**
 * @file grandmaster_controller.hpp
 * @brief Orchestration layer for PTP Grandmaster - integrates all modules
 * 
 * Coordinates GPS, RTC, PHC, Network adapters with Servo, Calibrator, and StateMachine engines.
 * Implements main control loop: GPS → Offset → Servo → State Machine → Network TX
 * 
 * Architecture:
 *   Adapters (Hardware) → Controller (Orchestration) → Engines (Logic)
 * 
 * Responsibilities:
 *   - Initialize all hardware adapters
 *   - Run PHC calibration on startup
 *   - Main loop: measure offset, calculate correction, apply to PHC
 *   - State machine transitions (RECOVERY → LOCKED → HOLDOVER)
 *   - Network PTP message transmission
 * 
 * Design Pattern: Dependency Injection (all dependencies passed to constructor)
 */

#ifndef GRANDMASTER_CONTROLLER_HPP
#define GRANDMASTER_CONTROLLER_HPP

#include <cstdint>
#include <atomic>
#include "gps_adapter.hpp"
#include "rtc_adapter.hpp"
#include "phc_adapter.hpp"
#include "network_adapter.hpp"
#include "servo_interface.hpp"
#include "pi_servo.hpp"
#include "phc_calibrator.hpp"
#include "servo_state_machine.hpp"

/**
 * @brief Controller configuration parameters
 */
struct GrandmasterConfig {
    int32_t step_threshold_ns;      // Offset threshold for step correction (default: 100ms)
    uint32_t sync_interval_ms;      // PTP Sync message interval (default: 1000ms)
    bool enable_ptp_tx;             // Enable PTP message transmission (default: true)
    bool verbose_logging;           // Enable detailed logging (default: false)
    
    // Default constructor
    GrandmasterConfig()
        : step_threshold_ns(100000000)  // 100ms
        , sync_interval_ms(1000)        // 1 second
        , enable_ptp_tx(true)
        , verbose_logging(false)
    {}
};

/**
 * @brief Grandmaster runtime statistics
 */
struct GrandmasterStats {
    uint64_t uptime_seconds;
    uint64_t sync_messages_sent;
    uint64_t announce_messages_sent;
    uint64_t step_corrections;
    int64_t current_offset_ns;
    int32_t current_freq_ppb;
    ServoState servo_state;
    bool calibrated;
};

/**
 * @brief Main orchestration class for PTP Grandmaster
 * 
 * Coordinates all hardware adapters and control engines to implement
 * IEEE 1588-2019 PTP Grandmaster functionality.
 */
class GrandmasterController {
public:
    /**
     * @brief Construct controller with all dependencies
     * 
     * @param gps GPS adapter for time reference and PPS
     * @param rtc RTC adapter for holdover
     * @param phc PHC adapter for i226 NIC clock
     * @param network Network adapter for PTP message I/O
     * @param config Controller configuration parameters
     */
    GrandmasterController(
        IEEE::_1588::PTP::_2019::Linux::GpsAdapter* gps,
        IEEE::_1588::PTP::_2019::Linux::RtcAdapter* rtc,
        PhcAdapter* phc,
        IEEE::_1588::PTP::_2019::Linux::NetworkAdapter* network,
        const GrandmasterConfig& config = GrandmasterConfig()
    );
    
    /**
     * @brief Destructor - cleanup resources
     */
    ~GrandmasterController();
    
    /**
     * @brief Initialize all modules and run calibration
     * 
     * Sequence:
     *   1. Initialize hardware adapters
     *   2. Create control engines (servo, calibrator, state machine)
     *   3. Run PHC frequency calibration
     *   4. Join PTP multicast groups
     * 
     * @return true if initialization successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Main control loop (blocking)
     * 
     * Loop:
     *   1. Get GPS time and PPS status
     *   2. Get PHC time
     *   3. Calculate offset
     *   4. Update state machine
     *   5. Apply servo correction or step
     *   6. Send PTP messages (Sync, Announce)
     *   7. Sleep until next cycle
     * 
     * Runs until shutdown() is called.
     */
    void run();
    
    /**
     * @brief Signal shutdown (thread-safe)
     * 
     * Stops the run() loop gracefully.
     */
    void shutdown();
    
    /**
     * @brief Get current runtime statistics
     * 
     * @param stats Output buffer for statistics
     */
    void get_stats(GrandmasterStats* stats) const;
    
    /**
     * @brief Check if controller is running
     * 
     * @return true if run() loop is active
     */
    bool is_running() const { return running_.load(); }

private:
    // Hardware adapters (injected dependencies)
    IEEE::_1588::PTP::_2019::Linux::GpsAdapter* gps_;
    IEEE::_1588::PTP::_2019::Linux::RtcAdapter* rtc_;
    PhcAdapter* phc_;
    IEEE::_1588::PTP::_2019::Linux::NetworkAdapter* network_;
    
    // Control engines (created by controller)
    ServoInterface* servo_;
    PhcCalibrator* calibrator_;
    ServoStateMachine* state_machine_;
    
    // Configuration
    GrandmasterConfig config_;
    
    // Runtime state
    std::atomic<bool> running_;
    bool initialized_;
    bool calibration_complete_;
    int32_t calibration_drift_ppb_;
    int32_t cumulative_freq_ppb_;
    
    // Statistics
    uint64_t start_time_sec_;
    uint64_t sync_count_;
    uint64_t announce_count_;
    uint64_t step_count_;
    int64_t last_offset_ns_;
    uint32_t cycles_since_step_;  // CRITICAL: Don't run servo immediately after step
    
    // Private helper methods
    
    /**
     * @brief Wait for GPS fix (up to 60 seconds)
     * 
     * @return true if GPS fix acquired
     */
    bool wait_for_gps_fix();
    
    /**
     * @brief Set initial time offsets for PHC and RTC
     * 
     * CRITICAL: This MUST be done BEFORE drift calibration!
     * Steps PHC and RTC to GPS time to correct offset errors.
     * 
     * @return true if offsets set successfully
     */
    bool set_initial_time();
    
    /**
     * @brief Run PHC frequency calibration (AFTER offset correction)
     * 
     * Uses PhcCalibrator to measure PHC drift vs GPS PPS.
     * Applies initial frequency correction.
     * 
     * @return true if calibration successful
     */
    bool calibrate_phc();
    
    /**
     * @brief Calculate time offset between GPS and PHC
     * 
     * @param gps_sec GPS seconds
     * @param gps_nsec GPS nanoseconds
     * @param phc_sec PHC seconds
     * @param phc_nsec PHC nanoseconds
     * @return Offset in nanoseconds (GPS - PHC)
     */
    int64_t calculate_offset(uint64_t gps_sec, uint32_t gps_nsec,
                            uint64_t phc_sec, uint32_t phc_nsec) const;
    
    /**
     * @brief Apply step correction (large offset)
     * 
     * Sets PHC time to GPS time.
     * Resets servo integrator.
     * Resets cumulative frequency to calibration baseline.
     */
    void apply_step_correction(uint64_t gps_sec, uint32_t gps_nsec);
    
    /**
     * @brief Apply servo correction (small offset)
     * 
     * Calculates correction from servo.
     * Updates cumulative frequency.
     * Applies to PHC.
     * 
     * @param offset_ns Current offset in nanoseconds
     */
    void apply_servo_correction(int64_t offset_ns);
    
    /**
     * @brief Send PTP Sync message
     * 
     * Constructs IEEE 1588-2019 Sync message.
     * Transmits on event socket (port 319).
     * Captures TX timestamp.
     */
    void send_sync_message();
    
    /**
     * @brief Send PTP Announce message
     * 
     * Constructs IEEE 1588-2019 Announce message.
     * Transmits on general socket (port 320).
     */
    void send_announce_message();
    
    /**
     * @brief Log controller state (if verbose enabled)
     * 
     * @param offset_ns Current offset
     * @param freq_ppb Current frequency
     * @param state Servo state
     */
    void log_state(int64_t offset_ns, int32_t freq_ppb, ServoState state) const;
};

#endif // GRANDMASTER_CONTROLLER_HPP
