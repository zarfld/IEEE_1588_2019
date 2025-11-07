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

# Use Case: UC-003 - Measure Clock Offset

**Use Case ID**: UC-003  
**Use Case Name**: Measure Clock Offset Using Delay Request-Response Mechanism  
**Author**: Requirements Engineering Team  
**Date**: 2025-11-07  
**Version**: 1.0.0

---

## 1. Brief Description

A PTP slave device measures the offset between its local clock and the grandmaster clock using the delay request-response mechanism specified in IEEE 1588-2019 Section 11.3. The slave exchanges timestamped messages (Sync, Follow_Up, Delay_Req, Delay_Resp) with the master to calculate network propagation delay and clock offset with nanosecond precision.

**Context**: Core timing measurement primitive used by clock servo to maintain synchronization accuracy.

---

## 2. Actors

### Primary Actor
- **PTP Slave Device**
  - **Description**: Device measuring clock offset from master
  - **Capabilities**: Precision timestamping (hardware or software), offset calculation
  - **Responsibilities**: Capture timestamps T1-T4, calculate delay and offset, validate measurements

### Supporting Actors
- **Grandmaster Clock**
  - **Description**: Reference time source providing timing messages
  - **Capabilities**: Transmit Sync/Follow_Up, respond to Delay_Req with Delay_Resp
  - **Responsibilities**: Provide accurate timestamps, maintain message timing integrity

---

## 3. Preconditions

### System State
- ✅ **Slave Synchronized**: Device in SLAVE state with master selected via BMCA
- ✅ **Timestamping Active**: Hardware or software timestamping operational
- ✅ **Network Path Stable**: Bidirectional communication with master established

### Environmental Conditions
- ✅ **Master Transmitting**: Sync/Follow_Up messages arriving at configured rate (default 1 Hz)
- ✅ **Low Packet Loss**: <1% packet loss on network path
- ✅ **Symmetric Delay**: Network delay approximately equal in both directions

---

## 4. Postconditions

### Success Postconditions
- ✅ **Offset Calculated**: Clock offset from master computed with nanosecond precision
- ✅ **Delay Measured**: Path delay estimated and validated
- ✅ **Data Valid**: Timestamps pass sanity checks (no negative delay, reasonable offset)
- ✅ **Servo Updated**: Offset passed to clock servo for frequency adjustment

### Failure Postconditions
- ⚠️ **Invalid Timestamps**: Measurement discarded, error logged
- ⚠️ **Timeout**: Missing message(s), measurement incomplete
- ⚠️ **Asymmetry Detected**: Large delay asymmetry indicates network issue

---

## 5. Main Success Scenario (Basic Flow)

### Phase 1: Sync Message Reception

**Step 1**: Master transmits **Sync** message at t=T1
- **Master Action**: Capture egress timestamp T1 (time Sync leaves master PHY)
- **Message**: Sync PDU with sequenceId=N, originTimestamp may be 0 (if two-step)
- **Traces To**: REQ-F-001 (Sync message per IEEE 1588-2019 Section 13.6), REQ-F-003 (E2E mechanism)

**Step 2**: Slave receives **Sync** message at t=T2
- **Slave Action**: Capture ingress timestamp T2 (time Sync arrives at slave PHY)
- **Hardware Timestamping**: NIC captures T2 with ±8ns accuracy
- **Software Timestamping**: Kernel captures T2 with ~1µs accuracy
- **Storage**: Store (sequenceId=N, T2) for later correlation with Follow_Up
- **Traces To**: REQ-F-003 (timestamp capture), REQ-NF-P-001 (accuracy)

**Step 3**: Master transmits **Follow_Up** message immediately after Sync
- **Master Action**: Include precise T1 timestamp in Follow_Up body
- **Message**: Follow_Up PDU with sequenceId=N, preciseOriginTimestamp=T1
- **Purpose**: Provide T1 to slave (required for two-step clocks that can't timestamp Sync immediately)
- **Traces To**: REQ-F-001 (Follow_Up message per Section 13.8), REQ-F-003 (E2E mechanism)

**Step 4**: Slave receives **Follow_Up** message
- **Slave Action**: Extract preciseOriginTimestamp (T1) from Follow_Up
- **Correlation**: Match Follow_Up sequenceId=N with previously received Sync
- **Storage**: Update record (sequenceId=N, T1, T2)
- **Validation**: Verify T1 is reasonable (within ±1 second of T2 typical)
- **Traces To**: REQ-F-003 (offset calculation), REQ-NF-S-001 (validation)

### Phase 2: Delay Request-Response Exchange

**Step 5**: Slave transmits **Delay_Req** message at t=T3
- **Slave Action**: Capture egress timestamp T3 (time Delay_Req leaves slave PHY)
- **Message**: Delay_Req PDU with sequenceId=M (slave's sequence counter)
- **Timing**: Triggered by sync receipt or periodic interval (default 1 Hz)
- **Storage**: Store (sequenceId=M, T3) awaiting Delay_Resp
- **Traces To**: REQ-F-001 (Delay_Req per Section 13.10), REQ-F-003 (E2E mechanism)

**Step 6**: Master receives **Delay_Req** message at t=T4
- **Master Action**: Capture ingress timestamp T4 (time Delay_Req arrives at master PHY)
- **Processing**: Note requester identity (slave clock ID) and sequenceId=M
- **Traces To**: REQ-F-001 (message reception), REQ-F-003 (delay measurement)

**Step 7**: Master transmits **Delay_Resp** message
- **Master Action**: Include T4 timestamp in Delay_Resp body
- **Message**: Delay_Resp PDU with:
  - sequenceId=M (matches Delay_Req)
  - receiveTimestamp=T4
  - requestingPortIdentity (slave's port ID)
- **Purpose**: Provide T4 to slave to complete 4-timestamp exchange
- **Traces To**: REQ-F-001 (Delay_Resp per Section 13.11), REQ-F-003 (E2E mechanism)

**Step 8**: Slave receives **Delay_Resp** message
- **Slave Action**: Extract receiveTimestamp (T4) from Delay_Resp
- **Correlation**: Match Delay_Resp sequenceId=M with previously sent Delay_Req
- **Storage**: Complete record (sequenceId=M, T3, T4)
- **Validation**: Verify T4 is reasonable (T4 > T3, within propagation time)
- **Traces To**: REQ-F-003 (timestamp collection), REQ-NF-S-001 (validation)

### Phase 3: Offset and Delay Calculation

**Step 9**: Calculate **mean path delay** using all four timestamps
- **Formula**: `meanPathDelay = ((T2 - T1) + (T4 - T3)) / 2`
- **Interpretation**:
  - `(T2 - T1)`: Time delta including master-to-slave delay + offset
  - `(T4 - T3)`: Time delta including slave-to-master delay - offset
  - Average cancels offset, yields symmetric path delay
- **Example**:
  - T1 = 1000.000000000 s (master egress)
  - T2 = 1000.000010500 s (slave ingress) → (T2-T1) = 10,500 ns
  - T3 = 1000.500000000 s (slave egress)
  - T4 = 1000.500010000 s (master ingress) → (T4-T3) = 10,000 ns
  - meanPathDelay = (10,500 + 10,000) / 2 = 10,250 ns
- **Traces To**: REQ-F-003 (delay calculation per IEEE 1588-2019 Section 11.3), REQ-NF-P-001 (accuracy)

**Step 10**: Calculate **clock offset** from master
- **Formula**: `offset = (T2 - T1) - meanPathDelay`
- **Interpretation**: 
  - `(T2 - T1)`: Measured time difference (includes delay + offset)
  - Subtract delay to isolate offset
- **Example**:
  - offset = 10,500 ns - 10,250 ns = 250 ns
  - Slave clock is 250 ns ahead of master
- **Sign Convention**:
  - Positive offset: Slave fast relative to master
  - Negative offset: Slave slow relative to master
- **Traces To**: REQ-F-003 (offset calculation), REQ-NF-P-001 (sub-microsecond accuracy)

**Step 11**: Validate measurement quality
- **Sanity Checks**:
  - ✅ Delay non-negative: `meanPathDelay >= 0` (negative indicates timestamp error)
  - ✅ Delay reasonable: `meanPathDelay < 10 ms` (typical LAN)
  - ✅ Offset bounded: `|offset| < 1 second` (prevents huge jumps)
  - ✅ Delay variance acceptable: `|currentDelay - previousDelay| < 1 ms` (stability)
- **Failure Actions**:
  - Invalid measurement: Discard, log error, do not pass to servo
  - Marginal measurement: Flag as suspicious, apply outlier filtering
- **Traces To**: REQ-NF-S-001 (input validation), REQ-NF-S-002 (bounds checking)

**Step 12**: Pass offset to clock servo for adjustment
- **Servo Input**: offset value (nanoseconds), measurement timestamp
- **Servo Action**: Update PI controller, calculate frequency adjustment
- **Traces To**: REQ-F-004 (PI servo), UC-004 (clock adjustment)

---

## 6. Alternative Flows

### 6a. One-Step Clock (T1 Embedded in Sync)

**Trigger**: Master is one-step clock (hardware timestamps Sync immediately)

**Alternative Steps**:
- **6a.1**: Master transmits Sync with originTimestamp=T1 embedded
- **6a.2**: Slave receives Sync, extracts T1 from originTimestamp field
- **6a.3**: No Follow_Up message needed or sent
- **6a.4**: Slave has (T1, T2) immediately after Sync reception
- **Skip**: Steps 3-4 (Follow_Up exchange)
- **Return**: Continue to Step 5 (Delay_Req)
- **Traces To**: REQ-F-001 (one-step vs two-step), REQ-F-003 (E2E mechanism)

### 6b. High Network Jitter (Outlier Filtering)

**Trigger**: Path delay measurement shows high variance (jitter >1ms)

**Alternative Steps**:
- **6b.1**: Calculate current meanPathDelay = 15,000 ns
- **6b.2**: Compare to previous filtered delay = 10,000 ns
- **6b.3**: Delta = 5,000 ns exceeds threshold (1,000 ns)
- **6b.4**: Apply median filter over last 5 measurements
- **6b.5**: Use filtered delay for offset calculation instead of raw delay
- **6b.6**: Log "High jitter detected: applying outlier filter"
- **Return**: Continue to Step 10 with filtered delay
- **Traces To**: REQ-NF-P-001 (accuracy under adverse conditions), REQ-F-003 (robust offset)

### 6c. Asymmetric Network Delay

**Trigger**: Significant difference between forward and reverse path delays

**Alternative Steps**:
- **6c.1**: Calculate forward delay: `d_ms = (T2 - T1) - offset_estimate`
- **6c.2**: Calculate reverse delay: `d_sm = (T4 - T3) + offset_estimate`
- **6c.3**: Detect asymmetry: `|d_ms - d_sm| > 100 ns` (threshold)
- **6c.4**: Log "Path delay asymmetry: forward=X, reverse=Y"
- **6c.5**: Apply asymmetry correction if configured:
  - `offset_corrected = offset + (d_ms - d_sm) / 2`
- **6c.6**: Otherwise continue with standard offset (accept degraded accuracy)
- **Return**: Continue to Step 12 with corrected/uncorrected offset
- **Traces To**: REQ-NF-P-001 (accuracy), REQ-F-003 (asymmetry handling)

### 6d. Packet Loss (Incomplete Measurement)

**Trigger**: One or more timing messages lost (Sync, Follow_Up, Delay_Resp)

**Alternative Steps**:
- **6d.1**: Timeout waiting for expected message (e.g., Follow_Up after Sync)
- **6d.2**: Log "Packet loss detected: missing [message type]"
- **6d.3**: Discard incomplete measurement (cannot calculate offset without all 4 timestamps)
- **6d.4**: Increment packet loss counter
- **6d.5**: If loss rate >10%, log "Excessive packet loss, degraded synchronization"
- **6d.6**: Wait for next Sync message to retry measurement
- **Return**: Resume from Step 1 (next Sync)
- **Traces To**: REQ-NF-S-001 (error handling), REQ-F-003 (robustness)

---

## 7. Exception Flows

### 7a. Timestamp Hardware Failure

**Trigger**: Timestamp capture returns error or invalid value

**Exception Steps**:
- **7a.1**: Detect timestamp error: `hal_get_timestamp()` returns error code
- **7a.2**: Log "Timestamp hardware error: [error details]"
- **7a.3**: Attempt fallback to software timestamping if available
- **7a.4**: If fallback unavailable, discard measurement
- **7a.5**: Increment error counter, trigger alarm if persistent
- **Recovery**: Resume when timestamp hardware restored
- **Traces To**: REQ-F-005 (HAL error handling), REQ-NF-S-002 (fault tolerance)

### 7b. Clock Wrap or Discontinuity

**Trigger**: Local clock wraps or experiences discontinuity (e.g., NTP adjustment)

**Exception Steps**:
- **7b.1**: Detect clock discontinuity: `T_new < T_prev` (time went backwards)
- **7b.2**: Log "Clock discontinuity detected: time went backwards"
- **7b.3**: Discard in-flight measurements (timestamps now invalid)
- **7b.4**: Reset measurement state, clear timestamp buffers
- **7b.5**: Wait for next Sync to restart measurement cycle
- **Recovery**: Resume from Step 1 after clock stabilizes
- **Traces To**: REQ-NF-S-001 (detect invalid data), REQ-F-003 (measurement integrity)

### 7c. Huge Offset Detection (>1 Second)

**Trigger**: Calculated offset exceeds reasonable bounds

**Exception Steps**:
- **7c.1**: Calculate offset = 5,000,000,000 ns (5 seconds)
- **7c.2**: Detect anomaly: `|offset| > 1,000,000,000 ns` (1 second threshold)
- **7c.3**: Log "Huge offset detected: [value], likely timestamp error"
- **7c.4**: Discard measurement, do not pass to servo
- **7c.5**: Increment anomaly counter
- **7c.6**: If persistent (3 consecutive huge offsets), transition to FAULTY state
- **Recovery**: Requires manual intervention or clock reset
- **Traces To**: REQ-NF-S-001 (anomaly detection), REQ-NF-S-002 (prevent corruption)

---

## 8. Special Requirements

### Performance Requirements
- **Measurement Rate**: Support 1-128 Hz sync rates (REQ-NF-P-002)
- **Processing Time**: Complete offset calculation in <50µs (REQ-NF-P-002)
- **Timestamp Precision**: Hardware: ±8ns, Software: ±1µs (REQ-NF-P-001)
- **Offset Accuracy**: <1µs P95 with hardware timestamping (REQ-NF-P-001)

### Accuracy Requirements
- **Path Delay Resolution**: 1 nanosecond (64-bit timestamp arithmetic)
- **Offset Resolution**: 1 nanosecond
- **Rounding**: Banker's rounding for divide-by-2 operations

### Determinism Requirements
- **Zero Dynamic Allocation**: All buffers statically allocated (REQ-NF-P-002)
- **WCET**: Worst-case execution time <50µs for offset calculation
- **Fixed Priority**: Timestamp capture runs at highest interrupt priority

### Robustness Requirements
- **Outlier Rejection**: Discard measurements with negative delay or huge offset
- **Median Filtering**: Apply to reduce impact of occasional bad measurements
- **Graceful Degradation**: Continue with previous offset if measurement fails

---

## 9. Technology and Data Variations List

### Timestamp Mechanisms
- **Hardware Timestamping**: NIC PHY captures timestamps (±8ns typical)
- **Software Timestamping**: Kernel driver captures timestamps (~1µs typical)
- **Assisted Timestamping**: Hardware capture + software correction

### Clock Types
- **One-Step Clock**: T1 embedded in Sync message (no Follow_Up)
- **Two-Step Clock**: T1 sent in Follow_Up (allows software clocks)

### Transport Mechanisms
- **Ethernet Layer 2**: Direct MAC addressing, Ethertype 0x88F7
- **UDP/IPv4**: Multicast 224.0.1.129, ports 319/320
- **UDP/IPv6**: Multicast FF0X::181

### Delay Mechanisms
- **End-to-End (E2E)**: Delay_Req/Delay_Resp (this use case)
- **Peer-to-Peer (P2P)**: Pdelay_Req/Pdelay_Resp (alternative, IEEE 802.1AS)

---

## 10. Frequency of Occurrence

**Measurement Cycle**: 1-128 Hz (default 1 Hz)  
**Continuous Operation**: 86,400 measurements per day at 1 Hz  
**Production Lifetime**: Billions of measurements over device lifetime

---

## 11. Open Issues

1. **Asymmetry Correction**: Should slave attempt to estimate/correct path asymmetry automatically?
2. **Outlier Threshold**: What is optimal threshold for delay variance outlier detection?
3. **Transparent Clock**: How to account for residence time corrections from TCs?
4. **Leap Second**: How to handle offset calculation during leap second insertion?

---

## 12. Traceability Matrix

| Use Case Element | Requirement ID | Description |
|------------------|----------------|-------------|
| Sync/Follow_Up (Steps 1-4) | REQ-F-001 | Message processing per IEEE 1588-2019 Sections 13.6, 13.8 |
| Delay_Req/Delay_Resp (Steps 5-8) | REQ-F-001 | Message processing per Sections 13.10, 13.11 |
| Offset Calculation (Steps 9-10) | REQ-F-003 | E2E delay mechanism per Section 11.3 |
| Validation (Step 11) | REQ-NF-S-001 | Input validation (negative delay, huge offset) |
| Bounds Checking (Step 11) | REQ-NF-S-002 | Sanity checks on timestamp values |
| Servo Integration (Step 12) | REQ-F-004 | Pass offset to PI controller |
| Timestamp Accuracy (General) | REQ-NF-P-001 | Sub-microsecond precision (hardware timestamping) |
| Deterministic Timing (General) | REQ-NF-P-002 | WCET <50µs, zero malloc |
| HAL Timestamping (Steps 2, 5) | REQ-F-005 | Timestamp HAL interface |
| Outlier Filtering (Alt 6b) | REQ-NF-P-001 | Maintain accuracy under jitter |
| Error Handling (Alt 6d, Exception 7a) | REQ-NF-S-001 | Graceful handling of errors |

---

## 13. Acceptance Criteria (Gherkin Format)

```gherkin
Feature: Measure Clock Offset Using Delay Request-Response
  As a PTP slave device
  I want to measure my clock offset from the grandmaster with nanosecond precision
  So that I can adjust my clock frequency to maintain synchronization

  Background:
    Given slave is in SLAVE state synchronized to a grandmaster
    And hardware timestamping is operational with ±8ns accuracy
    And network path delay is stable at approximately 10µs

  Scenario: Successful offset measurement with two-step clock
    Given master transmits Sync message at T1=1000.000000000s
    When slave receives Sync at T2=1000.000010500s (10.5µs delay)
    And master transmits Follow_Up with preciseOriginTimestamp=T1
    And slave receives Follow_Up and extracts T1
    And slave transmits Delay_Req at T3=1000.500000000s
    And master receives Delay_Req at T4=1000.500010000s (10µs delay)
    And master transmits Delay_Resp with receiveTimestamp=T4
    And slave receives Delay_Resp and extracts T4
    Then meanPathDelay shall be calculated as ((T2-T1)+(T4-T3))/2 = 10.25µs
    And offset shall be calculated as (T2-T1) - meanPathDelay = 0.25µs
    And offset shall be passed to clock servo for frequency adjustment
    And measurement shall complete in less than 50 microseconds

  Scenario: One-step clock operation (no Follow_Up)
    Given master is one-step clock with hardware timestamping
    When master transmits Sync with T1=1000.000000000s embedded in originTimestamp
    And slave receives Sync at T2=1000.000010000s
    Then slave shall extract T1 from Sync message directly
    And no Follow_Up message shall be expected or processed
    And Delay_Req/Delay_Resp exchange shall proceed normally
    And offset calculation shall use T1 from Sync message

  Scenario: Reject measurement with negative path delay
    Given slave has captured timestamps T1, T2, T3, T4
    When meanPathDelay calculation yields -500ns (negative)
    Then measurement shall be rejected as invalid
    And "Negative path delay detected: timestamp error" shall be logged
    And measurement shall not be passed to clock servo
    And slave shall wait for next Sync to retry

  Scenario: Handle packet loss gracefully
    Given master transmits Sync at T1
    And slave receives Sync at T2
    When Follow_Up message is lost in network
    And Follow_Up timeout (100ms) expires
    Then incomplete measurement shall be discarded
    And "Packet loss: missing Follow_Up" shall be logged
    And slave shall wait for next Sync message
    And synchronization shall continue with previous offset estimate

  Scenario: Filter outliers under high network jitter
    Given path delay has been stable at 10µs ±100ns
    When current measurement yields path delay = 25µs (15µs spike)
    Then outlier detection shall flag measurement as suspicious
    And median filter shall be applied over last 5 measurements
    And filtered delay shall be used for offset calculation
    And "High jitter: outlier filter active" shall be logged

  Scenario: Detect and reject huge offset anomaly
    Given offset measurements have been stable at ±500ns
    When calculated offset = 5 seconds (huge anomaly)
    Then offset shall be rejected (exceeds 1s threshold)
    And "Huge offset detected: likely timestamp error" shall be logged
    And measurement shall not be passed to servo
    And if 3 consecutive huge offsets occur, transition to FAULTY state

  Scenario: Performance meets real-time requirements
    Given slave is processing offset measurements at 128 Hz sync rate
    When offset calculation is invoked
    Then execution time shall be less than 50 microseconds worst-case
    And no dynamic memory allocation shall occur
    And CPU overhead shall be less than 2% at 128 Hz rate
```

---

## 14. Notes and Comments

### Timestamp Arithmetic Precision
```c
// All timestamps are 64-bit nanoseconds to preserve precision
typedef uint64_t ptp_timestamp_t;  // nanoseconds since epoch

// Offset calculation (signed arithmetic)
int64_t offset_ns = (int64_t)(T2 - T1) - (int64_t)meanPathDelay;

// Path delay calculation (avoid overflow)
uint64_t forward_delta = (T2 > T1) ? (T2 - T1) : 0;  // sanity check
uint64_t reverse_delta = (T4 > T3) ? (T4 - T3) : 0;
uint64_t meanPathDelay = (forward_delta + reverse_delta) / 2;
```

### Standards References
- **IEEE 1588-2019 Section 11.3**: Delay request-response mechanism
- **IEEE 1588-2019 Section 11.3.2**: Offset calculation formulas
- **IEEE 1588-2019 Sections 13.6-13.11**: Message formats (Sync, Follow_Up, Delay_Req, Delay_Resp)
- **IEEE 1588-2019 Section 7.3.4**: Timestamp point definitions

### Related Use Cases
- **UC-001**: Synchronize as Ordinary Clock Slave (overall synchronization flow)
- **UC-004**: Adjust Clock Frequency (downstream servo processing)

---

**Document Control**:
- **Created**: 2025-11-07
- **Last Updated**: 2025-11-07
- **Review Status**: Draft - Pending technical review
- **Approved By**: TBD (Requirements Review Board)
- **Next Review**: 2025-11-14
