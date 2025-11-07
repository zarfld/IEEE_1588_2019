---
specType: requirements
standard: "29148"
phase: "02-requirements"
version: "1.0.0"
author: "Requirements Engineering Team"
date: "2025-11-07"
status: "draft"
traceability:
  stakeholderRequirements:
    - StR-001  # STR-STD-001: IEEE 1588-2019 Protocol Compliance
    - StR-005  # STR-PERF-001: Synchronization Accuracy <1µs
    - StR-006  # STR-PERF-002: Timing Determinism
---

# Use Case: UC-001 - Synchronize as Ordinary Clock Slave

**Use Case ID**: UC-001  
**Use Case Name**: Synchronize as Ordinary Clock Slave  
**Author**: Requirements Engineering Team  
**Date**: 2025-11-07  
**Version**: 1.0.0

---

## 1. Brief Description

An embedded system running IEEE 1588-2019 PTP implementation synchronizes its local clock to a network grandmaster clock operating as an ordinary clock slave. The system receives PTP protocol messages, calculates clock offset and rate adjustment, and maintains synchronization accuracy within specified tolerances.

**Context**: Real-time embedded system (e.g., industrial automation controller, professional audio device, network test equipment) requiring nanosecond-precision time synchronization.

---

## 2. Actors

### Primary Actor
- **Embedded System** (PTP Ordinary Clock - Slave role)
  - **Description**: Device requiring precise time synchronization
  - **Capabilities**: Network interface with hardware or software timestamping, real-time clock adjustment
  - **Responsibilities**: Maintain synchronized time, respond to timing protocol messages

### Supporting Actors
- **Grandmaster Clock** (PTP Ordinary Clock - Master role)
  - **Description**: Network timing source providing reference time
  - **Capabilities**: GPS-disciplined or atomic clock reference, PTP protocol message transmission
  - **Responsibilities**: Transmit accurate timing messages, maintain stable time base

- **Network Infrastructure** (Transparent/Boundary Clocks, switches)
  - **Description**: Ethernet network transporting PTP protocol messages
  - **Capabilities**: Layer 2 forwarding, optional PTP-aware switching (IEEE 802.1AS)
  - **Responsibilities**: Forward PTP messages with minimal delay variation

---

## 3. Preconditions

### System State
- ✅ **PTP Stack Initialized**: `ptp_init()` completed successfully with HAL interfaces registered
- ✅ **Network Interface Active**: Ethernet link established, capable of multicast reception
- ✅ **Local Clock Running**: System tick counter operational, clock adjustment interface available
- ✅ **Configuration Loaded**: PTP domain number, priority, clock identity configured

### Environmental Conditions
- ✅ **Grandmaster Present**: Active grandmaster clock transmitting Announce messages
- ✅ **Network Connectivity**: Bidirectional communication between slave and master
- ✅ **Synchronization Budget**: Network delay <10ms (typical Ethernet LAN)

---

## 4. Postconditions

### Success Postconditions
- ✅ **Clock Synchronized**: Local clock offset from grandmaster <1µs (P95 <1µs per REQ-NF-P-001)
- ✅ **Stable State**: Clock servo converged, frequency adjustment stable
- ✅ **Synchronization Maintained**: Continuous synchronization updates every sync interval
- ✅ **Metrics Available**: Synchronization accuracy, offset, delay metrics logged/exported

### Failure Postconditions
- ⚠️ **Timeout State**: Grandmaster lost, slave enters UNCALIBRATED or LISTENING state
- ⚠️ **Degraded Accuracy**: Synchronization outside specified tolerances, error logged
- ⚠️ **Failover Triggered**: BMCA selects alternate grandmaster if available

---

## 5. Main Success Scenario (Basic Flow)

### Phase 1: Discovery and Master Selection

**Step 1**: Embedded system enters **LISTENING** state on network initialization
- **Action**: PTP stack binds to UDP port 319 (event messages) and 320 (general messages)
- **System Response**: Begin receiving multicast PTP messages on domain
- **Traces To**: REQ-F-001 (PTP message types), REQ-F-002 (BMCA state machine)

**Step 2**: Grandmaster transmits **Announce** messages (default 1/second)
- **Message Content**: ClockQuality, Priority1/Priority2, ClockClass, TimeSource, ClockIdentity
- **Slave Action**: Receive and validate Announce message format per IEEE 1588-2019 Section 13.5
- **Traces To**: REQ-F-001 (Announce message processing), REQ-F-002 (BMCA dataset)

**Step 3**: Slave runs **Best Master Clock Algorithm (BMCA)** on Announce receipt
- **Dataset Comparison**: Compare received Announce with current best master
- **Decision**: Select grandmaster as best master if criteria met (clock class, priority, accuracy)
- **State Transition**: LISTENING → SLAVE state
- **Traces To**: REQ-F-002 (BMCA per IEEE 1588-2019 Section 9.3)

### Phase 2: Clock Offset Measurement (Delay Request-Response)

**Step 4**: Grandmaster transmits **Sync** message (default 1/second)
- **Timestamp T1**: Grandmaster egress timestamp (precise time Sync transmitted)
- **Slave Action**: Receive Sync, capture ingress timestamp **T2** (slave reception time)
- **Storage**: Record T2, await Follow_Up or use embedded T1
- **Traces To**: REQ-F-001 (Sync message), REQ-F-003 (offset calculation)

**Step 5**: Grandmaster transmits **Follow_Up** message immediately after Sync
- **Timestamp T1**: Precise egress timestamp from Sync message (if not embedded)
- **Slave Action**: Receive Follow_Up, extract T1, associate with Sync sequence number
- **Storage**: Record (T1, T2) pair for offset calculation
- **Traces To**: REQ-F-001 (Follow_Up message), REQ-F-003 (E2E delay mechanism)

**Step 6**: Slave transmits **Delay_Req** message to grandmaster
- **Timestamp T3**: Slave egress timestamp (precise time Delay_Req transmitted)
- **Slave Action**: Send Delay_Req, capture T3, store for delay calculation
- **Traces To**: REQ-F-001 (Delay_Req message), REQ-F-003 (clock offset calculation)

**Step 7**: Grandmaster receives Delay_Req and transmits **Delay_Resp**
- **Timestamp T4**: Grandmaster ingress timestamp (Delay_Req reception time)
- **Slave Action**: Receive Delay_Resp, extract T4, associate with Delay_Req sequence number
- **Storage**: Record (T1, T2, T3, T4) for complete offset calculation
- **Traces To**: REQ-F-001 (Delay_Resp message), REQ-F-003 (delay calculation)

### Phase 3: Clock Offset Calculation

**Step 8**: Calculate **mean path delay** using E2E delay mechanism
- **Formula**: `delay = ((T2 - T1) + (T4 - T3)) / 2`
- **Validation**: Check for negative delay (indicates timestamp error)
- **Storage**: Update path delay estimate, filter outliers
- **Traces To**: REQ-F-003 (clock offset calculation per IEEE 1588-2019 Section 11.3)

**Step 9**: Calculate **clock offset** from grandmaster
- **Formula**: `offset = (T2 - T1) - delay`
- **Validation**: Offset within expected range (-1s to +1s typical)
- **Storage**: Pass offset to clock servo for frequency adjustment
- **Traces To**: REQ-F-003 (offset calculation), REQ-NF-P-001 (accuracy target)

### Phase 4: Clock Servo Adjustment

**Step 10**: PI controller processes clock offset
- **Proportional Term**: `P = Kp * offset` (immediate response to current error)
- **Integral Term**: `I += Ki * offset` (accumulated error correction)
- **Frequency Adjustment**: `freq_adjustment = P + I`
- **Anti-Windup**: Clamp integral term to prevent overshooting
- **Traces To**: REQ-F-004 (PI controller servo), REQ-NF-P-002 (deterministic timing)

**Step 11**: Apply frequency adjustment to local clock via HAL
- **HAL Call**: `hal_clock_adjust_frequency(freq_adjustment)`
- **Hardware Action**: Program clock oscillator frequency offset (e.g., ±100 ppm)
- **Validation**: Verify adjustment applied successfully, within hardware limits
- **Traces To**: REQ-F-005 (HAL interfaces), REQ-NF-M-001 (platform independence)

**Step 12**: Monitor synchronization accuracy and update metrics
- **Metrics Collection**:
  - Offset from master (current, P50, P95, P99)
  - Path delay (mean, variance, jitter)
  - Servo state (converged, tracking, adjusting)
  - Packet loss rate (missed Sync/Announce)
- **Validation**: Verify accuracy meets REQ-NF-P-001 (<1µs P95)
- **Logging**: Export metrics for monitoring/debugging
- **Traces To**: REQ-NF-P-001 (synchronization accuracy)

**Step 13**: Return to Step 4 (continuous synchronization loop)
- **Sync Interval**: Repeat Steps 4-12 every sync interval (default 1 second)
- **State**: Remain in SLAVE state while grandmaster active
- **Monitoring**: Detect grandmaster timeout (3x announce interval), trigger failover if needed

---

## 6. Alternative Flows

### 6a. Grandmaster Timeout (Failover Scenario)

**Trigger**: No Announce messages received for 3x announce interval (default 3 seconds)

**Alternative Steps**:
- **6a.1**: Slave detects announce timeout via watchdog timer
- **6a.2**: Transition SLAVE → UNCALIBRATED state
- **6a.3**: Stop applying clock adjustments (hold last frequency offset)
- **6a.4**: Log "Grandmaster timeout" event with last known master ID
- **6a.5**: If alternate grandmaster available (via BMCA), transition to new master
- **6a.6**: Otherwise, remain in UNCALIBRATED, continue listening for Announce
- **Return**: Resume from Step 2 when new Announce received
- **Traces To**: REQ-F-002 (BMCA state machine), REQ-NF-S-001 (error handling)

### 6b. Network Congestion (High Path Delay Variation)

**Trigger**: Path delay variance exceeds threshold (e.g., >1ms jitter)

**Alternative Steps**:
- **6b.1**: Detect high jitter via delay measurement statistical analysis
- **6b.2**: Increase Delay_Req transmission rate (faster offset updates)
- **6b.3**: Apply outlier filtering to offset measurements (median filter)
- **6b.4**: Reduce servo gain (Kp, Ki) to dampen response to noisy measurements
- **6b.5**: Log "Network congestion" event with jitter metrics
- **6b.6**: Continue synchronization with degraded accuracy
- **Return**: Resume normal parameters when jitter returns to baseline
- **Traces To**: REQ-F-003 (offset calculation), REQ-NF-P-001 (accuracy)

### 6c. Timestamp Error Detection

**Trigger**: Invalid timestamp detected (negative delay, huge offset)

**Alternative Steps**:
- **6c.1**: Validate timestamp sanity checks fail:
  - Negative path delay: `(T2 - T1) + (T4 - T3) < 0`
  - Offset exceeds limits: `|offset| > 1 second`
  - Timestamp regression: `T_new < T_prev` (clock went backwards)
- **6c.2**: Discard invalid measurement, do not apply to servo
- **6c.3**: Log "Timestamp error" event with offending timestamps
- **6c.4**: Increment error counter, trigger alarm if rate exceeds threshold
- **6c.5**: Continue with previous valid offset estimate
- **Return**: Resume from Step 4 on next Sync message
- **Traces To**: REQ-NF-S-001 (input validation), REQ-NF-S-002 (bounds checking)

### 6d. Clock Adjustment Failure (HAL Error)

**Trigger**: HAL clock adjustment returns error code

**Alternative Steps**:
- **6d.1**: Detect `hal_clock_adjust_frequency()` returns non-zero error
- **6d.2**: Log "HAL clock adjustment failed" with error code and requested adjustment
- **6d.3**: Attempt retry with exponential backoff (3 retries max)
- **6d.4**: If retries exhausted, transition to FAULTY state
- **6d.5**: Notify application via error callback or event flag
- **6d.6**: Stop synchronization attempts until HAL recovery/restart
- **Return**: Resume from Step 1 after HAL reinitialization
- **Traces To**: REQ-F-005 (HAL interfaces), REQ-NF-S-001 (error handling)

---

## 7. Exception Flows

### 7a. No Grandmaster Available (Startup Condition)

**Trigger**: System boots with no active grandmaster on network

**Exception Steps**:
- **7a.1**: PTP stack initializes, enters LISTENING state
- **7a.2**: Listen for Announce messages for timeout period (default 30 seconds)
- **7a.3**: No Announce received, remain in LISTENING indefinitely
- **7a.4**: Log "No grandmaster found" event, export unsynchronized status
- **7a.5**: Application continues with local clock (potentially drift)
- **Recovery**: Resume from Step 2 when Announce message received
- **Traces To**: REQ-F-002 (BMCA state machine), REQ-NF-M-001 (graceful degradation)

### 7b. BMCA Selects Different Grandmaster (Master Change)

**Trigger**: Better grandmaster appears on network (lower clock class, higher priority)

**Exception Steps**:
- **7b.1**: Receive Announce from new master with superior dataset
- **7b.2**: BMCA determines new master is better than current master
- **7b.3**: Transition SLAVE → UNCALIBRATED temporarily
- **7b.4**: Flush existing offset/delay measurements from old master
- **7b.5**: Reset servo integral term (prevent windup from stale data)
- **7b.6**: Transition UNCALIBRATED → SLAVE for new master
- **7b.7**: Log "Grandmaster change" event (old ID → new ID)
- **Return**: Resume from Step 4 with new master
- **Traces To**: REQ-F-002 (BMCA), REQ-F-004 (servo reset)

### 7c. Resource Exhaustion (Memory/CPU Overload)

**Trigger**: System resource constraints prevent PTP processing

**Exception Steps**:
- **7c.1**: Detect resource exhaustion: malloc failure, CPU overload, timer miss
- **7c.2**: Log "Resource exhaustion" event with resource type and severity
- **7c.3**: Transition to FAULTY state to prevent cascading failures
- **7c.4**: Stop PTP message processing, release allocated resources
- **7c.5**: Notify application via critical error callback
- **7c.6**: Remain in FAULTY until explicit `ptp_reset()` call
- **Recovery**: Requires application intervention to free resources and restart PTP
- **Traces To**: REQ-NF-P-003 (resource efficiency), REQ-NF-S-002 (memory safety)

---

## 8. Special Requirements

### Performance Requirements
- **Synchronization Accuracy**: P95 offset <1µs from grandmaster (REQ-NF-P-001)
- **Convergence Time**: Achieve <1µs accuracy within 60 seconds of entering SLAVE state
- **CPU Overhead**: PTP processing <5% CPU utilization at 1 Hz sync rate (REQ-NF-P-003)
- **Memory Footprint**: Total PTP stack RAM usage <32 KB (REQ-NF-P-003)

### Determinism Requirements
- **WCET Guarantees**: All PTP message processing <100µs worst-case execution time (REQ-NF-P-002)
- **Zero Dynamic Allocation**: No malloc/free in steady-state operation (REQ-NF-P-002)
- **Fixed Priority Scheduling**: PTP tasks run at configurable real-time priority

### Security Requirements
- **Input Validation**: All received PTP messages validated per IEEE 1588-2019 format (REQ-NF-S-001)
- **Bounds Checking**: Timestamp values range-checked, path delay sanity verified (REQ-NF-S-002)
- **Rate Limiting**: Delay_Req transmission rate limited to prevent network flooding

### Platform Independence Requirements
- **HAL Abstraction**: All hardware access via HAL function pointers (REQ-F-005, REQ-NF-M-001)
- **Portable Build**: Compile on embedded RTOS, Linux, Windows via CMake (REQ-NF-M-002)
- **Configurable Transport**: Support Ethernet Layer 2 and UDP/IPv4 transports

---

## 9. Technology and Data Variations List

### Network Transport Variations
- **Ethernet Layer 2** (IEEE 802.3): Direct Ethernet frames, Ethertype 0x88F7
- **UDP/IPv4** (Annex C): Multicast 224.0.1.129:319/320, unicast optional
- **UDP/IPv6** (Annex D): Multicast FF0X::181, scope-configurable

### Timestamp Mechanisms
- **Hardware Timestamping**: NIC captures timestamps at PHY egress/ingress (±8ns accuracy)
- **Software Timestamping**: Kernel captures timestamps in driver (~1µs accuracy)
- **Assisted Timestamping**: Hybrid approach with hardware timestamp + software correction

### Clock Adjustment Methods
- **Frequency Adjustment**: Adjust oscillator frequency via PPM offset (typical)
- **Phase Adjustment**: Step clock phase for large offsets (>1ms)
- **Combination**: Frequency servo + periodic phase corrections

### Servo Algorithm Variations
- **PI Controller**: Proportional + Integral terms (default, REQ-F-004)
- **PID Controller**: Add Derivative term for faster transient response
- **Kalman Filter**: Statistical optimal estimator for noisy networks

---

## 10. Frequency of Occurrence

**Production Systems**: Continuous operation, 24/7/365  
**Sync Message Rate**: 1-128 Hz (default 1 Hz per IEEE 1588-2019)  
**Use Case Execution**: Continuous loop, thousands of iterations per hour  
**MTBF Target**: >10,000 hours continuous synchronization without failure

---

## 11. Open Issues

1. **Multi-Domain Support**: How to handle multiple PTP domains simultaneously? (Future enhancement)
2. **Transparent Clock Correction**: Should slave account for TC residence time? (Depends on network)
3. **Security Extensions**: IEEE 1588-2019 optional security TLVs - implement or defer? (TBD)
4. **Profile Selection**: Default profile vs. custom profile for specific applications? (Configurable)

---

## 12. Traceability Matrix

| Use Case Element | Requirement ID | Description |
|------------------|----------------|-------------|
| Master Selection (Steps 1-3) | REQ-F-002 | BMCA state machine per IEEE 1588-2019 Section 9.3 |
| Message Processing (Steps 4-7) | REQ-F-001 | Sync, Follow_Up, Delay_Req, Delay_Resp message types |
| Offset Calculation (Steps 8-9) | REQ-F-003 | E2E delay mechanism per IEEE 1588-2019 Section 11.3 |
| Servo Adjustment (Steps 10-11) | REQ-F-004 | PI controller for frequency adjustment |
| HAL Integration (Step 11) | REQ-F-005 | Network, timestamp, clock, timer HAL interfaces |
| Accuracy Target (Step 12) | REQ-NF-P-001 | Synchronization <1µs (P95), <2µs (P99) |
| Determinism (Step 10) | REQ-NF-P-002 | Zero malloc, WCET <100µs per message |
| Resource Efficiency (Exception 7c) | REQ-NF-P-003 | RAM <32KB, CPU <5%, Flash <128KB |
| Input Validation (Alt 6c) | REQ-NF-S-001 | Packet format, field range, TLV validation |
| Memory Safety (Alt 6c, 6d) | REQ-NF-S-002 | Bounds checking, safe string ops, static analysis |
| Platform Independence (Steps 10-11) | REQ-NF-M-001 | HAL abstraction for embedded/Linux/Windows |
| Build System (General) | REQ-NF-M-002 | CMake 3.20+, GCC/Clang/MSVC, multiple generators |

---

## 13. Acceptance Criteria (Gherkin Format)

```gherkin
Feature: Synchronize as Ordinary Clock Slave
  As an embedded system with IEEE 1588-2019 PTP stack
  I want to synchronize my local clock to a network grandmaster
  So that I achieve sub-microsecond timing accuracy for time-sensitive applications

  Background:
    Given PTP stack is initialized with valid HAL interfaces
    And network interface is active with multicast enabled
    And a grandmaster clock is transmitting Announce messages on domain 0

  Scenario: Successful clock synchronization from cold start
    Given system is in LISTENING state with no prior synchronization
    When an Announce message is received from grandmaster
    And BMCA selects the grandmaster as best master
    And system transitions to SLAVE state
    And Sync/Follow_Up messages are received 10 times
    And Delay_Req/Delay_Resp exchanges complete 10 times
    Then clock offset from grandmaster shall be less than 1 microsecond (P95)
    And path delay shall be stable with variance less than 100 nanoseconds
    And clock servo shall be in converged state
    And synchronization metrics shall be available for monitoring

  Scenario: Maintain synchronization accuracy over time
    Given system is synchronized with offset less than 1 microsecond
    When 3600 seconds elapse (1 hour continuous operation)
    And Sync messages are received at 1 Hz rate (3600 samples)
    Then P50 offset shall be less than 500 nanoseconds
    And P95 offset shall be less than 1 microsecond
    And P99 offset shall be less than 2 microseconds
    And no synchronization timeouts shall occur

  Scenario: Graceful handling of grandmaster timeout
    Given system is synchronized in SLAVE state
    When Announce messages stop arriving
    And 3 announce intervals elapse (9 seconds)
    Then system shall transition to UNCALIBRATED state
    And last known frequency offset shall be maintained
    And "Grandmaster timeout" event shall be logged
    And synchronization shall resume when Announce messages return

  Scenario: Deterministic performance under real-time constraints
    Given system is running on embedded RTOS with 10ms tick
    When PTP message processing is invoked
    Then worst-case execution time shall be less than 100 microseconds
    And no dynamic memory allocation shall occur
    And CPU overhead shall be less than 5% at 1 Hz sync rate

  Scenario: Platform-independent operation via HAL
    Given PTP stack compiled for target platform (RTOS/Linux/Windows)
    When HAL clock adjustment is requested with frequency offset +50 ppm
    Then hal_clock_adjust_frequency(+50ppm) shall be called
    And HAL shall return success status code
    And actual clock frequency shall change by approximately +50 ppm
```

---

## 14. Notes and Comments

### Implementation Notes
- **Servo Tuning**: PI controller gains (Kp, Ki) require tuning per deployment network characteristics (delay, jitter, packet loss)
- **Timestamp Quality**: Hardware timestamping strongly recommended for <1µs accuracy; software timestamping typically achieves 1-10µs
- **Network Requirements**: Low-jitter Ethernet fabric preferred; avoid WiFi for precision timing
- **RTOS Integration**: PTP processing can run in dedicated task or timer interrupt context

### Standards References
- **IEEE 1588-2019**: Precision Time Protocol (PTPv2.1), Sections 9.2 (state machines), 9.3 (BMCA), 11.3 (delay mechanisms), 13 (message formats)
- **ISO/IEC/IEEE 29148:2018**: Requirements engineering, Section 6.4.5 (use case specification)
- **ISO/IEC/IEEE 42010:2011**: Architecture concerns (timing accuracy, resource constraints)

### Related Use Cases
- **UC-002**: Select Best Master via BMCA (detailed master selection logic)
- **UC-003**: Measure Clock Offset (detailed offset calculation algorithm)
- **UC-004**: Adjust Clock Frequency (detailed servo algorithm)
- **STORY-001**: Integrate PTP into RTOS Application (developer perspective)
- **STORY-002**: Verify Synchronization Accuracy (system integrator perspective)

---

**Document Control**:
- **Created**: 2025-11-07
- **Last Updated**: 2025-11-07
- **Review Status**: Draft - Pending technical review
- **Approved By**: TBD (Requirements Review Board)
- **Next Review**: 2025-11-14 (Technical review with stakeholders)
