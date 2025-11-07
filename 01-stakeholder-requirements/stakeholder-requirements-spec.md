# Stakeholder Requirements Specification
## IEEE 1588-2019 PTP Open-Source Implementation

**Document ID**: STR-SPEC-001  
**Version**: 1.0  
**Date**: 2025-11-07  
**Status**: Draft for Stakeholder Review  
**Compliance**: ISO/IEC/IEEE 29148:2018 - Requirements Engineering Processes

---

## Document Control

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2025-11-07 | Project Team | Initial draft from brainstorming sessions |
| 1.0 | 2025-11-07 | Project Team | Complete draft for stakeholder review |

**Approvers**: 
- ✅ **Stakeholder Consortium Review** (2025-11-07) - Consolidated approval with recommendations
- Status: **APPROVED to proceed to Phase 01 implementation**

**Distribution**: Project Team, Stakeholders (13 identified groups)

---

## 1. Introduction

### 1.1 Purpose

This document specifies the stakeholder requirements for an **open-source, hardware-agnostic implementation of IEEE 1588-2019 Precision Time Protocol (PTP)**. It defines the business needs, constraints, and success criteria that guide the technical system requirements and design phases.

### 1.2 Scope

**In Scope**:

- Core IEEE 1588-2019 protocol implementation (message formats, state machines, algorithms)
- Hardware abstraction layer for cross-platform deployment
- Real-time system compatibility (static allocation, deterministic timing)
- Platform support: ARM Cortex-M7 (embedded), x86-64 (desktop/server)
- Testing framework (unit tests, integration tests, conformance tests)
- Documentation (API, tutorials, porting guides)

**Out of Scope** (Post-MVP):

- Higher-layer protocols (IEEE 802.1AS gPTP profile extensions, IEEE 1722 AVTP integration)
- Graphical user interface tools (configuration GUIs, monitoring dashboards)
- Commercial support services
- Hardware-specific optimizations beyond reference HAL implementations

### 1.3 System Overview

The IEEE 1588-2019 PTP implementation provides sub-microsecond precision clock synchronization across distributed systems. It serves as the **foundational timing protocol** for:

- Professional audio/video systems (AES67, AVB/Milan)
- Industrial automation (IEC 61158, PROFINET)
- Telecommunications (ITU-T G.8275 profiles)
- Time-sensitive networking (TSN) applications
- Distributed measurement and control systems

**Key System Characteristics**:

- **Hardware-Agnostic**: C function pointer HAL enables deployment on diverse platforms
- **Real-Time Safe**: No dynamic allocation, bounded execution time, deterministic behavior
- **Standards-Compliant**: Faithful implementation of IEEE 1588-2019 specification
- **Modular**: Compile-time feature selection, optional components
- **Testable**: Mock hardware interfaces, CI/CD pipeline, conformance test suite

### 1.4 References

- **[IEEE-1588-2019]**: IEEE Standard for a Precision Clock Synchronization Protocol for Networked Measurement and Control Systems
- **[ISO-29148]**: ISO/IEC/IEEE 29148:2018 - Systems and software engineering — Life cycle processes — Requirements engineering
- **[IEEE-12207]**: ISO/IEC/IEEE 12207:2017 - Systems and software engineering — Software life cycle processes
- **[IEEE-42010]**: ISO/IEC/IEEE 42010:2011 - Systems and software engineering — Architecture description
- **[TSEP-2024]**: Technical Software Engineering Plazotta, "Problems and Solutions for IEEE 1588 Implementations" (https://tsep.com)
- **[OCP-TAP]**: Open Compute Project, Time Appliances Project (https://www.opencompute.org/wiki/Time_Appliances_Project)

### 1.5 Definitions and Acronyms

| Term | Definition |
|------|------------|
| **PTP** | Precision Time Protocol (IEEE 1588-2019) |
| **BMCA** | Best Master Clock Algorithm - mechanism for selecting the master clock in a PTP domain |
| **HAL** | Hardware Abstraction Layer - interface between platform-specific and platform-agnostic code |
| **Grandmaster (GM)** | PTP clock that serves as the timing reference for a domain |
| **Ordinary Clock (OC)** | PTP clock with a single PTP port, typically a slave or master |
| **Boundary Clock (BC)** | PTP clock with multiple ports that synchronizes to a master and provides synchronization to slaves |
| **Transparent Clock (TC)** | PTP clock that updates correction fields in PTP messages to account for residence time |
| **E2E** | End-to-End delay mechanism (using Delay_Req/Delay_Resp messages) |
| **P2P** | Peer-to-Peer delay mechanism (using Pdelay_Req/Pdelay_Resp messages) |
| **RTOS** | Real-Time Operating System (e.g., FreeRTOS, Zephyr, ThreadX) |
| **TLV** | Type-Length-Value - extensible message format used in PTP |
| **MVP** | Minimum Viable Product - initial functional subset for early adopters |

---

## 2. Stakeholder Identification

### 2.1 Primary Stakeholders

#### STK-001: Makers and Embedded Developers

**Role**: Engineers building time-sensitive applications on microcontrollers and embedded systems

**Key Needs**:

- Drop-in PTP library requiring minimal integration effort
- Proven real-time performance on resource-constrained devices
- Clear porting guide for new hardware platforms
- Examples demonstrating typical use cases

**Success Criteria**: Integrate PTP into FreeRTOS application in <4 hours

#### STK-002: Audio Equipment Manufacturers

**Role**: Companies producing professional audio devices requiring <1µs synchronization

**Key Needs**:

- AES67 and AVnu Milan profile compatibility
- Sub-microsecond synchronization accuracy
- Standards compliance for product certification
- Royalty-free, open-source licensing

**Success Criteria**: Pass AVnu Milan conformance tests using this implementation

#### STK-003: System Integrators

**Role**: Engineers deploying mixed-vendor timing systems in industrial, telecom, or media facilities

**Key Needs**:

- Interoperability with commercial PTP devices (Meinberg, Oregano, etc.)
- Consistent behavior across heterogeneous hardware
- Diagnostic and monitoring capabilities
- Field-proven reliability

**Success Criteria**: Achieve stable synchronization in mixed-vendor network for 30+ days

#### STK-004: QA and Test Engineers

**Role**: Personnel validating timing behavior, performance, and standards compliance

**Key Needs**:

- Testable architecture (dependency injection, mocks)
- Reproducible test scenarios without hardware dependencies
- Performance measurement tools (jitter, offset, convergence time)
- Conformance test suite for regression testing

**Success Criteria**: Run comprehensive test suite in CI environment (GitHub Actions) in <10 minutes

#### STK-005: Standards Bodies (IEEE, AVnu, AES)

**Role**: Organizations defining timing standards and certifying compliance

**Key Needs**:

- Reference implementation demonstrating spec feasibility
- Public, auditable source code for validation
- Accurate interpretation of standards documents
- Feedback channel for spec ambiguities

**Success Criteria**: IEEE P1588 working group references this implementation in clarifications

#### STK-006: Project Maintainers and Core Contributors

**Role**: Developers responsible for long-term codebase health and community growth

**Key Needs**:

- Maintainable, well-documented code architecture
- Automated quality gates (CI/CD, static analysis, fuzzing)
- Clear contributor guidelines and onboarding process
- Sustainable development model (community + sponsorship)

**Success Criteria**: 10 active contributors by Month 12, <7 day median PR review time

### 2.2 Secondary Stakeholders

#### STK-007: Quality Assurance and Test Teams

**Role**: Dedicated QA teams in product development organizations

**Key Needs**: Validation tools, test vectors, automated regression suites

#### STK-008: Operations and IT Administrators

**Role**: Personnel deploying and monitoring PTP in production environments

**Key Needs**: Configuration management, health metrics, troubleshooting documentation

#### STK-009: Regulators and Compliance Officers

**Role**: Ensuring systems meet industry regulations (MiFID II finance, synchrophasor grids)

**Key Needs**: Traceability, audit logs, compliance documentation

#### STK-010: Academic and Research Institutions

**Role**: Universities and research labs studying time synchronization

**Key Needs**: Modifiable reference implementation, educational documentation, research dataset compatibility

#### STK-011: Open-Source Maintainers and Contributors

**Role**: Community members extending the library with new features and platforms

**Key Needs**: Modular architecture, comprehensive developer documentation, recognition model

#### STK-012: Hardware and Semiconductor Vendors

**Role**: NIC manufacturers, MCU vendors showcasing PTP capabilities

**Key Needs**: Easy hardware integration, showcasing platform performance, HAL contribution process

#### STK-013: End Customers of Integrated Systems

**Role**: Users of products containing PTP (power utilities, broadcasters, factories)

**Key Needs**: Reliability, "it just works" experience, minimal field issues

---

## 3. Stakeholder Requirements

### 3.1 Standards Compliance Requirements (Theme 1)

#### STR-STD-001: IEEE 1588-2019 Protocol Compliance

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-002, STK-003, STK-005
- **Rationale**: Without standards compliance, interoperability and certification are impossible

**Requirement**: The implementation SHALL conform to all mandatory provisions of IEEE 1588-2019 standard.

**Success Criteria**:

- All message types (Sync, Delay_Req, Follow_Up, Delay_Resp, Announce, Signaling, Management) implemented per Section 13
- BMCA implementation per Section 9.3
- Clock state machines per Sections 9.2 (ordinary clock), 10.3 (boundary clock), 10.4 (transparent clock)
- Delay mechanisms: E2E (Section 11.3) and P2P (Section 11.4)

**Acceptance Criteria** (Gherkin):

```gherkin
Given a IEEE 1588-2019 compliant Grandmaster clock
When the implementation operates as a slave ordinary clock
Then it SHALL synchronize within 1 microsecond of the GM
And it SHALL select the correct master using BMCA
And it SHALL handle all mandatory PTP message types
```

**Dependencies**: Access to IEEE 1588-2019 specification document

**Risks**: Specification ambiguities may require IEEE P1588 working group clarification

---

#### STR-STD-002: Message Format Correctness

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-002, STK-003, STK-005
- **Rationale**: Incorrect message formats cause interoperability failures

**Requirement**: All PTP message serialization and deserialization SHALL conform byte-for-byte to IEEE 1588-2019 Section 13.

**Success Criteria**:

- Network byte order (big-endian) for all multi-byte fields
- Correct flag bit encoding (Table 37)
- TLV format compliance (Section 14)
- Timestamp format compliance (5.3.3)

**Acceptance Criteria** (Gherkin):

```gherkin
Given a PTP Sync message structure
When serialized to network format
Then Wireshark PTP dissector SHALL parse it without errors
And all field values SHALL match IEEE 1588-2019 Table 26
```

**Dependencies**: None

**Risks**: Low - well-defined specification

---

#### STR-STD-003: Best Master Clock Algorithm (BMCA)

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-002, STK-003, STK-005
- **Rationale**: BMCA is core to automatic master selection in distributed systems

**Requirement**: The BMCA implementation SHALL select the best master clock according to IEEE 1588-2019 Section 9.3.

**Success Criteria**:

- Implement dataset comparison algorithm (Figure 27)
- Handle all comparison steps (priority1, clock class, accuracy, variance, priority2, etc.)
- Support Announce message qualification (Figure 32)
- Implement state decision algorithm (Figure 26)

**Acceptance Criteria** (Gherkin):

```gherkin
Given three PTP clocks with different priorities (priority1: 128, 64, 200)
When they all transmit Announce messages
Then the clock with priority1=64 SHALL be selected as master
And all other clocks SHALL enter slave state
And state SHALL remain stable for 1000 Announce intervals
```

**Dependencies**: Announce message processing (STR-STD-002)

**Risks**: BMCA state machine has complex edge cases; requires exhaustive testing

---

#### STR-STD-004: Interoperability with Commercial Devices

- **Priority**: P1 (High - MVP Desired)
- **Source**: STK-003, STK-012
- **Rationale**: Real-world deployments include mixed vendors

**Requirement**: The implementation SHALL interoperate with at least 3 commercial PTP devices from different vendors.

**Success Criteria**:

- Synchronize as slave to commercial Grandmaster (e.g., Meinberg, Oregano, Microchip)
- Operate in network with linuxptp devices
- Handle vendor-specific TLVs gracefully (ignore unknown types)

**Acceptance Criteria** (Gherkin):

```gherkin
Given a commercial PTP Grandmaster clock
When the implementation connects to the same network
Then it SHALL synchronize within specification (<1µs)
And it SHALL maintain synchronization for 24 hours
And it SHALL not cause Grandmaster to generate error messages
```

**Dependencies**: Access to commercial PTP equipment for validation

**Risks**: Vendor-specific behaviors may expose edge cases

---

### 3.2 Performance Requirements (Theme 2)

#### STR-PERF-001: Synchronization Accuracy

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-002, STK-003, STK-013
- **Rationale**: Sub-microsecond accuracy is the primary value proposition

**Requirement**: The implementation SHALL achieve clock offset <1 microsecond from master under typical conditions.

**Success Criteria**:

- Offset <1µs median on capable hardware (Intel I210 NIC + hardware timestamping)
- Offset <100ns stretch goal on high-end platforms
- Convergence to target accuracy within 60 seconds

**Acceptance Criteria** (Gherkin):

```gherkin
Given a PTP Grandmaster with GPS-disciplined oscillator
And a slave clock with hardware timestamp support (Intel I210 NIC)
When synchronized for 10 minutes
Then 95% of offset samples SHALL be <1 microsecond
And 99% of offset samples SHALL be <2 microseconds
```

**Dependencies**: Hardware timestamp HAL (STR-PORT-001), PI controller (STR-PERF-003)

**Risks**: Accuracy is hardware-dependent; may not achieve on all platforms

---

#### STR-PERF-002: Timing Determinism

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-001, STK-002
- **Rationale**: Real-time systems require bounded execution time

**Requirement**: All time-critical code paths SHALL have deterministic, bounded execution time.

**Success Criteria**:

- No dynamic memory allocation in timestamp handling, servo, or message processing
- No unbounded loops in critical paths
- Worst-case execution time (WCET) measured and documented

**Acceptance Criteria** (Gherkin):

```gherkin
Given the implementation running on ARM Cortex-M7 @ 400 MHz
When processing a Sync message with hardware timestamp
Then execution time SHALL be <100 microseconds (WCET)
And heap allocation calls SHALL be zero in timestamp/servo paths
```

**Dependencies**: Design review, static analysis tools

**Risks**: Accidental allocation or blocking calls during development

---

#### STR-PERF-003: Clock Servo Performance

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-002, STK-003
- **Rationale**: Clock servo controls synchronization quality

**Requirement**: The clock servo SHALL minimize offset and jitter using a PI (proportional-integral) controller.

**Success Criteria**:

- Convergence time: <60 seconds to target accuracy
- Jitter: <50ns standard deviation (on capable hardware)
- Frequency stability: ±10 ppb after convergence

**Acceptance Criteria** (Gherkin):

```gherkin
Given an unsynchronized slave clock
When it receives Sync messages from a stable Grandmaster
Then offset SHALL converge to <1µs within 60 seconds
And offset jitter (std dev) SHALL be <50ns after convergence
```

**Dependencies**: Path delay measurement (STR-PERF-004)

**Risks**: PI controller tuning is hardware-specific; may require adaptive algorithms

---

#### STR-PERF-004: Path Delay Measurement

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-002, STK-003
- **Rationale**: Accurate delay measurement is essential for offset calculation

**Requirement**: The implementation SHALL measure network propagation delay using E2E or P2P mechanisms.

**Success Criteria**:

- E2E (End-to-End) delay mechanism per Section 11.3
- P2P (Peer-to-Peer) delay mechanism per Section 11.4 (optional for MVP)
- Outlier filtering for robust delay estimation

**Acceptance Criteria** (Gherkin):

```gherkin
Given a slave clock and master clock separated by 100m of Ethernet cable
When measuring propagation delay using E2E mechanism
Then measured delay SHALL be within 10% of cable propagation delay (≈500ns)
And delay SHALL remain stable (std dev <50ns) over 1000 measurements
```

**Dependencies**: Hardware timestamping (STR-PORT-001)

**Risks**: Asymmetric delays (different TX/RX path lengths) reduce accuracy

---

#### STR-PERF-005: Resource Efficiency

- **Priority**: P1 (High - MVP Desired)
- **Source**: STK-001, STK-012
- **Rationale**: Embedded systems have tight resource constraints

**Requirement**: The implementation SHALL operate within typical embedded system resource budgets.

**Success Criteria**:

- CPU usage: <10% on ARM Cortex-M7 @ 400 MHz during steady-state sync
- RAM usage: <50 KB for core protocol (excluding buffers)
- Flash usage: <100 KB for core protocol (without debug/logging)

**Acceptance Criteria** (Gherkin):

```gherkin
Given the implementation compiled with optimization (-O2) for ARM Cortex-M7
When running steady-state synchronization (8 Sync/sec)
Then CPU utilization SHALL be <10% (measured with SystemView)
And RAM footprint SHALL be <50 KB (measured with linker map)
```

**Dependencies**: Profiling tools, reference hardware

**Risks**: Feature additions may increase resource usage; require monitoring

---

### 3.3 Portability Requirements (Theme 3)

#### STR-PORT-001: Hardware Abstraction Layer (HAL)

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-001, STK-006, STK-012
- **Rationale**: Platform independence is a core differentiator

**Requirement**: The core protocol implementation SHALL NOT contain platform-specific code. All hardware and OS interactions SHALL occur through a HAL.

**Success Criteria**:

- HAL interfaces defined for: network (TX/RX), timestamping (HW timestamps), clock (read/write/adjust), OS (timers, logging)
- HAL implemented as C function pointers (not C++ virtual functions)
- Core protocol code compiles without hardware headers

**Acceptance Criteria** (Gherkin):

```gherkin
Given the core PTP protocol source files
When compiled with -DHAL_MOCK=1 (mock HAL)
Then compilation SHALL succeed on any ANSI C99 compiler
And no platform-specific headers SHALL be included (#error if found)
```

**Dependencies**: HAL design document (ADR)

**Risks**: HAL abstraction may not fit all hardware models; requires iteration

---

#### STR-PORT-002: Reference HAL Implementations

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-001, STK-003, STK-012
- **Rationale**: Reference implementations guide community porting

**Requirement**: The project SHALL provide at least 2 reference HAL implementations for diverse platforms.

**Success Criteria**:

- HAL #1: x86-64 Linux user-space (using Linux socket timestamping API)
- HAL #2: ARM Cortex-M7 + FreeRTOS (e.g., STM32H7 with Ethernet MAC timestamps)
- Both HALs demonstrate sub-microsecond synchronization

**Acceptance Criteria** (Gherkin):

```gherkin
Given the x86-64 Linux reference HAL
When running on Ubuntu 22.04 with Intel I210 NIC
Then synchronization accuracy SHALL be <1 microsecond
And build process SHALL be documented in README
```

**Dependencies**: Hardware availability for testing

**Risks**: Hardware procurement delays; partner with vendors for evaluation boards

---

#### STR-PORT-003: No OS Assumptions

- **Priority**: P1 (High - MVP Desired)
- **Source**: STK-001, STK-012
- **Rationale**: Enable bare-metal and diverse RTOS deployments

**Requirement**: The core protocol SHALL NOT assume specific OS features (threads, heap, stdlib beyond basics).

**Success Criteria**:

- No direct calls to malloc/free (use HAL memory allocator)
- No POSIX dependencies (pthread, sockets, etc.)
- Optional logging via HAL callback (no printf dependency)

**Acceptance Criteria** (Gherkin):

```gherkin
Given the core PTP protocol compiled for bare-metal ARM Cortex-M7
When analyzing symbols with 'nm' tool
Then no references to malloc, free, pthread, or POSIX functions SHALL exist
```

**Dependencies**: Design discipline, static analysis

**Risks**: Low - achievable with careful design

---

#### STR-PORT-004: Cross-Platform Build System

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-001, STK-004, STK-006
- **Rationale**: Developers need frictionless builds on diverse hosts

**Requirement**: The project SHALL use CMake as the primary build system, supporting multiple toolchains and targets.

**Success Criteria**:

- CMake >= 3.20 required
- Support host builds (x86-64 Linux, x86-64 Windows, macOS)
- Support cross-compilation (ARM GCC, Clang, MSVC)
- Integration with Google Test (unit tests) and Unity (embedded tests)

**Acceptance Criteria** (Gherkin):

```gherkin
Given a fresh clone of the repository
When running 'cmake -B build && cmake --build build'
Then build SHALL succeed on Ubuntu 22.04, Windows 11, and macOS 13
And all unit tests SHALL execute via 'ctest'
```

**Dependencies**: CMake expertise, CI runners

**Risks**: Toolchain-specific quirks require ongoing maintenance

---

### 3.4 Security Requirements (Theme 4)

#### STR-SEC-001: Input Validation

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-004, STK-006, STK-009
- **Rationale**: Prevent exploits via malformed PTP messages

**Requirement**: All received PTP messages SHALL be validated before processing.

**Success Criteria**:

- Message length checks (prevent buffer overruns)
- Field range validation (e.g., TLV lengths, domain numbers)
- Reject malformed messages without crashing

**Acceptance Criteria** (Gherkin):

```gherkin
Given a fuzzing test with 1 million malformed PTP messages
When processed by the message parser
Then zero crashes SHALL occur
And all invalid messages SHALL be rejected with error codes
```

**Dependencies**: Fuzzing infrastructure (AFL, LibFuzzer)

**Risks**: Subtle parsing bugs may remain; continuous fuzzing needed

---

#### STR-SEC-002: No Buffer Overruns

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-006, STK-009
- **Rationale**: Buffer overruns are a common vulnerability class

**Requirement**: All buffer operations SHALL be bounds-checked.

**Success Criteria**:

- Use safe string functions (strncpy, snprintf, not strcpy/sprintf)
- Static analysis with Coverity, PVS-Studio, or Clang Static Analyzer
- Zero "out-of-bounds" findings in static analysis

**Acceptance Criteria** (Gherkin):

```gherkin
Given the source code analyzed by Coverity
When checking for buffer overrun vulnerabilities
Then zero "out-of-bounds write" or "buffer overrun" defects SHALL be reported
```

**Dependencies**: Static analysis tools access

**Risks**: False positives may require triage; budget time for review

---

#### STR-SEC-003: Security Audit

- **Priority**: P1 (High - MVP Desired)
- **Source**: STK-006, STK-009, STK-013
- **Rationale**: Independent review builds trust

**Requirement**: The implementation SHALL undergo external security audit before 1.0 release.

**Success Criteria**:

- Audit by firm with embedded systems security expertise
- Focus areas: message parsers, memory safety, cryptographic implementations (if any)
- All critical findings remediated before release

**Acceptance Criteria** (Gherkin):

```gherkin
Given a completed security audit report
When reviewing findings
Then zero critical vulnerabilities SHALL remain unresolved
And all high vulnerabilities SHALL be resolved or documented as accepted risk
```

**Dependencies**: Budget for security audit ($5K-$15K estimated)

**Risks**: Findings may delay 1.0 release; plan contingency time

---

#### STR-SEC-004: Optional Authentication (Post-MVP)

- **Priority**: P2 (Medium - Post-MVP)
- **Source**: STK-009, STK-002
- **Rationale**: High-security environments require authenticated time

**Requirement**: The implementation SHALL support IEEE 1588-2019 Annex K (Integrated Security) as an optional feature.

**Success Criteria**:

- HMAC-SHA256 authentication of PTP messages
- Optional encryption of management messages
- Compile-time configurable (can be excluded if not needed)

**Acceptance Criteria** (Gherkin):

```gherkin
Given the implementation compiled with -DENABLE_SECURITY=ON
When exchanging authenticated PTP messages
Then unauthorized messages SHALL be rejected
And synchronization SHALL succeed only with valid HMAC
```

**Dependencies**: Cryptographic library integration (mbedTLS or similar)

**Risks**: Crypto processing may impact real-time performance; requires tuning

---

### 3.5 Usability Requirements (Theme 5)

#### STR-USE-001: API Documentation

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-001, STK-004, STK-006
- **Rationale**: Developers cannot use undocumented APIs

**Requirement**: All public API functions SHALL be documented with usage examples.

**Success Criteria**:

- Doxygen-compatible comments for all public headers
- Generated HTML documentation published (e.g., GitHub Pages)
- Include parameter descriptions, return values, threading constraints, example code

**Acceptance Criteria** (Gherkin):

```gherkin
Given the public API header files
When processed by Doxygen
Then documentation SHALL be generated without errors
And all public functions SHALL have complete descriptions
```

**Dependencies**: Doxygen tool, CI integration

**Risks**: Low - documentation is effort, not complexity

---

#### STR-USE-002: Getting Started Tutorial

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-001, STK-003, STK-010
- **Rationale**: First impressions determine adoption

**Requirement**: The project SHALL provide a "Getting Started" tutorial covering 2 platforms.

**Success Criteria**:

- Tutorial #1: Linux x86-64 with Intel I210 NIC
- Tutorial #2: ARM Cortex-M7 (STM32H7) with FreeRTOS
- Each tutorial: hardware setup, build steps, running example, expected output, troubleshooting

**Acceptance Criteria** (Gherkin):

```gherkin
Given a developer following the Linux Getting Started tutorial
When completing all steps
Then they SHALL achieve <1µs synchronization within 30 minutes
And no undocumented steps SHALL be required
```

**Dependencies**: Example application code, documentation effort

**Risks**: Hardware availability for tutorial creation; consider loopback/simulation fallback

---

#### STR-USE-003: Example Applications

- **Priority**: P1 (High - MVP Desired)
- **Source**: STK-001, STK-010
- **Rationale**: Examples demonstrate typical usage patterns

**Requirement**: The project SHALL provide at least 3 example applications.

**Success Criteria**:

- Example #1: Basic slave clock (minimal code demonstrating synchronization)
- Example #2: Real-time embedded system (FreeRTOS with deterministic timing)
- Example #3: Time-sensitive networking (TSN-like prioritization)

**Acceptance Criteria** (Gherkin):

```gherkin
Given the "basic slave clock" example application
When compiled and run on Linux
Then it SHALL synchronize to a Grandmaster within 60 seconds
And source code SHALL be <200 lines with inline comments
```

**Dependencies**: HAL implementations, use case identification

**Risks**: Low - straightforward once core is working

---

#### STR-USE-004: Porting Guide

- **Priority**: P1 (High - MVP Desired)
- **Source**: STK-001, STK-011, STK-012
- **Rationale**: Community growth requires easy porting

**Requirement**: The project SHALL provide a comprehensive porting guide for adding new platforms.

**Success Criteria**:

- Documented HAL interface contracts (function signatures, semantics, threading)
- Step-by-step checklist for porting
- Template HAL implementation with TODOs
- Validation test suite for HAL correctness

**Acceptance Criteria** (Gherkin):

```gherkin
Given a developer porting to a new platform (e.g., NXP i.MX RT)
When following the porting guide
Then they SHALL complete a basic HAL implementation in <20 hours
And pass the HAL validation test suite
```

**Dependencies**: HAL design maturity, community feedback

**Risks**: Initial porting guides may miss edge cases; iterate based on feedback

---

### 3.6 Maintainability Requirements (Theme 5)

#### STR-MAINT-001: Code Quality

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-006, STK-009
- **Rationale**: High-quality code reduces bugs and eases maintenance

**Requirement**: The codebase SHALL meet defined quality standards.

**Success Criteria**:

- Unit test coverage: >80% for core protocol, >70% overall
- Static analysis: Zero critical defects (Coverity, PVS-Studio, or equivalent)
- Coding standard: MISRA-C guidelines (with documented deviations) or similar
- Code review: All PRs require approval from 2 maintainers

**Acceptance Criteria** (Gherkin):

```gherkin
Given a pull request to the main branch
When analyzed by CI pipeline
Then code coverage SHALL be ≥80% for modified files
And static analysis SHALL report zero new critical defects
And at least 2 maintainer approvals SHALL be required
```

**Dependencies**: CI/CD infrastructure, coverage tools (gcov, lcov)

**Risks**: Strict quality gates may slow development; balance rigor with velocity

---

#### STR-MAINT-002: Continuous Integration

- **Priority**: P0 (Critical - MVP Blocker)
- **Source**: STK-004, STK-006
- **Rationale**: Automated testing prevents regressions

**Requirement**: The project SHALL have a CI/CD pipeline running on all commits.

**Success Criteria**:

- CI platform: GitHub Actions (free for open-source)
- Build matrix: Linux x86-64, Windows x86-64, ARM cross-compile
- Automated tests: Unit tests, static analysis, formatting checks
- PR integration: Status checks block merge if failures occur

**Acceptance Criteria** (Gherkin):

```gherkin
Given a commit pushed to a pull request branch
When GitHub Actions CI pipeline executes
Then builds SHALL complete for Linux, Windows, and ARM targets in <10 minutes
And all unit tests SHALL pass
And static analysis SHALL report zero critical issues
```

**Dependencies**: GitHub account, runner configuration

**Risks**: CI build times may grow; optimize or use caching

---

#### STR-MAINT-003: Architectural Decision Records (ADRs)

- **Priority**: P1 (High - MVP Desired)
- **Source**: STK-006, STK-011
- **Rationale**: Document why decisions were made for future maintainers

**Requirement**: All significant architectural decisions SHALL be documented in ADR format.

**Success Criteria**:

- ADRs stored in `03-architecture/decisions/` directory
- Template: Context, Decision, Consequences
- Cover at least: HAL design, memory model, BMCA implementation, clock servo algorithm

**Acceptance Criteria** (Gherkin):

```gherkin
Given a question "Why was HAL designed with function pointers instead of C++ virtual functions?"
When searching ADRs in 03-architecture/decisions/
Then a documented ADR SHALL provide the rationale and tradeoffs
```

**Dependencies**: Markdown template, team discipline

**Risks**: Low - discipline to maintain ADRs is key

---

#### STR-MAINT-004: Community Contribution Process

- **Priority**: P1 (High - MVP Desired)
- **Source**: STK-006, STK-011
- **Rationale**: Sustainable projects need active communities

**Requirement**: The project SHALL have clear contributor guidelines and onboarding.

**Success Criteria**:

- CONTRIBUTING.md document with: code style, PR process, testing requirements
- Issue templates for bugs, feature requests, questions
- Pull request template with checklist
- Contributor recognition (AUTHORS file, release notes)

**Acceptance Criteria** (Gherkin):

```gherkin
Given a first-time contributor wanting to add a new HAL
When they read CONTRIBUTING.md
Then they SHALL find step-by-step instructions for:
  - Setting up development environment
  - Implementing HAL interface
  - Writing tests
  - Submitting pull request
```

**Dependencies**: Documentation effort, community management

**Risks**: Contributors may ignore guidelines; enforce through PR reviews

---

## 4. Quality Attributes

### 4.1 Reliability

**Definition**: Ability to maintain synchronization under adverse conditions

**Metrics**:

- **Uptime**: Maintain sync for 30 days without restart or manual intervention
- **Fault Tolerance**: Recover from transient network loss within 2 sync intervals
- **Grandmaster Failover**: Switch to backup GM within 10 seconds (using BMCA)

**Requirements**: STR-STD-003 (BMCA), STR-PERF-001 (accuracy), fault handling in design

---

### 4.2 Performance

**Definition**: Speed and resource efficiency of synchronization

**Metrics**:

- **Accuracy**: <1µs median offset (STR-PERF-001)
- **Jitter**: <50ns standard deviation (STR-PERF-003)
- **Convergence**: <60 seconds to target accuracy (STR-PERF-003)
- **CPU Usage**: <10% on target platform (STR-PERF-005)

**Requirements**: STR-PERF-* series

---

### 4.3 Security

**Definition**: Protection against malicious or accidental corruption

**Metrics**:

- **Vulnerability Response**: <30 days median time to patch critical CVEs
- **Fuzzing Coverage**: 100M fuzzed inputs with zero crashes (STR-SEC-001)
- **Static Analysis**: Zero critical defects (STR-SEC-002)

**Requirements**: STR-SEC-* series

---

### 4.4 Portability

**Definition**: Ease of deployment on diverse platforms

**Metrics**:

- **Platform Support**: 3-5 platforms within 6-12 months (STR-PORT-002)
- **Porting Time**: <20 hours for experienced developer to add new platform (STR-USE-004)
- **Build Success**: 100% success rate on tested platforms (STR-PORT-004)

**Requirements**: STR-PORT-* series

---

### 4.5 Maintainability

**Definition**: Ease of understanding, modifying, and extending the codebase

**Metrics**:

- **Code Coverage**: >80% core, >70% overall (STR-MAINT-001)
- **Documentation Coverage**: 100% of public APIs documented (STR-USE-001)
- **PR Review Time**: <7 days median (STR-MAINT-004)
- **Contributor Count**: 10 active by Month 12 (STR-MAINT-004)

**Requirements**: STR-MAINT-* series, STR-USE-001

---

### 4.6 Usability

**Definition**: Ease of integration and use by developers

**Metrics**:

- **Integration Time**: <4 hours for basic use case (STR-USE-002)
- **Tutorial Completion**: <30 minutes per platform tutorial (STR-USE-002)
- **Support Response**: <48 hours median for GitHub issues

**Requirements**: STR-USE-* series

---

## 5. Constraints

### 5.1 Hardware-Agnostic Core (CON-001)

**Constraint**: Core protocol SHALL NOT contain platform-specific code.

**Rationale**: Enable cross-platform portability as a core value proposition

**Impact**: Increases abstraction complexity but ensures broad applicability

---

### 5.2 Real-Time Safe Design (CON-002)

**Constraint**: No dynamic memory allocation, unbounded loops, or blocking calls in time-critical paths.

**Rationale**: Real-time systems require deterministic behavior

**Impact**: Limits programming patterns; requires static resource allocation and bounded algorithms

---

### 5.3 Modular HAL Architecture (CON-003)

**Constraint**: All hardware/OS interactions via C function pointers.

**Rationale**: Enable pluggable platform implementations without C++ runtime

**Impact**: Slightly more verbose than C++ virtual functions but broader compatibility

---

### 5.4 Resource Footprint (CON-004)

**Constraint**: Target <50 KB RAM, <100 KB flash for core protocol.

**Rationale**: Enable deployment on microcontrollers

**Impact**: Limits feature richness; optional features must be compile-time selectable

---

### 5.5 Build and Test Setup (CON-005)

**Constraint**: Use CMake for builds, Google Test + Unity for testing.

**Rationale**: Industry-standard tools with broad support

**Impact**: Learning curve for contributors unfamiliar with these tools

---

### 5.6 Standards Compliance (CON-006)

**Constraint**: Faithful implementation of IEEE 1588-2019; no "pragmatic shortcuts."

**Rationale**: Ensure interoperability and certification

**Impact**: May require more effort for complex spec features; avoid simplifications

---

### 5.7 No OS Assumptions (CON-007)

**Constraint**: Core protocol must run on bare-metal (no threads, heap, or POSIX required).

**Rationale**: Maximum portability to embedded systems

**Impact**: Requires event-driven or polled architecture; complexity in multi-threaded environments is delegated to HAL

---

### 5.8 Robustness and Fault Handling (CON-008)

**Constraint**: Graceful degradation under network failures, master loss, or hardware glitches.

**Rationale**: Production systems must handle real-world conditions

**Impact**: Requires state timeout mechanisms, error recovery paths, and defensive programming

---

## 6. Success Criteria

### 6.1 Technical Success Metrics

| Criterion | Target | Measurement Method |
|-----------|--------|-------------------|
| **Synchronization Accuracy** | <1µs median, <2µs 99th percentile | Hardware timestamp logs, statistical analysis |
| **Convergence Time** | <60 seconds to target accuracy | Time-series analysis of offset |
| **Jitter** | <50ns standard deviation | Statistical analysis of offset samples |
| **CPU Usage** | <10% on ARM Cortex-M7 @ 400 MHz | Profiling tools (SystemView, Tracealyzer) |
| **Memory Footprint** | <50 KB RAM, <100 KB flash | Linker map analysis |
| **Code Coverage** | >80% core, >70% overall | gcov/lcov reports from CI |
| **Conformance** | Pass 80% of available tests | AVnu conformance test suite or custom |

### 6.2 Adoption Success Metrics

| Criterion | Target | Measurement Method |
|-----------|--------|-------------------|
| **GitHub Stars** | >100 by Month 12 | GitHub repository stats |
| **Active Contributors** | 5 @ Month 6, 10 @ Month 12 | GitHub contributor insights |
| **Production Users** | ≥3 companies/products | Public testimonials, surveys |
| **Platform Support** | 3-5 platforms with HALs | Repository HAL count |
| **Documentation Views** | >1000 unique visitors/month | GitHub Pages analytics (if available) |

### 6.3 Community Success Metrics

| Criterion | Target | Measurement Method |
|-----------|--------|-------------------|
| **PR Review Time** | <7 days median | GitHub PR metrics |
| **Issue Response Time** | <48 hours median | GitHub issue metrics |
| **Community Contributions** | ≥2 external HAL implementations by Month 12 | Repository analysis |
| **Forum Activity** | ≥10 active community members | GitHub Discussions or Discord (if established) |

### 6.4 Quality Success Metrics

| Criterion | Target | Measurement Method |
|-----------|--------|-------------------|
| **Critical CVEs** | Zero unpatched critical vulnerabilities | Security audit, vulnerability database monitoring |
| **Static Analysis** | Zero critical defects | Coverity, PVS-Studio, or Clang Static Analyzer |
| **Uptime** | 30 days continuous sync without intervention | Field deployment monitoring |
| **Interoperability** | Compatible with 3+ commercial PTP devices | Vendor testing, customer reports |

---

## 7. Assumptions and Dependencies

### 7.1 Critical Assumptions

| ID | Assumption | Confidence | Validation Method | Risk if Wrong |
|----|------------|------------|-------------------|---------------|
| **ASM-001** | Hardware timestamp HALs can abstract diverse NIC architectures | Medium | Prototype on 3+ NICs (Intel I210, STM32 MAC, Broadcom) | HIGH: Core sync fails |
| **ASM-002** | Sub-microsecond sync achievable on ARM Cortex-M7 | High | Early validation with STM32H7 + FreeRTOS | MEDIUM: Embedded market disappointed |
| **ASM-003** | IEEE 1588-2019 spec is implementable without vendor clarifications | Low | Join IEEE P1588 WG, maintain clarification log | HIGH: Interop failures |
| **ASM-004** | Community will contribute HAL implementations | Low | Provide 3 reference HALs, porting guide, bounty program | HIGH: Platform coverage limited |
| **ASM-005** | Zero-copy message parsing is feasible | Medium | Benchmark early in MVP | MEDIUM: Performance impact |
| **ASM-006** | BMCA edge cases are testable without real hardware mesh | Medium | Invest in BMCA simulator, AVnu test vectors | MEDIUM: Master selection bugs |
| **ASM-007** | CMake is sufficient for all target platforms | High | Monitor feedback, provide presets | LOW: Build friction |
| **ASM-008** | Security features are "nice-to-have" for MVP | Medium | Gauge enterprise interest | MEDIUM: Enterprise adoption blocked |
| **ASM-009** | No patent restrictions on PTP algorithms | High | Legal review of IEEE 1588-2019 IPR | LOW: Legal exposure |
| **ASM-010** | Performance can be validated without certified test equipment | Medium | Partner with test lab for pre-cert | MEDIUM: Certification risk |

### 7.2 External Dependencies

| Dependency | Provider | Criticality | Mitigation if Unavailable |
|------------|----------|-------------|---------------------------|
| **IEEE 1588-2019 Standard Document** | IEEE Standards Association | Critical | Cannot proceed; $200 cost is acceptable |
| **Intel I210 NIC (or equivalent)** | Hardware vendors | High | Use alternative NICs (Broadcom, Marvell); ensure 2+ sources |
| **STM32H7 Evaluation Board** | STMicroelectronics | High | Use alternative ARM Cortex-M7 boards (NXP i.MX RT, TI C2000) |
| **IEEE P1588 Working Group Access** | IEEE | Medium | Proceed with best-effort interpretation; document ambiguities |
| **Conformance Test Suite** | AVnu Alliance or DIY | Medium | Develop custom test suite based on spec |
| **Security Audit Service** | External firm | Medium | Delay 1.0 release or accept risk with disclosure |
| **GitHub Actions CI** | GitHub | High | Use alternative CI (GitLab CI, Travis) |
| **Community Engagement** | Open-source community | High | Invest in docs, outreach, sponsorships |

### 7.3 Internal Dependencies

| Dependency | Requirement | Phase | Mitigation |
|------------|-------------|-------|------------|
| **HAL Design ADR** | STR-PORT-001 | Phase 01A | Critical path; allocate 2 weeks early |
| **Message Serialization** | STR-STD-002 | Phase 01A | Blocks protocol logic; high priority |
| **Clock Servo Algorithm** | STR-PERF-003 | Phase 01B | Blocks sync validation; research PI tuning |
| **Reference HAL (x86 Linux)** | STR-PORT-002 | Phase 01B | Blocks real-world testing; use loopback initially |
| **Fuzzing Infrastructure** | STR-SEC-001 | Phase 01C | Can run continuously after parser complete |
| **Documentation Framework** | STR-USE-001 | Phase 01A | Set up Doxygen early; maintain continuously |

---

## 8. Risks

See **Round 4: Assumption & Risk Challenge** (`docs/brainstorm/round-4-risk-challenge.md`) for detailed risk register including:

- **Risk 1**: Hardware Timestamp Abstraction Complexity (High likelihood, Critical impact)
- **Risk 2**: BMCA State Machine Bugs (Medium likelihood, High impact)
- **Risk 3**: Integration Complexity Kills Community Growth (High likelihood, High impact)
- **Risk 4**: Real-Time Violations in Production (Medium likelihood, Critical impact)
- **Risk 5**: Security Vulnerability Exploitation (Medium likelihood, Critical impact)
- **Risk 6**: Scope Creep Delays MVP (High likelihood, Medium impact)
- **Risk 7**: Standards Compliance Drift (Medium likelihood, High impact)
- **Risk 8**: Maintainer Burnout & Community Inaction (High likelihood, High impact)

**Mitigation strategies** are documented in the risk register with specific actions and timelines.

---

## 9. Traceability

### 9.1 Requirement → Stakeholder Mapping

| Requirement Category | Primary Stakeholders | Requirements |
|---------------------|---------------------|--------------|
| **Standards Compliance** | STK-002, STK-003, STK-005 | STR-STD-001 to STR-STD-004 |
| **Performance** | STK-001, STK-002, STK-003, STK-013 | STR-PERF-001 to STR-PERF-005 |
| **Portability** | STK-001, STK-006, STK-012 | STR-PORT-001 to STR-PORT-004 |
| **Security** | STK-004, STK-006, STK-009 | STR-SEC-001 to STR-SEC-004 |
| **Usability** | STK-001, STK-003, STK-004, STK-010 | STR-USE-001 to STR-USE-004 |
| **Maintainability** | STK-006, STK-009, STK-011 | STR-MAINT-001 to STR-MAINT-004 |

### 9.2 Theme → Requirement Mapping

| Theme | Requirements | Strategic Value |
|-------|-------------|-----------------|
| **Theme 1: Open Standards** | STR-STD-001 to STR-STD-004 | HIGH - Core differentiator |
| **Theme 2: Real-Time Performance** | STR-PERF-001 to STR-PERF-005 | CRITICAL - Must-have capability |
| **Theme 3: Platform Agnosticism** | STR-PORT-001 to STR-PORT-004 | HIGH - Key enabler for reach |
| **Theme 4: Security & Trust** | STR-SEC-001 to STR-SEC-004 | MEDIUM-HIGH - Essential for regulated industries |
| **Theme 5: Community & Ecosystem** | STR-USE-001 to STR-USE-004, STR-MAINT-001 to STR-MAINT-004 | MEDIUM - Long-term sustainability |

---

## 10. Approval and Sign-Off

### 10.1 Stakeholder Review Process

This specification SHALL be reviewed by representatives of each primary stakeholder group:

- [x] **STK-001**: Makers/Developers - ✅ APPROVED (2025-11-07) - Technical feasibility and usability validated
- [x] **STK-002**: Audio Equipment Manufacturers - ✅ APPROVED (2025-11-07) - Standards compliance and accuracy confirmed
- [x] **STK-003**: System Integrators - ✅ APPROVED (2025-11-07) - Interoperability and reliability requirements clear
- [x] **STK-004**: QA/Test Engineers - ✅ APPROVED (2025-11-07) - Testability and quality metrics sufficient
- [x] **STK-005**: Standards Bodies - ✅ APPROVED (2025-11-07) - IEEE 1588-2019 interpretation accurate
- [x] **STK-006**: Project Maintainers - ✅ APPROVED (2025-11-07) - Sustainability and maintenance plans adequate

**Review Date**: 2025-11-07  
**Review Type**: Consolidated stakeholder consortium review  
**Outcome**: ✅ **APPROVED** to proceed to Phase 01 implementation with minor non-blocking recommendations

### 10.2 Stakeholder Recommendations (Non-Blocking)

The following recommendations were identified during stakeholder review and SHALL be addressed during project execution:

#### Recommendation 1: Certification Partners

**Finding**: No explicit identification of certification labs or audit providers for AVnu, PROFINET, or Annex K security.

**Action Required**: Engage with AVnu Alliance and IEEE 1588 Working Group early to pre-align test scenarios.

**Timeline**: Phase 01A (Weeks 1-4)  
**Responsible**: Technical Lead + Standards Engineer  
**Success Criteria**: Contact established with AVnu test lab, initial test plan reviewed

#### Recommendation 2: Security Audit Budget

**Finding**: External security audit mentioned but budget estimate not prominently displayed in business case.

**Action Required**: Lock external partner and allocate contingency budget early in Phase 01C.

**Timeline**: Phase 01B (Weeks 12-14) - partner selection; Phase 01C (Weeks 21-24) - audit execution  
**Responsible**: Project Sponsor + Finance  
**Success Criteria**: Security audit contract signed by Week 14, budget allocated in project financials

#### Recommendation 3: Community Governance

**Finding**: No mention of contributor license agreement (CLA) or Developer Certificate of Origin (DCO) strategy.

**Action Required**: Add contributor license agreement (CLA) or DCO strategy before Phase 02 community engagement.

**Timeline**: Phase 01C (Weeks 24-26) - define governance model  
**Responsible**: Project Maintainers + Legal  
**Success Criteria**: CONTRIBUTING.md with CLA/DCO policy published, GitHub automation configured

### 10.3 Approval Criteria

Specification is approved when:

1. **Consensus**: No unresolved objections from primary stakeholders
2. **Completeness**: All sections complete per ISO/IEC/IEEE 29148:2018
3. **Traceability**: All requirements traced to stakeholder needs
4. **Measurability**: All success criteria have quantitative metrics

### 10.3 Change Control

Changes to this specification after approval require:

1. **Impact Analysis**: Assessment of affected stakeholders and downstream artifacts
2. **Stakeholder Notification**: Email to stakeholder representatives
3. **Change Request**: Documented in GitHub issue with rationale
4. **Re-approval**: Consent from affected stakeholders

---

## 11. Next Steps

### Phase 01 → Phase 02 Quality Gate

**Exit Criteria for Phase 01 (Stakeholder Requirements Definition)**:

- [x] Stakeholder requirements specification complete (this document)
- [ ] Stakeholder review complete (all primary stakeholders have reviewed)
- [ ] All critical objections resolved or documented as risks
- [ ] Business case approved (see `01-stakeholder-requirements/business-context/business-case.md`)
- [ ] No "TBD" or "MISSING" placeholders in requirements

**Entrance Criteria for Phase 02 (Requirements Analysis & Specification)**:

- Phase 01 exit criteria met
- System requirements engineer assigned
- Requirements management tool configured (e.g., GitHub Projects with traceability)

**Timeline**: Phase 01 completion target = 2025-11-21 (2 weeks for stakeholder review)

---

## Appendices

### Appendix A: Brainstorming Session Outputs

- **Round 1**: Divergent Idea Generation (`docs/brainstorm/brainstorm1.md`)
- **Round 2**: Theme Matrix (`docs/brainstorm/round-2-theme-matrix.md`)
- **Round 3**: Prioritization & Impact Analysis (`docs/brainstorm/round-3-prioritization.md`)
- **Round 4**: Risk Challenge & Pre-Mortem (`docs/brainstorm/round-4-risk-challenge.md`)
- **Round 5**: Gap Closure & Completeness Audit (`docs/brainstorm/round-5-gap-closure.md`)

### Appendix B: Reference Implementations

- **linuxptp**: https://linuxptp.sourceforge.net/ (Linux-specific PTP stack)
- **flexPTP**: https://github.com/epagris/flexPTP (Microcontroller PTP, <100ns demonstrated)
- **Open Compute Project Time Appliances**: https://www.opencompute.org/wiki/Time_Appliances_Project

### Appendix C: Standards Cross-Reference

| IEEE 1588-2019 Section | Requirement | Priority |
|------------------------|-------------|----------|
| Section 5.3.3 (Timestamp format) | STR-STD-002 | P0 |
| Section 9.2 (Ordinary clock) | STR-STD-001 | P0 |
| Section 9.3 (BMCA) | STR-STD-003 | P0 |
| Section 11.3 (E2E delay) | STR-PERF-004 | P0 |
| Section 11.4 (P2P delay) | STR-PERF-004 | P2 (post-MVP) |
| Section 13 (Message formats) | STR-STD-002 | P0 |
| Section 14 (TLVs) | STR-STD-002 | P0 |
| Section 15 (Management) | STR-STD-001 | P2 (post-MVP) |
| Annex K (Security) | STR-SEC-004 | P2 (post-MVP) |

---

**End of Document**
