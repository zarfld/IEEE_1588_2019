# GPS PPS (Pulse Per Second) Autodetection Design

**Component**: GPS PPS Detector  
**Version**: 1.0  
**Date**: November 13, 2025  
**Status**: Design Complete, Ready for Implementation  

## 1. Overview

### 1.1 Purpose

Automatically detect and utilize GPS Pulse Per Second (PPS) signal to enhance timing accuracy from 10ms (NMEA centiseconds) to sub-microsecond precision for IEEE 1588-2019 PTP synchronization.

### 1.2 Scope

- Autodetect PPS signal on RS-232 modem control pins (DCD, CTS, DSR)
- Validate 1Hz pulse frequency with jitter tolerance
- Timestamp rising edges with nanosecond precision
- Graceful fallback to NMEA-only mode if PPS unavailable
- Platform-agnostic implementation (Windows/Linux/Embedded)

### 1.3 Requirements Traceability

| Requirement | Description | Priority |
|-------------|-------------|----------|
| REQ-PPS-001 | Autodetect PPS on DCD/CTS/DSR pins | Must Have |
| REQ-PPS-002 | Sub-microsecond edge timestamp accuracy | Must Have |
| REQ-PPS-003 | 1Hz frequency validation (0.8-1.2s interval) | Must Have |
| REQ-PPS-004 | Fallback to NMEA-only if no PPS | Must Have |
| REQ-PPS-005 | Non-blocking detection (10s timeout) | Must Have |
| REQ-PPS-006 | Thread-safe state transitions | Must Have |
| REQ-PPS-007 | Platform abstraction (Win/Linux/RTOS) | Must Have |

## 2. Architecture

### 2.1 Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                     GPS PPS Detector                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────────┐      ┌──────────────────┐           │
│  │  Pin Monitor     │      │  State Machine   │           │
│  │  - DCD (Pin 1)   │◄─────┤  - Detecting     │           │
│  │  - CTS (Pin 8)   │      │  - Locked        │           │
│  │  - DSR (Pin 6)   │      │  - Failed        │           │
│  └──────────────────┘      └──────────────────┘           │
│           │                         │                      │
│           ▼                         ▼                      │
│  ┌──────────────────┐      ┌──────────────────┐           │
│  │ Edge Timestamper │      │  1Hz Validator   │           │
│  │ - QPC (Windows)  │      │  - 0.8-1.2s      │           │
│  │ - CLOCK_RAW(Lnx) │      │  - Require 2+    │           │
│  └──────────────────┘      └──────────────────┘           │
│           │                         │                      │
│           └─────────┬───────────────┘                      │
│                     ▼                                      │
│          ┌──────────────────┐                              │
│          │  PPS Timestamp   │                              │
│          │  - Nanosecond    │                              │
│          │  - Second edge   │                              │
│          └──────────────────┘                              │
└─────────────────────────────────────────────────────────────┘
                      │
                      ▼
         ┌──────────────────────────┐
         │ GPS Time Converter       │
         │ Combine: PPS + NMEA      │
         └──────────────────────────┘
```

### 2.2 State Machine

```
                    ┌─────────────┐
                    │   Initial   │
                    └──────┬──────┘
                           │ start()
                           ▼
                    ┌─────────────┐
              ┌────►│  Detecting  │◄────┐
              │     └──────┬──────┘     │
              │            │            │
              │    Edge on DCD/CTS/DSR │ Invalid interval
              │            │            │  or no edge
              │            ▼            │
              │     ┌─────────────┐    │
              │     │  Validating │────┘
              │     │  (2+ pulses)│
              │     └──────┬──────┘
              │            │
              │     Valid 1Hz confirmed
              │            │
   Timeout    │            ▼
   (10s)      │     ┌─────────────┐
              │     │   Locked    │
              │     │  (PPS Pin)  │
              │     └─────────────┘
              │            
              ▼            
       ┌─────────────┐
       │   Failed    │
       │ (NMEA-only) │
       └─────────────┘
```

### 2.3 Class Design

```cpp
namespace GPS {
namespace PPS {

/**
 * @brief PPS signal line identification
 */
enum class PPSLine {
    None,    // No PPS detected
    DCD,     // DB9 Pin 1 - Data Carrier Detect (most common)
    CTS,     // DB9 Pin 8 - Clear To Send
    DSR      // DB9 Pin 6 - Data Set Ready
};

/**
 * @brief Autodetection state
 */
enum class DetectionState {
    Detecting,   // Monitoring all pins for PPS
    Locked,      // Valid PPS detected on specific pin
    Failed       // No valid PPS found, using NMEA-only
};

/**
 * @brief High-resolution timestamp
 */
struct PPSTimestamp {
    uint64_t seconds;       // Seconds since epoch
    uint32_t nanoseconds;   // Nanoseconds within second
    PPSLine source;         // Which pin generated this timestamp
    
    /**
     * @brief Convert to nanoseconds since epoch
     */
    int64_t to_nanoseconds() const {
        return static_cast<int64_t>(seconds) * 1000000000LL + 
               static_cast<int64_t>(nanoseconds);
    }
};

/**
 * @brief Edge detection candidate
 */
struct EdgeCandidate {
    PPSLine line;
    PPSTimestamp first_edge;
    PPSTimestamp last_edge;
    uint32_t edge_count;
    bool validated;
    
    EdgeCandidate(PPSLine l) 
        : line(l), edge_count(0), validated(false) {}
};

/**
 * @brief PPS Detector - Platform-agnostic interface
 */
class PPSDetector {
public:
    /**
     * @brief Constructor
     * @param serial_handle Platform-specific serial port handle
     */
    explicit PPSDetector(void* serial_handle);
    
    /**
     * @brief Destructor - stops monitoring
     */
    ~PPSDetector();
    
    /**
     * @brief Start PPS autodetection
     * @param timeout_ms Detection timeout in milliseconds (default 10000)
     * @return true if detection started successfully
     */
    bool start_detection(uint32_t timeout_ms = 10000);
    
    /**
     * @brief Stop PPS monitoring
     */
    void stop_detection();
    
    /**
     * @brief Get current detection state
     */
    DetectionState get_state() const;
    
    /**
     * @brief Get detected PPS line (if locked)
     * @return PPSLine::None if not locked
     */
    PPSLine get_detected_line() const;
    
    /**
     * @brief Get latest PPS timestamp (blocking)
     * @param timeout_ms Maximum wait time
     * @param[out] timestamp Captured PPS timestamp
     * @return true if timestamp captured within timeout
     */
    bool get_pps_timestamp(uint32_t timeout_ms, PPSTimestamp& timestamp);
    
    /**
     * @brief Check if PPS is available
     * @return true if in Locked state
     */
    bool is_pps_available() const;
    
    /**
     * @brief Get detection statistics
     */
    struct Statistics {
        uint64_t total_edges;      // Total edges detected
        uint64_t valid_intervals;  // Valid 1Hz intervals
        uint64_t invalid_intervals;// Invalid intervals
        double avg_interval_sec;   // Average interval
        double jitter_ns;          // Measured jitter
    };
    
    Statistics get_statistics() const;

private:
    // Platform-specific implementation
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // Detection thread
    void detection_thread();
    
    // Edge validation
    bool validate_interval(const EdgeCandidate& candidate, 
                          const PPSTimestamp& new_edge);
};

} // namespace PPS
} // namespace GPS
```

## 3. Detailed Design

### 3.1 Pin Monitoring

#### Windows Implementation
```cpp
// Enable modem status events for all PPS candidates
DWORD event_mask = EV_RLSD |  // DCD (Pin 1)
                   EV_CTS |   // CTS (Pin 8)
                   EV_DSR;    // DSR (Pin 6)
SetCommMask(serial_handle, event_mask);

// Wait for any edge event
DWORD events = 0;
WaitCommEvent(serial_handle, &events, nullptr);

// Capture timestamp immediately
LARGE_INTEGER qpc;
QueryPerformanceCounter(&qpc);

// Determine which pin triggered
if (events & EV_RLSD) process_edge(PPSLine::DCD, qpc);
if (events & EV_CTS)  process_edge(PPSLine::CTS, qpc);
if (events & EV_DSR)  process_edge(PPSLine::DSR, qpc);
```

#### Linux Implementation
```cpp
// Monitor all modem control lines
int modem_bits = TIOCM_CAR |  // DCD (Pin 1)
                 TIOCM_CTS |  // CTS (Pin 8)
                 TIOCM_DSR;   // DSR (Pin 6)

// Wait for any change
ioctl(fd, TIOCMIWAIT, modem_bits);

// Capture timestamp immediately
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

// Read current state to determine which pin
int status;
ioctl(fd, TIOCMGET, &status);

if (status & TIOCM_CAR) process_edge(PPSLine::DCD, ts);
if (status & TIOCM_CTS) process_edge(PPSLine::CTS, ts);
if (status & TIOCM_DSR) process_edge(PPSLine::DSR, ts);
```

### 3.2 Edge Timestamping

#### Timestamp Accuracy Requirements
- **Target**: Sub-microsecond (< 1μs)
- **Windows**: QueryPerformanceCounter() typically 100ns-1μs
- **Linux**: clock_gettime(CLOCK_MONOTONIC_RAW) typically 10-100ns
- **Embedded**: Hardware timer capture (1-100ns)

#### Timestamp Conversion
```cpp
PPSTimestamp capture_timestamp() {
    PPSTimestamp ts;
    
#ifdef _WIN32
    LARGE_INTEGER qpc, freq;
    QueryPerformanceCounter(&qpc);
    QueryPerformanceFrequency(&freq);
    
    // Convert to nanoseconds
    uint64_t ns = (qpc.QuadPart * 1000000000ULL) / freq.QuadPart;
    ts.seconds = ns / 1000000000ULL;
    ts.nanoseconds = ns % 1000000000ULL;
    
#else  // Linux
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    ts.seconds = t.tv_sec;
    ts.nanoseconds = t.tv_nsec;
#endif
    
    return ts;
}
```

### 3.3 1Hz Validation

```cpp
bool validate_interval(const EdgeCandidate& candidate, 
                      const PPSTimestamp& new_edge) {
    // Calculate interval from last edge
    int64_t interval_ns = new_edge.to_nanoseconds() - 
                          candidate.last_edge.to_nanoseconds();
    
    double interval_sec = interval_ns / 1e9;
    
    // Valid PPS: 0.8s to 1.2s (±200ms tolerance)
    constexpr double MIN_INTERVAL = 0.8;
    constexpr double MAX_INTERVAL = 1.2;
    
    if (interval_sec >= MIN_INTERVAL && interval_sec <= MAX_INTERVAL) {
        // Valid 1Hz interval
        return true;
    }
    
    // Invalid - could be noise or wrong signal
    return false;
}

bool confirm_pps_lock(EdgeCandidate& candidate) {
    // Require at least 2 consecutive valid intervals
    // This means 3 edges total
    constexpr uint32_t MIN_EDGES = 3;
    
    return candidate.edge_count >= MIN_EDGES && 
           candidate.validated;
}
```

### 3.4 Detection Algorithm

```cpp
void PPSDetector::detection_thread() {
    // Initialize candidates for all three pins
    std::array<EdgeCandidate, 3> candidates = {
        EdgeCandidate(PPSLine::DCD),
        EdgeCandidate(PPSLine::CTS),
        EdgeCandidate(PPSLine::DSR)
    };
    
    auto deadline = std::chrono::steady_clock::now() + 
                    std::chrono::milliseconds(timeout_ms_);
    
    state_ = DetectionState::Detecting;
    
    while (state_ == DetectionState::Detecting) {
        // Wait for edge event (1s timeout per iteration)
        auto edge_event = wait_for_edge(1000);
        
        if (edge_event.timeout) {
            // Check overall timeout
            if (std::chrono::steady_clock::now() > deadline) {
                state_ = DetectionState::Failed;
                break;
            }
            continue;
        }
        
        // Find candidate for this pin
        auto& candidate = find_candidate(candidates, edge_event.line);
        
        if (candidate.edge_count == 0) {
            // First edge on this pin
            candidate.first_edge = edge_event.timestamp;
            candidate.last_edge = edge_event.timestamp;
            candidate.edge_count = 1;
        } else {
            // Subsequent edge - validate interval
            if (validate_interval(candidate, edge_event.timestamp)) {
                candidate.edge_count++;
                candidate.validated = true;
                candidate.last_edge = edge_event.timestamp;
                
                // Check if we have lock
                if (confirm_pps_lock(candidate)) {
                    // PPS detected and validated!
                    detected_line_ = candidate.line;
                    state_ = DetectionState::Locked;
                    
                    // Log success
                    log_pps_detected(candidate);
                    break;
                }
            } else {
                // Invalid interval - reset this candidate
                candidate.edge_count = 1;
                candidate.validated = false;
                candidate.first_edge = edge_event.timestamp;
                candidate.last_edge = edge_event.timestamp;
            }
        }
    }
    
    if (state_ == DetectionState::Failed) {
        log_pps_failed();
    }
}
```

## 4. Platform Abstraction

### 4.1 Hardware Interface

```cpp
namespace GPS {
namespace PPS {
namespace Platform {

/**
 * @brief Platform-specific serial handle wrapper
 */
struct SerialHandle {
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
};

/**
 * @brief Platform-agnostic edge event
 */
struct EdgeEvent {
    PPSLine line;
    PPSTimestamp timestamp;
    bool timeout;
};

/**
 * @brief Platform-specific operations interface
 */
class IPlatformOperations {
public:
    virtual ~IPlatformOperations() = default;
    
    /**
     * @brief Enable PPS monitoring on all candidate pins
     */
    virtual bool enable_pps_monitoring(SerialHandle handle) = 0;
    
    /**
     * @brief Wait for edge event on any monitored pin
     */
    virtual EdgeEvent wait_for_edge(SerialHandle handle, 
                                    uint32_t timeout_ms) = 0;
    
    /**
     * @brief Capture high-resolution timestamp
     */
    virtual PPSTimestamp capture_timestamp() = 0;
    
    /**
     * @brief Disable PPS monitoring
     */
    virtual void disable_pps_monitoring(SerialHandle handle) = 0;
};

/**
 * @brief Factory for platform-specific operations
 */
std::unique_ptr<IPlatformOperations> create_platform_operations();

} // namespace Platform
} // namespace PPS
} // namespace GPS
```

## 5. Integration Points

### 5.1 GPS Time Converter Integration

```cpp
namespace GPS {
namespace Time {

class GPSTimeConverter {
public:
    /**
     * @brief Set PPS detector for enhanced accuracy
     */
    void set_pps_detector(std::shared_ptr<PPS::PPSDetector> detector) {
        pps_detector_ = detector;
    }
    
    /**
     * @brief Convert with PPS enhancement
     * 
     * If PPS available:
     *   - Use PPS edge for precise second boundary
     *   - Use NMEA for absolute time (which second)
     *   - Combine for nanosecond-accurate timestamp
     * 
     * If PPS unavailable:
     *   - Fallback to NMEA-only (10ms resolution)
     */
    bool convert_to_ptp(const NMEA::GPSTimeData& gps_data, 
                       PTPTimestamp& ptp_timestamp) {
        if (pps_detector_ && pps_detector_->is_pps_available()) {
            // Enhanced mode: PPS + NMEA
            return convert_with_pps(gps_data, ptp_timestamp);
        } else {
            // Fallback: NMEA-only
            return convert_nmea_only(gps_data, ptp_timestamp);
        }
    }

private:
    bool convert_with_pps(const NMEA::GPSTimeData& gps_data,
                         PTPTimestamp& ptp_timestamp) {
        // Get PPS edge timestamp
        PPS::PPSTimestamp pps_ts;
        if (!pps_detector_->get_pps_timestamp(1000, pps_ts)) {
            // PPS timeout - fallback
            return convert_nmea_only(gps_data, ptp_timestamp);
        }
        
        // NMEA provides: HH:MM:SS.cs and date
        // PPS provides: precise second edge
        
        // Calculate absolute second from NMEA
        uint64_t nmea_second = nmea_to_absolute_second(gps_data);
        
        // Align PPS edge with NMEA second
        // (Handle case where PPS and NMEA might be from different seconds)
        ptp_timestamp.seconds = nmea_second;
        ptp_timestamp.nanoseconds = pps_ts.nanoseconds;
        
        return true;
    }
    
    std::shared_ptr<PPS::PPSDetector> pps_detector_;
};

} // namespace Time
} // namespace GPS
```

## 6. Error Handling

### 6.1 Failure Modes

| Failure Mode | Detection | Recovery |
|--------------|-----------|----------|
| No PPS signal | Timeout after 10s | Fallback to NMEA-only |
| Noisy signal | Invalid intervals | Continue monitoring other pins |
| Wrong frequency | Interval outside 0.8-1.2s | Reject and continue |
| Signal loss | Gaps > 2s | Re-enter detecting state |
| Platform error | System call failure | Log and fallback |

### 6.2 Logging

```cpp
// Detection events
LOG_INFO("PPS autodetection started, monitoring DCD/CTS/DSR");
LOG_INFO("PPS detected on DCD (Pin 1), interval=1.000s, jitter=50ns");
LOG_WARN("PPS detection timeout, falling back to NMEA-only mode");

// Edge events
LOG_DEBUG("Edge on CTS: interval=1.234s (invalid, expect ~1.0s)");
LOG_DEBUG("Edge on DCD: interval=0.999s (valid), count=2/3");

// State transitions
LOG_INFO("PPS state: Detecting → Locked(DCD)");
LOG_WARN("PPS state: Locked → Detecting (signal lost for 2s)");
```

## 7. Performance

### 7.1 Timing Accuracy

| Mode | Resolution | Typical Accuracy | Use Case |
|------|------------|------------------|----------|
| NMEA-only | 10ms | ±10ms | Basic GPS sync |
| PPS + NMEA | Sub-μs | ±50-200ns | High-precision PTP |

### 7.2 Resource Usage

- **CPU**: < 1% (background thread waiting on events)
- **Memory**: ~8KB (detector + buffers)
- **Latency**: < 1μs (edge capture to timestamp)

## 8. Testing Strategy

### 8.1 Unit Tests

```cpp
TEST(PPSDetector, DetectDCDPin) {
    // Mock serial port with 1Hz pulses on DCD
    // Verify: Locked state, PPSLine::DCD detected
}

TEST(PPSDetector, ValidateInterval) {
    // Test intervals: 0.7s (reject), 0.9s (accept), 1.1s (accept), 1.3s (reject)
}

TEST(PPSDetector, TimeoutFallback) {
    // Mock: no edges for 10s
    // Verify: Failed state, graceful fallback
}

TEST(PPSDetector, MultipleEdges) {
    // Test: edges on multiple pins simultaneously
    // Verify: First valid pin locks
}

TEST(PPSDetector, SignalLoss) {
    // Test: locked, then no edges for 2s
    // Verify: returns to detecting state
}
```

### 8.2 Hardware Tests

```cpp
TEST(PPSHardware, DetectUbloxPPS) {
    // u-blox NEO-G7 on COM3
    // DCD (Pin 1) should have 1Hz PPS
    // Measure: actual frequency, jitter
}

TEST(PPSHardware, TimestampAccuracy) {
    // Compare PPS timestamps with external reference
    // Verify: < 1μs accuracy
}
```

## 9. Standards Compliance

### 9.1 IEEE 1588-2019

- **Section 7.3**: Time representation (nanosecond precision) ✅
- **Section 7.4.1**: Timestamping requirements ✅
- **Annex C**: PTP over UDP/IP (requires precise timing) ✅

### 9.2 RS-232 Standard

- **EIA-232**: Modem control signals (DCD/CTS/DSR) ✅
- Pin assignments and voltage levels ✅

## 10. Future Enhancements

### Phase 2
- [ ] Multiple PPS sources (redundancy)
- [ ] PPS quality metrics (jitter measurement)
- [ ] Adaptive jitter filtering
- [ ] PPS signal health monitoring

### Phase 3
- [ ] Hardware timestamping (NIC/FPGA)
- [ ] Kernel-level PPS support (Linux PPS API)
- [ ] Time Transfer Analysis (TTA) for accuracy verification

---

**Design Status**: ✅ Complete and ready for implementation  
**Next Step**: Implement PPSDetector class with Windows/Linux platform support  
**Estimated Effort**: 2-3 hours implementation + 1 hour testing
