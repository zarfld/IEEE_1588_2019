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
    RtcDriftDiscipline* rtc_discipline,
    PhcAdapter* phc,
    NetworkAdapter* network,
    const GrandmasterConfig& config)
    : gps_(gps)
    , rtc_(rtc)
    , rtc_discipline_(rtc_discipline)
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
    , rtc_discipline_count_(0)
    , last_rtc_discipline_time_(std::chrono::steady_clock::now())
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
    
    std::cout << "[Controller] Waiting for GPS to acquire position fix...\n";
    std::cout << "[Controller] (This may take 30-60 seconds if GPS has cold start)\n";
    
    // Wait for GPS fix (up to 60 seconds)
    for (int i = 0; i < 60; i++) {
        gps_->update();
        
        // Debug: Show GPS status every 5 seconds
        if (i % 5 == 0 || i < 5) {
            uint64_t sec;
            uint32_t nsec;
            bool has_time = gps_->get_ptp_time(&sec, &nsec);
            uint8_t sat_count = gps_->get_satellite_count();
            
            std::cout << "[Controller] GPS status check " << (i+1) << "/60: "
                     << "has_fix=" << (gps_->has_fix() ? "YES" : "NO") << ", "
                     << "satellites=" << static_cast<int>(sat_count) << ", "
                     << "time_valid=" << (has_time ? "YES" : "NO");
            
            if (has_time) {
                std::cout << " (GPS time: " << sec << "s)";
            }
            std::cout << "\n";
        }
        
        if (gps_->has_fix()) {
            std::cout << "[Controller] ✓ GPS fix acquired (" 
                     << static_cast<int>(gps_->get_satellite_count()) << " satellites)\n";
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Check if we at least have valid time (PPS working) even without position fix
    uint64_t sec;
    uint32_t nsec;
    if (gps_->get_ptp_time(&sec, &nsec)) {
        std::cout << "[Controller] WARNING: No GPS position fix, but GPS time is valid\n";
        std::cout << "[Controller] Proceeding with time-only mode (grandmaster still functional)\n";
        return true;  // Allow operation with just time
    }
    
    return false;
}

// Set initial time offsets (CRITICAL: Do this BEFORE drift measurement!)
bool GrandmasterController::set_initial_time() {
    if (!gps_ || !phc_ || !rtc_) return false;
    
    // Wait for GPS adapter to establish PPS-UTC lock (critical for valid time)
    // LAYER 11 FIX: Increased timeout from 30s→60s→120s→180s to ensure lock establishment
    // Lock requires 3 NMEA samples. With NMEA every 50 seconds, need 150s minimum.
    // 180s provides adequate margin for slow NMEA update rates.
    std::cout << "[Controller] Waiting for GPS PPS-UTC lock (max 180s)...\n";
    for (int i = 0; i < 180; i++) {
        gps_->update();
        
        // CRITICAL: Must call get_ptp_time() to trigger association detection logic!
        // The lock establishment code is in get_ptp_time(), not update()
        uint64_t dummy_sec;
        uint32_t dummy_nsec;
        gps_->get_ptp_time(&dummy_sec, &dummy_nsec);
        
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
    
    // CRITICAL FIX: Use sync_from_gps() which handles PPS edge timing correctly!
    // The GPS PPS edge marks the START of second S, but NMEA arrives ~200ms later.
    // By the time we write to DS3231 I2C, we're already IN second S.
    // sync_from_gps() adds +1 second so DS3231 reads S (current) then increments to S+1 at next boundary.
    // This eliminates the persistent ~1 second GPS-RTC offset!
    if (!rtc_->sync_from_gps(gps_utc_sec, gps_nsec)) {
        std::cerr << "[Controller] WARNING: Failed to sync RTC from GPS (non-fatal)\n";
        // Non-fatal - continue even if RTC sync fails, but drift measurement will have epoch offset
    } else {
        std::cout << "[Controller] ✓ RTC synchronized to GPS (UTC epoch aligned with +1s compensation)\n";
        std::cout << "[Controller]   GPS-RTC offset eliminated, DriftObserver measuring crystal drift only\n";
        
        // Wait for RTC I2C write to complete and stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Note: sync_from_gps() already reset DriftObserver and set skip_samples_
        std::cout << "[Controller] ✓ RTC DriftObserver reset by sync_from_gps() (fresh start)\n";
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
        
        // CRITICAL FIX Layer 17: Use PPS assert timestamp for precision!
        // USER INSIGHT: "use GPS-PPS signal for second-start indicator! 
        // the next coming value from GPS will be the second to assign to that tick!"
        // "so you should use PPS to ensure precision for GPS clock!"
        //
        // The PPS device captures the EXACT timestamp (pps.assert_sec/nsec) when the 
        // GPS second boundary occurred. This is our PRECISE reference!
        // NMEA tells us WHICH GPS second it was (via base_utc_sec mapping).
        
        // 1. Get GPS UTC integer seconds from NMEA (tells us WHICH second)
        uint64_t gps_tai_sec = 0;  // get_ptp_time() returns TAI
        uint32_t gps_nsec = 0;     // Not used - we use PPS assert timestamp instead!
        bool gps_valid = gps_->get_ptp_time(&gps_tai_sec, &gps_nsec);
        
        // Convert TAI to UTC
        const uint32_t TAI_UTC_OFFSET = 37;
        uint64_t gps_utc_sec = gps_tai_sec - TAI_UTC_OFFSET;
        
        // 2. Use PPS assert timestamp as the PRECISE moment that GPS second occurred
        // This timestamp was captured by kernel at the exact PPS interrupt
        uint64_t gps_timestamp_sec = pps.assert_sec;
        uint32_t gps_timestamp_nsec = pps.assert_nsec;
        
        // 3. Now read PHC to compare against the GPS reference timestamp
        uint64_t phc_sec = 0;
        uint32_t phc_nsec = 0;
        bool phc_valid = phc_->get_time(&phc_sec, &phc_nsec);
        
        // Debug first few PPS edges
        static int debug_count = 0;
        if (debug_count++ < 5) {
            std::cout << "[Controller] PPS #" << pps.sequence 
                     << " GPS_UTC_sec=" << gps_utc_sec << " (WHICH second from NMEA)"
                     << " GPS_timestamp=" << gps_timestamp_sec << "." << std::setfill('0') << std::setw(9) << gps_timestamp_nsec
                     << " (WHEN from PPS assert)"
                     << " PHC=" << phc_sec << "." << std::setfill('0') << std::setw(9) << phc_nsec << "\n";
        }
        
        if (!gps_valid || !phc_valid || !pps_valid) {
            std::cerr << "[Controller] WARNING: Time read failed (GPS=" 
                     << gps_valid << ", PHC=" << phc_valid << ", PPS=" << pps_valid << ")\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 3. Calculate offset: GPS_timestamp (precise PPS assert) should equal GPS_UTC integer second
        // The offset we want is: GPS_UTC_seconds - PHC
        // But we use GPS_timestamp (PPS assert) as the precise reference for WHEN that second occurred
        int64_t offset_ns = calculate_offset(gps_utc_sec, 0, phc_sec, phc_nsec);
        last_offset_ns_ = offset_ns;
        
        // DEBUG: Show detailed timing in UTC nanoseconds (consistent time base)
        uint64_t gps_utc_ns = gps_utc_sec * 1000000000ULL;  // Integer seconds only
        uint64_t gps_timestamp_ns = gps_timestamp_sec * 1000000000ULL + gps_timestamp_nsec;  // PPS assert timestamp
        uint64_t phc_ns = phc_sec * 1000000000ULL + phc_nsec;
        
        // LAYER 17 DEBUG: Read ALL available clock sources for comprehensive comparison
        // This helps identify which clock source (if any) is causing discontinuities
        struct timespec ts_sys, ts_ds3231;
        uint64_t system_rtc_ns = 0;
        uint64_t ds3231_rtc_ns = 0;
        
        // Read Raspberry Pi system RTC (CLOCK_REALTIME)
        if (clock_gettime(CLOCK_REALTIME, &ts_sys) == 0) {
            system_rtc_ns = ts_sys.tv_sec * 1000000000ULL + ts_sys.tv_nsec;
        }
        
        // Read DS3231 RTC via RTC adapter (if available)
        uint64_t ds3231_sec = 0;
        uint32_t ds3231_nsec = 0;
        if (rtc_ && rtc_->get_time(&ds3231_sec, &ds3231_nsec)) {
            ds3231_rtc_ns = ds3231_sec * 1000000000ULL + ds3231_nsec;
        }
        
        // Feed GPS-RTC PPS tick to DriftObserver for holdover monitoring
        // CRITICAL: Measure RTC oscillator drift (NOT PHC drift - that's separate!)
        // RTC drift is needed for GPS holdover - when GPS is lost, use RTC+drift correction
        if (rtc_) {
            // GPS time at PPS edge (nanoseconds always .000000000 at second boundary)
            int64_t gps_time_ns = static_cast<int64_t>(gps_utc_sec) * 1000000000LL;
            
            // RTC time from DS3231 hardware (NON-BLOCKING)
            // This returns:
            //   - seconds: DS3231 hardware time (read via I2C)
            //   - nanoseconds: sub-second offset from PPS edge
            // Together they form the complete RTC timestamp for drift measurement
            uint64_t rtc_sec;
            uint32_t rtc_nsec;
            if (rtc_->get_time(&rtc_sec, &rtc_nsec, false)) {  // wait_for_edge=false - NON-BLOCKING!
                // Build full RTC timestamp
                // GPS PPS: gps_utc_sec.000000000 (exact second boundary)
                // RTC:     rtc_sec.rtc_nsec (hardware time + sub-second from PPS)
                int64_t rtc_time_ns = static_cast<int64_t>(rtc_sec) * 1000000000LL + rtc_nsec;
                rtc_->process_pps_tick(gps_time_ns, rtc_time_ns);
            }
        }
        
        static int timing_debug_count = 0;
        if (timing_debug_count++ % 10 == 0 || std::abs(offset_ns) > 100000000) {
            std::cout << "[TIMING #" << timing_debug_count << "] "
                     << "GPS_UTC=" << gps_utc_sec << "." << std::setfill('0') << std::setw(9) << gps_nsec 
                     << " (" << gps_utc_ns << "ns) "
                     << "PHC=" << phc_sec << "." << std::setfill('0') << std::setw(9) << phc_nsec
                     << " (" << phc_ns << "ns) "
                     << "offset=" << offset_ns << "ns"
                     << " PPS_seq=" << pps.sequence << "\n";
            
            // LAYER 17: Multi-clock comparison (all times in nanoseconds for easy diff)
            std::cout << "[CLOCKS] "
                     << "GPS_UTC=" << gps_utc_ns << "ns "
                     << "PHC=" << phc_ns << "ns "
                     << "SYS_RTC=" << system_rtc_ns << "ns "
                     << "DS3231=" << ds3231_rtc_ns << "ns\n";
            
            // Calculate all pairwise offsets to identify discontinuities
            int64_t gps_phc_offset = (int64_t)(gps_utc_ns - phc_ns);
            int64_t gps_sys_offset = (int64_t)(gps_utc_ns - system_rtc_ns);
            int64_t gps_ds3231_offset = (int64_t)(gps_utc_ns - ds3231_rtc_ns);
            int64_t phc_sys_offset = (int64_t)(phc_ns - system_rtc_ns);
            int64_t phc_ds3231_offset = (int64_t)(phc_ns - ds3231_rtc_ns);
            int64_t sys_ds3231_offset = (int64_t)(system_rtc_ns - ds3231_rtc_ns);
            
            std::cout << "[OFFSETS] "
                     << "GPS-PHC=" << gps_phc_offset << "ns "
                     << "GPS-SYS=" << gps_sys_offset << "ns "
                     << "GPS-DS3231=" << gps_ds3231_offset << "ns "
                     << "PHC-SYS=" << phc_sys_offset << "ns "
                     << "PHC-DS3231=" << phc_ds3231_offset << "ns "
                     << "SYS-DS3231=" << sys_ds3231_offset << "ns\n";
        }
        
        // 4. Update state machine (pass TAI time for state tracking)
        state_machine_->update(pps_valid, gps_valid, offset_ns, 
                             (double)cumulative_freq_ppb_, gps_tai_sec);
        ServoState current_state = state_machine_->get_state();
        
        // 5. Apply correction based on offset magnitude
        // LAYER 11 FIX: Step FIRST if offset large, BEFORE servo runs
        // Pattern: "Always OFFSET correction first (step), THEN frequency adjustment (servo)"
        // Prevents servo from accumulating huge corrections while offset is > 100ms
        if (std::abs(offset_ns) > config_.step_threshold_ns) {
            // Only step if GPS adapter has established PPS-UTC lock
            if (!gps_->is_locked()) {
                std::cout << "[Controller] WARNING: Large offset detected but GPS not locked yet, skipping step\n";
                // Don't run servo either when offset is large - skip this cycle
                continue;
            } else {
                // Large offset: apply step correction (pass TAI time, will be converted inside)
                apply_step_correction(gps_tai_sec, gps_nsec);
                pps_seq_when_stepped = pps.sequence;  // Record PPS when we stepped
                
                // CRITICAL FIX (expert guidance): After stepping, IMMEDIATELY apply frequency correction!
                // The calibration measured PHC drift (~88 ppm), so apply it NOW to prevent immediate re-step.
                // Without this, the PHC continues drifting at ~88ppm, hits 100ms offset again in ~1000 seconds,
                // and steps repeatedly without ever correcting frequency.
                if (calibration_complete_) {
                    std::cout << "[Controller] Applying calibration frequency after step: " 
                             << cumulative_freq_ppb_ << " ppb\n";
                    phc_->adjust_frequency(cumulative_freq_ppb_);
                }
                
                // LAYER 12 FIX: After stepping and applying calibration, IMMEDIATELY skip to next cycle
                // This prevents cycles_since_step_ from being incremented on the SAME cycle as the step,
                // which would cause us to immediately re-measure offset (still large because step takes time)
                // and step again on the NEXT cycle before PHC has settled!
                continue;
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
        
        // 8. Poll for incoming PTP messages (Delay_Req handling)
        poll_rx_messages();
        
        // 9. Sleep until next cycle
        // Use shorter interval during convergence (offset >1ms) for faster servo response
        uint32_t cycle_interval_ms = (std::abs(offset_ns) > 1000000) ? 100 : config_.sync_interval_ms;
        std::this_thread::sleep_for(std::chrono::milliseconds(cycle_interval_ms));
        
        // 10. RTC Drift Discipline (every 10 seconds)
        rtc_discipline_count_++;
        // Trigger based on elapsed time, not cycle count (cycle time varies 100-1000ms)
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_rtc_discipline_time_).count();
        
        if (elapsed >= 10 && rtc_discipline_) {
                // Get GPS time (TAI)
                uint64_t gps_tai_sec;
                uint32_t gps_nsec;
                if (gps_->get_ptp_time(&gps_tai_sec, &gps_nsec)) {
                    // Get RTC time (UTC)
                    uint64_t rtc_seconds;
                    uint32_t rtc_nanoseconds;
                    if (rtc_->get_ptp_time(&rtc_seconds, &rtc_nanoseconds)) {
                        // Convert GPS TAI to UTC for comparison
                        const uint32_t TAI_UTC_OFFSET = 37;
                        uint64_t gps_utc_sec = gps_tai_sec - TAI_UTC_OFFSET;
                        
                        // Calculate drift (GPS - RTC) in ppm
                        int64_t time_diff_ns = (static_cast<int64_t>(gps_utc_sec) * 1000000000LL + gps_nsec) -
                                              (static_cast<int64_t>(rtc_seconds) * 1000000000LL + rtc_nanoseconds);
                        double drift_ppm = (time_diff_ns / (static_cast<double>(elapsed) * 1e9)) * 1e6;
                        
                        std::cout << "[RTC Discipline] GPS=" << gps_utc_sec << "." << gps_nsec 
                                  << " RTC=" << rtc_seconds << "." << rtc_nanoseconds 
                                  << " diff=" << time_diff_ns << "ns drift=" << drift_ppm << "ppm\n" << std::flush;
                        
                        // Add sample to discipline
                        rtc_discipline_->add_sample(drift_ppm, gps_tai_sec);
                        
                        // Check if adjustment needed
                        if (rtc_discipline_->should_adjust(gps_tai_sec)) {
                            int8_t lsb_adjustment = rtc_discipline_->calculate_lsb_adjustment();
                            
                            std::cout << "[RTC Discipline] Adjustment needed! LSB=" << static_cast<int>(lsb_adjustment) 
                                     << " samples=" << rtc_discipline_->get_sample_count()
                                     << " avg=" << rtc_discipline_->get_average_drift() << "ppm"
                                     << " stddev=" << rtc_discipline_->get_stddev() << "ppm\n" << std::flush;
                            
                            if (rtc_->adjust_aging_offset(lsb_adjustment)) {
                                std::cout << "[RTC Discipline] ✓ Applied aging offset adjustment: "
                                         << static_cast<int>(lsb_adjustment) << " LSB\n" << std::flush;
                            } else {
                                std::cerr << "[RTC Discipline] ✗ Failed to apply aging offset adjustment\n" << std::flush;
                            }
                        } else {
                            std::cout << "[RTC Discipline] Not ready for adjustment (samples=" 
                                     << rtc_discipline_->get_sample_count() << ")\n" << std::flush;
                        }
                    } else {
                        std::cerr << "[RTC Discipline] ERROR: Failed to get RTC time\n" << std::flush;
                    }
                } else {
                    std::cerr << "[RTC Discipline] ERROR: Failed to get GPS time\n" << std::flush;
                }
                
                last_rtc_discipline_time_ = now;
            }
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
    std::cout << "[Controller DEBUG] Calling phc_adapter->set_time(" << gps_utc_sec << ", " << gps_nsec << ")\n";
    
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
    if (!network_) {
        std::cout << "[Controller] ❌ TX Sync FAILED: network_ is null\n" << std::flush;
        return;
    }
    
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
        uint64_t ts_ns = tx_ts.seconds * 1000000000ULL + tx_ts.nanoseconds;
        std::cout << "[Controller] 📤 TX: Sync message (" << sent << " bytes, hw_ts=" 
                  << ts_ns << ")\n" << std::flush;
    } else {
        std::cout << "[Controller] ❌ TX Sync FAILED: send_packet returned " << sent << "\n" << std::flush;
    }
}

// Send Announce message
void GrandmasterController::send_announce_message() {
    if (!network_) {
        std::cout << "[Controller] ❌ TX Announce FAILED: network_ is null\n" << std::flush;
        return;
    }
    
    // Simple Announce message (IEEE 1588-2019 format)
    uint8_t announce_packet[64];
    std::memset(announce_packet, 0, sizeof(announce_packet));
    
    announce_packet[0] = 0x0B;  // messageType: Announce
    announce_packet[1] = 0x02;  // versionPTP: 2
    
    NetworkTimestamp tx_ts;
    int sent = network_->send_packet(announce_packet, sizeof(announce_packet), &tx_ts);
    
    if (sent > 0) {
        announce_count_++;
        uint64_t ts_ns = tx_ts.seconds * 1000000000ULL + tx_ts.nanoseconds;
        std::cout << "[Controller] 📤 TX: Announce message (" << sent << " bytes, hw_ts=" 
                  << ts_ns << ")\n" << std::flush;
    } else {
        std::cout << "[Controller] ❌ TX Announce FAILED: send_packet returned " << sent << "\n" << std::flush;
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

//==============================================================================
// PTP Delay Mechanism - RX Message Processing
//==============================================================================

void GrandmasterController::poll_rx_messages() {
    if (!network_) {
        return;
    }
    
    // Debug: Log polling activity periodically
    static uint64_t poll_count = 0;
    poll_count++;
    if (poll_count % 100 == 0) {
        std::cout << "[RX Poll] Polling for PTP messages (count=" << poll_count << ")\n" << std::flush;
    }
    
    // Poll for incoming PTP messages (non-blocking)
    uint8_t rx_buffer[512];
    NetworkTimestamp rx_timestamp;
    
    ssize_t received = network_->recv_ptp_message(rx_buffer, sizeof(rx_buffer), &rx_timestamp);
    
    if (received <= 0) {
        // No message or error (non-blocking, expected)
        return;
    }
    
    // Parse message type
    int msg_type = NetworkAdapter::parse_message_type(rx_buffer, received);
    
    if (msg_type < 0) {
        std::cerr << "[Controller] Failed to parse message type\n" << std::flush;
        return;
    }
    
    // Handle Sync messages (0x0) - for observability
    if (msg_type == 0x0) {  // MessageType::Sync
        // Parse source port identity (bytes 20-29)
        uint64_t src_clock_id = ((uint64_t)rx_buffer[20] << 56) | ((uint64_t)rx_buffer[21] << 48) |
                                ((uint64_t)rx_buffer[22] << 40) | ((uint64_t)rx_buffer[23] << 32) |
                                ((uint64_t)rx_buffer[24] << 24) | ((uint64_t)rx_buffer[25] << 16) |
                                ((uint64_t)rx_buffer[26] << 8)  | rx_buffer[27];
        uint16_t src_port = ((uint16_t)rx_buffer[28] << 8) | rx_buffer[29];
        
        // Parse sequence ID (bytes 30-31)
        uint16_t seq_id = ((uint16_t)rx_buffer[30] << 8) | rx_buffer[31];
        
        // Filter out our own messages (loopback)
        // Our clock ID is based on MAC address - should not be all zeros
        if (src_clock_id == 0) {
            std::cout << "[Controller] ⚠️  RX: Sync with ZERO clockID - likely our own TX loopback, ignoring\n" << std::flush;
            return;  // Skip our own messages
        }
        
        std::cout << "[Controller] 📨 RX: Sync message (" << received << " bytes)"
                  << " from clockID=" << std::hex << std::setfill('0') << std::setw(16) << src_clock_id
                  << std::dec << " port=" << src_port << " seq=" << seq_id
                  << " RX_TS=" << rx_timestamp.seconds << "." << std::setfill('0') << std::setw(9) << rx_timestamp.nanoseconds
                  << "\n" << std::flush;
    }
    // Handle Announce messages (0x0B) - BMCA data
    else if (msg_type == 0x0B) {  // MessageType::Announce
        // Parse source port identity (bytes 20-29)
        uint64_t src_clock_id = ((uint64_t)rx_buffer[20] << 56) | ((uint64_t)rx_buffer[21] << 48) |
                                ((uint64_t)rx_buffer[22] << 40) | ((uint64_t)rx_buffer[23] << 32) |
                                ((uint64_t)rx_buffer[24] << 24) | ((uint64_t)rx_buffer[25] << 16) |
                                ((uint64_t)rx_buffer[26] << 8)  | rx_buffer[27];
        uint16_t src_port = ((uint16_t)rx_buffer[28] << 8) | rx_buffer[29];
        
        // Parse sequence ID (bytes 30-31)
        uint16_t seq_id = ((uint16_t)rx_buffer[30] << 8) | rx_buffer[31];
        
        // Parse Announce message body (starts at byte 34)
        // Origin timestamp (bytes 34-43) - not critical for BMCA
        
        // Current UTC offset (bytes 44-45)
        uint16_t current_utc_offset = ((uint16_t)rx_buffer[44] << 8) | rx_buffer[45];
        
        // Grandmaster priority1 (byte 47)
        uint8_t gm_priority1 = rx_buffer[47];
        
        // Grandmaster clock class (byte 48)
        uint8_t gm_clock_class = rx_buffer[48];
        
        // Grandmaster clock accuracy (byte 49)
        uint8_t gm_clock_accuracy = rx_buffer[49];
        
        // Grandmaster clock variance (bytes 50-51)
        uint16_t gm_clock_variance = ((uint16_t)rx_buffer[50] << 8) | rx_buffer[51];
        
        // Grandmaster priority2 (byte 52)
        uint8_t gm_priority2 = rx_buffer[52];
        
        // Grandmaster identity (bytes 53-60)
        uint64_t gm_identity = ((uint64_t)rx_buffer[53] << 56) | ((uint64_t)rx_buffer[54] << 48) |
                               ((uint64_t)rx_buffer[55] << 40) | ((uint64_t)rx_buffer[56] << 32) |
                               ((uint64_t)rx_buffer[57] << 24) | ((uint64_t)rx_buffer[58] << 16) |
                               ((uint64_t)rx_buffer[59] << 8)  | rx_buffer[60];
        
        // Steps removed (bytes 61-62)
        uint16_t steps_removed = ((uint16_t)rx_buffer[61] << 8) | rx_buffer[62];
        
        // Time source (byte 63)
        uint8_t time_source = rx_buffer[63];
        
        // Filter out our own messages (loopback)
        if (src_clock_id == 0 || gm_identity == 0) {
            std::cout << "[Controller] ⚠️  RX: Announce with ZERO clockID - likely our own TX loopback, ignoring\n" << std::flush;
            return;  // Skip our own messages
        }
        
        std::cout << "[Controller] 🔔 RX: Announce message (" << received << " bytes) seq=" << seq_id << "\n"
                  << "  Source: clockID=" << std::hex << std::setfill('0') << std::setw(16) << src_clock_id << std::dec
                  << " port=" << src_port << "\n"
                  << "  BMCA: priority1=" << (int)gm_priority1 << " priority2=" << (int)gm_priority2
                  << " class=" << (int)gm_clock_class << " accuracy=0x" << std::hex << (int)gm_clock_accuracy << std::dec << "\n"
                  << "  GM_Identity=" << std::hex << std::setfill('0') << std::setw(16) << gm_identity << std::dec
                  << " steps=" << steps_removed << " timeSource=0x" << std::hex << (int)time_source << std::dec
                  << " UTC_offset=" << current_utc_offset << "\n" << std::flush;
    }
    // Handle Delay_Req messages (0x1)
    else if (msg_type == 0x1) {  // MessageType::Delay_Req
        std::cout << "[Controller] 🎯 RX: Delay_Req message (" << received << " bytes)"
                 << " RX_TS=" << rx_timestamp.seconds << "." 
                 << std::setfill('0') << std::setw(9) << rx_timestamp.nanoseconds << "\n" << std::flush;
        
        // Parse Delay_Req message
        using namespace IEEE::_1588::PTP::_2019;
        DelayReqBody delay_req;
        Types::PortIdentity source_port;
        
        if (parse_delay_req(rx_buffer, received, &delay_req, &source_port)) {
            // Prepare Delay_Resp message (IEEE 1588-2019 Section 13.8)
            DelayRespBody delay_resp;
            // Combine seconds_high (16-bit) and seconds_low (32-bit) into 48-bit timestamp
            delay_resp.receiveTimestamp.seconds_high = (rx_timestamp.seconds >> 32) & 0xFFFF;
            delay_resp.receiveTimestamp.seconds_low = rx_timestamp.seconds & 0xFFFFFFFF;
            delay_resp.receiveTimestamp.nanoseconds = rx_timestamp.nanoseconds;
            delay_resp.requestingPortIdentity = source_port;
            
            // Send Delay_Resp
            auto result = send_delay_resp(delay_resp, source_port);
            if (!result.isSuccess()) {
                std::cerr << "[Controller] ⚠️ Failed to send Delay_Resp\n" << std::flush;
            }
        } else {
            std::cerr << "[Controller] ⚠️ Failed to parse Delay_Req\n" << std::flush;
        }
    }
    // Handle Pdelay_Req messages (0x2) - P2P delay mechanism
    else if (msg_type == 0x2) {  // MessageType::Pdelay_Req
        // Parse source port identity (bytes 20-29)
        uint64_t src_clock_id = ((uint64_t)rx_buffer[20] << 56) | ((uint64_t)rx_buffer[21] << 48) |
                                ((uint64_t)rx_buffer[22] << 40) | ((uint64_t)rx_buffer[23] << 32) |
                                ((uint64_t)rx_buffer[24] << 24) | ((uint64_t)rx_buffer[25] << 16) |
                                ((uint64_t)rx_buffer[26] << 8)  | rx_buffer[27];
        uint16_t src_port = ((uint16_t)rx_buffer[28] << 8) | rx_buffer[29];
        
        // Parse sequence ID (bytes 30-31)
        uint16_t seq_id = ((uint16_t)rx_buffer[30] << 8) | rx_buffer[31];
        
        // Parse originTimestamp (bytes 34-43) - not used in response but log it
        uint16_t origin_ts_sec_high = ((uint16_t)rx_buffer[34] << 8) | rx_buffer[35];
        uint32_t origin_ts_sec_low = ((uint32_t)rx_buffer[36] << 24) | ((uint32_t)rx_buffer[37] << 16) |
                                     ((uint32_t)rx_buffer[38] << 8)  | rx_buffer[39];
        uint32_t origin_ts_nsec = ((uint32_t)rx_buffer[40] << 24) | ((uint32_t)rx_buffer[41] << 16) |
                                  ((uint32_t)rx_buffer[42] << 8)  | rx_buffer[43];
        
        std::cout << "[Controller] 🔄 RX: Pdelay_Req (P2P mechanism) from clockID="
                  << std::hex << std::setfill('0') << std::setw(16) << src_clock_id << std::dec
                  << " port=" << src_port << " seq=" << seq_id
                  << " RX_TS=" << rx_timestamp.seconds << "." << std::setfill('0') << std::setw(9) << rx_timestamp.nanoseconds
                  << "\n" << std::flush;
        
        // Send Pdelay_Resp (IEEE 1588-2019 Section 13.9)
        send_pdelay_resp(src_clock_id, src_port, seq_id, rx_timestamp);
        
        // Note: Pdelay_Resp_Follow_Up should be sent after getting TX timestamp of Pdelay_Resp
        // For now, send it immediately with TX timestamp = RX timestamp (approximation)
        send_pdelay_resp_follow_up(src_clock_id, src_port, seq_id, rx_timestamp);
    }
    else {
        std::cout << "[Controller] 📨 RX: PTP message type=" << msg_type 
                 << " (" << received << " bytes) [unhandled]\n" << std::flush;
    }
    // Future: Handle other message types (Follow_Up, Management, Signaling, etc.)
}

// ============================================================================
// P2P Delay Mechanism Support (IEEE 1588-2019 Section 11.4)
// ============================================================================

void GrandmasterController::send_pdelay_resp(
    uint64_t requesting_clock_id,
    uint16_t requesting_port_id,
    uint16_t sequence_id,
    const IEEE::_1588::PTP::_2019::Types::Timestamp& request_receipt_timestamp) {
    
    // Construct Pdelay_Resp message (IEEE 1588-2019 Section 13.9)
    uint8_t pdelay_resp[54];  // 34 header + 20 body
    memset(pdelay_resp, 0, sizeof(pdelay_resp));
    
    // PTP Header (bytes 0-33)
    pdelay_resp[0] = 0x03;  // messageType = Pdelay_Resp (3), transportSpecific = 0x0
    pdelay_resp[1] = 0x02;  // versionPTP = 2
    pdelay_resp[2] = 0x00;  // messageLength high byte
    pdelay_resp[3] = 0x36;  // messageLength low byte (54)
    pdelay_resp[4] = 0x00;  // domainNumber = 0
    pdelay_resp[5] = 0x00;  // reserved
    pdelay_resp[6] = 0x00;  // flagField high byte
    pdelay_resp[7] = 0x08;  // flagField low byte (timescale=PTP)
    // bytes 8-15: correctionField (all zeros for now)
    // bytes 16-19: reserved
    // bytes 20-27: sourcePortIdentity.clockIdentity (our clock ID - use MAC-based)
    // For now, use a simple clock ID - should get from network adapter
    pdelay_resp[20] = 0x00; pdelay_resp[21] = 0x80;
    pdelay_resp[22] = 0xC2; pdelay_resp[23] = 0xFF;
    pdelay_resp[24] = 0xFE; pdelay_resp[25] = 0x00;
    pdelay_resp[26] = 0x00; pdelay_resp[27] = 0x01;  // Placeholder clock ID
    pdelay_resp[28] = 0x00;  // sourcePortIdentity.portNumber high byte
    pdelay_resp[29] = 0x01;  // sourcePortIdentity.portNumber low byte (port 1)
    pdelay_resp[30] = (sequence_id >> 8) & 0xFF;  // sequenceId high byte
    pdelay_resp[31] = sequence_id & 0xFF;         // sequenceId low byte
    pdelay_resp[32] = 0x05;  // controlField = Other (5)
    pdelay_resp[33] = 0x7F;  // logMessageInterval = 0x7F (not periodic)
    
    // Pdelay_Resp body (bytes 34-53)
    // requestReceiptTimestamp (bytes 34-43) - timestamp when we received Pdelay_Req
    uint64_t receipt_sec = request_receipt_timestamp.seconds;
    pdelay_resp[34] = (receipt_sec >> 40) & 0xFF;  // seconds high 16 bits
    pdelay_resp[35] = (receipt_sec >> 32) & 0xFF;
    pdelay_resp[36] = (receipt_sec >> 24) & 0xFF;  // seconds low 32 bits
    pdelay_resp[37] = (receipt_sec >> 16) & 0xFF;
    pdelay_resp[38] = (receipt_sec >> 8) & 0xFF;
    pdelay_resp[39] = receipt_sec & 0xFF;
    pdelay_resp[40] = (request_receipt_timestamp.nanoseconds >> 24) & 0xFF;  // nanoseconds
    pdelay_resp[41] = (request_receipt_timestamp.nanoseconds >> 16) & 0xFF;
    pdelay_resp[42] = (request_receipt_timestamp.nanoseconds >> 8) & 0xFF;
    pdelay_resp[43] = request_receipt_timestamp.nanoseconds & 0xFF;
    
    // requestingPortIdentity (bytes 44-53) - copy from Pdelay_Req source
    pdelay_resp[44] = (requesting_clock_id >> 56) & 0xFF;
    pdelay_resp[45] = (requesting_clock_id >> 48) & 0xFF;
    pdelay_resp[46] = (requesting_clock_id >> 40) & 0xFF;
    pdelay_resp[47] = (requesting_clock_id >> 32) & 0xFF;
    pdelay_resp[48] = (requesting_clock_id >> 24) & 0xFF;
    pdelay_resp[49] = (requesting_clock_id >> 16) & 0xFF;
    pdelay_resp[50] = (requesting_clock_id >> 8) & 0xFF;
    pdelay_resp[51] = requesting_clock_id & 0xFF;
    pdelay_resp[52] = (requesting_port_id >> 8) & 0xFF;
    pdelay_resp[53] = requesting_port_id & 0xFF;
    
    // Send via event socket (port 319) - P2P uses event messages
    auto result = network_adapter_->send_packet(pdelay_resp, sizeof(pdelay_resp), true);  // event=true
    if (!result) {
        std::cout << \"[Controller] ❌ Failed to send Pdelay_Resp: \" << result.error().message << \"\\n\" << std::flush;
    } else {
        std::cout << \"[Controller] ✅ TX: Pdelay_Resp to clockID=\"
                  << std::hex << std::setfill('0') << std::setw(16) << requesting_clock_id << std::dec
                  << \" port=\" << requesting_port_id << \" seq=\" << sequence_id << \"\\n\" << std::flush;
    }
}

void GrandmasterController::send_pdelay_resp_follow_up(
    uint64_t requesting_clock_id,
    uint16_t requesting_port_id,
    uint16_t sequence_id,
    const IEEE::_1588::PTP::_2019::Types::Timestamp& response_origin_timestamp) {
    
    // Construct Pdelay_Resp_Follow_Up message (IEEE 1588-2019 Section 13.11)
    uint8_t pdelay_resp_fup[54];  // 34 header + 20 body
    memset(pdelay_resp_fup, 0, sizeof(pdelay_resp_fup));
    
    // PTP Header (bytes 0-33)
    pdelay_resp_fup[0] = 0x0A;  // messageType = Pdelay_Resp_Follow_Up (10), transportSpecific = 0x0
    pdelay_resp_fup[1] = 0x02;  // versionPTP = 2
    pdelay_resp_fup[2] = 0x00;  // messageLength high byte
    pdelay_resp_fup[3] = 0x36;  // messageLength low byte (54)
    pdelay_resp_fup[4] = 0x00;  // domainNumber = 0
    pdelay_resp_fup[5] = 0x00;  // reserved
    pdelay_resp_fup[6] = 0x00;  // flagField high byte
    pdelay_resp_fup[7] = 0x08;  // flagField low byte (timescale=PTP)
    // bytes 8-15: correctionField (all zeros for now)
    // bytes 16-19: reserved
    // bytes 20-27: sourcePortIdentity.clockIdentity
    pdelay_resp_fup[20] = 0x00; pdelay_resp_fup[21] = 0x80;
    pdelay_resp_fup[22] = 0xC2; pdelay_resp_fup[23] = 0xFF;
    pdelay_resp_fup[24] = 0xFE; pdelay_resp_fup[25] = 0x00;
    pdelay_resp_fup[26] = 0x00; pdelay_resp_fup[27] = 0x01;
    pdelay_resp_fup[28] = 0x00;  // sourcePortIdentity.portNumber high byte
    pdelay_resp_fup[29] = 0x01;  // sourcePortIdentity.portNumber low byte
    pdelay_resp_fup[30] = (sequence_id >> 8) & 0xFF;  // sequenceId
    pdelay_resp_fup[31] = sequence_id & 0xFF;
    pdelay_resp_fup[32] = 0x05;  // controlField = Other (5)
    pdelay_resp_fup[33] = 0x7F;  // logMessageInterval = 0x7F
    
    // Pdelay_Resp_Follow_Up body (bytes 34-53)
    // responseOriginTimestamp (bytes 34-43) - TX timestamp of Pdelay_Resp
    uint64_t origin_sec = response_origin_timestamp.seconds;
    pdelay_resp_fup[34] = (origin_sec >> 40) & 0xFF;
    pdelay_resp_fup[35] = (origin_sec >> 32) & 0xFF;
    pdelay_resp_fup[36] = (origin_sec >> 24) & 0xFF;
    pdelay_resp_fup[37] = (origin_sec >> 16) & 0xFF;
    pdelay_resp_fup[38] = (origin_sec >> 8) & 0xFF;
    pdelay_resp_fup[39] = origin_sec & 0xFF;
    pdelay_resp_fup[40] = (response_origin_timestamp.nanoseconds >> 24) & 0xFF;
    pdelay_resp_fup[41] = (response_origin_timestamp.nanoseconds >> 16) & 0xFF;
    pdelay_resp_fup[42] = (response_origin_timestamp.nanoseconds >> 8) & 0xFF;
    pdelay_resp_fup[43] = response_origin_timestamp.nanoseconds & 0xFF;
    
    // requestingPortIdentity (bytes 44-53)
    pdelay_resp_fup[44] = (requesting_clock_id >> 56) & 0xFF;
    pdelay_resp_fup[45] = (requesting_clock_id >> 48) & 0xFF;
    pdelay_resp_fup[46] = (requesting_clock_id >> 40) & 0xFF;
    pdelay_resp_fup[47] = (requesting_clock_id >> 32) & 0xFF;
    pdelay_resp_fup[48] = (requesting_clock_id >> 24) & 0xFF;
    pdelay_resp_fup[49] = (requesting_clock_id >> 16) & 0xFF;
    pdelay_resp_fup[50] = (requesting_clock_id >> 8) & 0xFF;
    pdelay_resp_fup[51] = requesting_clock_id & 0xFF;
    pdelay_resp_fup[52] = (requesting_port_id >> 8) & 0xFF;
    pdelay_resp_fup[53] = requesting_port_id & 0xFF;
    
    // Send via general socket (port 320)
    auto result = network_adapter_->send_packet(pdelay_resp_fup, sizeof(pdelay_resp_fup), false);  // event=false
    if (!result) {
        std::cout << \"[Controller] ❌ Failed to send Pdelay_Resp_Follow_Up\\n\" << std::flush;
    } else {
        std::cout << \"[Controller] ✅ TX: Pdelay_Resp_Follow_Up seq=\" << sequence_id << \"\\n\" << std::flush;
    }
}

// ============================================================================
// E2E Delay Mechanism Support (IEEE 1588-2019 Section 11.3)
// ============================================================================

// Send Delay_Resp message (IEEE 1588-2019 Section 13.8)
IEEE::_1588::PTP::_2019::Types::PTPResult<void> GrandmasterController::send_delay_resp(
    const IEEE::_1588::PTP::_2019::DelayRespBody& message,
    const IEEE::_1588::PTP::_2019::Types::PortIdentity& requesting_port)
{
    using namespace IEEE::_1588::PTP::_2019;
    
    if (!network_) {
        return Types::PTPResult<void>::failure(Types::PTPError::Invalid_Parameter);
    }
    
    // Construct Delay_Resp packet (IEEE 1588-2019 Section 13.8)
    uint8_t resp_packet[64];
    std::memset(resp_packet, 0, sizeof(resp_packet));
    
    // PTP Header (bytes 0-33)
    resp_packet[0] = 0x09;  // messageType: Delay_Resp
    resp_packet[1] = 0x02;  // versionPTP: 2
    resp_packet[2] = 0x00;  // messageLength (high byte)
    resp_packet[3] = 54;    // messageLength (low byte) = 54 bytes
    
    // Bytes 4-7: domain, flags, correction field placeholder
    resp_packet[4] = 0;     // domainNumber: 0 (default)
    
    // Bytes 34-43: receiveTimestamp (from Delay_Req RX timestamp)
    // IEEE 1588-2019 Timestamp: 6 bytes seconds (48-bit) + 4 bytes nanoseconds
    uint64_t sec = ((uint64_t)message.receiveTimestamp.seconds_high << 32) | 
                    message.receiveTimestamp.seconds_low;
    uint32_t nsec = message.receiveTimestamp.nanoseconds;
    
    // Seconds (6 bytes, big-endian) at offset 34
    resp_packet[34] = (sec >> 40) & 0xFF;
    resp_packet[35] = (sec >> 32) & 0xFF;
    resp_packet[36] = (sec >> 24) & 0xFF;
    resp_packet[37] = (sec >> 16) & 0xFF;
    resp_packet[38] = (sec >> 8) & 0xFF;
    resp_packet[39] = sec & 0xFF;
    
    // Nanoseconds (4 bytes, big-endian) at offset 40
    resp_packet[40] = (nsec >> 24) & 0xFF;
    resp_packet[41] = (nsec >> 16) & 0xFF;
    resp_packet[42] = (nsec >> 8) & 0xFF;
    resp_packet[43] = nsec & 0xFF;
    
    // Bytes 44-53: requestingPortIdentity (10 bytes)
    // ClockIdentity (8 bytes)
    std::memcpy(&resp_packet[44], requesting_port.clock_identity.data(), 8);
    
    // PortNumber (2 bytes, big-endian)
    uint16_t port_num = requesting_port.port_number;
    resp_packet[52] = (port_num >> 8) & 0xFF;
    resp_packet[53] = port_num & 0xFF;
    
    // Send packet
    NetworkTimestamp tx_ts;
    int sent = network_->send_packet(resp_packet, 54, &tx_ts, false);  // Use general socket (port 320)
    
    if (sent > 0) {
        std::cout << "[Controller] 📤 TX: Delay_Resp message (" << sent << " bytes, "
                  << "RX_TS=" << sec << "." << std::setfill('0') << std::setw(9) << nsec << ")\n" << std::flush;
        return Types::PTPResult<void>::success();
    }
    
    return Types::PTPResult<void>::failure(Types::PTPError::Network_Error);
}

// Parse Delay_Req message (IEEE 1588-2019 Section 13.6)
bool GrandmasterController::parse_delay_req(const uint8_t* packet, size_t length,
                                           IEEE::_1588::PTP::_2019::DelayReqBody* delay_req,
                                           IEEE::_1588::PTP::_2019::Types::PortIdentity* source_port)
{
    using namespace IEEE::_1588::PTP::_2019;
    
    // Minimum Delay_Req length is 44 bytes (header + origin timestamp)
    if (!packet || length < 44 || !delay_req || !source_port) {
        return false;
    }
    
    // Verify messageType = 0x01 (Delay_Req)
    if ((packet[0] & 0x0F) != 0x01) {
        return false;
    }
    
    // Extract originTimestamp (bytes 34-43, typically zero for Delay_Req)
    uint64_t sec = ((uint64_t)packet[34] << 40) |
                   ((uint64_t)packet[35] << 32) |
                   ((uint64_t)packet[36] << 24) |
                   ((uint64_t)packet[37] << 16) |
                   ((uint64_t)packet[38] << 8) |
                   ((uint64_t)packet[39]);
    
    uint32_t nsec = ((uint32_t)packet[40] << 24) |
                    ((uint32_t)packet[41] << 16) |
                    ((uint32_t)packet[42] << 8) |
                    ((uint32_t)packet[43]);
    
    delay_req->originTimestamp.seconds_high = (sec >> 32) & 0xFFFF;
    delay_req->originTimestamp.seconds_low = sec & 0xFFFFFFFF;
    delay_req->originTimestamp.nanoseconds = nsec;
    
    // Extract sourcePortIdentity from PTP header (bytes 20-29)
    // ClockIdentity (8 bytes)
    std::memcpy(source_port->clock_identity.data(), &packet[20], 8);
    
    // PortNumber (2 bytes, big-endian)
    source_port->port_number = ((uint16_t)packet[28] << 8) | packet[29];
    
    return true;
}
