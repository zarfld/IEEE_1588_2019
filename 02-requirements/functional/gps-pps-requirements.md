---
specType: requirements
standard: "29148"
phase: "02-requirements"
version: "1.0.0"
author: "Requirements Engineering Team"
date: "2025-12-02"
status: "approved"
traceability:
  stakeholderRequirements:
    - StR-005  # STR-PERF-001: Synchronization Accuracy
  linkedFrom:
    - "01-stakeholder-requirements/stakeholder-requirements-spec.md"
  linkedTo:
    - "04-design/components/gps-pps-autodetect.md"
    - "examples/04-gps-nmea-sync/pps_detector.hpp"
    - "examples/04-gps-nmea-sync/pps_detector.cpp"
    - "examples/04-gps-nmea-sync/tests/test_pps_hardware.cpp"
---
# GPS PPS (Pulse Per Second) Autodetection Requirements

**Standards Compliance**: ISO/IEC/IEEE 29148:2018  
**Document Version**: 1.0  
**Date**: December 2, 2025  
**Prepared by**: Standards-Compliant Software Development Team

## Executive Summary

This document specifies requirements for GPS Pulse Per Second (PPS) signal autodetection and timestamping to enhance IEEE 1588-2019 PTP synchronization accuracy from 10ms (NMEA-only) to sub-microsecond precision. The PPS detector shall automatically identify and utilize GPS module PPS output signals on RS-232 modem control pins.

## 1. Stakeholder Requirements

### 1.1 Professional Audio System Integrators
- **StR-005**: Need sub-microsecond timing accuracy for media synchronization (enhanced by GPS PPS)

### 1.2 Industrial Automation Engineers
- **StR-005**: Need precise timing reference for automation systems

### 1.3 Network Infrastructure Providers
- **StR-005**: Need traceable UTC timing with enhanced accuracy

## 2. System Requirements

### 2.1 PPS Signal Detection

**REQ-PPS-001**: PPS Pin Autodetection
- **Description**: The system shall automatically detect GPS Pulse Per Second (PPS) signals on RS-232 modem control pins (DCD Pin 1, CTS Pin 8, DSR Pin 6) without requiring manual configuration.
- **Rationale**: GPS modules from different manufacturers output PPS on different RS-232 pins. Autodetection eliminates configuration complexity and enables plug-and-play operation.
- **Traces to**: StR-005 (Synchronization Accuracy)
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Monitors all three modem control pins simultaneously during detection phase
  - Identifies pin with valid 1Hz signal within 10 seconds
  - Locks to detected pin once validated
  - Provides API to query detected pin (DCD/CTS/DSR/None)
- **Verification**: Hardware test with u-blox NEO-G7 GPS module (PPS on DCD), integration test with mock serial interface

**REQ-PPS-002**: Sub-Microsecond Timestamp Accuracy
- **Description**: The system shall timestamp PPS rising edges with sub-microsecond accuracy (target: 50-200ns typical).
- **Rationale**: Sub-microsecond timestamps are required to achieve IEEE 1588-2019 timing accuracy improvements over NMEA-only mode (10ms resolution).
- **Traces to**: StR-005 (Synchronization Accuracy)
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Uses QueryPerformanceCounter (Windows) or CLOCK_MONOTONIC_RAW (Linux) for timestamping
  - Timestamp captured within 1 microsecond of actual rising edge
  - Timestamp format matches IEEE 1588-2019 PTP structure (seconds + nanoseconds)
  - Jitter measured over 100 samples < 1 microsecond RMS
- **Verification**: Hardware test with oscilloscope comparison, statistical jitter analysis

**REQ-PPS-003**: 1Hz Frequency Validation
- **Description**: The system shall validate PPS signals have 1Hz frequency (0.8-1.2s interval tolerance) with minimum 2 consecutive valid intervals before lock.
- **Rationale**: GPS PPS standard specifies 1Hz signal. Validation prevents false detection from noise or non-PPS signals.
- **Traces to**: StR-005 (Synchronization Accuracy), REQ-PPS-001
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Measures interval between consecutive rising edges
  - Accepts intervals in range 800ms - 1200ms (±20% tolerance)
  - Requires minimum 3 edges (2 valid intervals) for lock confirmation
  - Rejects signals outside frequency tolerance
  - Provides statistics (average interval, min, max, jitter)
- **Verification**: Unit test with mock timing, hardware test with real GPS

**REQ-PPS-004**: Fallback to NMEA-Only Mode
- **Description**: The system shall gracefully fall back to NMEA-only time synchronization (10ms resolution) if PPS signal is unavailable or lost.
- **Rationale**: Ensures continuous operation when GPS module lacks PPS output, PPS cable is disconnected, or GPS loses satellite lock.
- **Traces to**: StR-005 (Synchronization Accuracy), REQ-S-002 (Fault Recovery)
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Continues NMEA sentence parsing regardless of PPS status
  - Degrades timing accuracy gracefully from sub-microsecond to 10ms
  - Provides clear status indication (PPS locked / NMEA-only)
  - Automatically re-enables PPS if signal recovers (signal loss timeout: 2 seconds)
  - No crashes or hangs when PPS unavailable
- **Verification**: Integration test with NMEA-only GPS, PPS disconnect test

**REQ-PPS-005**: Non-Blocking Detection with Timeout
- **Description**: The system shall perform PPS detection in non-blocking mode with configurable timeout (default: 10 seconds).
- **Rationale**: PTP clock synchronization requires real-time operation without blocking delays. Timeout prevents indefinite hangs waiting for PPS.
- **Traces to**: REQ-NFR-PTP-024 (Real-Time Performance), REQ-PPS-001
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Detection runs asynchronously via poll() or background thread
  - Returns immediately with current state (Detecting, Locked, Failed)
  - Configurable timeout parameter (default: 10000ms)
  - After timeout, falls back to NMEA-only mode automatically
  - Bounded execution time per poll() call < 100 microseconds
- **Verification**: Unit test with mock timing, performance profiling

**REQ-PPS-006**: Thread-Safe State Transitions
- **Description**: The system shall implement thread-safe state machine transitions protecting shared state with mutexes or atomic operations.
- **Rationale**: PPS detector may be accessed from multiple threads (NMEA parser thread, PTP clock adjustment thread, monitoring thread).
- **Traces to**: REQ-NFR-PTP-040 (Thread Safety)
- **Priority**: Must Have
- **Acceptance Criteria**:
  - All public APIs are thread-safe (poll(), get_statistics(), get_state())
  - Shared state protected by mutex or std::atomic
  - No data races detected by ThreadSanitizer
  - Lock-free fast path for timestamp reads (atomic loads)
  - Deadlock-free under stress testing (10+ threads, 1 hour)
- **Verification**: Unit test with ThreadSanitizer, stress test with concurrent access

**REQ-PPS-007**: Platform Abstraction (Win/Linux/Embedded)
- **Description**: The system shall provide platform-agnostic API using hardware abstraction layer (HAL) for serial port access across Windows, Linux, and embedded RTOS platforms.
- **Rationale**: Library must compile and run on Windows, Linux, macOS, and embedded systems (FreeRTOS, Zephyr) without platform-specific code in protocol logic.
- **Traces to**: REQ-STK-ARCH-001 (Hardware-Agnostic Architecture), ADR-001 (Hardware Abstraction Interfaces)
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Core PPSDetector class has zero platform-specific code
  - Platform differences encapsulated in HAL::Serial interface
  - Compiles on Windows (MSVC), Linux (GCC/Clang), macOS (Clang)
  - Tested on Windows 10+ and Ubuntu 20.04+
  - Embedded HAL stubs provided for FreeRTOS integration
- **Verification**: Multi-platform CI build (Windows/Linux/macOS), embedded stub compilation test

## 3. Non-Functional Requirements

### 3.1 Performance

**REQ-PPS-PERF-001**: Detection Latency
- **Description**: PPS detection shall complete within 10 seconds maximum (configurable)
- **Acceptance Criteria**: 
  - Average detection time < 5 seconds with valid PPS signal
  - Maximum detection time < 10 seconds (timeout)

**REQ-PPS-PERF-002**: Timestamp Jitter
- **Description**: Timestamp jitter shall be < 1 microsecond RMS over 100 samples
- **Acceptance Criteria**:
  - Standard deviation of 100 consecutive timestamps < 1000 nanoseconds
  - No outliers > 10 microseconds

### 3.2 Reliability

**REQ-PPS-REL-001**: Signal Loss Recovery
- **Description**: System shall detect PPS signal loss within 2 seconds and automatically recover when signal returns
- **Acceptance Criteria**:
  - Signal loss timeout: 2000ms (configurable)
  - Automatic re-detection on recovery
  - No manual reset required

**REQ-PPS-REL-002**: Noise Immunity
- **Description**: PPS detector shall reject noise and invalid signals (non-1Hz frequencies)
- **Acceptance Criteria**:
  - Rejects signals < 0.8Hz or > 1.2Hz
  - Requires 2+ consecutive valid intervals for lock
  - No false positives from contact bounce or EMI

### 3.3 Usability

**REQ-PPS-USE-001**: Zero Configuration
- **Description**: PPS detector shall require zero configuration for standard GPS modules
- **Acceptance Criteria**:
  - Works with u-blox NEO-6M/7M/8M out-of-box
  - Automatic pin detection (no user selection required)
  - Graceful fallback if PPS unavailable

**REQ-PPS-USE-002**: Status Reporting
- **Description**: System shall provide clear status and statistics for debugging
- **Acceptance Criteria**:
  - Current state (Idle, Detecting, Locked, Failed)
  - Detected pin (DCD/CTS/DSR/None)
  - Statistics (total edges, intervals, jitter)
  - Last timestamp with source pin

## 4. Traceability Matrix

| Requirement | Stakeholder Req | Design Element | Test Case |
|-------------|----------------|----------------|-----------|
| REQ-PPS-001 | StR-005 | gps-pps-autodetect.md §2.1 | test_pps_hardware.cpp::test_pps_detection |
| REQ-PPS-002 | StR-005 | gps-pps-autodetect.md §4.3 | test_pps_hardware.cpp::test_edge_timestamping |
| REQ-PPS-003 | StR-005 | gps-pps-autodetect.md §4.5 | test_pps_hardware.cpp::test_frequency_validation |
| REQ-PPS-004 | StR-005 | gps-pps-autodetect.md §7 | test_pps_hardware.cpp::test_fallback_nmea_only |
| REQ-PPS-005 | REQ-NFR-PTP-024 | gps-pps-autodetect.md §5.4 | test_pps_detector.cpp::test_nonblocking_timeout |
| REQ-PPS-006 | REQ-NFR-PTP-040 | gps-pps-autodetect.md §5.5 | test_pps_detector.cpp::test_thread_safety |
| REQ-PPS-007 | REQ-STK-ARCH-001 | gps-pps-autodetect.md §6 | CI builds on Win/Linux/macOS |

## 5. Validation Strategy

### 5.1 Unit Tests
- Mock serial interface tests (fast, no hardware)
- State machine transition validation
- Frequency validation logic
- Thread safety with ThreadSanitizer

### 5.2 Hardware Tests
- u-blox NEO-G7 GPS module with PPS on DCD
- Oscilloscope timestamp accuracy verification
- Long-term jitter characterization (1 hour)
- Signal loss and recovery scenarios

### 5.3 Integration Tests
- GPS NMEA + PPS combined timing
- PTP clock quality updates based on PPS status
- Multi-platform CI builds (Windows/Linux/macOS)
- Embedded stub compilation (FreeRTOS/Zephyr)

## 6. Standards Compliance

### 6.1 IEEE 1588-2019 Integration
- **Section 7.3**: Time representation (nanosecond precision PPS timestamps)
- **Section 7.4.1**: Timestamping requirements (sub-microsecond accuracy)

### 6.2 RS-232 Standard
- **EIA-232**: Modem control signals (DCD/CTS/DSR pin definitions)
- Voltage levels and electrical characteristics

### 6.3 ISO/IEC/IEEE 29148:2018
- Requirements traceability to stakeholder needs
- Testable acceptance criteria
- Verification methods defined

## 7. References

- **IEEE 1588-2019**: Precision Clock Synchronization Protocol
- **ISO/IEC/IEEE 29148:2018**: Requirements Engineering Processes
- **RS-232 / EIA-232**: Serial Port Standard
- **04-design/components/gps-pps-autodetect.md**: Detailed design specification
- **examples/04-gps-nmea-sync/PPS_IMPLEMENTATION_STATUS.md**: Implementation status
- **examples/04-gps-nmea-sync/PPS_TESTING_STRATEGY.md**: Testing strategy

## 8. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-02 | Requirements Engineering Team | Initial creation from design document requirements |

