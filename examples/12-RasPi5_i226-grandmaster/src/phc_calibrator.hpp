/**
 * @file phc_calibrator.hpp
 * @brief PHC frequency calibration against GPS PPS reference
 * 
 * Implements IEEE 1588-2019 compliant frequency calibration using
 * GPS PPS pulses as reference. Measures PHC drift over multiple
 * PPS intervals (20 pulses = 20 seconds) and applies corrections.
 * 
 * CRITICAL: All drift calculations use nanosecond precision integers
 * until final ratio to avoid floating-point accumulation errors.
 * 
 * @see IEEE 1588-2019, Section 11.3 "Delay request-response mechanism"
 * @see deb.md expert recommendations on PPS-based calibration
 */

#ifndef PHC_CALIBRATOR_HPP
#define PHC_CALIBRATOR_HPP

#include <cstdint>
#include <mutex>

// Forward declarations for hardware adapters
// PhcAdapter is in global namespace (extracted early)
class PhcAdapter;
// GpsAdapter is in IEEE::_1588::PTP::_2019::Linux namespace
namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {
    class GpsAdapter;
} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE

/**
 * @brief PHC calibration configuration
 */
struct PhcCalibratorConfig {
    uint32_t interval_pulses;      ///< Number of PPS pulses for measurement (default: 20)
    int32_t max_correction_ppb;    ///< Maximum correction per iteration (default: 500000)
    double drift_threshold_ppm;    ///< Drift threshold for completion (default: 100.0)
    double sanity_threshold_ppm;   ///< Reject measurements > this value (default: 2000.0)
    uint32_t max_iterations;       ///< Maximum calibration iterations (default: 5)
};

/**
 * @brief PHC calibration state
 */
struct PhcCalibrationState {
    bool calibrated;               ///< Calibration complete flag
    int32_t cumulative_freq_ppb;   ///< Total frequency correction applied
    uint32_t iterations;           ///< Calibration iterations performed
    double last_drift_ppm;         ///< Last measured drift (parts per million)
    int64_t last_phc_delta_ns;     ///< Last PHC interval measurement
    int64_t last_ref_delta_ns;     ///< Last reference interval
};

/**
 * @brief PHC Frequency Calibrator
 * 
 * Measures PHC clock drift against GPS PPS reference and applies
 * frequency corrections to eliminate drift. Uses 20-pulse intervals
 * for accurate measurement with low noise.
 * 
 * Algorithm:
 * 1. Capture PHC timestamp at first PPS edge (baseline)
 * 2. Wait N PPS pulses (default: 20 = 20 seconds)
 * 3. Capture PHC timestamp at final PPS edge
 * 4. Calculate drift: ((PHC_delta - REF_delta) / REF_delta) × 10^6 ppm
 * 5. Apply correction: freq_adjustment = -drift_ppm × 1000 ppb
 * 6. Repeat until drift < threshold (100 ppm) or max iterations (5)
 * 
 * CRITICAL DESIGN NOTES:
 * - Uses PPS pulse count (not GPS time-of-day) to avoid NMEA latency
 * - Samples PHC IMMEDIATELY after PPS edge to minimize latency
 * - Applies cumulative corrections (hardware doesn't support read-back)
 * - Rejects measurements > 2000 ppm as invalid (likely sampling errors)
 * - Thread-safe for multi-threaded use
 */
class PhcCalibrator {
public:
    /**
     * @brief Construct calibrator with configuration
     * @param config Calibration parameters
     */
    explicit PhcCalibrator(const PhcCalibratorConfig& config);
    
    /**
     * @brief Initialize calibrator with hardware adapters
     * @param phc PHC hardware adapter
     * @param gps GPS hardware adapter
     * @return 0 on success, negative on error
     */
    int initialize(PhcAdapter* phc, IEEE::_1588::PTP::_2019::Linux::GpsAdapter* gps);
    
    /**
     * @brief Start calibration sequence
     * 
     * Captures baseline PHC timestamp at current PPS.
     * Call on first valid PPS edge.
     * 
     * @param pps_sequence Current PPS sequence number
     * @param phc_timestamp_ns PHC time at PPS edge (nanoseconds)
     * @return 0 on success, negative on error
     */
    int start_calibration(uint32_t pps_sequence, int64_t phc_timestamp_ns);
    
    /**
     * @brief Update calibration with new PPS sample
     * 
     * Call on each PPS edge during calibration. Automatically
     * completes calibration when enough pulses accumulated or
     * drift threshold met.
     * 
     * @param pps_sequence Current PPS sequence number
     * @param phc_timestamp_ns PHC time at PPS edge (nanoseconds)
     * @return 0 if calibration continuing, 1 if complete, negative on error
     */
    int update_calibration(uint32_t pps_sequence, int64_t phc_timestamp_ns);
    
    /**
     * @brief Check if calibration is complete
     * @return true if calibrated, false otherwise
     */
    bool is_calibrated() const;
    
    /**
     * @brief Get calibration state
     * @param state Output state structure
     */
    void get_state(PhcCalibrationState* state) const;
    
    /**
     * @brief Reset calibration (for recalibration)
     */
    void reset();
    
    /**
     * @brief Get cumulative frequency correction
     * @return Total frequency correction applied (ppb)
     */
    int32_t get_cumulative_frequency() const;

private:
    PhcCalibratorConfig config_;
    
    // Hardware adapters (not owned)
    PhcAdapter* phc_;
    IEEE::_1588::PTP::_2019::Linux::GpsAdapter* gps_;
    
    // Calibration state
    bool calibrated_;
    uint32_t baseline_pps_seq_;
    int64_t baseline_phc_ns_;
    int32_t cumulative_freq_ppb_;      ///< Clamped frequency applied to hardware
    int32_t measured_drift_ppb_;       ///< Actual measured drift (not clamped)
    uint32_t iterations_;
    uint32_t correlation_failures_;
    
    // Last measurement results
    double last_drift_ppm_;
    int64_t last_phc_delta_ns_;
    int64_t last_ref_delta_ns_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    /**
     * @brief Calculate drift from interval measurements
     * @param phc_delta_ns PHC elapsed time (nanoseconds)
     * @param ref_delta_ns Reference elapsed time (nanoseconds)
     * @return Drift in parts per million (ppm)
     */
    double calculate_drift_ppm(int64_t phc_delta_ns, int64_t ref_delta_ns) const;
    
    /**
     * @brief Apply frequency correction to PHC
     * @param correction_ppb Correction to apply (parts per billion)
     * @return 0 on success, negative on error
     */
    int apply_frequency_correction(int32_t correction_ppb);
};

#endif // PHC_CALIBRATOR_HPP
