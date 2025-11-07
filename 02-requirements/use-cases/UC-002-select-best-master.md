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
    - StR-003  # STR-STD-003: Best Master Clock Algorithm
    - StR-004  # STR-STD-004: Interoperability with Commercial Devices
---

# Use Case: UC-002 - Select Best Master via BMCA

**Use Case ID**: UC-002  
**Use Case Name**: Select Best Master Clock via Best Master Clock Algorithm (BMCA)  
**Author**: Requirements Engineering Team  
**Date**: 2025-11-07  
**Version**: 1.0.0

---

## 1. Brief Description

A PTP slave device receives Announce messages from multiple potential master clocks on the network and uses the Best Master Clock Algorithm (BMCA) per IEEE 1588-2019 Section 9.3 to select the optimal grandmaster. The algorithm compares clock quality datasets (priority, class, accuracy, variance) to determine the best timing source and handles master failover when the current master becomes unavailable or a superior master appears.

**Context**: Multi-master PTP network where redundant grandmasters provide failover capability for mission-critical timing synchronization.

---

## 2. Actors

### Primary Actor
- **PTP Slave Device** (Ordinary Clock - Slave role)
  - **Description**: Device requiring timing source selection
  - **Capabilities**: BMCA dataset comparison, state machine management
  - **Responsibilities**: Select best available master, detect master quality changes, trigger failover

### Supporting Actors
- **Grandmaster Clocks** (Multiple - Primary and Backup)
  - **Description**: Timing sources advertising their quality via Announce messages
  - **Capabilities**: Transmit Announce with clock quality datasets
  - **Responsibilities**: Advertise accurate clock quality information, maintain stable timing

- **Network Infrastructure**
  - **Description**: Ethernet network distributing Announce messages
  - **Responsibilities**: Deliver Announce messages from all masters to all slaves

---

## 3. Preconditions

### System State
- ✅ **PTP Stack Initialized**: Slave device operational in LISTENING or SLAVE state
- ✅ **Network Active**: Multicast reception enabled, can receive from multiple sources
- ✅ **BMCA Enabled**: Best Master Clock Algorithm implementation active

### Environmental Conditions
- ✅ **Multiple Masters Present**: At least 2 grandmaster clocks on network
- ✅ **Announce Transmission**: Masters transmitting Announce messages (default 1/second)
- ✅ **Same PTP Domain**: All masters and slave configured for same domain number

---

## 4. Postconditions

### Success Postconditions
- ✅ **Best Master Selected**: Slave synchronized to optimal grandmaster per BMCA criteria
- ✅ **State Transition Complete**: Slave in SLAVE state with selected master
- ✅ **Announce Processing Active**: Continuous monitoring of Announce messages
- ✅ **Failover Ready**: Alternate masters tracked for rapid failover

### Failure Postconditions
- ⚠️ **No Masters Available**: Slave remains in LISTENING state
- ⚠️ **BMCA Error**: Algorithm failure logged, previous master maintained
- ⚠️ **Dataset Corruption**: Invalid Announce data detected and discarded

---

## 5. Main Success Scenario (Basic Flow)

### Phase 1: Announce Message Reception

**Step 1**: Slave receives **Announce message from Master A**
- **Message Content**: 
  - ClockQuality: {clockClass, clockAccuracy, offsetScaledLogVariance}
  - Priority1: 128 (default)
  - Priority2: 128 (default)
  - GrandmasterIdentity: 00:1B:21:AA:BB:CC:DD:EE
  - StepsRemoved: 0 (ordinary clock)
- **Action**: Parse Announce, validate format per IEEE 1588-2019 Section 13.5
- **Storage**: Store Master A dataset in announce receipt table
- **Traces To**: REQ-F-001 (Announce message processing), REQ-F-002 (BMCA)

**Step 2**: Slave receives **Announce message from Master B**
- **Message Content**:
  - ClockQuality: {clockClass, clockAccuracy, offsetScaledLogVariance}
  - Priority1: 128
  - Priority2: 128
  - GrandmasterIdentity: 00:1B:21:11:22:33:44:55
  - StepsRemoved: 0
- **Action**: Parse Announce, validate format
- **Storage**: Store Master B dataset in announce receipt table
- **Traces To**: REQ-F-001 (Announce message), REQ-F-002 (BMCA dataset)

### Phase 2: BMCA Dataset Comparison

**Step 3**: Trigger BMCA on Announce receipt timeout or new Announce
- **Trigger Conditions**:
  - New Announce received from unknown master
  - Periodic BMCA evaluation (every announce interval)
  - Current master timeout detected
- **Action**: Retrieve all valid datasets from announce receipt table
- **Traces To**: REQ-F-002 (BMCA trigger conditions)

**Step 4**: Compare datasets using BMCA algorithm (IEEE 1588-2019 Section 9.3.2)
- **Comparison Steps** (in order of precedence):
  1. **Priority1**: Lower value wins
     - Master A: 128
     - Master B: 128
     - Result: TIE → continue to next criterion
  
  2. **Clock Class**: Lower value wins (better quality)
     - Master A: 6 (primary reference, GPS)
     - Master B: 7 (primary reference, local)
     - Result: **Master A WINS** (better clock class)
  
  3. **Clock Accuracy**: (not evaluated, Master A already won)
  
  4. **Offset Scaled Log Variance**: (not evaluated)
  
  5. **Priority2**: (not evaluated)
  
  6. **Grandmaster Identity**: (not evaluated, used as tiebreaker if all equal)

- **Decision**: Master A selected as best master
- **Traces To**: REQ-F-002 (BMCA dataset comparison per Section 9.3.2)

**Step 5**: Check if selected master differs from current master
- **Current Master**: None (slave in LISTENING state) or Master B
- **Selected Master**: Master A
- **Comparison**: Masters differ, state transition required
- **Traces To**: REQ-F-002 (BMCA state machine)

### Phase 3: State Transition to New Master

**Step 6**: If slave in LISTENING state, transition to UNCALIBRATED
- **Action**: Prepare for synchronization with Master A
- **State**: LISTENING → UNCALIBRATED
- **Logging**: Log "Best master selected: Master A (00:1B:21:AA:BB:CC:DD:EE)"
- **Traces To**: REQ-F-002 (state machine transitions)

**Step 7**: Begin synchronization with Master A
- **Action**: Process Sync/Follow_Up messages from Master A only
- **Filtering**: Ignore timing messages from Master B (still process Announce)
- **State**: UNCALIBRATED → SLAVE (after first valid offset measurement)
- **Traces To**: REQ-F-002 (BMCA state transitions), UC-001 (synchronization)

**Step 8**: Continue monitoring Announce messages from all masters
- **Action**: Process Announce from Master A and Master B
- **Purpose**: Detect master quality changes or failover scenarios
- **Frequency**: Every announce interval (default 1 second)
- **Traces To**: REQ-F-002 (continuous BMCA evaluation)

---

## 6. Alternative Flows

### 6a. Master Failover (Primary Master Timeout)

**Trigger**: Current master (Master A) stops transmitting Announce messages

**Alternative Steps**:
- **6a.1**: Detect Announce timeout from Master A (3x announce interval = 3 seconds)
- **6a.2**: Remove Master A from announce receipt table (mark as unavailable)
- **6a.3**: Trigger BMCA re-evaluation with remaining masters
- **6a.4**: BMCA selects Master B as new best master
- **6a.5**: Transition SLAVE → UNCALIBRATED (flush Master A timing data)
- **6a.6**: Reset clock servo integral term (prevent stale data corruption)
- **6a.7**: Transition UNCALIBRATED → SLAVE with Master B
- **6a.8**: Log "Master failover: A → B (timeout)"
- **Return**: Resume from Step 8 (monitor Announce)
- **Traces To**: REQ-F-002 (BMCA failover), REQ-F-004 (servo reset)

### 6b. Better Master Appears (Master Quality Improvement)

**Trigger**: New master (Master C) appears with superior clock quality

**Alternative Steps**:
- **6b.1**: Receive Announce from Master C with clockClass=4 (atomic clock)
- **6b.2**: Add Master C to announce receipt table
- **6b.3**: Trigger BMCA comparison (Master A vs Master B vs Master C)
- **6b.4**: BMCA selects Master C (clockClass 4 beats Master A clockClass 6)
- **6b.5**: Transition SLAVE → UNCALIBRATED (switch from Master A to Master C)
- **6b.6**: Reset servo, flush timing data from Master A
- **6b.7**: Transition UNCALIBRATED → SLAVE with Master C
- **6b.8**: Log "Master upgrade: A → C (better clock quality)"
- **Return**: Resume from Step 8 (monitor Announce)
- **Traces To**: REQ-F-002 (BMCA comparison), REQ-S-001 (graceful transition)

### 6c. Priority Override (Manual Master Selection)

**Trigger**: Administrator configures Master B with Priority1=64 (override)

**Alternative Steps**:
- **6c.1**: Receive Announce from Master B with Priority1=64 (lower than Master A's 128)
- **6c.2**: Trigger BMCA comparison
- **6c.3**: BMCA selects Master B (Priority1=64 beats Master A Priority1=128)
- **6c.4**: Transition SLAVE → UNCALIBRATED (switch from Master A to Master B)
- **6c.5**: Log "Master override: A → B (Priority1 64 < 128)"
- **6c.6**: Synchronize with Master B
- **Return**: Resume from Step 8
- **Traces To**: REQ-F-002 (Priority1 precedence), REQ-S-004 (configuration)

### 6d. Invalid Announce Data (Dataset Validation Failure)

**Trigger**: Announce message contains invalid clock quality values

**Alternative Steps**:
- **6d.1**: Receive Announce with invalid data:
  - clockClass > 255 (out of range)
  - clockAccuracy = 0xFF but claimed clockClass=6 (inconsistent)
  - offsetScaledLogVariance out of valid range
- **6d.2**: Validate dataset per IEEE 1588-2019 Table 6 (clock class ranges)
- **6d.3**: Validation fails, reject Announce
- **6d.4**: Log "Invalid Announce from Master X: [reason]"
- **6d.5**: Do not add/update master in announce receipt table
- **6d.6**: Continue with existing master selection
- **Return**: Resume from Step 1 (await next Announce)
- **Traces To**: REQ-NF-S-001 (input validation), REQ-F-002 (BMCA data integrity)

---

## 7. Exception Flows

### 7a. No Masters Available (Cold Start)

**Trigger**: Slave boots with no active masters on network

**Exception Steps**:
- **7a.1**: Enter LISTENING state, announce receipt table empty
- **7a.2**: Wait for Announce messages (timeout: 30 seconds)
- **7a.3**: No Announce received, remain in LISTENING indefinitely
- **7a.4**: Log "No PTP masters found on domain X"
- **7a.5**: Application continues with unsynchronized local clock
- **Recovery**: Resume from Step 1 when first Announce received
- **Traces To**: REQ-F-002 (LISTENING state), REQ-NF-M-001 (graceful degradation)

### 7b. BMCA Algorithm Error (Internal Failure)

**Trigger**: BMCA comparison logic encounters unexpected condition

**Exception Steps**:
- **7b.1**: Detect BMCA internal error (e.g., null pointer, assert failure)
- **7b.2**: Log "BMCA algorithm error: [details]"
- **7b.3**: Maintain current master selection (avoid disruption)
- **7b.4**: Increment error counter, trigger diagnostic if threshold exceeded
- **7b.5**: Attempt BMCA retry on next Announce receipt
- **Recovery**: If retries successful, resume normal operation
- **Escalation**: If persistent, transition to FAULTY state
- **Traces To**: REQ-NF-S-002 (error handling), REQ-F-002 (BMCA robustness)

### 7c. Announce Storm (Too Many Masters)

**Trigger**: Excessive number of masters on network (>10)

**Exception Steps**:
- **7c.1**: Detect announce receipt table exceeds capacity (>10 entries)
- **7c.2**: Apply rate limiting: process only first 10 masters, ignore others
- **7c.3**: Log "Announce storm detected: X masters, limiting to 10"
- **7c.4**: Perform BMCA on limited set of masters
- **7c.5**: Select best master from available candidates
- **7c.6**: Continue monitoring, periodically age out stale entries
- **Traces To**: REQ-NF-P-003 (resource limits), REQ-NF-S-001 (rate limiting)

---

## 8. Special Requirements

### Performance Requirements
- **BMCA Execution Time**: Complete dataset comparison in <10ms (REQ-NF-P-002)
- **Failover Time**: Detect master timeout and select new master within 5 seconds
- **Memory Footprint**: Announce receipt table <2KB (10 masters × 200 bytes/entry)

### Accuracy Requirements
- **Dataset Comparison**: Strictly follow IEEE 1588-2019 Section 9.3.2 precedence order
- **Timestamp Handling**: Preserve nanosecond precision in clock quality comparisons
- **State Machine**: Comply with IEEE 1588-2019 Section 9.2 state transitions

### Interoperability Requirements
- **Vendor Interoperability**: Support Announce from commercial PTP devices (REQ-S-004)
- **Profile Compatibility**: Default PTP profile (Annex I) and custom profiles
- **Clock Class Ranges**: Recognize all standard clock classes per IEEE 1588-2019 Table 6

### Robustness Requirements
- **Invalid Data Handling**: Reject malformed Announce messages gracefully (REQ-NF-S-001)
- **Failover Reliability**: <1 second service interruption during master switch
- **Logging**: Comprehensive event logging for master selection changes

---

## 9. Technology and Data Variations List

### BMCA Variants
- **Default Algorithm**: IEEE 1588-2019 Section 9.3.2 (dataset comparison)
- **Alternate Algorithm**: IEEE 802.1AS gPTP modified BMCA (domain-specific)
- **Custom Profiles**: Application-specific priority schemes

### Clock Class Definitions (IEEE 1588-2019 Table 6)
- **6**: GPS/atomic clock (primary reference)
- **7**: Primary holdover (GPS lost <24h)
- **13**: Application-specific (industrial automation)
- **52**: Degraded by boundary clock
- **187**: Alternative timescale (non-PTP)
- **248**: Default (no external reference)

### Priority Schemes
- **Default**: Priority1=128, Priority2=128 (all equal)
- **Manual Override**: Administrator sets Priority1 to force master selection
- **Topology-Based**: Priority2 set by network topology (prefer local masters)

---

## 10. Frequency of Occurrence

**BMCA Evaluation**: Every announce interval (1-10 seconds typical)  
**Master Failover**: Rare in stable networks (<1 per month), frequent during maintenance  
**Master Selection**: Once per device boot, plus any failover events  
**Announce Processing**: Continuous, 1-10 Hz depending on announce interval

---

## 11. Open Issues

1. **Multi-Domain BMCA**: How to handle BMCA across multiple PTP domains simultaneously?
2. **Boundary Clock Interaction**: Should slave account for boundary clock steps removed?
3. **Security**: Authenticate Announce messages to prevent rogue master injection? (IEEE 1588 security TLV)
4. **Hysteresis**: Add stability margin to prevent master "flapping" due to marginal quality differences?

---

## 12. Traceability Matrix

| Use Case Element | Requirement ID | Description |
|------------------|----------------|-------------|
| Announce Reception (Steps 1-2) | REQ-F-001 | Announce message processing per IEEE 1588-2019 Section 13.5 |
| BMCA Algorithm (Steps 3-5) | REQ-F-002 | Dataset comparison per IEEE 1588-2019 Section 9.3.2 |
| State Transitions (Steps 6-7) | REQ-F-002 | State machine per IEEE 1588-2019 Section 9.2 |
| Master Failover (Alt 6a) | REQ-F-002 | BMCA failover mechanism |
| Servo Reset (Alt 6a, 6b) | REQ-F-004 | Reset integral term on master change |
| Input Validation (Alt 6d) | REQ-NF-S-001 | Validate Announce message fields |
| Performance (General) | REQ-NF-P-002 | BMCA execution time <10ms |
| Resource Limits (Exception 7c) | REQ-NF-P-003 | Memory footprint <2KB for announce table |
| Interoperability (General) | REQ-S-004 | Support commercial PTP devices |
| Graceful Degradation (Exception 7a) | REQ-NF-M-001 | Continue operation without master |

---

## 13. Acceptance Criteria (Gherkin Format)

```gherkin
Feature: Select Best Master Clock via BMCA
  As a PTP slave device
  I want to automatically select the optimal timing master from available sources
  So that I synchronize to the most accurate and stable clock on the network

  Background:
    Given PTP stack is initialized in LISTENING state
    And network interface can receive multicast Announce messages
    And BMCA algorithm is enabled per IEEE 1588-2019 Section 9.3

  Scenario: Select best master from multiple candidates
    Given Master A transmits Announce with clockClass=6 (GPS), Priority1=128
    And Master B transmits Announce with clockClass=7 (local), Priority1=128
    When both Announce messages are received and validated
    And BMCA comparison is triggered
    Then Master A shall be selected as best master (clockClass 6 < 7)
    And slave shall transition from LISTENING to UNCALIBRATED
    And slave shall begin synchronization with Master A only
    And "Best master selected: Master A" event shall be logged

  Scenario: Failover to backup master on primary timeout
    Given slave is synchronized to Master A (primary)
    And Master B is available as backup (tracked via Announce)
    When Master A stops transmitting Announce messages
    And 3 announce intervals elapse (9 seconds)
    Then Master A shall be removed from announce receipt table
    And BMCA shall select Master B as new best master
    And slave shall transition SLAVE → UNCALIBRATED → SLAVE with Master B
    And clock servo integral term shall be reset
    And "Master failover: A → B (timeout)" event shall be logged
    And synchronization shall resume within 5 seconds

  Scenario: Switch to superior master when better clock appears
    Given slave is synchronized to Master A (clockClass=7)
    When Master C appears with Announce clockClass=4 (atomic clock)
    And BMCA comparison is triggered
    Then Master C shall be selected (clockClass 4 < 7)
    And slave shall switch from Master A to Master C
    And "Master upgrade: A → C (better clock quality)" shall be logged
    And synchronization accuracy shall improve

  Scenario: Reject invalid Announce data
    Given slave is monitoring Announce messages
    When Announce is received with clockClass=300 (invalid, >255)
    Then Announce shall fail validation per IEEE 1588-2019 Table 6
    And invalid Announce shall be discarded
    And master shall not be added to announce receipt table
    And "Invalid Announce from Master X: clockClass out of range" shall be logged
    And current master selection shall remain unchanged

  Scenario: Handle manual priority override
    Given slave is synchronized to Master A (Priority1=128)
    When administrator configures Master B with Priority1=64
    And Announce is received from Master B with Priority1=64
    Then BMCA shall select Master B (64 < 128 takes precedence)
    And slave shall switch from Master A to Master B
    And "Master override: A → B (Priority1)" shall be logged

  Scenario: BMCA performance meets timing requirements
    Given slave has 10 masters in announce receipt table
    When BMCA comparison is triggered
    Then dataset comparison shall complete in less than 10 milliseconds
    And CPU overhead shall be less than 1% of total processing
    And no dynamic memory allocation shall occur during BMCA
```

---

## 14. Notes and Comments

### Implementation Notes
- **Announce Receipt Table**: Fixed-size array (10 entries) to avoid dynamic allocation
- **Aging Mechanism**: Remove stale entries after 3x announce interval timeout
- **Tiebreaker**: Use Grandmaster Identity (lowest MAC wins) if all quality factors equal
- **Hysteresis**: Consider adding ±1 clock class margin to prevent master flapping

### BMCA Algorithm Pseudocode
```c
BestMaster* bmca_compare(AnnounceDataset* datasets, int count) {
    BestMaster* best = &datasets[0];
    
    for (int i = 1; i < count; i++) {
        // Priority1 comparison (lower wins)
        if (datasets[i].priority1 < best->priority1) {
            best = &datasets[i];
            continue;
        } else if (datasets[i].priority1 > best->priority1) {
            continue;
        }
        
        // Clock Class comparison (lower wins)
        if (datasets[i].clockClass < best->clockClass) {
            best = &datasets[i];
            continue;
        } else if (datasets[i].clockClass > best->clockClass) {
            continue;
        }
        
        // Clock Accuracy comparison (lower wins)
        if (datasets[i].clockAccuracy < best->clockAccuracy) {
            best = &datasets[i];
            continue;
        }
        // ... continue with remaining criteria
    }
    
    return best;
}
```

### Standards References
- **IEEE 1588-2019 Section 9.2**: PTP state machine (LISTENING, SLAVE, MASTER, etc.)
- **IEEE 1588-2019 Section 9.3**: Best Master Clock Algorithm
- **IEEE 1588-2019 Section 9.3.2**: Dataset comparison algorithm (detailed precedence)
- **IEEE 1588-2019 Table 6**: Clock class definitions and ranges
- **IEEE 1588-2019 Section 13.5**: Announce message format

### Related Use Cases
- **UC-001**: Synchronize as Ordinary Clock Slave (downstream synchronization)
- **UC-003**: Measure Clock Offset (timing measurement details)
- **UC-004**: Adjust Clock Frequency (servo control)

---

**Document Control**:
- **Created**: 2025-11-07
- **Last Updated**: 2025-11-07
- **Review Status**: Draft - Pending technical review
- **Approved By**: TBD (Requirements Review Board)
- **Next Review**: 2025-11-14
