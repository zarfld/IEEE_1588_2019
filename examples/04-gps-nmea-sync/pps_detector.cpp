/**
 * @file pps_detector.cpp
 * @brief GPS PPS Detector Implementation
 * 
 * Platform-specific implementation for Windows and Linux PPS detection.
 */

#include "pps_detector.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <time.h>
#endif

namespace GPS {
namespace PPS {

/**
 * @brief Platform-specific implementation details (PIMPL)
 */
struct PPSDetector::Impl {
#ifdef _WIN32
    HANDLE serial_handle;
    HANDLE event_handle;
    OVERLAPPED overlapped;
#else
    int serial_fd;
#endif
    
    /**
     * @brief Capture high-resolution timestamp
     */
    static PPSTimestamp capture_timestamp(PPSLine source = PPSLine::None) {
        PPSTimestamp ts;
        ts.source = source;
        
#ifdef _WIN32
        // Windows: QueryPerformanceCounter
        LARGE_INTEGER qpc, freq;
        QueryPerformanceCounter(&qpc);
        QueryPerformanceFrequency(&freq);
        
        // Convert to nanoseconds
        uint64_t ns = (qpc.QuadPart * 1000000000ULL) / freq.QuadPart;
        ts.seconds = ns / 1000000000ULL;
        ts.nanoseconds = static_cast<uint32_t>(ns % 1000000000ULL);
#else
        // Linux: clock_gettime with CLOCK_MONOTONIC_RAW
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC_RAW, &t);
        ts.seconds = static_cast<uint64_t>(t.tv_sec);
        ts.nanoseconds = static_cast<uint32_t>(t.tv_nsec);
#endif
        
        return ts;
    }
    
    /**
     * @brief Enable PPS monitoring on all pins
     */
    bool enable_monitoring() {
#ifdef _WIN32
        // Set up event mask for DCD, CTS, DSR
        DWORD mask = EV_RLSD |  // DCD (Pin 1)
                     EV_CTS |   // CTS (Pin 8)
                     EV_DSR;    // DSR (Pin 6)
        
        if (!SetCommMask(serial_handle, mask)) {
            std::cerr << "PPS: Failed to set comm mask: " 
                      << GetLastError() << std::endl;
            return false;
        }
        
        // Create event for overlapped I/O
        event_handle = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (event_handle == nullptr) {
            std::cerr << "PPS: Failed to create event: " 
                      << GetLastError() << std::endl;
            return false;
        }
        
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = event_handle;
        
        return true;
#else
        // Linux: No setup needed, will use ioctl(TIOCMIWAIT)
        return serial_fd >= 0;
#endif
    }
    
    /**
     * @brief Wait for edge event on any monitored pin
     * @param timeout_ms Timeout in milliseconds
     * @param[out] line Which pin triggered (if any)
     * @param[out] timestamp Edge timestamp
     * @return true if edge detected, false on timeout
     */
    bool wait_for_edge(uint32_t timeout_ms, PPSLine& line, PPSTimestamp& timestamp) {
#ifdef _WIN32
        DWORD events = 0;
        
        // Start async wait
        if (!WaitCommEvent(serial_handle, &events, &overlapped)) {
            if (GetLastError() != ERROR_IO_PENDING) {
                std::cerr << "PPS: WaitCommEvent failed: " 
                          << GetLastError() << std::endl;
                return false;
            }
            
            // Wait for completion with timeout
            DWORD result = WaitForSingleObject(event_handle, timeout_ms);
            
            if (result == WAIT_TIMEOUT) {
                // Cancel pending operation
                CancelIo(serial_handle);
                return false;
            }
            
            if (result != WAIT_OBJECT_0) {
                std::cerr << "PPS: Wait failed: " << GetLastError() << std::endl;
                return false;
            }
            
            // Get completion result
            DWORD bytes;
            if (!GetOverlappedResult(serial_handle, &overlapped, &bytes, FALSE)) {
                std::cerr << "PPS: GetOverlappedResult failed: " 
                          << GetLastError() << std::endl;
                return false;
            }
        }
        
        // Capture timestamp immediately
        timestamp = capture_timestamp();
        
        // Determine which pin triggered
        // Priority: DCD > CTS > DSR (most common first)
        if (events & EV_RLSD) {
            line = PPSLine::DCD;
            timestamp.source = PPSLine::DCD;
        } else if (events & EV_CTS) {
            line = PPSLine::CTS;
            timestamp.source = PPSLine::CTS;
        } else if (events & EV_DSR) {
            line = PPSLine::DSR;
            timestamp.source = PPSLine::DSR;
        } else {
            // Unknown event
            return false;
        }
        
        // Reset event for next wait
        ResetEvent(event_handle);
        
        return true;
#else
        // Linux: Wait for modem status change
        int modem_bits = TIOCM_CAR |  // DCD (Pin 1)
                         TIOCM_CTS |  // CTS (Pin 8)
                         TIOCM_DSR;   // DSR (Pin 6)
        
        // Set up timeout using select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serial_fd, &readfds);
        
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        // Note: ioctl(TIOCMIWAIT) doesn't support timeout directly,
        // so we use a polling approach with short sleeps
        auto deadline = std::chrono::steady_clock::now() + 
                       std::chrono::milliseconds(timeout_ms);
        
        int prev_status = 0;
        ioctl(serial_fd, TIOCMGET, &prev_status);
        
        while (std::chrono::steady_clock::now() < deadline) {
            // Short sleep to avoid busy-wait
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            int status = 0;
            if (ioctl(serial_fd, TIOCMGET, &status) < 0) {
                std::cerr << "PPS: ioctl(TIOCMGET) failed" << std::endl;
                return false;
            }
            
            // Check for rising edges
            int changes = (status ^ prev_status) & status;
            
            if (changes) {
                // Capture timestamp immediately
                timestamp = capture_timestamp();
                
                // Determine which pin changed (priority: DCD > CTS > DSR)
                if (changes & TIOCM_CAR) {
                    line = PPSLine::DCD;
                    timestamp.source = PPSLine::DCD;
                } else if (changes & TIOCM_CTS) {
                    line = PPSLine::CTS;
                    timestamp.source = PPSLine::CTS;
                } else if (changes & TIOCM_DSR) {
                    line = PPSLine::DSR;
                    timestamp.source = PPSLine::DSR;
                } else {
                    prev_status = status;
                    continue;
                }
                
                return true;
            }
            
            prev_status = status;
        }
        
        // Timeout
        return false;
#endif
    }
    
    /**
     * @brief Cleanup
     */
    void cleanup() {
#ifdef _WIN32
        if (event_handle != nullptr) {
            CloseHandle(event_handle);
            event_handle = nullptr;
        }
#endif
    }
};

// Constructor
PPSDetector::PPSDetector(void* serial_handle)
    : pimpl_(std::make_unique<Impl>())
    , state_(DetectionState::Idle)
    , detected_line_(PPSLine::None)
    , stop_requested_(false)
    , timestamp_available_(false)
    , candidates_{{EdgeCandidate(PPSLine::DCD),
                   EdgeCandidate(PPSLine::CTS),
                   EdgeCandidate(PPSLine::DSR)}}
    , timeout_ms_(10000)
{
#ifdef _WIN32
    pimpl_->serial_handle = static_cast<HANDLE>(serial_handle);
    pimpl_->event_handle = nullptr;
#else
    pimpl_->serial_fd = reinterpret_cast<intptr_t>(serial_handle);
#endif
}

// Destructor
PPSDetector::~PPSDetector() {
    stop_detection();
    pimpl_->cleanup();
}

// Start detection
bool PPSDetector::start_detection(uint32_t timeout_ms) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (state_ != DetectionState::Idle && state_ != DetectionState::Failed) {
        std::cerr << "PPS: Detection already running" << std::endl;
        return false;
    }
    
    timeout_ms_ = timeout_ms;
    
    // Enable monitoring
    if (!pimpl_->enable_monitoring()) {
        std::cerr << "PPS: Failed to enable monitoring" << std::endl;
        return false;
    }
    
    // Reset state
    stop_requested_ = false;
    state_ = DetectionState::Detecting;
    detected_line_ = PPSLine::None;
    timestamp_available_ = false;
    
    // Reset candidates
    for (auto& candidate : candidates_) {
        candidate.reset();
    }
    
    // Start detection thread
    detection_thread_ = std::make_unique<std::thread>(
        &PPSDetector::detection_thread_func, this);
    
    log_detection_event("PPS autodetection started, monitoring DCD/CTS/DSR");
    
    return true;
}

// Stop detection
void PPSDetector::stop_detection() {
    stop_requested_ = true;
    
    // Wake up waiting threads
    timestamp_cv_.notify_all();
    
    // Wait for thread to finish
    if (detection_thread_ && detection_thread_->joinable()) {
        detection_thread_->join();
    }
    
    detection_thread_.reset();
}

// Get state
DetectionState PPSDetector::get_state() const {
    return state_.load();
}

// Get detected line
PPSLine PPSDetector::get_detected_line() const {
    return detected_line_.load();
}

// Get PPS timestamp (blocking)
bool PPSDetector::get_pps_timestamp(uint32_t timeout_ms, PPSTimestamp& timestamp) {
    if (!is_pps_available()) {
        return false;
    }
    
    std::unique_lock<std::mutex> lock(timestamp_mutex_);
    
    // Wait for new timestamp
    bool got_timestamp = timestamp_cv_.wait_for(
        lock,
        std::chrono::milliseconds(timeout_ms),
        [this] { return timestamp_available_ || stop_requested_; }
    );
    
    if (got_timestamp && !stop_requested_) {
        timestamp = latest_timestamp_;
        timestamp_available_ = false;  // Consume timestamp
        return true;
    }
    
    return false;
}

// Get statistics
PPSStatistics PPSDetector::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

// Reset statistics
void PPSDetector::reset_statistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = PPSStatistics();
}

// Detection thread
void PPSDetector::detection_thread_func() {
    auto deadline = std::chrono::steady_clock::now() + 
                   std::chrono::milliseconds(timeout_ms_);
    
    while (!stop_requested_ && state_ == DetectionState::Detecting) {
        // Calculate remaining timeout
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            // Timeout - no PPS detected
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_ = DetectionState::Failed;
            log_detection_event("PPS detection timeout, falling back to NMEA-only mode");
            break;
        }
        
        auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now).count();
        
        // Max 1s per iteration
        uint32_t wait_timeout = (remaining_ms < 1000) 
            ? static_cast<uint32_t>(remaining_ms) 
            : 1000;
        
        // Wait for edge on any pin
        PPSLine line;
        PPSTimestamp timestamp;
        
        if (pimpl_->wait_for_edge(wait_timeout, line, timestamp)) {
            // Edge detected - process it
            process_edge(line, timestamp);
        }
    }
    
    // Thread exiting
    if (state_ == DetectionState::Locked) {
        log_detection_event("PPS detection locked, continuing to provide timestamps");
        
        // Continue providing timestamps in locked state
        while (!stop_requested_ && state_ == DetectionState::Locked) {
            PPSLine line;
            PPSTimestamp timestamp;
            
            if (pimpl_->wait_for_edge(2000, line, timestamp)) {
                // Verify it's still the same line
                if (line == detected_line_) {
                    // Publish timestamp
                    {
                        std::lock_guard<std::mutex> lock(timestamp_mutex_);
                        latest_timestamp_ = timestamp;
                        timestamp_available_ = true;
                    }
                    timestamp_cv_.notify_all();
                } else {
                    // Different line - signal lost?
                    log_detection_event("PPS signal lost or changed pins");
                    state_ = DetectionState::Failed;
                    break;
                }
            } else {
                // Timeout waiting for PPS - signal lost
                log_detection_event("PPS signal lost (2s timeout)");
                state_ = DetectionState::Failed;
                break;
            }
        }
    }
}

// Process edge
void PPSDetector::process_edge(PPSLine line, const PPSTimestamp& timestamp) {
    // Find candidate for this line
    size_t idx = static_cast<size_t>(line) - 1;  // DCD=1, CTS=2, DSR=3
    if (idx >= candidates_.size()) {
        return;
    }
    
    EdgeCandidate& candidate = candidates_[idx];
    
    if (candidate.edge_count == 0) {
        // First edge on this pin
        candidate.first_edge = timestamp;
        candidate.last_edge = timestamp;
        candidate.edge_count = 1;
        
        // Log first edge
        char msg[128];
        snprintf(msg, sizeof(msg), "First edge detected on %s", to_string(line));
        log_detection_event(msg);
    } else {
        // Subsequent edge - validate interval
        if (validate_interval(candidate, timestamp)) {
            candidate.edge_count++;
            candidate.valid_count++;
            candidate.validated = true;
            candidate.last_edge = timestamp;
            
            // Check if we have lock
            if (confirm_pps_lock(candidate)) {
                // PPS detected and validated!
                std::lock_guard<std::mutex> lock(state_mutex_);
                detected_line_ = candidate.line;
                state_ = DetectionState::Locked;
                
                char msg[128];
                double interval = timestamp - candidate.first_edge;
                snprintf(msg, sizeof(msg), 
                        "PPS detected on %s, validated with %u edges, avg interval=%.3fs",
                        to_string(candidate.line), candidate.edge_count, 
                        interval / (candidate.edge_count - 1));
                log_detection_event(msg);
            }
        } else {
            // Invalid interval - reset candidate
            candidate.edge_count = 1;
            candidate.valid_count = 0;
            candidate.validated = false;
            candidate.first_edge = timestamp;
            candidate.last_edge = timestamp;
            
            double interval = timestamp - candidate.last_edge;
            update_statistics(interval, false);
        }
    }
}

// Validate interval
bool PPSDetector::validate_interval(const EdgeCandidate& candidate, 
                                   const PPSTimestamp& new_edge) {
    double interval_sec = new_edge - candidate.last_edge;
    
    bool valid = (interval_sec >= MIN_INTERVAL_SEC && 
                  interval_sec <= MAX_INTERVAL_SEC);
    
    update_statistics(interval_sec, valid);
    
    return valid;
}

// Confirm PPS lock
bool PPSDetector::confirm_pps_lock(const EdgeCandidate& candidate) const {
    return candidate.edge_count >= MIN_EDGES_FOR_LOCK && 
           candidate.validated &&
           candidate.valid_count >= (MIN_EDGES_FOR_LOCK - 1);
}

// Update statistics
void PPSDetector::update_statistics(double interval_sec, bool valid) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_edges++;
    
    if (valid) {
        statistics_.valid_intervals++;
        
        // Update interval stats
        if (interval_sec < statistics_.min_interval_sec) {
            statistics_.min_interval_sec = interval_sec;
        }
        if (interval_sec > statistics_.max_interval_sec) {
            statistics_.max_interval_sec = interval_sec;
        }
        
        // Update average (rolling)
        double alpha = 0.1;  // Smoothing factor
        if (statistics_.avg_interval_sec == 0.0) {
            statistics_.avg_interval_sec = interval_sec;
        } else {
            statistics_.avg_interval_sec = 
                alpha * interval_sec + (1 - alpha) * statistics_.avg_interval_sec;
        }
        
        // Estimate jitter (deviation from 1.0s)
        double deviation_ns = std::abs(interval_sec - 1.0) * 1e9;
        if (statistics_.jitter_ns == 0.0) {
            statistics_.jitter_ns = deviation_ns;
        } else {
            statistics_.jitter_ns = 
                alpha * deviation_ns + (1 - alpha) * statistics_.jitter_ns;
        }
    } else {
        statistics_.invalid_intervals++;
    }
}

// Log detection event
void PPSDetector::log_detection_event(const char* message) {
    // Simple logging to stdout
    // In production, integrate with proper logging system
    std::cout << "[PPS] " << message << std::endl;
}

} // namespace PPS
} // namespace GPS
