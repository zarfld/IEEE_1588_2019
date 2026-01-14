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
    , cycles_since_step_(999)  // Start high so servo runs immediately
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
    
    // PI Servo: MUCH smaller gains for GPS disciplining (calibration already handles bulk drift)
    // Expert: After calibration removes ~80ppm drift, servo only needs to correct small residuals
    // Kp=0.01 means -10ms offset → -100ppb correction (gentle)
    // Ki=0.0001 provides slow integration
    // freq_max=10ppm prevents runaway
    PIServoConfig servo_config;
    servo_config.kp = 0.01;                   // Was 0.7 - WAY too aggressive after calibration
    servo_config.ki = 0.0001;                 // Was 0.3 - caused integral windup
    servo_config.integral_max_ns = 10000000;  // 10ms max integral (was 50ms)
    servo_config.freq_max_ppb = 10000;        // 10ppm max per sample (was 100ppm - caused runaway)
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
    // LAYER 11 FIX: Increased timeout from 30s→60s→120s to ensure lock establishment
    // Lock requires ~10 PPS samples, but timing varies significantly with GPS update processing
    // and NMEA message arrival patterns. 120s provides adequate margin.
    std::cout << "[Controller] Waiting for GPS PPS-UTC lock (max 120s)...\n";
    for (int i = 0; i < 120; i++) {
        gps_->update();
        if (gps_->is_locked()) {
            std::cout << "[Controller] ✓ GPS PPS-UTC lock established after " << i << " seconds\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    if (!gps_->is_locked()) {
        std::cerr << "[Controller] ERROR: GPS PPS-UTC lock not established!\n";
        std::cerr << "[Controller] CRITICAL: Cannot step PHC before PPS-UTC lock (expert guidance: deb.md)\n";
        std::cerr << "[Controller] PHC will remain at system time until lock is established\n";
        // DO NOT step before lock! (expert guidance from deb.md)
        // Before lock we don't know if GPS second label is correct.
        // Stepping with wrong second causes permanent ~1s offsets.
        return false;  // Fail initialization - must have lock
    }
    
    std::cout << "[Controller] ✓ GPS PPS-UTC lock established - safe to proceed\n";
    
    // Get GPS time (this returns TAI: UTC + 37 seconds)
    uint64_t gps_tai_sec;
    uint32_t gps_nsec;
    if (!gps_->get_ptp_time(&gps_tai_sec, &gps_nsec)) {
        std::cerr << "[Controller] ERROR: Failed to get GPS time\n";
        return false;
    }
    
    // CRITICAL FIX (expert guidance from deb.md):
    // Convert TAI to UTC for PHC stepping!
    // The servo computes offsets in UTC (GPS_UTC = TAI - 37),
    // so PHC MUST also be in UTC, otherwise we get permanent ~37s offset.
    const uint32_t TAI_UTC_OFFSET = 37;  // seconds (valid 2017-2025)
    uint64_t gps_utc_sec = gps_tai_sec - TAI_UTC_OFFSET;
    
    std::cout << "[Controller] GPS time (TAI): " << gps_tai_sec << "." 
              << std::setfill('0') << std::setw(9) << gps_nsec << " s\n";
    std::cout << "[Controller] GPS time (UTC): " << gps_utc_sec << "." 
              << std::setfill('0') << std::setw(9) << gps_nsec << " s\n";
    
    // 1. Step PHC to GPS UTC time (NOT TAI!)
    std::cout << "[Controller] Stepping PHC to GPS UTC time...\n";
    if (!phc_->set_time(gps_utc_sec, gps_nsec)) {
        std::cerr << "[Controller] ERROR: Failed to set PHC time\n";
        return false;
    }
    std::cout << "[Controller] ✓ PHC synchronized to GPS (UTC timescale)\n";
    
    // 2. Step RTC to GPS UTC time (already converted above)
    std::cout << "[Controller] Stepping RTC to GPS UTC time...\n";
    RtcTime rtc_time;
    // Use UTC seconds (already converted from TAI above)
    uint64_t utc_sec = gps_utc_sec;
    
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
    uint32_t last_processed_pps_seq = 0;  // Track last PPS sequence we processed
    
    for (int attempt = 0; attempt < 120; attempt++) {  // 120 seconds max (was 30)
        // UPDATE GPS DATA to get fresh PPS!
        gps_->update();
        
        // Get current PPS data
        PpsData pps;
        if (!gps_->get_pps_data(&pps, nullptr)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // CRITICAL FIX (expert guidance from deb.md): Only process NEW PPS pulses!
        // The calibrator must count actual PPS edges (sequence changes), not "valid reads".
        // If we call update_calibration() with the same sequence multiple times, it will
        // think "20 pulses elapsed" when only 18 PPS edges actually happened, producing
        // impossible drift measurements like "-99936 ppm" (actually a measurement bug).
        if (pps.sequence == last_processed_pps_seq && last_processed_pps_seq != 0) {
            // No new PPS pulse yet, wait for next edge
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Check for PPS dropout (sequence jumped by >1)
        if (last_processed_pps_seq != 0 && (pps.sequence - last_processed_pps_seq) > 1) {
            std::cerr << "[Controller] WARNING: PPS dropout detected (seq jumped from " 
                     << last_processed_pps_seq << " to " << pps.sequence << ")\n";
            if (baseline_set) {
                std::cout << "[Controller] Restarting calibration due to dropout...\n";
                baseline_set = false;  // Restart calibration
            }
        }
        
        last_processed_pps_seq = pps.sequence;  // Remember this sequence
        
        // Get PHC timestamp
        uint64_t phc_sec;
        uint32_t phc_nsec;
        if (!phc_->get_time(&phc_sec, &phc_nsec)) {
            std::cerr << "[Controller] ERROR: Failed to read PHC time\n";
            return false;
        }
        
        int64_t phc_ns = static_cast<int64_t>(phc_sec) * 1000000000LL 
                       + static_cast<int64_t>(phc_nsec);
        
        // Start calibration ONCE, then update on subsequent NEW PPS edges
        if (!baseline_set) {
            calibrator_->start_calibration(pps.sequence, phc_ns);
            baseline_set = true;
            // Don't sleep here - immediately wait for next PPS edge
            continue;
        }
        
        // Update calibration with NEW measurement (guaranteed new PPS edge)
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
    static uint32_t pps_seq_when_stepped = 0;  // Track when we last stepped PHC
    static uint32_t last_processed_pps_seq = 0;  // Track last GPS sample we processed
    
    while (running_) {
        loop_count++;
        
        // CRITICAL: Update GPS data to fetch new NMEA sentences and PPS timestamps!
        gps_->update();
        
        // CRITICAL BUG #14 FIX: Check if we need to wait for PPS stabilization BEFORE fetching GPS time
        // After PHC step, PPS timestamps are captured in OLD timescale. We must wait for 3 complete
        // PPS pulses in the NEW timescale before measuring offset again.
        const PpsData& pps_check = gps_->get_pps_data();
        if (pps_seq_when_stepped != 0) {
            uint32_t pulses_since_step = pps_check.sequence - pps_seq_when_stepped;
            if (pulses_since_step < 3) {
                // Still waiting for PPS to stabilize - skip entire loop iteration
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;  // Don't fetch GPS time yet!
            } else {
                // 3 pulses have passed, reset the wait
                pps_seq_when_stepped = 0;
            }
        }
        
        // CRITICAL BUG #14 ROOT CAUSE FIX: Only process new GPS samples!
        // GPS time is derived from PPS sequence, which only updates once per second.
        // If we run the servo loop at 10 Hz but GPS updates at 1 Hz, we'll process
        // the same stale GPS time 10 times, causing incorrect offset calculations
        // and unnecessary step corrections.
        //
        // SOLUTION: Only update servo when PPS sequence advances (new GPS sample).
        // CRITICAL (expert deb.md): Only process offset on NEW PPS edges!
        // "Your loop prints multiple times between pulses. Only produce a new servo input when pps_seq changed."
        PpsData pps;
        bool pps_valid = gps_->get_pps_data(&pps, nullptr);
        
        if (pps.sequence == last_processed_pps_seq && last_processed_pps_seq != 0) {
            // No new PPS edge, skip servo processing (expert requirement)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // NEW PPS EDGE DETECTED - Process offset measurement
        last_processed_pps_seq = pps.sequence;
        
        // 1. Get GPS UTC time at PPS edge (integer seconds only!)
        uint64_t gps_tai_sec = 0;  // get_ptp_time() returns TAI
        uint32_t gps_nsec = 0;     // NOW returns 0 (PPS = integer second boundary)
        bool gps_valid = gps_->get_ptp_time(&gps_tai_sec, &gps_nsec);
        
        // Convert TAI to UTC for offset calculation
        const uint32_t TAI_UTC_OFFSET = 37;
        uint64_t gps_utc_sec = gps_tai_sec - TAI_UTC_OFFSET;
        
        // 2. Read PHC time AT PPS EDGE (expert: "immediately read PHC right after the pulse")
        uint64_t phc_sec = 0;
        uint32_t phc_nsec = 0;
        bool phc_valid = phc_->get_time(&phc_sec, &phc_nsec);
        
        // Debug first few PPS edges
        static int debug_count = 0;
        if (debug_count++ < 5) {
            std::cout << "[Controller] PPS #" << pps.sequence 
                     << " GPS_UTC=" << gps_utc_sec << "." << std::setfill('0') << std::setw(9) << gps_nsec
                     << " (integer sec at PPS edge)"
                     << " PHC=" << phc_sec << "." << std::setfill('0') << std::setw(9) << phc_nsec
                     << " (read at PPS edge)\n";
        }
        
        if (!gps_valid || !phc_valid || !pps_valid) {
            std::cerr << "[Controller] WARNING: Time read failed (GPS=" 
                     << gps_valid << ", PHC=" << phc_valid << ", PPS=" << pps_valid << ")\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 3. Calculate offset: GPS_UTC(integer sec at PPS) - PHC(at PPS edge)
        // Expert: "offset_ns = utc_ns - phc_ns" where utc_ns = pps_seq mapping (integer seconds)
        int64_t offset_ns = calculate_offset(gps_utc_sec, gps_nsec, phc_sec, phc_nsec);
        last_offset_ns_ = offset_ns;
        
        // DEBUG: Show detailed timing in UTC nanoseconds (consistent time base)
        uint64_t gps_utc_ns = gps_utc_sec * 1000000000ULL + gps_nsec;
        uint64_t phc_ns = phc_sec * 1000000000ULL + phc_nsec;
        
        static int timing_debug_count = 0;
        if (timing_debug_count++ % 10 == 0 || std::abs(offset_ns) > 100000000) {
            std::cout << "[TIMING #" << timing_debug_count << "] "
                     << "GPS_UTC=" << gps_utc_sec << "." << std::setfill('0') << std::setw(9) << gps_nsec 
                     << " (" << gps_utc_ns << "ns) "
                     << "PHC=" << phc_sec << "." << std::setfill('0') << std::setw(9) << phc_nsec
                     << " (" << phc_ns << "ns) "
                     << "offset=" << offset_ns << "ns"
                     << " PPS_seq=" << pps.sequence << "\n";
        }
        
        // 4. Update state machine (pass TAI time for state tracking)
        state_machine_->update(pps_valid, gps_valid, offset_ns, 
                             (double)cumulative_freq_ppb_, gps_tai_sec);
        ServoState current_state = state_machine_->get_state();
        
        // 5. Apply correction based on offset magnitude
        if (std::abs(offset_ns) > config_.step_threshold_ns) {
            // Only step if GPS adapter has established PPS-UTC lock
            if (!gps_->is_locked()) {
                std::cout << "[Controller] WARNING: Large offset detected but GPS not locked yet, skipping step\n";
            } else {
                // Large offset: apply step correction (pass TAI time, will be converted inside)
                apply_step_correction(gps_tai_sec, gps_nsec);
                pps_seq_when_stepped = pps.sequence;  // Record PPS when we stepped
                
                // CRITICAL FIX (expert guidance): After stepping, IMMEDIATELY apply frequency correction!
                // The calibration measured PHC drift (~166 ppm), so apply it NOW to prevent immediate re-step.
                // Without this, the PHC continues drifting at ~160ppm, hits 100ms offset again in ~600 seconds,
                // and steps repeatedly without ever correcting frequency.
                if (calibration_complete_) {
                    std::cout << "[Controller] Applying calibration frequency after step: " 
                             << cumulative_freq_ppb_ << " ppb\n";
                    phc_->adjust_frequency(cumulative_freq_ppb_);
                }
            }
        }
        
        // CRITICAL FIX (Layer 9): Only increment cycle counter on NEW PPS edges!
        // Previously incremented every loop, causing servo to run hundreds of times per second
        // and accumulate corrections incorrectly. Settling cycles are meant to be PPS cycles.
        
        // ALWAYS run servo correction (even after step) to track offset changes
        // Expert: "Servo should always process offset measurements, not just when offset is small"
        // BUT: Skip for several cycles after a step to let PHC frequency settle
        // (clock_settime() resets frequency to 0, we reapply calibration, but servo needs time)
        cycles_since_step_++;
        const uint32_t SETTLE_CYCLES = 10;  // Skip servo for 10 PPS cycles (~10 seconds) after step
        if (cycles_since_step_ < SETTLE_CYCLES) {
            std::cout << "[Controller] Skipping servo (settling after step, PPS cycle " 
                     << cycles_since_step_ << "/" << SETTLE_CYCLES << ")\n";
        } else {
            // Only run servo on NEW PPS edges (offset measurement is per-PPS)
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
void GrandmasterController::apply_step_correction(uint64_t gps_tai_sec, uint32_t gps_nsec) {
    // CRITICAL FIX (expert guidance from deb.md):
    // Convert GPS time from TAI to UTC before stepping PHC!
    // The input gps_tai_sec is in TAI (from get_ptp_time()),
    // but PHC must be in UTC to match offset calculation (GPS_UTC - PHC).
    const uint32_t TAI_UTC_OFFSET = 37;  // seconds (valid 2017-2025)
    uint64_t gps_utc_sec = gps_tai_sec - TAI_UTC_OFFSET;
    
    static uint64_t last_gps_utc_sec = 0;
    static uint32_t last_gps_nsec = 0;
    
    if (gps_utc_sec == last_gps_utc_sec && gps_nsec == last_gps_nsec) {
        std::cout << "[Controller] WARNING: GPS time not updating! Same as last step: "
                 << gps_utc_sec << "." << gps_nsec << " (UTC)\n";
    }
    last_gps_utc_sec = gps_utc_sec;
    last_gps_nsec = gps_nsec;
    
    std::cout << "[Controller] Applying step correction (offset > " 
             << (config_.step_threshold_ns / 1000000) << " ms)\n";
    std::cout << "[Controller]   GPS (TAI): " << gps_tai_sec << "." << gps_nsec << " s\n";
    std::cout << "[Controller]   GPS (UTC): " << gps_utc_sec << "." << gps_nsec << " s\n";
    std::cout << "[Controller]   Stepping PHC to UTC timescale\n";
    
    // NO LONGER calculating step delta or notifying GPS adapter
    // The notify_phc_stepped() approach was incorrect and caused Bug #14 regression.
    // GPS time should be purely based on NMEA+PPS mapping, not adjusted for PHC steps.
    
    // SANITY CHECK (expert recommended guardrail):
    // If TAI-UTC difference is not ~37s, something is very wrong
    int64_t tai_utc_delta = gps_tai_sec - gps_utc_sec;
    if (std::abs(tai_utc_delta - 37) > 2) {
        std::cerr << "[Controller] ERROR: TAI-UTC delta is " << tai_utc_delta 
                 << "s (expected ~37s)!\n";
        std::cerr << "[Controller] Refusing to step - timescale corruption detected!\n";
        return;
    }
    
    // 1. Set PHC time to GPS UTC time (NOT TAI!)
    bool step_success = phc_->set_time(gps_utc_sec, gps_nsec);
    
    if (!step_success) {
        std::cerr << "[Controller] WARNING: PHC step failed\n";
    }
    
    // 2. Reset servo integrator
    if (servo_) {
        servo_->reset();
    }
    
    // 3. Reset cumulative frequency to calibration baseline
    cumulative_freq_ppb_ = calibration_drift_ppb_;
    phc_->adjust_frequency(cumulative_freq_ppb_);
    
    // 4. Reset settle counter - DON'T run servo for several cycles
    // CRITICAL (expert guidance): After clock_settime(), the frequency adjustment
    // gets reset to ZERO by the kernel. We've re-applied the calibration frequency,
    // but the servo should NOT run for ~10 cycles to let the system settle.
    // Running servo immediately after a step causes it to add its own large correction
    // on top of calibration, creating oscillations and repeated stepping.
    cycles_since_step_ = 0;
    
    step_count_++;
}

// Apply servo correction
void GrandmasterController::apply_servo_correction(int64_t offset_ns) {
    if (!servo_) {
        std::cerr << "[Servo] ERROR: servo_ is null!\n";
        return;
    }
    
    // 1. Calculate servo correction
    int32_t correction_ppb = servo_->calculate_correction(offset_ns);
    
    std::cout << "[Servo] offset=" << offset_ns << "ns correction=" << correction_ppb 
              << "ppb current_freq=" << cumulative_freq_ppb_ << "ppb\n";
    
    // 2. Update cumulative frequency
    int32_t new_freq_ppb = cumulative_freq_ppb_ + correction_ppb;
    
    // 3. Clamp to PHC limits (±500 ppm for i226)
    int32_t max_freq = phc_->get_max_frequency_ppb();
    if (new_freq_ppb > max_freq) {
        std::cout << "[Servo] Clamping " << new_freq_ppb << " to " << max_freq << " ppb\n";
        new_freq_ppb = max_freq;
    } else if (new_freq_ppb < -max_freq) {
        std::cout << "[Servo] Clamping " << new_freq_ppb << " to " << -max_freq << " ppb\n";
        new_freq_ppb = -max_freq;
    }
    
    // 4. Apply to PHC
    phc_->adjust_frequency(new_freq_ppb);
    
    std::cout << "[Servo] Applied new_freq=" << new_freq_ppb << " ppb to PHC\n";
    
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
