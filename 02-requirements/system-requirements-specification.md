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
    # Note: Schema enforces StR-### pattern (simple numeric)
    # Mapping to Phase 01 STR-<CAT>-### requirements documented in Section 6.1
    - StR-001  # Maps to STR-STD-001 (IEEE 1588-2019 Protocol Compliance)
    - StR-002  # Maps to STR-STD-002 (Message Format Correctness)
    - StR-003  # Maps to STR-STD-003 (Best Master Clock Algorithm)
    - StR-004  # Maps to STR-STD-004 (Interoperability)
    - StR-005  # Maps to STR-PERF-001 (Synchronization Accuracy)
    - StR-006  # Maps to STR-PERF-002 (Timing Determinism)
    - StR-007  # Maps to STR-PERF-003 (Clock Servo Performance)
    - StR-008  # Maps to STR-PERF-004 (Path Delay Measurement)
    - StR-009  # Maps to STR-PERF-005 (Resource Efficiency)
    - StR-010  # Maps to STR-PORT-001 (Hardware Abstraction Layer)
    - StR-011  # Maps to STR-PORT-002 (Reference HAL Implementations)
    - StR-012  # Maps to STR-PORT-003 (Platform Independence)
    - StR-013  # Maps to STR-PORT-004 (Build System Portability)
    - StR-014  # Maps to STR-SEC-001 (Input Validation)
    - StR-015  # Maps to STR-SEC-002 (Memory Safety)
    - StR-016  # Maps to STR-SEC-003 (Security Documentation)
    - StR-017  # Maps to STR-USE-001 (API Usability)
    - StR-018  # Maps to STR-USE-002 (Documentation Quality)
    - StR-019  # Maps to STR-USE-003 (Examples and Tutorials)
    - StR-020  # Maps to STR-USE-004 (Diagnostic Capabilities)
    - StR-021  # Maps to STR-MAINT-001 (Coding Standards)
    - StR-022  # Maps to STR-MAINT-002 (Test Coverage)
    - StR-023  # Maps to STR-MAINT-003 (Continuous Integration)
    - StR-024  # Maps to STR-MAINT-004 (Version Control)
---

# System Requirements Specification (SyRS)

## IEEE 1588-2019 PTP Open-Source Implementation

**Document ID**: SYS-REQ-001  
**Version**: 1.0.0  
**Date**: 2025-11-07  
**Status**: Draft for Technical Review  
**Compliance**: ISO/IEC/IEEE 29148:2018 - Section 5.3 (System Requirements)

---

## 1. Introduction

### 1.1 Purpose

This System Requirements Specification (SyRS) transforms stakeholder requirements from Phase 01 into detailed, testable system-level requirements that define WHAT the IEEE 1588-2019 PTP implementation must do, without prescribing HOW it will be implemented.

This document serves as:

- **Contract** between stakeholders and development team
- **Basis** for architecture design (Phase 03)
- **Foundation** for verification and validation (Phase 07)
- **Traceability anchor** linking stakeholder needs to implementation

### 1.2 Scope

**Covered in This Specification** (MVP v1.0.0):

- Core IEEE 1588-2019 protocol implementation
  - Message formats and serialization (Sync, Delay_Req, Follow_Up, Delay_Resp, Announce)
  - Best Master Clock Algorithm (BMCA)
  - Clock offset calculation and synchronization
  - Clock servo (PI controller)
- Hardware abstraction layer (HAL) interfaces
- Performance requirements (accuracy, determinism, resource efficiency)
- Security requirements (input validation, memory safety)
- Portability requirements (cross-platform, no OS dependencies)

**Explicitly Out of Scope** (Post-MVP):

- Advanced PTP features (Transparent Clock, Multi-Domain, Management Protocol)
- Cross-standards integration (IEEE 802.1AS gPTP, IEEE 1722 AVTP, AES67)
- Security authentication mechanisms (IEEE 1588-2019 Annex P)
- Graphical user interfaces and configuration tools
- Commercial support and consulting services

### 1.3 Definitions, Acronyms, Abbreviations

| Term | Definition |
|------|------------|
| **BMCA** | Best Master Clock Algorithm - algorithm for selecting the best master clock in a PTP domain |
| **E2E** | End-to-End delay mechanism - uses Delay_Req/Delay_Resp messages |
| **GM** | Grandmaster - highest-quality clock in a PTP domain |
| **HAL** | Hardware Abstraction Layer - interface isolating hardware-specific code |
| **OC** | Ordinary Clock - PTP device with single network port |
| **P2P** | Peer-to-Peer delay mechanism - uses Pdelay_Req/Pdelay_Resp messages |
| **PTP** | Precision Time Protocol (IEEE 1588-2019) |
| **SyRS** | System Requirements Specification (this document) |
| **WCET** | Worst-Case Execution Time - maximum time for code execution |

### 1.4 References

- **[IEEE-1588-2019]**: IEEE Standard for a Precision Clock Synchronization Protocol
- **[ISO-29148]**: ISO/IEC/IEEE 29148:2018 - Requirements engineering
- **[STR-SPEC]**: `01-stakeholder-requirements/stakeholder-requirements-spec.md` - Stakeholder Requirements Specification
- **[REQ-ELICIT]**: `02-requirements/REQUIREMENTS-ELICITATION-SESSION-2025-11-07.md` - Requirements elicitation session report
- **[COPILOT-INST]**: `.github/instructions/copilot-instructions.md` - AI coding guidelines (hardware-agnostic principle)

### 1.5 Overview

This SyRS is organized as follows:

- **Section 2**: Functional Requirements (REQ-F-###)
- **Section 3**: Non-Functional Requirements (REQ-NF-[CAT]-###)
- **Section 4**: System Interfaces
- **Section 5**: Constraints
- **Section 6**: Traceability Matrix

Each requirement includes:

- **ID**: Unique identifier (REQ-F-### or REQ-NF-[CAT]-###)
- **Trace to**: Stakeholder requirement(s) from Phase 01
- **Priority**: P0 (Critical), P1 (High), P2 (Medium), P3 (Low)
- **Description**: Clear statement of what the system shall do
- **Rationale**: Why this requirement exists
- **Acceptance Criteria**: Testable conditions in Given-When-Then format
- **Dependencies**: Other requirements or external dependencies

---

## 2. Functional Requirements

### 2.1 PTP Message Handling

#### REQ-F-001: IEEE 1588-2019 Message Type Support

**Trace to**: STR-STD-001, STR-STD-002  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Standards Compliance

**Description**: The system SHALL implement parsing, validation, and serialization for all mandatory IEEE 1588-2019 message types used in ordinary clock operation.

**Rationale**: Message handling is the foundation of PTP protocol implementation. Without correct message parsing and serialization, no synchronization is possible.

**Mandatory Message Types** (IEEE 1588-2019 Section 13):

1. **Sync** (0x0) - Carries master clock timestamp
2. **Delay_Req** (0x1) - Slave requests delay measurement
3. **Follow_Up** (0x8) - Provides precise Sync egress timestamp
4. **Delay_Resp** (0x9) - Provides Delay_Req ingress timestamp
5. **Announce** (0xB) - Carries master clock quality for BMCA
6. **Signaling** (0xC) - Optional extensions and negotiations
7. **Management** (0xD) - Configuration and monitoring (Phase 01B)

**Functional Behavior**:

- **Parsing**: Deserialize network packets (network byte order) into C structures
- **Validation**: Check message header, flags, TLVs per IEEE 1588-2019
- **Serialization**: Convert C structures to network packets (network byte order)
- **Timestamping**: Capture ingress/egress timestamps via HAL

**Acceptance Criteria**:

```gherkin
Scenario: Parse valid Sync message
  Given a raw Sync message packet (44 bytes) conforming to IEEE 1588-2019 Table 26
  And messageType field = 0x0 (SYNC)
  And versionPTP field = 0x02 (IEEE 1588-2019)
  When parsed by ptp_parse_sync_message()
  Then return PTP_SUCCESS
  And populate ptp_sync_message_t structure with correct field values
  And sequenceId field matches packet bytes 30-31 (network byte order)

Scenario: Reject malformed message
  Given a packet with invalid messageType field = 0xFF (undefined)
  When parsed by ptp_parse_message()
  Then return PTP_ERROR_INVALID_MESSAGE_TYPE
  And log error message to diagnostics interface
  And do not modify output structure

Scenario: Serialize Delay_Req message
  Given a ptp_delay_req_message_t structure with sequenceId = 42
  When serialized by ptp_serialize_delay_req()
  Then produce 44-byte packet conforming to IEEE 1588-2019 Table 30
  And bytes 30-31 SHALL equal 0x00, 0x2A (sequenceId in network byte order)
  And Wireshark PTP dissector SHALL parse packet without errors
```

**Dependencies**:

- HAL network interface for packet send/receive
- HAL timestamp interface for ingress/egress capture

**Risks**:

- Message format correctness is critical for interoperability
- Wireshark dissector used for validation (requires test infrastructure)

---

#### REQ-F-002: Best Master Clock Algorithm (BMCA) and Passive Tie Handling

**Trace to**: STR-STD-003  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Standards Compliance

**Description**: The system SHALL implement the Best Master Clock Algorithm (BMCA) dataset comparison and state decision algorithms per IEEE 1588-2019 Section 9.3, INCLUDING passive (RS_PASSIVE) role recommendation when a foreign candidate's priority vector is EXACTLY equal to the local candidate (true tie). Self-comparison (local vs. itself) SHALL NOT constitute a tie.

**Rationale**: BMCA is mandatory for automatic master selection in multi-master networks. Without BMCA, manual configuration is required, limiting usability.

**Functional Behavior**:

1. **Announce Reception**: Receive and validate Announce messages from multiple sources
1. **Dataset Comparison** (IEEE 1588-2019 Figure 27):
   - Priority1 (lowest wins)
   - Clock Class (lowest wins)
   - Clock Accuracy (best wins)
   - Offset Scaled Log Variance (lowest wins)
   - Priority2 (lowest wins)
   - Clock Identity (lowest wins, tiebreaker)
   - Steps Removed (lowest wins)
1. **State Decision** (IEEE 1588-2019 Figure 26):

   - Determine if local clock is MASTER, SLAVE, PASSIVE, or LISTENING
   - Recommend PASSIVE only when at least one foreign Announce-derived priority vector matches the local priority vector field-for-field through all BMCA comparison steps (priority1, clockClass, clockAccuracy, offsetScaledLogVariance, priority2, clockIdentity, stepsRemoved). In that case neither clock exerts master role; local port enters PASSIVE.

1. **Announce Timeout**: Detect master loss via announce receipt timeout

**BMCA State Machine**:

```text
INITIALIZING → LISTENING → UNCALIBRATED → SLAVE
              ↓                ↓
            MASTER          PASSIVE
```

**Acceptance Criteria**:

```gherkin
Scenario: Select best master from multiple sources
  Given three PTP clocks transmitting Announce messages:
    | Clock ID                | Priority1 | Class | Accuracy | Priority2 |
    | 00:11:22:FF:FE:33:44:55 | 128       | 248   | 0x21     | 128       |
    | AA:BB:CC:FF:FE:DD:EE:FF | 64        | 248   | 0x21     | 128       |
    | FF:EE:DD:FF:FE:CC:BB:AA | 200       | 248   | 0x21     | 128       |
  When BMCA executes dataset comparison
  Then select clock AA:BB:CC:FF:FE:DD:EE:FF as best master (priority1 = 64)
  And transition from LISTENING to UNCALIBRATED state
  And emit bmca_state_changed callback with (old_state=LISTENING, new_state=UNCALIBRATED)

Scenario: Handle master timeout
  Given system synchronized to master AA:BB:CC:FF:FE:DD:EE:FF for 600 seconds
  And announce interval = 2 seconds
  And announce receipt timeout = 6 intervals (12 seconds)
  When no Announce received from master for 12 seconds
  Then transition to LISTENING state
  And restart BMCA with empty candidate list
  And emit bmca_state_changed callback with (old_state=SLAVE, new_state=LISTENING)
  And stop adjusting clock frequency (freeze servo)

Scenario: Ignore inferior masters

Scenario: Recommend PASSIVE on true tie
  Given local priority vector:
    | priority1 | clockClass | clockAccuracy | variance | priority2 | identity (u64) | stepsRemoved |
    | 64        | 248        | 0x21          |   5000   | 128       | 0x0011223344556677 | 0 |
  And a foreign Announce producing identical vector except different port_number
  When BMCA executes dataset comparison
  Then local best and foreign best SHALL compare Equal across all ordered fields
  And BMCA SHALL recommend PASSIVE (RS_PASSIVE)
  And state transition callback SHALL emit (old_state=LISTENING, new_state=PASSIVE)
  And metric BMCA_PassiveWins SHALL increment by 1

Scenario: Do NOT recommend PASSIVE on self-only equality
  Given only the local priority vector present (no foreign masters)
  When BMCA executes dataset comparison
  Then best index = local (0)
  And no foreign candidate equals local (foreign list empty)
  And BMCA SHALL NOT recommend PASSIVE
  And BMCA SHALL proceed to MASTER or remain LISTENING per existing rules

Scenario: Foreign better but identical except stepsRemoved
  Given foreign candidate identical to local except stepsRemoved=1 (lower is better)
  When BMCA executes dataset comparison
  Then comparison SHALL determine foreign is better (due to stepsRemoved)
  And BMCA SHALL recommend SLAVE (RS_SLAVE), NOT PASSIVE
  And BMCA_PassiveWins SHALL NOT increment

Scenario: Ignore inferior master compared to current
  Given current master with priority1 = 64
  And new foreign Announce received with priority1 = 128 (inferior)
  When BMCA executes dataset comparison
  Then current master remains selected
  And state remains SLAVE
  And no state transition callback emitted
  And BMCA_ForeignWins SHALL NOT increment
  And BMCA_PassiveWins SHALL NOT increment
```

**Dependencies**:

- REQ-F-001 (Announce message parsing)
- HAL timer interface for announce timeout detection

**Risks**:

- Tie detection mis-implementation could incorrectly suppress master selection leading to degraded synchronization.
- Metrics omission (BMCA_PassiveWins) reduces observability of tie scenarios.
- BMCA state machine has complex edge cases (e.g., simultaneous master loss and new master appearance)
- Announce timeout must be configurable (2-10 intervals typical)

---

#### REQ-F-003: Clock Offset Calculation

**Trace to**: STR-STD-001, STR-PERF-001  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Time Synchronization

**Description**: The system SHALL calculate clock offset from master using End-to-End (E2E) delay mechanism with Sync/Follow_Up and Delay_Req/Delay_Resp message exchanges.

**Rationale**: Offset calculation is the core of PTP synchronization. Sub-microsecond accuracy depends on precise timestamp capture and arithmetic.

**Functional Behavior** (IEEE 1588-2019 Section 11.3):

1. **Capture Timestamps**:
   - **T1**: Sync egress timestamp at master (from Follow_Up message)
   - **T2**: Sync ingress timestamp at slave (hardware timestamp)
   - **T3**: Delay_Req egress timestamp at slave (hardware timestamp)
   - **T4**: Delay_Req ingress timestamp at master (from Delay_Resp message)

1. **Calculate Offset**:

  ```c
   offset_from_master = ((T2 - T1) - (T4 - T3)) / 2;
   ```

1. **Calculate Path Delay**:

  ```c
   mean_path_delay = ((T2 - T1) + (T4 - T3)) / 2;
   ```

1. **Outlier Detection**:
   - Discard samples with |offset| > 1 second (likely timestamp error)
   - Log warning for path delay changes >10% (network instability)

**Acceptance Criteria**:

```gherkin
Scenario: Calculate offset from message timestamps
  Given master sends Sync at T1 = 1000000000 ns
  And slave receives Sync at T2 = 1000000500 ns (hardware timestamp)
  And slave sends Delay_Req at T3 = 1000001000 ns (hardware timestamp)
  And master receives Delay_Req at T4 = 1000001450 ns
  When offset calculation executes via ptp_calculate_offset()
  Then offset_from_master SHALL equal ((1000000500 - 1000000000) - (1000001450 - 1000001000)) / 2
    = (500 - 450) / 2 = 25 ns
  And mean_path_delay SHALL equal ((1000000500 - 1000000000) + (1000001450 - 1000001000)) / 2
    = (500 + 450) / 2 = 475 ns
  And offset_valid flag SHALL be TRUE

Scenario: Detect timestamp outlier
  Given master sends Sync at T1 = 1000000000 ns
  And slave receives Sync at T2 = 2000000000 ns (1-second jump, likely error)
  When offset calculation executes
  Then offset_valid flag SHALL be FALSE
  And log warning "Offset outlier detected: |offset| = 1000000000 ns exceeds threshold"
  And do not update clock servo with invalid sample

Scenario: Handle missing Follow_Up
  Given slave receives Sync message at T2 = 1000000500 ns
  And Follow_Up timeout expires (2 * sync_interval) without Follow_Up reception
  When offset calculation executes
  Then offset_valid flag SHALL be FALSE
  And discard incomplete Sync/Follow_Up pair
  And wait for next Sync message
```

**Dependencies**:

- REQ-F-001 (message parsing: Sync, Follow_Up, Delay_Req, Delay_Resp)
- HAL timestamp interface for T2 and T3 capture (hardware timestamping)

**Risks**:

- Hardware timestamp accuracy limits overall synchronization accuracy
- Software timestamps (if hardware unavailable) add 10-100µs jitter

---

### 2.2 Clock Synchronization

#### REQ-F-004: PI Controller Clock Adjustment

**Trace to**: STR-PERF-003  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Clock Servo

**Description**: The system SHALL implement a Proportional-Integral (PI) controller to adjust slave clock frequency based on measured offset from master.

**Rationale**: PI controller provides smooth, stable clock adjustments with fast convergence and minimal overshoot. Alternative algorithms (PD, PID) add complexity without significant benefit for PTP.

**Functional Behavior**:

1. **Input**: Clock offset from REQ-F-003 (offset_from_master_ns)
2. **Proportional Term**: `P = Kp * offset`
3. **Integral Term**: `I += Ki * offset * dt` (with anti-windup)
4. **Output**: `frequency_adjustment_ppb = P + I`
5. **Clamp**: Limit output to hardware range (typically ±100 ppm = ±100000 ppb)
6. **Apply**: Call HAL `clock_interface_t::adjust_frequency(frequency_adjustment_ppb)`

**PI Controller Parameters** (tunable):

- **Kp** (Proportional gain): 0.0 - 1.0 (typical: 0.7)
- **Ki** (Integral gain): 0.0 - 0.01 (typical: 0.001)
- **Anti-windup limit**: ±10000 (prevent integral saturation)

**Convergence Behavior**:

- **Phase 1** (0-10s): Proportional term dominates, rapid offset reduction
- **Phase 2** (10-60s): Integral term compensates for frequency offset
- **Phase 3** (60s+): Steady-state, offset <1µs, servo adjustments minimal

**Acceptance Criteria**:

```gherkin
Scenario: Converge to master clock
  Given initial offset = 10000 ns (10 µs)
  And PI controller with Kp = 0.7, Ki = 0.001
  And sync interval = 1 Hz (1 second between samples)
  When servo runs for 60 seconds
  Then final offset SHALL be <1000 ns (1 µs) for 95% of samples
  And frequency adjustment SHALL stabilize (variance <10 ppb over 10 seconds)
  And integral term SHALL not exceed ±5000 (anti-windup active)

Scenario: Handle offset outlier gracefully
  Given servo converged with offset ~100 ns
  And sudden offset spike to 500000 ns (500 µs, likely measurement error)
  When servo processes outlier sample
  Then discard outlier (offset > 3 * standard_deviation)
  And do not update frequency adjustment
  And log warning "Servo: outlier discarded, offset = 500000 ns"
  And servo remains stable on next valid sample

Scenario: Clamp frequency adjustment to hardware limits
  Given slave clock with ±100 ppm frequency adjustment range (±100000 ppb)
  And PI controller calculates adjustment = 150000 ppb (exceeds limit)
  When adjustment applied via HAL
  Then actual adjustment SHALL be clamped to 100000 ppb
  And log warning "Servo: frequency adjustment clamped to hardware limit"
  And integral term SHALL not continue growing (anti-windup)
```

**Dependencies**:

- REQ-F-003 (clock offset calculation provides servo input)
- HAL clock interface for frequency adjustment

**Risks**:

- PI tuning is hardware-dependent (oscillator quality, network jitter)
- Poor tuning causes slow convergence or instability (oscillation)

---

### 2.3 Hardware Abstraction

#### REQ-F-005: Hardware Abstraction Layer (HAL) Interfaces

**Trace to**: STR-PORT-001  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Portability

**Description**: The system SHALL access all hardware functionality exclusively through defined HAL interfaces using C function pointers, ensuring zero direct hardware or OS dependencies in the PTP core.

**Rationale**: Hardware abstraction is fundamental to cross-platform portability. HAL enables testing with mock implementations, deployment on diverse platforms (embedded RTOS, Linux, Windows), and hardware vendor integration.

**Required HAL Interfaces**:

1. **Network Interface** (`network_interface_t`):
   - Send/receive Ethernet frames
   - Get MAC address
   - Enable/disable multicast addresses

2. **Timestamp Interface** (`timestamp_interface_t`):
   - Capture hardware timestamps for TX/RX packets
   - Get current time in nanoseconds
   - Timestamp resolution metadata

3. **Clock Interface** (`clock_interface_t`):
   - Adjust clock frequency (ppb)
   - Step clock time (large offset correction)
   - Get current time

4. **Timer Interface** (`timer_interface_t`):
   - Schedule periodic callbacks
   - One-shot timers for timeouts
   - Get elapsed time

**HAL Interface Definition** (excerpt):

```c
// Network Interface
typedef struct {
    int (*send_packet)(const void* data, size_t length);
    int (*receive_packet)(void* buffer, size_t* length, uint32_t timeout_ms);
    int (*get_mac_address)(uint8_t mac[6]);
    int (*enable_multicast)(const uint8_t multicast_mac[6]);
} network_interface_t;

// Timestamp Interface
typedef struct {
    uint64_t (*get_time_ns)(void);
    int (*capture_tx_timestamp)(uint16_t sequence_id, uint64_t* timestamp_ns);
    int (*capture_rx_timestamp)(const void* packet, uint64_t* timestamp_ns);
    uint32_t timestamp_resolution_ns; // e.g., 8 ns for Intel I210
} timestamp_interface_t;

// Clock Interface
typedef struct {
    int (*adjust_frequency)(int32_t frequency_ppb);
    int (*step_time)(int64_t offset_ns);
    uint64_t (*get_current_time_ns)(void);
} clock_interface_t;

// Timer Interface
typedef void (*timer_callback_t)(void* user_data);
typedef struct {
    int (*schedule_periodic)(uint32_t interval_us, timer_callback_t callback, void* user_data);
    int (*schedule_oneshot)(uint32_t delay_us, timer_callback_t callback, void* user_data);
    uint32_t (*get_elapsed_us)(void);
} timer_interface_t;
```

**Acceptance Criteria**:

```gherkin
Scenario: Compile without hardware headers
  Given PTP core implementation source code in src/ptp_core/
  When compiled with CMake option -DHARDWARE_ABSTRACTION=1
  Then no #include of vendor-specific headers (e.g., <intel_hal.h>, <stm32_hal.h>)
  And no direct hardware register access (e.g., MMIO writes)
  And no OS-specific calls (e.g., Windows API, Linux syscalls)
  And all hardware operations via HAL function pointers

Scenario: Runtime HAL injection
  Given PTP library initialized with ptp_init(NULL, NULL, NULL, NULL) // NULL HALs
  When HAL interfaces registered via ptp_register_network_hal(), ptp_register_timestamp_hal(), etc.
  Then all hardware operations use registered HAL function pointers
  And PTP library operates correctly on mock HAL (unit tests)

Scenario: Mock HAL for unit testing
  Given mock network HAL that records sent packets
  And mock timestamp HAL that returns synthetic timestamps
  When running unit test test_ptp_sync_message_handling()
  Then test executes without real hardware
  And mock HAL verifies correct packet format and timing
  And unit test passes on CI/CD server (no physical NIC required)
```

**Dependencies**:

- Architecture design ADR-001 (Hardware Abstraction Layer)
- Reference HAL implementations (STR-PORT-002)

**Risks**:

- HAL abstraction adds indirection (function pointer overhead ~2-5 CPU cycles)
- Complex HALs may leak hardware details (e.g., timestamp buffer management)

---

### 2.4 System Behavior

#### REQ-S-001: Graceful BMCA State Transitions

**Trace to**: STR-STD-003, UC-002, ADR-002  
**Priority**: P1 (High)  
**Category**: System Behavior

**Description**: The system SHALL perform graceful state transitions during BMCA re-evaluation (e.g., master changeover), ensuring no abrupt clock steps and no loss of synchronization beyond acceptable bounds.

**Rationale**: BMCA changes may occur due to network dynamics or master failure. Graceful transitions minimize service disruption and time discontinuities.

**Acceptance Criteria**:

```gherkin
Scenario: Master changeover without time discontinuity
  Given the node is in SLAVE state synchronized to Master A
  And a superior Master B begins transmitting Announce messages
  When BMCA selects Master B and state transitions occur
  Then the node SHALL avoid phase steps > 1 µs
  And servo adjustments SHALL remain within hardware limits
  And log "bmca_transition" with old/new master identities

Scenario: Graceful demotion from MASTER to SLAVE
  Given the node is MASTER and receives superior Announce
  When BMCA demotes the node to SLAVE
  Then transmission of Sync SHALL cease within 2 announce intervals
  And the node SHALL enter LISTENING/UNCALIBRATED before SLAVE
```

**Dependencies**: REQ-F-002 (BMCA), REQ-F-004 (Servo)

---

#### REQ-S-002: Fault Recovery and Graceful Degradation

**Trace to**: STR-PERF-001, REQ-PPS-004  
**Priority**: P1 (High)  
**Category**: System Behavior

**Description**: The system SHALL detect and recover gracefully from fault conditions including loss of timing reference (GPS PPS signal loss, master clock failure), degrading to fallback timing modes without crashes or synchronization hangs.

**Rationale**: Real-time systems require continuous operation despite component failures. Graceful degradation maintains service at reduced accuracy rather than complete failure.

**Acceptance Criteria**:

```gherkin
Scenario: GPS PPS signal loss with NMEA fallback
  Given the node is synchronized using GPS PPS (sub-microsecond accuracy)
  When GPS PPS signal is lost or becomes invalid
  Then the system SHALL detect loss within 2 seconds
  And fall back to NMEA-only time synchronization (10ms accuracy)
  And continue PTP operation with degraded timing accuracy
  And log "timing_reference_degraded" event
  And automatically re-enable PPS if signal recovers

Scenario: Master clock failure recovery
  Given the node is SLAVE synchronized to a PTP master
  When master stops transmitting Sync messages (ANNOUNCE_RECEIPT_TIMEOUT)
  Then BMCA SHALL select next best available master
  And state transition SHALL occur per REQ-S-001
  And synchronization SHALL resume within 4 announce intervals
  And log "master_timeout_recovery" with old/new master identities
```

**Dependencies**: REQ-S-001 (Graceful Transitions), REQ-PPS-004 (PPS Fallback), REQ-F-002 (BMCA)

---

#### REQ-S-004: Interoperability and Configuration Compatibility

**Trace to**: STR-STD-004, UC-002, ADR-003  
**Priority**: P1 (High)  
**Category**: System Behavior

**Description**: The system SHALL interoperate with commercial IEEE 1588 devices and support configuration parameters required for typical deployments (e.g., Priority1, domainNumber, announce/sync intervals) via management/configuration APIs.

**Rationale**: Interoperability ensures adoption in mixed-vendor environments; configuration compatibility enables deployment without vendor lock-in.

**Acceptance Criteria**:

```gherkin
Scenario: Accept Announce from commercial devices
  Given a commercial PTP Grandmaster broadcasting Announce
  When receiving and validating Announce
  Then fields SHALL be parsed per REQ-F-001
  And interoperability SHALL be documented for vendor devices used in tests

Scenario: Configure BMCA parameters
  Given configuration API access
  When setting Priority1, domainNumber, and logAnnounceInterval
  Then values SHALL take effect within one announce interval
  And BMCA decisions SHALL reflect updated parameters
```

**Dependencies**: REQ-F-001 (message parsing), REQ-F-002 (BMCA)

---

## 3. Non-Functional Requirements

### 3.1 Performance

#### REQ-NF-P-001: Synchronization Accuracy

**Trace to**: STR-PERF-001  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Performance

**Description**: The system SHALL achieve clock offset <1 microsecond (µs) from master on capable hardware with hardware timestamping support.

**Metric**: Clock offset (nanoseconds)  
**Target Values**:

- **P50 (Median)**: <500 ns (0.5 µs)
- **P95 (95th percentile)**: <1000 ns (1 µs)
- **P99 (99th percentile)**: <2000 ns (2 µs)
- **P99.9 (99.9th percentile)**: <5000 ns (5 µs)

**Test Environment**:

- **Master**: GPS-disciplined Grandmaster (<50ns accuracy to UTC)
- **Slave Hardware**: Intel I210 NIC with hardware timestamping (8ns resolution)
- **Network**: Gigabit Ethernet LAN, <1ms latency
- **Duration**: 10 minutes minimum (600 samples at 1 Hz sync rate)

**Measurement Method**:

```python
# Collect offset samples
offset_samples = []
for i in range(600):  # 10 minutes at 1 Hz
    offset_ns = ptp_get_offset_from_master()
    offset_samples.append(abs(offset_ns))
    time.sleep(1)

# Calculate percentiles
p50 = numpy.percentile(offset_samples, 50)
p95 = numpy.percentile(offset_samples, 95)
p99 = numpy.percentile(offset_samples, 99)

# Verify targets
assert p50 < 500, f"P50 offset {p50}ns exceeds 500ns target"
assert p95 < 1000, f"P95 offset {p95}ns exceeds 1000ns target"
assert p99 < 2000, f"P99 offset {p99}ns exceeds 2000ns target"
```

**Acceptance Criteria**:

```gherkin
Scenario: Achieve sub-microsecond accuracy on capable hardware
  Given PTP Grandmaster with GPS-disciplined oscillator (<50ns UTC accuracy)
  And slave clock with Intel I210 NIC (hardware timestamping, 8ns resolution)
  And network latency <1ms (LAN environment, no WAN)
  When synchronized for 10 minutes at 1 Hz sync rate
  Then 50% of offset samples SHALL be <500 ns
  And 95% of offset samples SHALL be <1000 ns
  And 99% of offset samples SHALL be <2000 ns
  And synchronization SHALL remain stable (no >10µs jumps)

Scenario: Document accuracy limitations on software timestamps
  Given slave clock with software timestamping (no hardware support)
  When synchronized for 10 minutes at 1 Hz sync rate
  Then document achieved accuracy (typically 10-100 µs)
  And log warning "Software timestamps: accuracy limited to ~50µs"
  And do not fail requirement (hardware limitation documented)
```

**Dependencies**:

- REQ-F-003 (offset calculation)
- REQ-F-004 (servo convergence)
- Hardware timestamp support in HAL

**Risks**:

- Accuracy is hardware-dependent (NIC capability, oscillator quality)
- Network jitter can degrade accuracy (switch latency, congestion)
- Software timestamps cannot meet <1µs target (document limitation)

---

#### REQ-NF-P-002: Deterministic Timing

**Trace to**: STR-PERF-002  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Performance / Real-Time

**Description**: All time-critical code paths SHALL have bounded worst-case execution time (WCET) with no dynamic memory allocation in critical sections.

**Rationale**: Real-time systems require deterministic behavior. Dynamic allocation (malloc) is non-deterministic and can cause priority inversion, heap fragmentation, or unbounded latency.

**Time-Critical Code Paths**:

1. **Message Parsing**: `ptp_parse_sync_message()`, `ptp_parse_announce_message()`, etc.
2. **Offset Calculation**: `ptp_calculate_offset()`
3. **Servo Update**: `ptp_servo_update()`
4. **BMCA Execution**: `ptp_bmca_execute()`

**WCET Targets** (ARM Cortex-M7 @ 400 MHz):

- **Message Parsing**: <10 µs per message
- **Offset Calculation**: <5 µs
- **Servo Update**: <15 µs
- **BMCA Execution**: <100 µs (worst-case 256 announce sources)

**Memory Allocation Policy**:

- **Static Allocation**: Pre-allocate all buffers at initialization
- **Stack Allocation**: Small temporary variables only
- **NO Heap Allocation**: Zero calls to malloc/calloc/realloc in critical paths

**Acceptance Criteria**:

```gherkin
Scenario: No dynamic allocation in critical path
  Given system compiled with -DDEBUG_ALLOC_TRACE (allocation tracking)
  When processing 1000 PTP messages (Sync/Follow_Up/Delay_Req/Delay_Resp)
  Then zero calls to malloc(), calloc(), realloc(), or free()
  And all message buffers allocated from static ptp_message_pool[]
  And all temporary variables allocated on stack

Scenario: Bounded execution time on ARM Cortex-M7
  Given ARM Cortex-M7 @ 400 MHz with cycle counter enabled
  When executing ptp_parse_sync_message() 10000 times with varied payloads
  Then maximum execution time SHALL be <10 µs (4000 CPU cycles)
  And 99.9th percentile SHALL be <8 µs (3200 CPU cycles)
  And execution time variance SHALL be <20% (predictable)

Scenario: Servo update determinism
  Given ptp_servo_update() called at 1 Hz for 1 hour (3600 iterations)
  When measuring execution time with hardware cycle counter
  Then maximum execution time SHALL be <15 µs
  And standard deviation SHALL be <2 µs (low jitter)
  And no unbounded loops (all loops have fixed iteration count)
```

**Dependencies**:

- None (fundamental architecture requirement)
- Static buffer pool design (architecture)

**Risks**:

- Static allocation limits flexibility (fixed buffer sizes)
- WCET measurement requires hardware cycle counters (platform-dependent)

---

#### REQ-NF-P-003: Resource Efficiency

**Trace to**: STR-PERF-005  
**Priority**: P1 (High - MVP Desired)  
**Category**: Performance / Resource Usage

**Description**: The system SHALL operate efficiently on resource-constrained embedded platforms with limited RAM and CPU.

**Rationale**: Embedded systems (e.g., ARM Cortex-M7 in industrial PLCs, audio DSPs) have tight resource budgets. Excessive resource usage prevents adoption.

**Resource Targets**:

| Resource | Target | Measurement Method |
|----------|--------|-------------------|
| **RAM (Static)** | <32 KB | `size ptp_core.elf` (data + bss sections) |
| **RAM (Stack)** | <8 KB per thread | Stack watermark instrumentation |
| **Flash (Code)** | <128 KB | `size ptp_core.elf` (text section) |
| **CPU Usage** | <5% average | Profiling on ARM Cortex-M7 @ 400 MHz, 1 Hz sync rate |

**Acceptance Criteria**:

```gherkin
Scenario: RAM footprint on embedded target
  Given PTP core compiled for ARM Cortex-M7 with -Os (optimize for size)
  When analyzing binary with `arm-none-eabi-size ptp_core.elf`
  Then static RAM (data + bss) SHALL be <32 KB
  And stack usage per thread SHALL be <8 KB (measured with stack canaries)

Scenario: CPU usage on ARM Cortex-M7
  Given ARM Cortex-M7 @ 400 MHz running PTP at 1 Hz sync rate
  When profiling with SystemView or ARM ETM trace
  Then average CPU usage SHALL be <5% (2 ms per second)
  And peak CPU usage SHALL be <20% (200 ms burst for BMCA)

Scenario: Flash footprint
  Given PTP core compiled with all features enabled
  When analyzing binary size
  Then Flash usage (text section) SHALL be <128 KB
  And optional features compilable separately (e.g., Management disabled saves 15 KB)
```

**Dependencies**:

- Compiler optimization settings
- Modular architecture for feature toggling

**Risks**:

- Resource targets are platform-dependent (may need adjustment)
- Feature-rich implementation may exceed targets (require optimization)

---

### 3.2 Security

#### REQ-NF-S-001: Input Validation

**Trace to**: STR-SEC-001  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Security

**Description**: The system SHALL validate all inputs from untrusted sources (network packets, configuration files) before processing to prevent security vulnerabilities.

**Rationale**: PTP receives packets from the network without authentication (in basic mode). Malicious or malformed packets can exploit vulnerabilities (buffer overflows, logic errors) if not validated.

**Validation Checklist**:

1. **Packet Length**: Verify actual packet length matches expected size for message type
2. **Field Ranges**: Validate enumerated fields (messageType: 0x0-0xF, versionPTP: 0x02)
3. **TLV Validation**: Check TLV lengthField does not exceed packet boundary
4. **Sequence ID**: Validate monotonically increasing (detect replays)
5. **Clock Identity**: Reject invalid formats (all-zeros, multicast addresses)

**Input Sources Requiring Validation**:

- Network packets (untrusted, highest risk)
- Configuration files (trusted but validate syntax)
- Management messages (if enabled, untrusted)
- User API inputs (trusted but validate range)

**Acceptance Criteria**:

```gherkin
Scenario: Reject oversized TLV
  Given PTP message with TLV header lengthField = 2000 (exceeds 1500 byte MTU)
  When TLV parser executes ptp_parse_tlv()
  Then return PTP_ERROR_INVALID_TLV_LENGTH
  And do not read beyond packet buffer boundary
  And log security warning "TLV length exceeds packet size"
  And increment security_violation_counter

Scenario: Reject invalid message type
  Given PTP message with messageType = 0xFF (undefined/reserved)
  When message dispatcher executes ptp_dispatch_message()
  Then return PTP_ERROR_INVALID_MESSAGE_TYPE
  And discard packet without further processing
  And log security warning "Invalid messageType received"

Scenario: Validate PTP version
  Given PTP message with versionPTP = 0x01 (IEEE 1588-2008, incompatible)
  When message parser executes
  Then return PTP_ERROR_INCOMPATIBLE_VERSION
  And log warning "Received IEEE 1588-2008 message, expected 2019"
  And optionally support backward compatibility (configuration flag)
```

**Dependencies**:

- REQ-F-001 (message parsing must include validation)

**Risks**: 

- Missing validation can lead to crashes or exploits
- Over-validation may reject legitimate packets (interoperability risk)

---

#### REQ-NF-S-002: Memory Safety

**Trace to**: STR-SEC-002  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Security

**Description**: The system SHALL prevent memory safety vulnerabilities (buffer overflows, null pointer dereferences, use-after-free) through defensive coding practices and static analysis.

**Rationale**: Memory safety bugs are the most common source of security vulnerabilities. C language requires manual memory management; mistakes can be catastrophic.

**Memory Safety Practices**:

1. **Bounds Checking**: Validate array indices before access
2. **Safe String Operations**: Use `strncpy()` instead of `strcpy()`, always null-terminate
3. **Pointer Validation**: Check for NULL before dereference
4. **Buffer Sizes**: Always pass buffer size to functions (never assume)
5. **Static Analysis**: Run `cppcheck`, `clang-tidy`, `coverity` on all code

**Code Examples**:

```c
// ✅ CORRECT: Safe string copy
void ptp_set_clock_name(const char* name) {
    if (name == NULL) return; // NULL check
    strncpy(clock_name, name, sizeof(clock_name) - 1);
    clock_name[sizeof(clock_name) - 1] = '\0'; // Force null termination
}

// ❌ WRONG: Unsafe string copy (buffer overflow risk)
void ptp_set_clock_name_unsafe(const char* name) {
    strcpy(clock_name, name); // NO bounds check!
}

// ✅ CORRECT: Bounds-checked array access
int ptp_get_port_state(uint16_t port_index) {
    if (port_index >= MAX_PORTS) return -1; // Bounds check
    return port_states[port_index];
}

// ❌ WRONG: Unchecked array access (out-of-bounds risk)
int ptp_get_port_state_unsafe(uint16_t port_index) {
    return port_states[port_index]; // NO bounds check!
}
```

**Acceptance Criteria**:

```gherkin
Scenario: Safe string operations
  Given source string "IEEE_1588_2019_IMPLEMENTATION" (30 characters)
  And destination buffer size = 20 bytes
  When copying with strncpy(dest, src, sizeof(dest) - 1)
  Then destination SHALL be null-terminated
  And no buffer overflow SHALL occur
  And only 19 characters copied (leaving room for null terminator)

Scenario: Static analysis passes
  Given PTP codebase analyzed by cppcheck and clang-tidy
  When running static analysis with security checks enabled
  Then zero buffer overflow warnings
  And zero null pointer dereference warnings
  And zero use-after-free warnings
  And address sanitizer (ASAN) runtime checks pass all tests

Scenario: Null pointer safety
  Given function ptp_parse_sync_message(packet, length, &output)
  When called with output = NULL (invalid parameter)
  Then return PTP_ERROR_INVALID_PARAMETER
  And do not dereference NULL pointer
  And do not crash
```

**Dependencies**: 

- Coding standards (STR-MAINT-001)
- CI/CD integration with static analysis tools

**Risks**: 

- C language inherently unsafe (no memory safety guarantees)
- Static analysis produces false positives (requires human review)

---

### 3.4 Usability

#### REQ-NF-U-001: Learnability and Developer Usability

**Trace to**: STR-017, STR-018, STR-019; STORY-001, STORY-002  
**Priority**: P2 (Medium)  
**Category**: Usability

**Description**: The API and documentation SHALL enable a new developer to integrate and obtain basic synchronization within one working day, with a quick-start guide and runnable examples.

**Rationale**: Clear APIs and documentation reduce integration time and errors for adopters.

**Acceptance Criteria**:

```gherkin
Scenario: Quick-start integration
  Given the quick-start guide and example projects
  When a developer follows the documented steps on a supported platform
  Then the example SHALL build and run without code changes
  And the node SHALL reach SLAVE state and report offset within 10 minutes

Scenario: API discoverability
  Given the public headers and docs
  When searching for functions to initialize and start PTP
  Then functions ptp_init(), ptp_start() SHALL be clearly documented with parameters and return codes
```

**Dependencies**: REQ-F-005 (HAL interfaces), build system docs (REQ-NF-M-002)

---

### 3.3 Portability

#### REQ-NF-M-001: Platform Independence

**Trace to**: STR-PORT-003  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Maintainability / Portability

**Description**: The system SHALL compile and operate correctly on multiple target platforms without OS-specific code in the PTP core.

**Rationale**: Hardware-agnostic design is a core project principle. OS dependencies limit portability and violate stakeholder requirements (STR-PORT-003).

**Target Platforms** (MVP):

1. **Embedded RTOS**: ARM Cortex-M7 with FreeRTOS or bare-metal
2. **Linux**: x86-64 Ubuntu 20.04+ with glibc
3. **Windows**: x86-64 Windows 10+ with MSVC or MinGW

**OS Abstraction Requirements**:

- **NO OS-Specific Calls**: No direct calls to POSIX, Windows API, or RTOS primitives
- **Thread Abstraction**: Threading via HAL (if needed), not pthreads/Windows threads
- **Time Functions**: Time via HAL, not `clock_gettime()` or `QueryPerformanceCounter()`
- **Network I/O**: Network via HAL, not sockets/WinSock

**Acceptance Criteria**:

```gherkin
Scenario: Compile on multiple platforms
  Given PTP core source code
  When compiled with CMake for:
    - ARM Cortex-M7 (arm-none-eabi-gcc)
    - Linux x86-64 (gcc 9.4)
    - Windows x86-64 (MSVC 2022)
  Then all platforms compile without errors
  And no OS-specific #ifdef in PTP core (allowed in HAL implementations only)

Scenario: Run on embedded RTOS
  Given PTP core compiled for ARM Cortex-M7 with FreeRTOS HAL
  When running on STM32H7 evaluation board
  Then PTP synchronizes to Grandmaster
  And achieves sub-microsecond accuracy (with hardware timestamps)
  And operates for 24 hours without crash or memory leak

Scenario: Run on Linux
  Given PTP core compiled for Linux x86-64 with Linux HAL
  When running on Ubuntu 20.04 with Intel I210 NIC
  Then PTP synchronizes to Grandmaster
  And achieves sub-microsecond accuracy
  And passes all integration tests
```

**Dependencies**: 

- REQ-F-005 (HAL abstraction)
- Platform-specific HAL implementations (STR-PORT-002)

**Risks**: 

- OS differences may require HAL extensions (e.g., thread priority)
- Performance may vary across platforms (acceptable if documented)

---

#### REQ-NF-M-002: Build System Portability

**Trace to**: STR-PORT-004  
**Priority**: P1 (High - MVP Desired)  
**Category**: Maintainability / Build

**Description**: The system SHALL use CMake as the primary build system, supporting multiple compilers and platforms with minimal configuration.

**Rationale**: CMake is industry-standard, cross-platform, and supports all target platforms (embedded, Linux, Windows). Alternative build systems (Make, Visual Studio) are platform-specific.

**CMake Requirements**:

- **Minimum Version**: CMake 3.20+
- **Generators**: Ninja, Unix Makefiles, Visual Studio, ARM embedded toolchain
- **Compilers**: GCC, Clang, MSVC, arm-none-eabi-gcc
- **Feature Toggles**: Optional components via CMake options (e.g., `-DENABLE_MANAGEMENT=OFF`)

**CMakeLists.txt Structure**:

```cmake
cmake_minimum_required(VERSION 3.20)
project(IEEE1588_2019_PTP VERSION 1.0.0 LANGUAGES C)

# Options for optional features
option(ENABLE_MANAGEMENT "Enable Management protocol support" ON)
option(ENABLE_SECURITY "Enable security features (Annex P)" OFF)
option(BUILD_EXAMPLES "Build example applications" ON)

# Platform-agnostic core library
add_library(ptp_core STATIC
    src/ptp_message.c
    src/ptp_bmca.c
    src/ptp_servo.c
    ...
)
target_include_directories(ptp_core PUBLIC include/)

# Platform-specific HAL (user provides)
# add_subdirectory(hal/linux)     # For Linux builds
# add_subdirectory(hal/windows)   # For Windows builds
# add_subdirectory(hal/stm32h7)   # For STM32 embedded builds
```

**Acceptance Criteria**:

```gherkin
Scenario: Build on Linux with GCC
  Given PTP source code with CMakeLists.txt
  When building with:
    $ mkdir build && cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=Release
    $ cmake --build .
  Then libptp_core.a compiles without errors
  And all unit tests compile and link
  And examples compile (if BUILD_EXAMPLES=ON)

Scenario: Build on Windows with MSVC
  Given PTP source code with CMakeLists.txt
  When building with:
    $ mkdir build && cd build
    $ cmake .. -G "Visual Studio 17 2022"
    $ cmake --build . --config Release
  Then ptp_core.lib compiles without errors
  And all tests build successfully

Scenario: Cross-compile for ARM Cortex-M7
  Given PTP source code with CMakeLists.txt
  And ARM toolchain file (arm-none-eabi.cmake)
  When building with:
    $ cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake
    $ cmake --build .
  Then ptp_core.a compiles for ARM target
  And binary size meets embedded constraints (<128 KB flash)
```

**Dependencies**: 

- CMake 3.20+ installed on build systems
- Toolchain files for embedded targets

**Risks**: 

- CMake learning curve for contributors unfamiliar with it
- Platform-specific quirks may require toolchain customization

---

## 4. System Interfaces

### 4.1 External Interfaces

#### Network Interface

**Purpose**: Send and receive Ethernet frames for PTP messages

**Protocol**: IEEE 802.3 Ethernet

**Addressing**:

- **Unicast**: Individual PTP clock MAC addresses
- **Multicast**:
  - `01:1B:19:00:00:00` (PTP multicast, Event messages)
  - `01:80:C2:00:00:0E` (PTP peer delay, Link-local)

**Message Types**:

- Event messages (timestamped): Sync, Delay_Req, Pdelay_Req, Pdelay_Resp
- General messages (not timestamped): Follow_Up, Delay_Resp, Announce, Signaling, Management

**EtherType**: `0x88F7` (IEEE 1588 PTP)

#### Timestamp Interface

**Purpose**: Capture hardware timestamps for TX/RX events

**Requirements**:

- Nanosecond resolution (64-bit unsigned integer)
- Hardware timestamping preferred (software acceptable but limited accuracy)
- TX timestamp captured at MAC egress
- RX timestamp captured at MAC ingress

**Timestamp Sources** (hardware-dependent):

- Intel I210 NIC: 8ns resolution hardware timestamps
- STM32H7 Ethernet MAC: 20ns resolution hardware timestamps
- Software timestamps: ~1µs resolution (kernel timestamp)

#### Clock Interface

**Purpose**: Adjust slave clock frequency and phase

**Operations**:

- **Frequency Adjustment**: ±100 ppm typical range (hardware-dependent)
- **Phase Step**: Large offset correction (>1 second)
- **Get Time**: Read current clock value in nanoseconds

**Clock Sources** (hardware-dependent):

- IEEE 1588 PHC (PTP Hardware Clock) on Intel NICs
- STM32 RTC with PTP-synchronized XTAL
- System clock (less accurate, higher jitter)

### 4.2 User Interfaces

#### C API

**Purpose**: Programmatic interface for application integration

**Key Functions**:

```c
// Initialization
int ptp_init(const network_interface_t* net, 
             const timestamp_interface_t* ts,
             const clock_interface_t* clk,
             const timer_interface_t* tmr);

// Runtime operations
int ptp_start(ptp_mode_t mode); // ORDINARY_CLOCK_SLAVE, ORDINARY_CLOCK_MASTER
int ptp_stop(void);
int ptp_get_offset_from_master(int64_t* offset_ns);
int ptp_get_state(ptp_port_state_t* state);

// Configuration
int ptp_set_domain_number(uint8_t domain);
int ptp_set_priority1(uint8_t priority);
int ptp_set_sync_interval_log2(int8_t log_interval); // 2^log_interval seconds
```

**Error Handling**: All functions return `int` status code:

- `PTP_SUCCESS` (0): Operation succeeded
- `PTP_ERROR_INVALID_PARAMETER` (-1): Invalid input
- `PTP_ERROR_NOT_INITIALIZED` (-2): ptp_init() not called
- `PTP_ERROR_HARDWARE_FAILURE` (-3): HAL operation failed

#### Logging Interface

**Purpose**: Diagnostic output for debugging and monitoring

**Log Levels**:

- `PTP_LOG_ERROR`: Critical errors (e.g., hardware failure)
- `PTP_LOG_WARNING`: Non-critical issues (e.g., packet validation failure)
- `PTP_LOG_INFO`: Informational messages (e.g., state transitions)
- `PTP_LOG_DEBUG`: Verbose debugging (e.g., packet contents)

**Log Function**:

```c
typedef void (*ptp_log_callback_t)(ptp_log_level_t level, const char* message);
void ptp_set_log_callback(ptp_log_callback_t callback);
```

---

## 5. Constraints

### 5.1 Design Constraints

1. **Programming Language**: C11 or C99 (embedded compatibility)
2. **No C++ Dependencies**: Core PTP library is pure C (HAL may use C++)
3. **No Standard Library Dependencies** (embedded): Minimal libc usage, no FILE I/O
4. **Static Memory Allocation**: No malloc/free in critical paths
5. **Hardware Abstraction**: Zero direct hardware or OS dependencies

### 5.2 Implementation Constraints

1. **Compiler Support**: GCC 9+, Clang 10+, MSVC 2019+, arm-none-eabi-gcc 10+
2. **Endianness**: Big-endian network byte order for PTP messages
3. **Alignment**: Natural alignment for structures (4-byte or 8-byte)
4. **Thread Safety**: User must ensure single-threaded access or provide locking
5. **Interrupt Safety**: HAL timestamp callbacks may execute in ISR context

### 5.3 Standards Constraints

1. **IEEE 1588-2019 Compliance**: Mandatory conformance to all normative requirements
2. **No Proprietary Extensions**: Core PTP uses standard messages only
3. **Interoperability**: Must work with commercial PTP devices (Meinberg, Oregano, Microchip)

---

## 6. Traceability Matrix

### 6.1 Requirements → Stakeholder Requirements

| System Requirement | Stakeholder Requirement(s) | Priority | Phase |
|--------------------|---------------------------|----------|-------|
| **REQ-F-001** | STR-STD-001, STR-STD-002 | P0 | 01A |
| **REQ-F-002** | STR-STD-003 | P0 | 01B |
| **REQ-F-003** | STR-STD-001, STR-PERF-001 | P0 | 01A |
| **REQ-F-004** | STR-PERF-003 | P0 | 01A |
| **REQ-F-005** | STR-PORT-001 | P0 | 01A |
| **REQ-NF-P-001** | STR-PERF-001 | P0 | 01A |
| **REQ-NF-P-002** | STR-PERF-002 | P0 | 01A |
| **REQ-NF-P-003** | STR-PERF-005 | P1 | 01A |
| **REQ-NF-S-001** | STR-SEC-001 | P0 | 01A |
| **REQ-NF-S-002** | STR-SEC-002 | P0 | 01A |
| **REQ-NF-M-001** | STR-PORT-003 | P0 | 01A |
| **REQ-NF-M-002** | STR-PORT-004 | P1 | 01A |
| **REQ-S-001** | STR-STD-003 | P1 | 01B |
| **REQ-S-004** | STR-STD-004 | P1 | 01B |
| **REQ-NF-U-001** | STR-017, STR-018, STR-019 | P2 | 01A |

### 6.2 Requirements → Architecture Decisions (To Be Created)

| System Requirement | Architecture Decision |
|--------------------|----------------------|
| **REQ-F-005** | ADR-001: Hardware Abstraction Layer Design |
| **REQ-F-002** | ADR-002: BMCA State Machine Architecture |
| **REQ-F-003** | ADR-003: Timestamp Capture Strategy |
| **REQ-F-004** | ADR-004: Clock Servo Implementation |

### 6.3 Requirements → Test Cases (To Be Created)

| System Requirement | Test Case(s) |
|--------------------|-------------|
| **REQ-F-001** | TEST-MSG-001: Message Parsing Validation |
| **REQ-F-002** | TEST-BMCA-001: BMCA Master Selection |
| **REQ-F-003** | TEST-SYNC-001: Offset Calculation Accuracy |
| **REQ-F-004** | TEST-SERVO-001: PI Controller Convergence |
| **REQ-NF-P-001** | TEST-PERF-001: Synchronization Accuracy |
| **REQ-NF-P-002** | TEST-WCET-001: Deterministic Timing |
| **REQ-NF-S-001** | TEST-SEC-001: Input Validation Fuzzing |
| **REQ-NF-S-002** | TEST-SEC-002: Memory Safety (ASAN) |
| **REQ-F-005** | TEST-HAL-001: Mock HAL Unit Tests |

---

## 7. Appendices

### Appendix A: Requirement ID Taxonomy

**Pattern**: `REQ-<TYPE>-<CATEGORY>-<NUMBER>`

**Types**:

- **F**: Functional requirement (describes system behavior)
- **NF**: Non-functional requirement (describes quality attributes)

**Categories** (Non-Functional):

- **P**: Performance (speed, throughput, resource usage)
- **S**: Security (safety, privacy, authentication)
- **M**: Maintainability (portability, testability, build)
- **U**: Usability (API design, documentation)

**Numbering**: Sequential 001-999 within each category

**Examples**:

- `REQ-F-001`: Functional requirement #1 (Message parsing)
- `REQ-NF-P-001`: Non-functional performance requirement #1 (Accuracy)
- `REQ-NF-S-001`: Non-functional security requirement #1 (Input validation)

### Appendix B: Phase Allocation

**Phase 01A** (Weeks 1-8): Core Message Handling

- REQ-F-001, REQ-F-003, REQ-F-004, REQ-F-005
- REQ-NF-P-001, REQ-NF-P-002, REQ-NF-P-003
- REQ-NF-S-001, REQ-NF-S-002
- REQ-NF-M-001, REQ-NF-M-002

**Phase 01B** (Weeks 9-16): Advanced Features

- REQ-F-002 (BMCA)
- Management protocol (deferred)

**Phase 01C** (Weeks 17-26): Optimization & Validation

- Performance tuning
- Conformance testing
- Security audit (if sponsor available)

### Appendix C: References to IEEE 1588-2019 Specification

**CRITICAL COPYRIGHT NOTICE**: The following references cite specific clauses and sections of IEEE 1588-2019 for compliance verification. No copyrighted content from the specification is reproduced in this document.

| Requirement | IEEE 1588-2019 Reference |
|-------------|-------------------------|
| REQ-F-001 | Section 13 (Message formats) |
| REQ-F-002 | Section 9.3 (Best master clock algorithm) |
| REQ-F-003 | Section 11.3 (End-to-end delay mechanism) |
| REQ-F-004 | Section 11.2 (Clock synchronization) |

---

**Document Status**: Draft for Technical Review  
**Next Review**: 2025-11-08 (Team meeting)  
**Approval Required**: Technical Lead, Project Sponsor

**Traceability Status**:

- ✅ All requirements trace to stakeholder requirements
- ⏳ Architecture decisions (ADR-XXX) to be created in Phase 03
- ⏳ Test cases (TEST-XXX) to be created in Phase 02B/07
