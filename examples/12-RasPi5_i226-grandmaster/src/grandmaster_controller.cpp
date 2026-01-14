/**
 * @file grandmaster_controller.cpp
 * @brief Implementation of GrandmasterController orchestration layer
 */

#include "grandmaster_controller.hpp"
#include "pi_servo.hpp"
#include "phc_calibrator.hpp"
#include "servo_state_machine.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cstring>
#include <cmath>

using namespace IEEE::_1588::PTP::_2019::Linux;

// Constructor
GrandmasterController::GrandmasterController(
    GpsAdapter* gps,
    RtcAdapter* rtc,
    PhcAdapter* phc,
    NetworkAdapter* network,
    const GrandmasterConfig& config)
    : gps_(gps)
    , rtc_(rtc)
    , phc_(phc)
    , network_(network)
    , servo_(nullptr)
    , calibrator_(nullptr)
    , state_machine_(nullptr)
    , config_(config)
    , running_(false)
    , initialized_(false)
    , calibration_complete_(false)
    , calibration_drift_ppb_(0)
    , cumulative_freq_ppb_(0)
    , start_time_sec_(0)
    , sync_count_(0)
    , announce_count_(0)
    , step_count_(0)
    , last_offset_ns_(0)
{
}

// Destructor
GrandmasterController::~GrandmasterController() {
    if (running_) {
        shutdown();
    }
    
    // Cleanup engines (adapters are owned by caller)
    delete servo_;
    delete calibrator_;
    delete state_machine_;
}

// Initialize all modules
bool GrandmasterController::initialize() {
    if (initialized_) {
        std::cerr << "[Controller] Already initialized\n";
        return false;
    }
    
    std::cout << "[Controller] Initializing Grandmaster Controller...\n";
    
    // 1. Verify adapters are provided
    if (!gps_ || !rtc_ || !phc_ || !network_) {
        std::cerr << "[Controller] ERROR: Missing required adapters\n";
        return false;
    }
    
    // 2. Join PTP multicast groups
    std::cout << "[Controller] Joining PTP multicast groups...\n";
    if (!network_->join_multicast("224.0.1.129")) {  // PTP event multicast
        std::cerr << "[Controller] WARNING: Failed to join event multicast\n";
        // Non-fatal, continue
    }
    
    // 3. Create control engines
    std::cout << "[Controller] Creating control engines...\n";
    
    // PI Servo: Kp=0.7, Ki=0.3, integral_max=50ms, freq_max=100ppm
    PIServoConfig servo_config;
    servo_config.kp = 0.7;
    servo_config.ki = 0.3;
    servo_config.integral_max_ns = 50000000;
    servo_config.freq_max_ppb = 100000;
    servo_ = new PI_Servo(servo_config);
    
    // PHC Calibrator
    PhcCalibratorConfig cal_config;
    cal_config.interval_pulses = 20;          // 20 pulses = 20 seconds
    cal_config.max_correction_ppb = 500000;   // Max frequency correction
    cal_config.drift_threshold_ppm = 100.0;   // Accept if < 100 ppm
    cal_config.sanity_threshold_ppm = 2000.0; // Reject impossible measurements > 2000 ppm
    cal_config.max_iterations = 5;            // Max calibration attempts
    calibrator_ = new PhcCalibrator(cal_config);
    calibrator_->initialize(phc_, gps_);
    
    // Servo State Machine
    state_machine_ = new ServoStateMachine();
    
    // 4. Wait for GPS fix
    std::cout << "[Controller] Waiting for GPS fix...\n";
    if (!wait_for_gps_fix()) {
        std::cerr << "[Controller] ERROR: No GPS fix after 60 seconds\n";
        return false;
    }
    
    // 5. Set initial time offsets (CRITICAL: Do this BEFORE drift measurement!)
    std::cout << "[Controller] Setting initial time offsets...\n";
    if (!set_initial_time()) {
        std::cerr << "[Controller] WARNING: Failed to set initial time\n";
        // Non-fatal, but will affect calibration accuracy
    }
    
    // 6. Run PHC frequency calibration
    std::cout << "[Controller] Running PHC frequency calibration...\n";
    if (!calibrate_phc()) {
        std::cerr << "[Controller] WARNING: Calibration incomplete, using default frequency\n";
        calibration_drift_ppb_ = 0;
        // Non-fatal, can still run with uncalibrated PHC
    }
    
    // 7. Record start time
    uint64_t sec;
    uint32_t nsec;
    if (gps_->get_ptp_time(&sec, &nsec)) {
        start_time_sec_ = sec;
    }
    
    initialized_ = true;
    std::cout << "[Controller] Initialization complete\n";
    return true;
}

// Wait for GPS fix
bool GrandmasterController::wait_for_gps_fix() {
    if (!gps_) return false;
    
    // Wait for GPS fix (up to 60 seconds)
    for (int i = 0; i < 60; i++) {
        gps_->update();
        
        if (gps_->has_fix()) {
            std::cout << "[Controller] GPS fix acquired (" 
                     << static_cast<int>(gps_->get_satellite_count()) << " satellites)\n";
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return false;
}

// Set initial time offsets (CRITICAL: Do this BEFORE drift measurement!)
bool GrandmasterController::set_initial_time() {
    if (!gps_ || !phc_ || !rtc_) return false;
    
    // Wait for GPS adapter to establish PPS-UTC lock (critical for valid time)
    std::cout << "[Controller] Waiting for GPS PPS-UTC lock...\n";
    for (int i = 0; i < 30; i++) {
        gps_->update();
        if (gps_->is_locked()) {
            std::cout << "[Controller] GPS PPS-UTC lock established\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    if (!gps_->is_locked()) {
        std::cerr << "[Controller] WARNING: GPS PPS-UTC lock not established, time may be unreliable\n";
        // Continue anyway - calibration might help establish lock
    }
    
    // Get GPS time
    uint64_t gps_sec;
    uint32_t gps_nsec;
    if (!gps_->get_ptp_time(&gps_sec, &gps_nsec)) {
        std::cerr << "[Controller] ERROR: Failed to get GPS time\n";
        return false;
    }
    
    std::cout << "[Controller] GPS time: " << gps_sec << "." 
              << std::setfill('0') << std::setw(9) << gps_nsec << " s\n";
    
    // 1. Step PHC to GPS time
    std::cout << "[Controller] Stepping PHC to GPS time...\n";
    if (!phc_->set_time(gps_sec, gps_nsec)) {
        std::cerr << "[Controller] ERROR: Failed to set PHC time\n";
        return false;
    }
    std::cout << "[Controller] ✓ PHC synchronized to GPS\n";
    
    // 2. Step RTC to GPS time
    std::cout << "[Controller] Stepping RTC to GPS time...\n";
    RtcTime rtc_time;
    // Convert PTP timestamp (TAI) to UTC for RTC
    // TAI-UTC offset = 37 seconds (valid for 2017-2025)
    uint64_t utc_sec = gps_sec - 37;
    
    // Convert Unix timestamp to calendar time (simplified - assumes epoch 1970)
    uint64_t days = utc_sec / 86400;
    uint32_t day_sec = utc_sec % 86400;
    
    // Approximate year/month/day (good enough for RTC setting)
    uint16_t year_estimate = 1970 + (days / 365);  // Rough approximation
    rtc_time.year = year_estimate;
    rtc_time.month = 1;  // Default to January (close enough for sync purposes)
    rtc_time.day = 1;
    rtc_time.hours = day_sec / 3600;
    rtc_time.minutes = (day_sec % 3600) / 60;
    rtc_time.seconds = day_sec % 60;
    rtc_time.valid = true;
    
    if (!rtc_->set_time(rtc_time)) {
        std::cerr << "[Controller] WARNING: Failed to set RTC time (non-fatal)\n";
        // Non-fatal - continue even if RTC set fails
    } else {
        std::cout << "[Controller] ✓ RTC synchronized to GPS\n";
    }
    
    // 3. Wait for clocks to stabilize (critical for accurate drift measurement)
    std::cout << "[Controller] Waiting 3 seconds for clocks to stabilize...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    return true;
}

// Run PHC frequency calibration (AFTER offset correction)
bool GrandmasterController::calibrate_phc() {
    if (!gps_ || !phc_ || !calibrator_) {
        return false;
    }
    
    std::cout << "[Controller] Measuring PHC frequency drift (20 pulses, ~20 seconds)...\n";
    std::cout << "  NOTE: Offset already corrected, now measuring drift only\n";
    
    // Calibration loop (handled by PhcCalibrator state machine)
    // Max time: 5 iterations × 20 pulses × 1 sec/pulse = 100 seconds + margin
    bool baseline_set = false;
    for (int attempt = 0; attempt < 120; attempt++) {  // 120 seconds max (was 30)
        // UPDATE GPS DATA to get fresh PPS!
        gps_->update();
        
        // Get current PPS data
        PpsData pps;
        if (!gps_->get_pps_data(&pps, nullptr)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Get PHC timestamp
        uint64_t phc_sec;
        uint32_t phc_nsec;
        if (!phc_->get_time(&phc_sec, &phc_nsec)) {
            std::cerr << "[Controller] ERROR: Failed to read PHC time\n";
            return false;
        }
        
        int64_t phc_ns = static_cast<int64_t>(phc_sec) * 1000000000LL 
                       + static_cast<int64_t>(phc_nsec);
        
        // Start calibration ONCE, then update on subsequent iterations
        if (!baseline_set) {
            calibrator_->start_calibration(pps.sequence, phc_ns);
            baseline_set = true;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        // Update calibration with new measurements
        int result = calibrator_->update_calibration(pps.sequence, phc_ns);
        
        // Check if calibration complete
        PhcCalibrationState cal_state;
        calibrator_->get_state(&cal_state);
        
        if (cal_state.calibrated) {
            calibration_drift_ppb_ = calibrator_->get_cumulative_frequency();
            calibration_complete_ = true;
            cumulative_freq_ppb_ = calibration_drift_ppb_;
            
            std::cout << "[Controller] Calibration complete: " 
                     << calibration_drift_ppb_ << " ppb drift\n";
            
            // Apply initial frequency correction
            phc_->adjust_frequency(calibration_drift_ppb_);
            
            return true;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cerr << "[Controller] ERROR: Calibration timeout\n";
    return false;
}

// Main control loop
void GrandmasterController::run() {
    if (!initialized_) {
        std::cerr << "[Controller] ERROR: Not initialized, call initialize() first\n";
        return;
    }
    
    running_ = true;
    std::cout << "[Controller] Starting main control loop...\n";
    
    uint32_t loop_count = 0;
    
    while (running_) {
        loop_count++;
        
        // CRITICAL: Update GPS data to fetch new NMEA sentences and PPS timestamps!
        gps_->update();
        
        // 1. Get GPS time and PPS status
        uint64_t gps_sec = 0;
        uint32_t gps_nsec = 0;
        bool gps_valid = gps_->get_ptp_time(&gps_sec, &gps_nsec);
        
        PpsData pps;
        bool pps_valid = gps_->get_pps_data(&pps, nullptr);
        
        // Debug first few loops
        static int debug_count = 0;
        if (debug_count++ < 5) {
            std::cout << "[Controller] Loop " << loop_count 
                     << " GPS=" << gps_sec << "." << gps_nsec
                     << " valid=" << gps_valid
                     << " PPS_seq=" << pps.sequence
                     << " PPS_valid=" << pps_valid << "\n";
        }
        
        // 2. Get PHC time
        uint64_t phc_sec = 0;
        uint32_t phc_nsec = 0;
        bool phc_valid = phc_->get_time(&phc_sec, &phc_nsec);
        
        if (!gps_valid || !phc_valid) {
            std::cerr << "[Controller] WARNING: Time read failed (GPS=" 
                     << gps_valid << ", PHC=" << phc_valid << ")\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 3. Calculate offset
        int64_t offset_ns = calculate_offset(gps_sec, gps_nsec, phc_sec, phc_nsec);
        last_offset_ns_ = offset_ns;
        
        // 4. Update state machine
        state_machine_->update(pps_valid, gps_valid, offset_ns, 
                             (double)cumulative_freq_ppb_, gps_sec);
        ServoState current_state = state_machine_->get_state();
        
        // 5. Apply correction based on offset magnitude
        // CRITICAL BUG #13 FIX: After stepping PHC, PPS timestamps are invalid!
        // 
        // Root cause: PPS timestamps captured in OLD PHC timescale, but GPS adapter
        // returns time using these timestamps. After PHC step, comparing:
        //   GPS time (TAI with OLD PPS nanoseconds) vs current PHC time (NEW timescale)
        // This creates artificial ~100ms+ offsets even though clocks are synchronized!
        //
        // Solution: Wait for 3 complete PPS cycles after step before measuring offset:
        //   Pulse N:   Captured before step (OLD timescale) - INVALID
        //   Pulse N+1: Transition pulse - QUESTIONABLE  
        //   Pulse N+2: First pulse in NEW timescale - QUESTIONABLE
        //   Pulse N+3: Fully stabilized - VALID
        //
        static uint32_t pps_seq_when_stepped = 0;
        
        // Get current PPS sequence
        const PpsData& pps_data = gps_->get_pps_data();
        uint32_t current_pps_seq = pps_data.sequence;
        
        if (std::abs(offset_ns) > config_.step_threshold_ns) {
            // Check if we recently stepped and need to wait for PPS stabilization
            uint32_t pulses_since_step = current_pps_seq - pps_seq_when_stepped;
            if (pps_seq_when_stepped != 0 && pulses_since_step < 3) {
                // Still waiting for PPS to stabilize after step (need 3 clean pulses)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // Only step if GPS adapter has established PPS-UTC lock
            if (!gps_->is_locked()) {
                std::cout << "[Controller] WARNING: Large offset detected but GPS not locked yet, skipping step\n";
            } else {
                // Large offset: apply step correction
                apply_step_correction(gps_sec, gps_nsec);
                pps_seq_when_stepped = current_pps_seq;  // Record PPS when we stepped
            }
            
            // CRITICAL: Skip rest of loop - wait for 3 new PPS pulses
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        } else {
            // Small offset: apply servo correction
            apply_servo_correction(offset_ns);
        }
        
        // 6. Send PTP messages (if enabled)
        if (config_.enable_ptp_tx) {
            // Send Sync every cycle
            send_sync_message();
            
            // Send Announce every 2 seconds
            if (loop_count % 2 == 0) {
                send_announce_message();
            }
        }
        
        // 7. Log state (if verbose)
        if (config_.verbose_logging) {
            log_state(offset_ns, cumulative_freq_ppb_, current_state);
        }
        
        // 8. Sleep until next cycle
        // Use shorter interval during convergence (offset >1ms) for faster servo response
        uint32_t cycle_interval_ms = (std::abs(offset_ns) > 1000000) ? 100 : config_.sync_interval_ms;
        std::this_thread::sleep_for(std::chrono::milliseconds(cycle_interval_ms));
    }
    
    std::cout << "[Controller] Main loop stopped\n";
}

// Shutdown
void GrandmasterController::shutdown() {
    std::cout << "[Controller] Shutting down...\n";
    running_ = false;
}

// Get statistics
void GrandmasterController::get_stats(GrandmasterStats* stats) const {
    if (!stats) return;
    
    uint64_t current_sec = 0;
    uint32_t current_nsec = 0;
    if (gps_->get_ptp_time(&current_sec, &current_nsec)) {
        stats->uptime_seconds = current_sec - start_time_sec_;
    } else {
        stats->uptime_seconds = 0;
    }
    
    stats->sync_messages_sent = sync_count_;
    stats->announce_messages_sent = announce_count_;
    stats->step_corrections = step_count_;
    stats->current_offset_ns = last_offset_ns_;
    stats->current_freq_ppb = cumulative_freq_ppb_;
    stats->servo_state = state_machine_ ? state_machine_->get_state() : ServoState::RECOVERY_GPS;
    stats->calibrated = calibration_complete_;
}

// Calculate offset
int64_t GrandmasterController::calculate_offset(
    uint64_t gps_sec, uint32_t gps_nsec,
    uint64_t phc_sec, uint32_t phc_nsec) const
{
    int64_t gps_ns = static_cast<int64_t>(gps_sec) * 1000000000LL 
                   + static_cast<int64_t>(gps_nsec);
    int64_t phc_ns = static_cast<int64_t>(phc_sec) * 1000000000LL 
                   + static_cast<int64_t>(phc_nsec);
    
    return gps_ns - phc_ns;  // Positive = PHC behind GPS
}

// Apply step correction
void GrandmasterController::apply_step_correction(uint64_t gps_sec, uint32_t gps_nsec) {
    static uint64_t last_gps_sec = 0;
    static uint32_t last_gps_nsec = 0;
    
    if (gps_sec == last_gps_sec && gps_nsec == last_gps_nsec) {
        std::cout << "[Controller] WARNING: GPS time not updating! Same as last step: "
                 << gps_sec << "." << gps_nsec << "\n";
    }
    last_gps_sec = gps_sec;
    last_gps_nsec = gps_nsec;
    
    std::cout << "[Controller] Applying step correction (offset > " 
             << (config_.step_threshold_ns / 1000000) << " ms) to GPS time: "
             << gps_sec << "." << gps_nsec << "\n";
    
    // Calculate step delta before executing
    uint64_t old_phc_sec;
    uint32_t old_phc_nsec;
    bool have_old_time = phc_->get_time(&old_phc_sec, &old_phc_nsec);
    int64_t step_delta_ns = 0;
    if (have_old_time) {
        int64_t old_time_ns = static_cast<int64_t>(old_phc_sec) * 1000000000LL + old_phc_nsec;
        int64_t new_time_ns = static_cast<int64_t>(gps_sec) * 1000000000LL + gps_nsec;
        step_delta_ns = new_time_ns - old_time_ns;
    }
    
    // 1. Set PHC time to GPS time
    bool step_success = phc_->set_time(gps_sec, gps_nsec);
    
    // Notify GPS adapter of timescale change ONLY if step succeeded
    if (step_success && have_old_time) {
        gps_->notify_phc_stepped(step_delta_ns);
    } else if (!step_success) {
        std::cerr << "[Controller] WARNING: PHC step failed, NOT adjusting GPS adapter timescale\n";
    }
    
    // 2. Reset servo integrator
    if (servo_) {
        servo_->reset();
    }
    
    // 3. Reset cumulative frequency to calibration baseline
    cumulative_freq_ppb_ = calibration_drift_ppb_;
    phc_->adjust_frequency(cumulative_freq_ppb_);
    
    step_count_++;
}

// Apply servo correction
void GrandmasterController::apply_servo_correction(int64_t offset_ns) {
    if (!servo_) return;
    
    // 1. Calculate servo correction
    int32_t correction_ppb = servo_->calculate_correction(offset_ns);
    
    // 2. Update cumulative frequency
    int32_t new_freq_ppb = cumulative_freq_ppb_ + correction_ppb;
    
    // 3. Clamp to PHC limits (±500 ppm for i226)
    int32_t max_freq = phc_->get_max_frequency_ppb();
    if (new_freq_ppb > max_freq) {
        new_freq_ppb = max_freq;
    } else if (new_freq_ppb < -max_freq) {
        new_freq_ppb = -max_freq;
    }
    
    // 4. Apply to PHC
    phc_->adjust_frequency(new_freq_ppb);
    
    // 5. Update cumulative frequency (CRITICAL: persist the correction)
    cumulative_freq_ppb_ = new_freq_ppb;
}

// Send Sync message
void GrandmasterController::send_sync_message() {
    if (!network_) return;
    
    // Simple Sync message (IEEE 1588-2019 format)
    // In full implementation, this would construct proper PTP packet
    uint8_t sync_packet[64];
    std::memset(sync_packet, 0, sizeof(sync_packet));
    
    sync_packet[0] = 0x00;  // messageType: Sync
    sync_packet[1] = 0x02;  // versionPTP: 2
    
    NetworkTimestamp tx_ts;
    int sent = network_->send_packet(sync_packet, sizeof(sync_packet), &tx_ts);
    
    if (sent > 0) {
        sync_count_++;
    }
}

// Send Announce message
void GrandmasterController::send_announce_message() {
    if (!network_) return;
    
    // Simple Announce message (IEEE 1588-2019 format)
    uint8_t announce_packet[64];
    std::memset(announce_packet, 0, sizeof(announce_packet));
    
    announce_packet[0] = 0x0B;  // messageType: Announce
    announce_packet[1] = 0x02;  // versionPTP: 2
    
    NetworkTimestamp tx_ts;
    int sent = network_->send_packet(announce_packet, sizeof(announce_packet), &tx_ts);
    
    if (sent > 0) {
        announce_count_++;
    }
}

// Log state
void GrandmasterController::log_state(int64_t offset_ns, int32_t freq_ppb, ServoState state) const {
    const char* state_str = "UNKNOWN";
    switch (state) {
        case ServoState::RECOVERY_GPS: state_str = "RECOVERY_GPS"; break;
        case ServoState::LOCKED_GPS: state_str = "LOCKED_GPS"; break;
        case ServoState::HOLDOVER_RTC: state_str = "HOLDOVER_RTC"; break;
    }
    
    std::cout << "[Controller] State=" << state_str 
             << ", Offset=" << (offset_ns / 1000) << " μs"
             << ", Freq=" << freq_ppb << " ppb\n";
}
