# GitHub Issue Traceability - Verified Extraction

**Extraction Date**: 2025-12-02  
**Data Source**: GitHub API (mcp_io_github_git_list_issues)  
**Total Issues**: 217 (all OPEN)  
**Repository**: zarfld/IEEE_1588_2019

## ⚠️ CRITICAL DISCREPANCY FOUND

**Issue #197 Mapping Error**:
- **Claimed in Issue Bodies**: Multiple issues claim "StR-005 (Maps to STR-PERF-001: Synchronization Accuracy)"
- **GitHub API Truth**: Issue #197 title is "**StR-005: Exclusion of Specific PTP Port States**"
- **Affected Issues**: #9, #180, #184, #189 (all incorrectly reference #197)

---

## Stakeholder Requirements (StR-* Pattern, Issues #193-#217)

| Issue # | Title | StR-ID | Category | Notes |
|---------|-------|--------|----------|-------|
| #217 | REQ-F-204: Peer-to-Peer Delay Mechanism for Full-Duplex Links | - | Functional | |
| #216 | REQ-F-205: Dataset/MIB-Based Management (No PTP Mgmt Messages under gPTP) | - | Functional | |
| #215 | REQ-F-201: Profile Strategy Selection (gPTP, Industrial, AES67) | - | Functional | |
| #214 | REQ-F-203: Domain 0 Default with External Control Disabled | - | Functional | |
| #213 | REQ-F-202: Deterministic BMCA per gPTP Constraints | - | Functional | |
| #212 | StR-016: IEC/IEEE 60802 Four Synchronization Domains | StR-016 | IEC/IEEE 60802 | |
| #211 | StR-020: IEC/IEEE 60802 Capability to Disable EEE | StR-020 | IEC/IEEE 60802 | |
| #210 | StR-017: IEC/IEEE 60802 CMLDS Mandatory Requirement | StR-017 | IEC/IEEE 60802 | |
| #209 | StR-019: IEC/IEEE 60802 Convergence <1 µs in <1 Second per Hop | StR-019 | IEC/IEEE 60802 | |
| #208 | StR-018: IEC/IEEE 60802 Timestamp Accuracy ≤8 ns | StR-018 | IEC/IEEE 60802 | |
| #207 | StR-012: CMLDS Mandatory for Multi-Domain Support | StR-012 | Multi-Domain | |
| #206 | StR-015: Optional Delay Asymmetry Modeling and Compensation | StR-015 | Optional Features | |
| #205 | StR-011: Optional Support for Multiple PTP Domains (1-127) | StR-011 | Optional Features | |
| #204 | StR-013: Optional External Port Configuration Support | StR-013 | Optional Features | |
| #203 | StR-014: Optional One-Step Transmit/Receive Mode Support | StR-014 | Optional Features | |
| #202 | StR-007: Management via 802.1AS Data Sets and MIB | StR-007 | Management | |
| #201 | StR-008: Exclusion of IEEE 1588 Integrated Security | StR-008 | Security | |
| #200 | StR-010: LocalClock Frequency Offset and Measurement Granularity | StR-010 | Clock | |
| #199 | StR-006: Exclusion of Foreign Master Feature | StR-006 | Exclusions | |
| #198 | StR-009: Prohibition of MAC PAUSE and PFC on 802.1AS Traffic | StR-009 | Network | |
| **#197** | **StR-005: Exclusion of Specific PTP Port States** | **StR-005** | **Exclusions** | **⚠️ NOT Synchronization!** |
| #196 | StR-003: BMCA Implementation per 802.1AS with Domain 0 Constraints | StR-003 | BMCA | |
| #195 | StR-001: P2P Path Delay Mechanism on Full-Duplex 802.3 Links | StR-001 | Delay Mechanism | |
| #194 | StR-004: Path Trace TLV Processing and Transmission | StR-004 | Path Trace | |
| #193 | StR-002: Full-Duplex Point-to-Point 802.3 with Untagged Frames | StR-002 | Network | |

---

## GPS-PPS Requirements (Issues #9-#17)

| Issue # | Title | Requirement ID | Notes |
|---------|-------|----------------|-------|
| #17 | REQ-STK-PTP-004: Hardware-agnostic cross-platform implementation | REQ-STK-PTP-004 | |
| #16 | REQ-STK-PTP-002: Deterministic timing behavior for real-time media | REQ-STK-PTP-002 | |
| #15 | REQ-PPS-004: Fallback to NMEA-Only Mode | REQ-PPS-004 | **⚠️ Claims "Traces to StR-005" - INCORRECT** |
| #14 | REQ-PPS-006: Thread-Safe State Transitions | REQ-PPS-006 | |
| #13 | REQ-PPS-005: Non-Blocking Detection with Timeout | REQ-PPS-005 | |
| #12 | REQ-PPS-003: 1Hz Frequency Validation | REQ-PPS-003 | **⚠️ Claims "Traces to StR-005" - INCORRECT** |
| #11 | REQ-PPS-002: Sub-Microsecond Timestamp Accuracy | REQ-PPS-002 | **⚠️ Claims "Traces to StR-005" - INCORRECT** |
| #10 | REQ-PPS-007: Platform Abstraction (Win/Linux/Embedded) | REQ-PPS-007 | |
| **#9** | **REQ-PPS-001: PPS Pin Autodetection** | REQ-PPS-001 | **⚠️ Claims "Traces to #197" - INCORRECT** |

---

## Roadmap / Feature Requests (Issues #1-#8)

| Issue # | Title | Type | Milestone |
|---------|-------|------|-----------|
| #8 | Conformance Test Suite Automation (IEEE 1588-2019 and 802.1AS) | Enhancement | v1.1.0 |
| #7 | Performance Benchmarking Tools (Automated Accuracy/Latency/CPU Measurement) | Enhancement | v1.1.0 |
| #6 | Additional HAL Implementations (Embedded Platforms: ARM, RISC-V, RTOS) | Enhancement | v1.1.0 |
| #5 | AVnu Milan Profile Compliance (Professional Audio/Video) | Enhancement | v2.0.0 |
| #4 | IEEE 1722-2016 (AVTP) Audio Video Transport Protocol | Enhancement | v1.3.0 |
| #3 | IEEE 802.1AS-2020 (gPTP) Full Implementation | Enhancement | v1.2.0 |
| #2 | IEEE 1588-2019 Optional Features Implementation | Enhancement | v1.1.0 |
| #1 | 🚀 v1.0.0-MVP Release Checklist (Target: 2025-11-25) | Release | v1.0.0-MVP |

---

## Stakeholder Requirements (REQ-STK-*, Issues #18-#33)

| Issue # | Title | Requirement ID |
|---------|-------|----------------|
| #33 | REQ-STK-PTP-019: Traceability to UTC for regulatory compliance | REQ-STK-PTP-019 |
| #32 | REQ-STK-PTP-018: Security framework compliance for critical infrastructure | REQ-STK-PTP-018 |
| #31 | REQ-STK-PTP-017: Full IEEE 1588-2019 standards compliance | REQ-STK-PTP-017 |
| #30 | REQ-STK-PTP-016: Bounded execution time for real-time applications | REQ-STK-PTP-016 |
| #29 | REQ-STK-PTP-020: Comprehensive validation and certification procedures | REQ-STK-PTP-020 |
| #28 | REQ-STK-PTP-013: Deterministic APIs without dynamic memory allocation | REQ-STK-PTP-013 |
| #27 | REQ-STK-PTP-012: Comprehensive management and diagnostic capabilities | REQ-STK-PTP-012 |
| #26 | REQ-STK-PTP-014: Hardware abstraction for cross-platform development | REQ-STK-PTP-014 |
| #25 | REQ-STK-PTP-011: Backward compatibility with IEEE 1588-2008 and 802.1AS | REQ-STK-PTP-011 |
| #24 | REQ-STK-PTP-015: Comprehensive error handling without exceptions | REQ-STK-PTP-015 |
| #23 | REQ-STK-PTP-007: Transparent clock support for industrial Ethernet | REQ-STK-PTP-007 |
| #22 | REQ-STK-PTP-008: Management protocol for network configuration | REQ-STK-PTP-008 |
| #21 | REQ-STK-PTP-006: Security mechanisms for industrial network protection | REQ-STK-PTP-006 |
| #20 | REQ-STK-PTP-009: Scalable multi-domain timing architecture | REQ-STK-PTP-009 |
| #19 | REQ-STK-PTP-010: Enhanced calibration procedures | REQ-STK-PTP-010 |
| #18 | REQ-STK-PTP-001: Enterprise-grade timing precision beyond basic gPTP | REQ-STK-PTP-001 |

---

## System Requirements (REQ-SYS-*, Issues #34-#45)

| Issue # | Title | Requirement ID |
|---------|-------|----------------|
| #45 | REQ-SYS-PTP-013: Code Quality Standards (80% Test Coverage, Static Analysis) | REQ-SYS-PTP-013 |
| #44 | REQ-SYS-PTP-012: Operational Support (Logging, Diagnostics, Remote Management) | REQ-SYS-PTP-012 |
| #43 | REQ-SYS-PTP-011: Hardware Abstraction Layer (HAL) Requirements | REQ-SYS-PTP-011 |
| #42 | REQ-SYS-PTP-010: Performance Requirements (Microsecond Sync, Deterministic Execution) | REQ-SYS-PTP-010 |
| #41 | REQ-SYS-PTP-009: Backward Compatibility with IEEE 1588-2008 | REQ-SYS-PTP-009 |
| #40 | REQ-SYS-PTP-008: Standards Compliance (IEEE 1588-2019, Safety Standards) | REQ-SYS-PTP-008 |
| #39 | REQ-SYS-PTP-007: Cross-Platform Support (Windows, Linux, Embedded RTOS) | REQ-SYS-PTP-007 |
| #38 | REQ-SYS-PTP-003: Enhanced security mechanisms | REQ-SYS-PTP-003 |
| #37 | REQ-SYS-PTP-004: Comprehensive management protocol | REQ-SYS-PTP-004 |
| #36 | REQ-SYS-PTP-001: Enterprise-grade timing synchronization beyond gPTP | REQ-SYS-PTP-001 |
| #35 | REQ-SYS-PTP-005: Deterministic design patterns for time-sensitive applications | REQ-SYS-PTP-005 |
| #34 | REQ-SYS-PTP-002: Multi-domain timing architecture (Post-MVP) | REQ-SYS-PTP-002 |

---

## Functional Requirements (REQ-FUN-*, Issues #46-#93)

*[217 issues total - functional requirements span #46-#93, 48 issues]*

**Key Functional Requirements**:
- #50: REQ-FUN-PTP-001: Fundamental data types (ClockIdentity, PortNumber, etc.)
- #48: REQ-FUN-PTP-002: 48-bit timestamp precision
- #46: REQ-FUN-PTP-003: CorrectionField with scaled nanosecond representation
- #47: REQ-FUN-PTP-004: All IEEE 1588-2019 integer types
- #49: REQ-FUN-PTP-005: Complete PTP message header structure
- #55: REQ-FUN-PTP-006: All PTP message types
- #53: REQ-FUN-PTP-007: Message serialization/deserialization
- #52: REQ-FUN-PTP-008: TLV framework for protocol extensions
- #54: REQ-FUN-PTP-009: Ordinary Clock state machines
- #51: REQ-FUN-PTP-010: Boundary Clock functionality

*(Full list available in GitHub issues #46-#93)*

---

## Non-Functional Requirements (REQ-NFR-*, Issues #94-#117)

*[24 non-functional requirements spanning #94-#117]*

**Key Non-Functional Requirements**:
- #95: REQ-NFR-PTP-001: Microsecond-level timing accuracy (±1μs typical)
- #94: REQ-NFR-PTP-002: Sub-microsecond accuracy with hardware timestamping (±100ns)
- #98: REQ-NFR-PTP-003: Timing accuracy under network load and jitter
- #96: REQ-NFR-PTP-004: Deterministic timing behavior
- #97: REQ-NFR-PTP-005: Bounded execution time (<10μs)
- #102: REQ-NFR-PTP-006: No dynamic memory allocation in critical paths
- #101: REQ-NFR-PTP-007: Predictable CPU usage
- #100: REQ-NFR-PTP-008: High-frequency timing operations (1000+ Hz)
- #99: REQ-NFR-PTP-009: Graceful degradation under network faults

*(Full list available in GitHub issues #94-#117)*

---

## Functional/System Requirements (REQ-F-*, REQ-S-*, Issues #118-#192)

*[75 detailed functional and system requirements spanning #118-#192]*

**Sample Requirements**:
- #180: REQ-F-003: Core Message Structures & Types (⚠️ **Claims StR-005 mapping - INCORRECT**)
- #184: REQ-S-002: Fault Recovery with Announce Timeout (⚠️ **Claims StR-005 mapping - INCORRECT**)
- #189: REQ-NF-P-001: Timing Accuracy <1μs with HW Timestamps (⚠️ **Claims StR-005 mapping - INCORRECT**)

*(Full list available in GitHub issues #118-#192)*

---

## 🚨 ACTION ITEMS - Issues Requiring Manual Correction

### PRIORITY 1: Fix Incorrect StR-005 References

**Issues claiming "StR-005 = Synchronization Accuracy" (WRONG)**:
1. **Issue #9** (REQ-PPS-001): Claims "Traces to: #197 (StR-005: Exclusion of Specific PTP Port States - Synchronization Accuracy)"
   - **CORRECTION NEEDED**: #197 is about **Port State Exclusions**, NOT Synchronization
   - **FIND**: Which issue represents STR-PERF-001 (Synchronization Accuracy)?

2. **Issue #15** (REQ-PPS-004): Claims "Traces to: StR-005 (Synchronization Accuracy)"
3. **Issue #12** (REQ-PPS-003): Claims "Traces to: StR-005 (Synchronization Accuracy)"
4. **Issue #11** (REQ-PPS-002): Claims "Traces to: StR-005 (Synchronization Accuracy)"
5. **Issue #180** (REQ-F-003): Claims "StR-005 (Maps to STR-PERF-001: Synchronization Accuracy)"
6. **Issue #184** (REQ-S-002): Claims "StR-005 (Maps to STR-PERF-001: Synchronization Accuracy)"
7. **Issue #189** (REQ-NF-P-001): Claims "StR-005 (Maps to STR-PERF-001: Synchronization Accuracy)"

### PRIORITY 2: Identify Missing Mappings

**URGENT QUESTION**: Which GitHub issue represents **STR-PERF-001 (Synchronization Accuracy)** from stakeholder-requirements-spec.md?

**Search Strategy**:
- Look for "Synchronization Accuracy" or "timing accuracy" in issues #193-#217 (StR-* range)
- Check non-functional requirements (#94-#117) for accuracy requirements
- Verify against stakeholder-requirements-spec.md definitions

---

## Verification Notes

**Data Source**: GitHub API via mcp_io_github_git_list_issues  
**Query Pagination**:
- Page 1: Issues #118-#217 (100 issues)
- Page 2: Issues #1-#117 (117 issues)

**Verification Method**: Direct API extraction ensures no human transcription errors  
**Confidence Level**: HIGH (GitHub API is authoritative source)  
**Discrepancies**: 7+ issues with incorrect StR-005 mappings identified

---

## Next Steps

1. ✅ **COMPLETED**: Extract all 217 GitHub issue titles from API
2. 🚧 **IN PROGRESS**: Create this verified mapping table
3. ⏳ **PENDING**: Identify correct mapping for STR-PERF-001 (Synchronization Accuracy)
4. ⏳ **PENDING**: Manually update all 7+ issues with incorrect "Traces to" links
5. ⏳ **PENDING**: Re-run CI validation to confirm ≥95% traceability coverage

---

*This document supersedes previous traceability documentation. Use this as the authoritative source for all GitHub issue mappings.*
