# Requirements Elicitation Session Report
## IEEE 1588-2019 PTP Open-Source Implementation - Phase 02

**Session Date**: 2025-11-07  
**Session ID**: ELICIT-20251107-001  
**Standard Compliance**: ISO/IEC/IEEE 29148:2018 Section 5.3 (System Requirements)  
**Phase**: Phase 02 - Requirements Analysis & Specification  

---

## Executive Summary

This requirements elicitation session transforms 25 stakeholder requirements (STR-XXX-###) from Phase 01 into detailed system-level functional (REQ-F-###) and non-functional requirements (REQ-NF-<CAT>-###) following ISO/IEC/IEEE 29148:2018 methodology.

**Current State Analysis**:
- **Phase 01 Complete**: 25 stakeholder requirements documented (STR-STD-001 through STR-MAINT-004)
- **Phase 02 Existing**: Multiple requirements documents with ~200+ requirement IDs
- **Validation Issues**: 44 schema violations, 207 orphaned requirements
- **Root Cause**: Invalid traceability patterns (REQ-STK-XXX instead of StR-XXX), missing ADR/test linkages

**Critical Findings**:

1. **Invalid Traceability Pattern**: Existing requirements use `REQ-STK-XXX` in YAML front matter, but Phase 01 uses `STR-XXX-###` pattern per schema `^StR-(?:[A-Z]{4}-)?\\d{3}$`

2. **Orphaned Requirements**: 207 requirements have no linkage to architecture decisions (ADR-XXX), components (ARC-C-XXX), quality scenarios (QA-SC-XXX), or tests (TEST-XXX)

3. **Schema Compliance**: `requirements-spec.schema.json` enforces:
   - `specType: requirements`
   - `standard: "29148"` (short form, NOT "ISO/IEC/IEEE 29148:2018")
   - `traceability.stakeholderRequirements: [StR-XXX, ...]` (NOT REQ-STK-XXX)

**Approach**:
This session will create a **definitive System Requirements Specification (SyRS)** that:
- Maps each STR-XXX to proper REQ-F-### (functional) or REQ-NF-<CAT>-### (non-functional) 
- Uses 8-dimension elicitation framework for completeness
- Ensures 100% traceability (StR → REQ → ADR → Test)
- Eliminates all dummy/orphaned requirements

---

## 1. Phase 01 Stakeholder Requirements (Authoritative Source)

From `01-stakeholder-requirements/stakeholder-requirements-spec.md`:

### 1.1 Standards Compliance Theme (4 requirements)

| ID | Title | Priority | Stakeholders |
|----|-------|----------|--------------|
| **STR-STD-001** | IEEE 1588-2019 Protocol Compliance | P0 (Critical) | STK-002, STK-003, STK-005 |
| **STR-STD-002** | Message Format Correctness | P0 (Critical) | STK-002, STK-003, STK-005 |
| **STR-STD-003** | Best Master Clock Algorithm (BMCA) | P0 (Critical) | STK-002, STK-003, STK-005 |
| **STR-STD-004** | Interoperability with Commercial Devices | P1 (High) | STK-003, STK-012 |

### 1.2 Performance Theme (5 requirements)

| ID | Title | Priority | Stakeholders |
|----|-------|----------|--------------|
| **STR-PERF-001** | Synchronization Accuracy | P0 (Critical) | STK-002, STK-003, STK-013 |
| **STR-PERF-002** | Timing Determinism | P0 (Critical) | STK-001, STK-002 |
| **STR-PERF-003** | Clock Servo Performance | P0 (Critical) | STK-002, STK-003 |
| **STR-PERF-004** | Path Delay Measurement | P0 (Critical) | STK-002, STK-003 |
| **STR-PERF-005** | Resource Efficiency | P1 (High) | STK-001, STK-002, STK-004 |

### 1.3 Portability Theme (4 requirements)

| ID | Title | Priority | Stakeholders |
|----|-------|----------|--------------|
| **STR-PORT-001** | Hardware Abstraction Layer (HAL) | P0 (Critical) | STK-001, STK-002, STK-004, STK-012 |
| **STR-PORT-002** | Reference HAL Implementations | P0 (Critical) | STK-001, STK-004, STK-012 |
| **STR-PORT-003** | No OS Assumptions | P0 (Critical) | STK-001, STK-004 |
| **STR-PORT-004** | Cross-Platform Build System | P1 (High) | STK-004, STK-011 |

### 1.4 Security Theme (4 requirements)

| ID | Title | Priority | Stakeholders |
|----|-------|----------|--------------|
| **STR-SEC-001** | Input Validation | P0 (Critical) | STK-002, STK-003 |
| **STR-SEC-002** | No Buffer Overruns | P0 (Critical) | STK-002, STK-003 |
| **STR-SEC-003** | Security Audit | P1 (High) | STK-002, STK-003 |
| **STR-SEC-004** | Optional Authentication (Post-MVP) | P2 (Medium) | STK-002, STK-003 |

### 1.5 Usability Theme (4 requirements)

| ID | Title | Priority | Stakeholders |
|----|-------|----------|--------------|
| **STR-USE-001** | API Documentation | P0 (Critical) | STK-004, STK-011 |
| **STR-USE-002** | Getting Started Tutorial | P1 (High) | STK-004, STK-010 |
| **STR-USE-003** | Example Applications | P1 (High) | STK-004, STK-010, STK-012 |
| **STR-USE-004** | Porting Guide | P1 (High) | STK-004, STK-012 |

### 1.6 Maintainability Theme (4 requirements)

| ID | Title | Priority | Stakeholders |
|----|-------|----------|--------------|
| **STR-MAINT-001** | Code Quality | P1 (High) | STK-011 |
| **STR-MAINT-002** | Continuous Integration | P1 (High) | STK-011 |
| **STR-MAINT-003** | Architectural Decision Records (ADRs) | P2 (Medium) | STK-011 |
| **STR-MAINT-004** | Community Contribution Process | P2 (Medium) | STK-011 |

---

## 2. Existing Phase 02 Requirements Analysis

### 2.1 Files Analyzed

1. **`functional/ieee-1588-2019-ptp-requirements.md`**
   - 135 requirements (REQ-STK-PTP-001 through REQ-NFR-PTP-048)
   - ❌ INVALID: Uses `REQ-STK-PTP-XXX` in traceability (should be `STR-XXX-###`)
   - ✅ GOOD: Comprehensive IEEE 1588-2019 functional breakdown

2. **`functional/architectural-compliance-requirements.md`**
   - 12 requirements (REQ-STK-ARCH-001 through REQ-NFR-ARCH-005)
   - ❌ INVALID: Uses `REQ-STK-ARCH-XXX` pattern
   - ✅ GOOD: Architecture principles well-defined

3. **`functional/cross-standards-architecture-integration-requirements.md`**
   - 58 requirements (REQ-STK-CROSSARCH-001 through REQ-NF-CROSSARCH-030)
   - ❌ INVALID: Uses `REQ-STK-CROSSARCH-XXX` pattern
   - ⚠️ OUT OF SCOPE: Cross-standards integration is POST-MVP (see copilot-instructions.md)

4. **`functional/ieee-1588-2019-requirements-analysis.md`**
   - Gap analysis document
   - ❌ INVALID: Uses `REQ-STK-TIMING-001`, `REQ-STK-SYNC-001`
   - ✅ GOOD: Identifies missing components (BMCA, transport, management)

5. **`non-functional/cross-standard-dependency-analysis.md`**
   - 5 requirements (REQ-STK-CROSSSTD-001 through REQ-STK-CROSSSTD-005)
   - ❌ INVALID: Uses `REQ-STK-CROSSSTD-XXX` pattern
   - ⚠️ OUT OF SCOPE: Cross-standards is POST-MVP

### 2.2 Orphan Analysis Summary

**Total Requirements Identified**: ~220  
**Orphaned (No ADR/Test Links)**: 207 (94%)  
**Properly Linked**: 13 (6%)

**Well-Linked Requirements** (Examples to Follow):
- `REQ-F-001`: Linked to ADR-001, ADR-002, ADR-003, ARC-C-001, QA-SC-001
- `REQ-F-010`: Linked to ADR-001, ADR-002, ARC-C-002, QA-SC-001
- `REQ-NF-P-001`: Linked to ADR-001, ADR-004, QA-SC-001

---

## 3. Requirements Elicitation Strategy

### 3.1 Approach

Following ISO/IEC/IEEE 29148:2018 Section 5.3.2, we will:

1. **Transform Stakeholder Requirements → System Requirements**
   - Each STR-XXX-### becomes 1-N REQ-F-### or REQ-NF-<CAT>-###
   - Apply 8-dimension elicitation framework
   - Ensure testability and verifiability

2. **Establish Complete Traceability**
   - StR-XXX-### → REQ-F-### (functional requirements)
   - StR-XXX-### → REQ-NF-<CAT>-### (non-functional requirements)
   - REQ-XXX → ADR-XXX (architectural decisions)
   - REQ-XXX → QA-SC-XXX (quality scenarios)
   - REQ-XXX → TEST-XXX (test cases)

3. **Eliminate Invalid Patterns**
   - Remove all `REQ-STK-XXX` pseudo-requirements
   - Remove out-of-scope cross-standards requirements
   - Consolidate duplicate/redundant requirements

### 3.2 8-Dimension Elicitation Framework

For each stakeholder requirement, we analyze:

1. **Functional Behavior**: What must the system do?
2. **Boundary Values & Ranges**: Min/max limits, edge cases
3. **Error Handling**: Failure modes, recovery, degradation
4. **Performance**: Response time, throughput, resource usage
5. **Security**: Threats, vulnerabilities, mitigations
6. **Integration**: Interfaces, dependencies, external systems
7. **Priority & Risk**: Business criticality, technical risk
8. **Acceptance Criteria**: Testable verification conditions

---

## 4. System Requirements Specification (SyRS) - MVP Scope

### 4.1 Functional Requirements - Standards Compliance

#### REQ-F-001: IEEE 1588-2019 Message Type Support

**Trace to**: STR-STD-001, STR-STD-002  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Standards Compliance

**Description**: The system SHALL implement parsing, validation, and serialization for all mandatory IEEE 1588-2019 message types.

**8-Dimension Analysis**:

1. **Functional Behavior**:
   - Parse: Sync, Delay_Req, Follow_Up, Delay_Resp, Announce, Signaling, Management
   - Validate: Header fields, TLVs, timestamps, flags per IEEE 1588-2019 Section 13
   - Serialize: Convert in-memory structures to network byte order

2. **Boundary Values**:
   - Message length: 44 bytes (Sync) to 1500 bytes (max Ethernet MTU)
   - Field ranges per IEEE 1588-2019 tables (e.g., domainNumber: 0-255)

3. **Error Handling**:
   - Invalid message type → return ERROR_INVALID_MESSAGE_TYPE
   - Checksum failure → discard message, log warning
   - TLV parsing failure → skip unknown TLVs gracefully

4. **Performance**:
   - Parse latency: <10µs on ARM Cortex-M7 @ 400MHz
   - Zero dynamic allocation (pre-allocated message buffers)

5. **Security**:
   - Validate all length fields before buffer operations
   - Bounds checking on array indices

6. **Integration**:
   - HAL: `network_interface_t::receive_packet()` → message parser
   - Output: `ptp_message_t` structure

7. **Priority**: P0 - No PTP without message handling

8. **Acceptance Criteria**:
```gherkin
Scenario: Parse valid Sync message
  Given a raw Sync message packet (44 bytes) conforming to IEEE 1588-2019 Table 26
  When parsed by message handler
  Then return PTP_SUCCESS
  And fill ptp_sync_message_t structure with correct field values
  And messageType field SHALL equal 0x0 (SYNC)

Scenario: Reject malformed message
  Given a packet with invalid messageType field (0xFF)
  When parsed by message handler
  Then return PTP_ERROR_INVALID_MESSAGE_TYPE
  And log error message
  And do not modify output structure
```

**Dependencies**: None (foundation capability)  
**Risks**: Message format compliance is testable via Wireshark dissector

---

#### REQ-F-002: Best Master Clock Algorithm (BMCA) State Machine

**Trace to**: STR-STD-003  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Standards Compliance

**Description**: The system SHALL implement BMCA dataset comparison and state decision algorithms per IEEE 1588-2019 Section 9.3.

**8-Dimension Analysis**:

1. **Functional Behavior**:
   - Receive Announce messages from multiple masters
   - Compare datasets: priority1, clockClass, clockAccuracy, offsetScaledLogVariance, priority2, clockIdentity, stepsRemoved
   - Execute state decision algorithm (Figure 26)
   - Transition states: LISTENING → UNCALIBRATED → SLAVE or MASTER

2. **Boundary Values**:
   - Max announce sources: 256 (typical network limit)
   - Announce timeout: 2-10 announce intervals (configurable)
   - BMCA execution frequency: Every announce reception + periodic (1s)

3. **Error Handling**:
   - Announce timeout → switch to LISTENING state, restart BMCA
   - Conflicting Announce from same clockId → use most recent
   - All masters lost → enter LISTENING, search for new master

4. **Performance**:
   - BMCA execution: <100µs worst-case
   - State transition: <1ms including notification callbacks

5. **Security**:
   - Validate Announce message TLVs
   - Ignore Announce from unauthorized sources (if security enabled)

6. **Integration**:
   - Input: Announce message handler → BMCA engine
   - Output: State machine transitions, best master selection
   - Callback: `bmca_state_changed_callback_t`

7. **Priority**: P0 - BMCA is mandatory for multi-master networks

8. **Acceptance Criteria**:
```gherkin
Scenario: Select best master from multiple sources
  Given three PTP clocks transmitting Announce messages:
    | Clock ID | Priority1 | Class | Accuracy | Priority2 |
    | 00:11:22:FF:FE:33:44:55 | 128 | 248 | 0x21 | 128 |
    | AA:BB:CC:FF:FE:DD:EE:FF | 64  | 248 | 0x21 | 128 |
    | FF:EE:DD:FF:FE:CC:BB:AA | 200 | 248 | 0x21 | 128 |
  When BMCA executes dataset comparison
  Then select clock AA:BB:CC:FF:FE:DD:EE:FF as best master (lowest priority1=64)
  And transition to UNCALIBRATED state
  And emit bmca_state_changed(LISTENING → UNCALIBRATED)

Scenario: Handle master timeout
  Given system synchronized to master for 600 seconds
  And announce interval = 2 seconds, timeout = 6 intervals
  When no Announce received for 12 seconds
  Then transition to LISTENING state
  And restart BMCA with empty candidate list
  And emit bmca_state_changed(SLAVE → LISTENING)
```

**Dependencies**: REQ-F-001 (Announce message parsing)  
**Risks**: BMCA edge cases require exhaustive state machine testing

---

#### REQ-F-003: Clock Offset Calculation

**Trace to**: STR-STD-001, STR-PERF-001  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Time Synchronization

**Description**: The system SHALL calculate clock offset from master using Sync/Follow_Up and Delay_Req/Delay_Resp message exchanges.

**8-Dimension Analysis**:

1. **Functional Behavior**:
   - Capture T1 (Sync egress timestamp from master, via Follow_Up)
   - Capture T2 (Sync ingress timestamp at slave, hardware timestamp)
   - Capture T3 (Delay_Req egress timestamp at slave)
   - Capture T4 (Delay_Req ingress timestamp at master, via Delay_Resp)
   - Calculate offset: `offset = ((T2 - T1) - (T4 - T3)) / 2`
   - Calculate path delay: `delay = ((T2 - T1) + (T4 - T3)) / 2`

2. **Boundary Values**:
   - Timestamp resolution: Nanoseconds (64-bit signed integer)
   - Offset range: ±2^63 ns (±292 years, theoretical)
   - Typical offset: ±10ms (initial) → <1µs (converged)

3. **Error Handling**:
   - Missing Follow_Up → discard Sync, wait for complete pair
   - Out-of-order messages → match by sequenceId
   - Timestamp overflow → wrap-around detection (unlikely in practice)

4. **Performance**:
   - Offset calculation: <5µs (simple arithmetic)
   - Update rate: 1-128 Hz (per sync interval)

5. **Security**:
   - Validate timestamp sanity (detect impossibly large jumps >1s)
   - Sequence ID matching prevents replay attacks

6. **Integration**:
   - Input: Timestamp capture from HAL, PTP messages
   - Output: `clock_offset_ns`, `path_delay_ns`
   - Consumer: Clock servo (REQ-F-004)

7. **Priority**: P0 - Offset calculation is core PTP function

8. **Acceptance Criteria**:
```gherkin
Scenario: Calculate offset from message timestamps
  Given master sends Sync at T1 = 1000000000 ns
  And slave receives Sync at T2 = 1000000500 ns (hardware timestamp)
  And slave sends Delay_Req at T3 = 1000001000 ns
  And master receives Delay_Req at T4 = 1000001450 ns
  When offset calculation executes
  Then offset SHALL equal ((1000000500 - 1000000000) - (1000001450 - 1000001000)) / 2
    = (500 - 450) / 2 = 25 ns
  And path_delay SHALL equal ((1000000500 - 1000000000) + (1000001450 - 1000001000)) / 2
    = (500 + 450) / 2 = 475 ns
```

**Dependencies**: REQ-F-001 (message parsing), STR-PORT-001 (HAL timestamp interface)  
**Risks**: Hardware timestamp accuracy limits synchronization precision

---

### 4.2 Functional Requirements - Clock Servo

#### REQ-F-004: PI Controller Clock Adjustment

**Trace to**: STR-PERF-003  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Clock Servo

**Description**: The system SHALL implement a Proportional-Integral (PI) controller to adjust slave clock frequency based on measured offset.

**8-Dimension Analysis**:

1. **Functional Behavior**:
   - Input: Clock offset from REQ-F-003
   - Calculate proportional term: `P = Kp * offset`
   - Calculate integral term: `I += Ki * offset * dt`
   - Output adjustment: `freq_adjustment = P + I`
   - Clamp output to hardware limits

2. **Boundary Values**:
   - Kp (proportional gain): 0.0 - 1.0 (tunable)
   - Ki (integral gain): 0.0 - 0.01 (tunable)
   - Frequency adjustment range: ±100 ppm (typical NIC limit)
   - Integral windup limit: ±10000 (prevent runaway)

3. **Error Handling**:
   - Offset outlier (>1s) → discard sample, freeze integral
   - Convergence failure → reset integral term, restart servo
   - Hardware adjustment failure → log error, retry next cycle

4. **Performance**:
   - Servo update rate: 1-128 Hz (matches sync interval)
   - Convergence time: <60 seconds to <1µs offset
   - CPU usage: <1% on ARM Cortex-M7

5. **Security**:
   - Limit maximum frequency adjustment to prevent malicious time jumps

6. **Integration**:
   - Input: `clock_offset_ns` from REQ-F-003
   - Output: `clock_frequency_adjustment_ppb` to HAL
   - HAL: `clock_interface_t::adjust_frequency()`

7. **Priority**: P0 - Servo is required for synchronization

8. **Acceptance Criteria**:
```gherkin
Scenario: Converge to master clock
  Given initial offset = 10000 ns (10µs)
  And Kp = 0.7, Ki = 0.001
  And sync interval = 1 Hz
  When servo runs for 60 seconds
  Then final offset SHALL be <1000 ns (1µs)
  And frequency adjustment SHALL stabilize (variance <10 ppb)
  And integral term SHALL not exceed ±5000
```

**Dependencies**: REQ-F-003 (offset calculation), STR-PORT-001 (HAL clock interface)  
**Risks**: PI tuning is hardware-dependent; may need per-platform calibration

---

### 4.3 Non-Functional Requirements - Performance

#### REQ-NF-P-001: Synchronization Accuracy

**Trace to**: STR-PERF-001  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Performance

**Description**: The system SHALL achieve clock offset <1 microsecond (µs) from master on capable hardware with hardware timestamping.

**Metric**: Clock offset (ns)  
**Target**: 
- P95 offset: <1000 ns (1 µs)
- P99 offset: <2000 ns (2 µs)
- Median offset: <500 ns (0.5 µs)

**Measurement Method**:
```yaml
test: sync_accuracy
platform: x86-64 Linux, Intel I210 NIC (hardware timestamps)
master: GPS-disciplined Grandmaster (Meinberg, Oregano, or Microchip)
duration: 10 minutes (600 samples at 1 Hz)
metric: |
  offset_samples = collect_offsets(duration=600s, rate=1Hz)
  p50 = percentile(offset_samples, 50)
  p95 = percentile(offset_samples, 95)
  p99 = percentile(offset_samples, 99)
  assert p50 < 500ns
  assert p95 < 1000ns
  assert p99 < 2000ns
```

**Acceptance Criteria**:
```gherkin
Scenario: Achieve sub-microsecond accuracy
  Given PTP Grandmaster with GPS-disciplined oscillator (<50ns accuracy)
  And slave clock with Intel I210 NIC (hardware timestamping)
  And network latency <1ms (LAN environment)
  When synchronized for 10 minutes at 1 Hz sync rate
  Then 95% of offset samples SHALL be <1000 ns
  And 99% of offset samples SHALL be <2000 ns
  And median offset SHALL be <500 ns
```

**Dependencies**: REQ-F-003, REQ-F-004, STR-PORT-001 (HAL with hardware timestamps)  
**Risks**: Accuracy is hardware-limited; software-only timestamps may achieve 10-100µs

---

#### REQ-NF-P-002: Deterministic Timing

**Trace to**: STR-PERF-002  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Performance / Real-Time

**Description**: All time-critical code paths SHALL have bounded worst-case execution time (WCET) with no dynamic memory allocation.

**Metric**: Worst-Case Execution Time (µs)  
**Targets**:
- Message parsing: WCET <10 µs
- Offset calculation: WCET <5 µs
- Servo update: WCET <15 µs
- BMCA execution: WCET <100 µs

**Measurement Method**:
```c
// Instrumentation using hardware cycle counter
uint32_t start_cycles = HAL_GetCycleCount();
ptp_parse_sync_message(packet, packet_len, &sync_msg);
uint32_t end_cycles = HAL_GetCycleCount();
uint32_t elapsed_us = (end_cycles - start_cycles) / (CPU_FREQ_MHZ);
assert(elapsed_us < 10); // WCET <10µs
```

**Acceptance Criteria**:
```gherkin
Scenario: No dynamic allocation in critical path
  Given system compiled with -DDEBUG_ALLOC_TRACE
  When processing 1000 PTP messages (Sync/Follow_Up/Delay_Req/Delay_Resp)
  Then zero calls to malloc(), calloc(), or realloc()
  And all memory allocated from static or stack buffers

Scenario: Bounded execution time on ARM Cortex-M7
  Given ARM Cortex-M7 @ 400 MHz
  When executing message_parse() 10000 times with varied payloads
  Then maximum execution time SHALL be <10 µs
  And 99.9th percentile SHALL be <8 µs
```

**Dependencies**: None (fundamental architecture requirement)  
**Risks**: Requires static allocation strategy; limits flexibility

---

### 4.4 Non-Functional Requirements - Security

#### REQ-NF-S-001: Input Validation

**Trace to**: STR-SEC-001  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Security

**Description**: The system SHALL validate all inputs from untrusted sources (network packets, configuration) before processing.

**Validation Checklist**:
1. **Message Length**: Verify packet length matches expected size before parsing
2. **Field Ranges**: Validate enumerated fields (messageType: 0x0-0xF, domainNumber: 0-255)
3. **TLV Lengths**: Verify TLV lengthField does not exceed packet boundary
4. **String Inputs**: Null-terminate and length-check all string fields
5. **Array Indices**: Bounds-check all array accesses

**Acceptance Criteria**:
```gherkin
Scenario: Reject oversized TLV
  Given PTP message with TLV lengthField = 2000 (exceeds 1500 byte MTU)
  When TLV parser executes
  Then return PTP_ERROR_INVALID_TLV_LENGTH
  And do not read beyond packet buffer
  And log security warning

Scenario: Reject invalid message type
  Given PTP message with messageType = 0xFF (undefined)
  When message handler executes
  Then return PTP_ERROR_INVALID_MESSAGE_TYPE
  And discard packet
  And increment invalid_message_counter
```

**Dependencies**: REQ-F-001 (message parsing must include validation)  
**Risks**: Missing validation can lead to buffer overruns or crashes

---

#### REQ-NF-S-002: Memory Safety

**Trace to**: STR-SEC-002  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Security

**Description**: The system SHALL prevent buffer overruns through bounds checking and safe string operations.

**Safety Mechanisms**:
1. Use `strncpy()` instead of `strcpy()`
2. Check array bounds before indexing
3. Validate pointer arithmetic
4. Use static analysis tools (Coverity, cppcheck)

**Acceptance Criteria**:
```gherkin
Scenario: Safe string copy
  Given source string "IEEE_1588_2019_IMPLEMENTATION" (30 chars)
  And destination buffer size = 20 bytes
  When copying with strncpy(dest, src, sizeof(dest)-1)
  Then dest SHALL be null-terminated
  And no buffer overflow SHALL occur

Scenario: Static analysis passes
  Given codebase analyzed by cppcheck and clang-tidy
  When analyzing all source files
  Then zero buffer overflow warnings
  And zero null pointer dereference warnings
```

**Dependencies**: STR-MAINT-001 (code quality practices)  
**Risks**: Requires discipline and tooling; manual review is error-prone

---

### 4.5 Non-Functional Requirements - Portability

#### REQ-NF-M-001: Hardware Abstraction Layer (HAL) Compliance

**Trace to**: STR-PORT-001  
**Priority**: P0 (Critical - MVP Blocker)  
**Category**: Maintainability / Portability

**Description**: The system SHALL access hardware only through defined HAL interfaces using C function pointers.

**HAL Interface Contracts**:

```c
// Network Interface
typedef struct {
    int (*send_packet)(const void* data, size_t length);
    int (*receive_packet)(void* buffer, size_t* length, uint32_t timeout_ms);
    int (*get_mac_address)(uint8_t mac[6]);
} network_interface_t;

// Timestamp Interface  
typedef struct {
    uint64_t (*get_time_ns)(void);
    int (*capture_tx_timestamp)(uint16_t sequence_id, uint64_t* timestamp_ns);
    int (*capture_rx_timestamp)(void* packet, uint64_t* timestamp_ns);
} timestamp_interface_t;

// Clock Interface
typedef struct {
    int (*adjust_frequency)(int32_t frequency_ppb);
    int (*step_time)(int64_t offset_ns);
    uint64_t (*get_current_time_ns)(void);
} clock_interface_t;
```

**Acceptance Criteria**:
```gherkin
Scenario: Compile without hardware headers
  Given PTP implementation source code
  When compiled with HARDWARE_ABSTRACTION=1
  Then no #include of vendor-specific headers (e.g., <intel_hal.h>)
  And no direct hardware register access
  And all hardware operations via HAL function pointers

Scenario: Runtime HAL injection
  Given PTP library initialized with NULL HAL
  When HAL interface registered via ptp_register_hal()
  Then all hardware operations use registered HAL
  And library operates correctly on mock HAL (unit tests)
```

**Dependencies**: Architecture design (ADR-001)  
**Risks**: HAL abstraction may add overhead; performance testing required

---

## 5. Out-of-Scope Items (Post-MVP)

The following requirements from existing documents are **OUT OF SCOPE** for MVP v1.0.0 (per stakeholder approval and copilot-instructions.md):

### 5.1 Cross-Standards Integration (Post-MVP Phase 02)

**Reason**: Phase 01 focuses on IEEE 1588-2019 PTP only. Cross-standards integration (IEEE 802.1AS gPTP, IEEE 1722 AVTP, AES67, etc.) is deferred to Phase 02 per project roadmap.

**Affected Requirements** (to be removed from MVP scope):
- All `REQ-*-CROSSARCH-*` (58 requirements)
- All `REQ-*-CROSSSTD-*` (5 requirements)
- `cross-standards-architecture-integration-requirements.md` (entire file deferred)
- `cross-standard-dependency-analysis.md` (entire file deferred)

**Future Action**: Create Phase 02 requirements specification for cross-standards integration when IEEE 1588-2019 core is complete (estimated 2026-Q2).

### 5.2 Advanced Security Features (Post-MVP)

**Reason**: STR-SEC-004 (Optional Authentication) is P2 priority, deferred to post-MVP.

**Affected Requirements**:
- IEEE 1588-2019 security mechanisms (Annex P)
- Authentication TLVs
- Key management

**MVP Approach**: Focus on input validation and memory safety (REQ-NF-S-001, REQ-NF-S-002). Security audit (STR-SEC-003) is highest-priority sponsorship target.

### 5.3 Management Protocol (Phase 01B-C)

**Reason**: Management messages are lower priority than core synchronization (BMCA, servo).

**Affected Requirements**: REQ-F-??? (management message handling)

**Phased Approach**:
- **Phase 01A** (Weeks 1-8): Core message types (Sync, Delay, Announce)
- **Phase 01B** (Weeks 9-16): Management messages, dataset queries
- **Phase 01C** (Weeks 17-26): Transparent clock, multi-domain

---

## 6. Requirements Traceability Matrix (MVP Scope)

| System Requirement | Stakeholder Requirement(s) | Priority | Phase | ADR Link | Test Link |
|--------------------|---------------------------|----------|-------|----------|-----------|
| REQ-F-001 | STR-STD-001, STR-STD-002 | P0 | 01A | ADR-001 | TEST-MSG-001 |
| REQ-F-002 | STR-STD-003 | P0 | 01B | ADR-002 | TEST-BMCA-001 |
| REQ-F-003 | STR-STD-001, STR-PERF-001 | P0 | 01A | ADR-003 | TEST-SYNC-001 |
| REQ-F-004 | STR-PERF-003 | P0 | 01A | ADR-004 | TEST-SERVO-001 |
| REQ-NF-P-001 | STR-PERF-001 | P0 | 01A | - | TEST-PERF-001 |
| REQ-NF-P-002 | STR-PERF-002 | P0 | 01A | ADR-001 | TEST-WCET-001 |
| REQ-NF-S-001 | STR-SEC-001 | P0 | 01A | - | TEST-SEC-001 |
| REQ-NF-S-002 | STR-SEC-002 | P0 | 01A | ADR-001 | TEST-SEC-002 |
| REQ-NF-M-001 | STR-PORT-001 | P0 | 01A | ADR-001 | TEST-HAL-001 |

**Traceability Goals**:
- ✅ Every REQ traces to 1+ STR
- ✅ Every P0 REQ traces to ADR (architecture decision)
- ✅ Every REQ traces to TEST (test case)
- ✅ Zero orphaned requirements

---

## 7. Next Steps

### 7.1 Immediate Actions (Phase 02A - Weeks 1-2)

1. **Fix YAML Front Matter** (Task 6)
   - Replace all `traceability.stakeholderRequirements: [REQ-STK-*]` with proper `[STR-XXX-###]` references
   - Ensure `standard: "29148"` (short form)
   - Run `py scripts\validate-spec-structure.py` until zero errors

2. **Create System Requirements Specification (SyRS)** (Task 5)
   - Consolidate MVP requirements into single `system-requirements-specification.md`
   - Use template from `phase-02-requirements.instructions.md`
   - Include all REQ-F-### and REQ-NF-###-### from Section 4

3. **Archive Out-of-Scope Requirements** (Task 5)
   - Move `cross-standards-architecture-integration-requirements.md` to `02-requirements/post-mvp/`
   - Move `cross-standard-dependency-analysis.md` to `02-requirements/post-mvp/`
   - Add README explaining Phase 02 deferral

### 7.2 Requirements Validation (Phase 02A - Week 2)

4. **Create Use Cases** (Task 7)
   - UC-001: Synchronize as Ordinary Clock Slave
   - UC-002: Select Best Master via BMCA
   - UC-003: Measure Clock Offset
   - UC-004: Adjust Clock Frequency
   - Follow format from `phase-02-requirements.instructions.md`

5. **Create User Stories** (Task 7)
   - STORY-001: As an embedded developer, I want to integrate PTP into my RTOS
   - STORY-002: As a system integrator, I want to verify synchronization accuracy
   - STORY-003: As a hardware vendor, I want to port PTP to my NIC
   - Use Given-When-Then acceptance criteria

6. **Re-run Traceability** (Task 8)
   - `py scripts\generate-traceability-matrix.py`
   - Target: Zero orphaned requirements
   - Document traceability matrix in `reports/traceability-matrix.md`

### 7.3 Phase Gate Criteria (Phase 02 Exit)

**Phase 02 Complete When**:
- ✅ All YAML front matter validates (zero errors from `validate-spec-structure.py`)
- ✅ System Requirements Specification (SyRS) complete with 100% STR traceability
- ✅ Use cases created for all P0 functional requirements
- ✅ User stories created with testable acceptance criteria
- ✅ Traceability matrix shows zero orphans
- ✅ Phase 02 → Phase 03 handoff: All requirements reviewed and approved

---

## 8. References

### 8.1 Phase 01 Deliverables (Inputs)

- `01-stakeholder-requirements/stakeholder-requirements-spec.md` - **AUTHORITATIVE SOURCE** for STR-XXX-### IDs
- `01-stakeholder-requirements/business-context/business-case.md` - Business justification
- `01-stakeholder-requirements/roadmap.md` - 26-week MVP timeline

### 8.2 Standards

- **ISO/IEC/IEEE 29148:2018** - Requirements Engineering Processes
- **IEEE 1588-2019** - Precision Time Protocol (PTPv2.1)
- **ISO/IEC/IEEE 12207:2017** - Software Life Cycle Processes

### 8.3 Project Artifacts

- `.github/instructions/copilot-instructions.md` - AI coding guidelines (hardware-agnostic principle)
- `.github/instructions/phase-02-requirements.instructions.md` - Requirements phase guidance
- `spec-kit-templates/schemas/requirements-spec.schema.json` - **AUTHORITATIVE SCHEMA** for YAML validation
- `scripts/validate-spec-structure.py` - YAML validation tool
- `scripts/generate-traceability-matrix.py` - Orphan detection tool

---

**Document Status**: Draft for Team Review  
**Next Review**: 2025-11-08 (Team kickoff meeting)  
**Approval Required**: Technical Lead, Requirements Engineer

**Session Completion**: This elicitation session provides the foundation for Phase 02 execution. Upon approval, proceed to Task 5 (Create SyRS) and Task 6 (Fix YAML).
