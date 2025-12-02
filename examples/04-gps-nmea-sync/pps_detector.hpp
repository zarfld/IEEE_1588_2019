/**
 * @file pps_detector.hpp
 * @brief GPS PPS (Pulse Per Second) Autodetection
 * 
 * Automatically detects GPS PPS signal on RS-232 modem control pins (DCD/CTS/DSR)
 * and provides sub-microsecond timestamping for IEEE 1588-2019 PTP synchronization.
 * 
 * Features:
 * - Autodetect PPS on DCD (Pin 1), CTS (Pin 8), or DSR (Pin 6)
 * - 1Hz frequency validation with ±200ms jitter tolerance
 * - Nanosecond-precision edge timestamping
 * - Graceful fallback to NMEA-only if no PPS detected
 * - Platform-agnostic (Windows/Linux/Embedded)
 * - Thread-safe operation
 * 
 * Accuracy Enhancement:
 * - NMEA-only: 10ms resolution (centiseconds)
 * - PPS + NMEA: Sub-microsecond resolution (50-200ns typical)
 * 
 * @see 04-design/components/gps-pps-autodetect.md for design specification
 * @see docs/Serial_NMEA_example/PPS_integration.md for integration guide
 * 
 * IEEE 1588-2019 References:
 * - Section 7.3: Time representation with nanosecond precision
 * - Section 7.4.1: Timestamp generation requirements
 * - Annex C: UDP/IP implementation requirements
 */

#ifndef GPS_PPS_DETECTOR_HPP
#define GPS_PPS_DETECTOR_HPP

#include <cstdint>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <array>

namespace GPS {
namespace PPS {

/**
 * @brief PPS signal line identification
 * 
 * GPS modules typically output PPS on one of the RS-232 modem control lines.
 * Most common is DCD (Pin 1), but some modules use CTS or DSR.
 */
enum class PPSLine {
    None,    ///< No PPS detected
    DCD,     ///< DB9 Pin 1 - Data Carrier Detect (u-blox default)
    CTS,     ///< DB9 Pin 8 - Clear To Send
    DSR      ///< DB9 Pin 6 - Data Set Ready
};

/**
 * @brief Convert PPSLine to string for logging
 */
inline const char* to_string(PPSLine line) {
    switch (line) {
        case PPSLine::None: return "None";
        case PPSLine::DCD:  return "DCD (Pin 1)";
        case PPSLine::CTS:  return "CTS (Pin 8)";
        case PPSLine::DSR:  return "DSR (Pin 6)";
        default:            return "Unknown";
    }
}

/**
 * @brief Autodetection state
 */
enum class DetectionState {
    Idle,        ///< Not started
    Detecting,   ///< Monitoring all pins for PPS
    Locked,      ///< Valid PPS detected on specific pin
    Failed       ///< No valid PPS found, using NMEA-only
};

/**
 * @brief Convert DetectionState to string
 */
inline const char* to_string(DetectionState state) {
    switch (state) {
        case DetectionState::Idle:      return "Idle";
        case DetectionState::Detecting: return "Detecting";
        case DetectionState::Locked:    return "Locked";
        case DetectionState::Failed:    return "Failed";
        default:                        return "Unknown";
    }
}

/**
 * @brief High-resolution timestamp with nanosecond precision
 * 
 * Timestamp format matches IEEE 1588-2019 PTP timestamp structure.
 */
struct PPSTimestamp {
    uint64_t seconds;       ///< Seconds since epoch (monotonic time)
    uint32_t nanoseconds;   ///< Nanoseconds within second (0-999999999)
    PPSLine source;         ///< Which pin generated this timestamp
    
    /**
     * @brief Default constructor - zero timestamp
     */
    PPSTimestamp() 
        : seconds(0), nanoseconds(0), source(PPSLine::None) {}
    
    /**
     * @brief Constructor with values
     */
    PPSTimestamp(uint64_t sec, uint32_t nsec, PPSLine src = PPSLine::None)
        : seconds(sec), nanoseconds(nsec), source(src) {}
    
    /**
     * @brief Convert to nanoseconds since epoch
     * @return Total nanoseconds
     */
    int64_t to_nanoseconds() const {
        return static_cast<int64_t>(seconds) * 1000000000LL + 
               static_cast<int64_t>(nanoseconds);
    }
    
    /**
     * @brief Create from nanoseconds
     */
    static PPSTimestamp from_nanoseconds(int64_t ns, PPSLine src = PPSLine::None) {
        PPSTimestamp ts;
        ts.seconds = static_cast<uint64_t>(ns / 1000000000LL);
        ts.nanoseconds = static_cast<uint32_t>(ns % 1000000000LL);
        ts.source = src;
        return ts;
    }
    
    /**
     * @brief Calculate time difference in seconds
     */
    double operator-(const PPSTimestamp& other) const {
        int64_t diff_ns = to_nanoseconds() - other.to_nanoseconds();
        return diff_ns / 1e9;
    }
};

/**
 * @brief Edge detection candidate
 * 
 * Tracks edge events for one pin during autodetection phase.
 */
struct EdgeCandidate {
    PPSLine line;                ///< Which pin this candidate monitors
    PPSTimestamp first_edge;     ///< Timestamp of first detected edge
    PPSTimestamp last_edge;      ///< Timestamp of most recent edge
    uint32_t edge_count;         ///< Total edges detected
    uint32_t valid_count;        ///< Count of valid 1Hz intervals
    bool validated;              ///< True if confirmed as 1Hz PPS
    
    /**
     * @brief Constructor
     */
    explicit EdgeCandidate(PPSLine l) 
        : line(l)
        , edge_count(0)
        , valid_count(0)
        , validated(false) {}
    
    /**
     * @brief Reset to initial state
     */
    void reset() {
        edge_count = 0;
        valid_count = 0;
        validated = false;
    }
};

/**
 * @brief PPS detection statistics
 */
struct PPSStatistics {
    uint64_t total_edges;         ///< Total edges detected across all pins
    uint64_t valid_intervals;     ///< Valid 1Hz intervals
    uint64_t invalid_intervals;   ///< Invalid intervals (noise/wrong freq)
    double avg_interval_sec;      ///< Average interval between edges
    double min_interval_sec;      ///< Minimum observed interval
    double max_interval_sec;      ///< Maximum observed interval
    double jitter_ns;             ///< Measured jitter in nanoseconds
    
    /**
     * @brief Constructor - initialize to zero
     */
    PPSStatistics()
        : total_edges(0)
        , valid_intervals(0)
        , invalid_intervals(0)
        , avg_interval_sec(0.0)
        , min_interval_sec(999.0)
        , max_interval_sec(0.0)
        , jitter_ns(0.0) {}
};

/**
 * @brief PPS Detector - Hardware-agnostic GPS PPS autodetection
 * 
 * Monitors RS-232 modem control pins for 1Hz PPS signal and provides
 * high-precision timestamps for PTP synchronization.
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Internal state protected by mutex
 * - Detection runs in background thread
 * 
 * Usage Pattern:
 * @code
 * // 1. Create detector with serial port handle
 * #ifdef _WIN32
 *     HANDLE serial = ...; // Windows handle
 * #else
 *     int serial = ...;    // Linux file descriptor
 * #endif
 * 
 * PPSDetector detector(reinterpret_cast<void*>(serial));
 * 
 * // 2. Start autodetection (10 second timeout)
 * if (detector.start_detection(10000)) {
 *     std::cout << "PPS detection started..." << std::endl;
 * }
 * 
 * // 3. Check if PPS was detected
 * std::this_thread::sleep_for(std::chrono::seconds(11));
 * 
 * if (detector.is_pps_available()) {
 *     std::cout << "PPS detected on " 
 *               << to_string(detector.get_detected_line()) 
 *               << std::endl;
 *     
 *     // 4. Get PPS timestamps
 *     PPSTimestamp ts;
 *     if (detector.get_pps_timestamp(2000, ts)) {
 *         std::cout << "PPS: " << ts.seconds << "." 
 *                   << ts.nanoseconds << " ns" << std::endl;
 *     }
 * } else {
 *     std::cout << "No PPS detected, using NMEA-only" << std::endl;
 * }
 * @endcode
 */
class PPSDetector {
public:
    /**
     * @brief Constructor
     * @param serial_handle Platform-specific serial port handle
     *                      Windows: HANDLE from CreateFile()
     *                      Linux: int file descriptor from open()
     */
    explicit PPSDetector(void* serial_handle);
    
    /**
     * @brief Destructor - stops monitoring and cleans up
     */
    ~PPSDetector();
    
    // Disable copy/move (manages thread and system resources)
    PPSDetector(const PPSDetector&) = delete;
    PPSDetector& operator=(const PPSDetector&) = delete;
    PPSDetector(PPSDetector&&) = delete;
    PPSDetector& operator=(PPSDetector&&) = delete;
    
    /**
     * @brief Start PPS autodetection
     * 
     * Launches background thread that monitors all three pins (DCD/CTS/DSR)
     * for 1Hz PPS signal. First pin to show valid 1Hz will be locked.
     * 
     * @param timeout_ms Detection timeout in milliseconds (default 10000)
     * @return true if detection started successfully, false on error
     * 
     * @note Non-blocking - returns immediately
     * @note Call get_state() or is_pps_available() to check result
     */
    bool start_detection(uint32_t timeout_ms = 10000);
    
    /**
     * @brief Stop PPS monitoring
     * 
     * Stops detection thread and releases resources.
     * Safe to call multiple times.
     */
    void stop_detection();
    
    /**
     * @brief Get current detection state
     * @return Current state (Idle/Detecting/Locked/Failed)
     */
    DetectionState get_state() const;
    
    /**
     * @brief Get detected PPS line
     * @return PPSLine::DCD/CTS/DSR if locked, PPSLine::None otherwise
     */
    PPSLine get_detected_line() const;
    
    /**
     * @brief Get latest PPS timestamp (blocking)
     * 
     * Waits for next PPS edge and returns its timestamp.
     * Only works if state is Locked.
     * 
     * @param timeout_ms Maximum wait time in milliseconds
     * @param[out] timestamp Captured PPS timestamp
     * @return true if timestamp captured within timeout, false on timeout/error
     * 
     * @note Blocks until edge or timeout
     * @note Returns false if not in Locked state
     */
    bool get_pps_timestamp(uint32_t timeout_ms, PPSTimestamp& timestamp);
    
    /**
     * @brief Check if PPS is available
     * @return true if in Locked state (PPS detected and operational)
     */
    bool is_pps_available() const {
        return get_state() == DetectionState::Locked;
    }
    
    /**
     * @brief Get detection statistics
     * @return Copy of current statistics
     */
    PPSStatistics get_statistics() const;
    
    /**
     * @brief Reset statistics counters
     */
    void reset_statistics();

private:
    // Platform-specific implementation (PIMPL pattern)
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // State management
    mutable std::mutex state_mutex_;
    std::atomic<DetectionState> state_;
    std::atomic<PPSLine> detected_line_;
    
    // Detection thread
    std::unique_ptr<std::thread> detection_thread_;
    std::atomic<bool> stop_requested_;
    
    // Latest timestamp and synchronization
    mutable std::mutex timestamp_mutex_;
    std::condition_variable timestamp_cv_;
    PPSTimestamp latest_timestamp_;
    bool timestamp_available_;
    
    // Edge candidates (DCD, CTS, DSR)
    std::array<EdgeCandidate, 3> candidates_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    PPSStatistics statistics_;
    
    // Configuration
    uint32_t timeout_ms_;
    
    // Detection constants
    static constexpr double MIN_INTERVAL_SEC = 0.8;   ///< Min 1Hz interval (±200ms)
    static constexpr double MAX_INTERVAL_SEC = 1.2;   ///< Max 1Hz interval (±200ms)
    static constexpr uint32_t MIN_EDGES_FOR_LOCK = 3; ///< Require 3 edges (2 intervals)
    
    /**
     * @brief Main detection thread function
     */
    void detection_thread_func();
    
    /**
     * @brief Process edge event
     * @param line Which pin triggered
     * @param timestamp Edge timestamp
     */
    void process_edge(PPSLine line, const PPSTimestamp& timestamp);
    
    /**
     * @brief Validate interval between edges
     * @param candidate Edge candidate to validate
     * @param new_edge New edge timestamp
     * @return true if interval is valid 1Hz (0.8-1.2s)
     */
    bool validate_interval(const EdgeCandidate& candidate, 
                          const PPSTimestamp& new_edge);
    
    /**
     * @brief Check if candidate has confirmed PPS lock
     * @param candidate Edge candidate to check
     * @return true if candidate has 3+ edges with valid intervals
     */
    bool confirm_pps_lock(const EdgeCandidate& candidate) const;
    
    /**
     * @brief Update statistics with new interval
     */
    void update_statistics(double interval_sec, bool valid);
    
    /**
     * @brief Log detection event
     */
    void log_detection_event(const char* message);
};

} // namespace PPS
} // namespace GPS

#endif // GPS_PPS_DETECTOR_HPP
