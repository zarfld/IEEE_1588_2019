---
title: "External Standards Derived System Requirements (Top-5)"
specType: requirements
standard: 29148
phase: 02-requirements
version: 1.0.0
author: "standards-integration-workstream"
date: "2025-11-09"
status: draft
traceability:
  stakeholderRequirements:
    - StR-EXTS-001
    - StR-EXTS-003
    - StR-EXTS-004
    - StR-EXTS-009
    - StR-EXTS-022
---

# External Standards Derived System Requirements (Top-5)

This specification derives concrete system/functional requirements from the top-priority external standards stakeholder inputs. It follows ISO/IEC/IEEE 29148 principles and maintains traceability back to StR-EXTS items, enabling test-first verification.

## Feature: Profile Abstraction Layer

### REQ-F-201: Profile Strategy Selection (gPTP, Industrial, AES67)

Priority: Critical

Description:
The system shall provide a profile strategy layer that selects protocol behaviors at runtime or initialization time to satisfy different standards profiles without code duplication. At minimum, the following profiles shall be supported as selectable strategies: gPTP (IEEE 802.1AS), Industrial (IEC/IEEE 60802 constraints), AES67 (media profile using IEEE 1588 default/E2E behavior). The strategy determines delay mechanism, management model, allowed states/features, and transport mapping constraints.

Rationale:
Cleanly separates conflicting profile choices (e.g., P2P vs E2E, management model) and prevents accidental feature leakage across profiles.

Trace to Stakeholder Requirements:
- StR-EXTS-022

Acceptance Criteria:
```gherkin
Scenario: Select gPTP profile at initialization
  Given the application sets profile = "gPTP"
  When the PTP instance starts
  Then the delay mechanism SHALL be P2P by default
  And the management model SHALL be dataset/MIB (no PTP management messages)
  And excluded states (FAULTY, UNCALIBRATED, LISTENING, PRE_MASTER) SHALL be disabled

Scenario: Select AES67 profile at initialization
  Given the application sets profile = "AES67"
  When the PTP instance starts
  Then the delay mechanism SHALL be E2E by default
  And the transport mapping SHALL support UDP/IPv4 encapsulation in the abstraction
  And optional features SHALL be inactive unless explicitly enabled via management
```

Dependencies:
- None (foundation for other requirements)

Constraints:
- No platform-specific code in standards layer; selection is via dependency injection/config.

Test Strategy:
- Unit: strategy selection toggles (gPTP vs AES67) affect configuration flags
- Integration: multi-instance, different profiles in the same process
- Conformance: profile PICS checks consistent with selected strategy

---

## Feature: Best Master Clock Selection

### REQ-F-202: Deterministic BMCA per gPTP Constraints

Priority: Critical

Description:
The system shall implement deterministic Best Master Clock selection consistent with gPTP constraints, including the lexicographic comparison of the relevant dataset members and defined tie-breakers, ensuring stable master election and role assignment.

Rationale:
Correct, stable master election is foundational for synchronization.

Trace to Stakeholder Requirements:
- StR-EXTS-003

Acceptance Criteria:
```gherkin
Scenario: BMCA selects the grandmaster with better clock quality
  Given two Announce streams with different clockClass/accuracy/variance
  When BMCA executes
  Then the candidate with the superior priority vector SHALL be selected as master
  And the local port role SHALL transition accordingly within one announce interval

Scenario: BMCA tie resolution is deterministic
  Given two identical priority vectors but different clockIdentity values
  When BMCA executes
  Then the candidate with the lower clockIdentity (lexicographic order) SHALL win
```

Dependencies:
- REQ-F-201 (profile may constrain BMCA options)

Constraints:
- Avoid disallowed states under gPTP.

Test Strategy:
- Unit: vector comparator, tie behavior (with fault-injection)
- Integration: multi-node selection stability for 1000 announce intervals

---

## Feature: Domain Configuration

### REQ-F-203: Domain 0 Default with External Control Disabled

Priority: Critical

Description:
For the primary domain (domainNumber = 0), the system shall operate BMCA in normal mode with external port configuration control disabled by default (externalPortConfigurationEnabled = FALSE). Other domains remain configurable by profile.

Rationale:
Ensures default domain master election is not overridden by external control when operating per gPTP.

Trace to Stakeholder Requirements:
- StR-EXTS-004

Acceptance Criteria:
```gherkin
Scenario: Domain 0 starts with external control disabled
  Given a default configuration
  When the PTP instance starts in domain 0
  Then externalPortConfigurationEnabled SHALL be FALSE
  And any attempt to set desiredState externally SHALL be ignored for domain 0
```

Dependencies:
- REQ-F-201

Test Strategy:
- Unit: config defaults verification
- Integration: attempt external override in domain 0 → no effect

---

## Feature: Delay Mechanism (gPTP)

### REQ-F-204: Peer-to-Peer Delay Mechanism for Full-Duplex Links

Priority: High

Description:
The system shall implement the peer-to-peer (P2P) delay mechanism for full-duplex point-to-point links, including Pdelay_Req, Pdelay_Resp, Pdelay_Resp_Follow_Up exchanges, and computation of peer mean path delay with proper correction field handling.

Rationale:
P2P is mandatory in gPTP for full-duplex 802.3 links.

Trace to Stakeholder Requirements:
- StR-EXTS-001

Acceptance Criteria:
```gherkin
Scenario: Single-hop P2P delay calculation
  Given two time-aware systems on a full-duplex point-to-point link with profile = gPTP
  When they exchange Pdelay messages per interval
  Then each peer’s mean path delay SHALL converge within a bounded tolerance (P2P_TOLERANCE_NS = 100ns)
  And the correction field contributions SHALL be updated consistently

Scenario: Multi-hop accumulation does not exceed bounds
  Given a three-hop chain of P2P links
  When Pdelay exchanges are stable
  Then the effective path delay seen by a slave SHALL equal the sum of peer delays within tolerance (±100ns per hop accumulated)
```

Dependencies:
- REQ-F-201 (profile selection)

Constraints:
- No OS/hardware dependencies in standards layer; timestamp capture abstracted.

Test Strategy:
- Unit: arithmetic for peer mean path delay and correction field scaling
- Integration: simulated links with deterministic timestamps; multi-hop chain measuring deviation ≤ 100ns per hop

---

## Feature: Management & Data Sets (gPTP)

### REQ-F-205: Dataset/MIB-Based Management (No PTP Mgmt Messages under gPTP)

Priority: High

Description:
When operating under the gPTP profile, the system shall expose management via the profile’s dataset/MIB model (e.g., defaultDS, currentDS, parentDS, portDS, timePropertiesDS, pathTraceDS) and SHALL NOT rely on PTP Management Messages. Basic read accessors SHALL be provided; write access shall be constrained by profile rules.

Rationale:
Aligns management with profile expectations and avoids conflicting management paths.

Trace to Stakeholder Requirements:
- StR-EXTS-009

Acceptance Criteria:
```gherkin
Scenario: Read datasets via management interface
  Given an active gPTP instance
  When a manager queries defaultDS/currentDS/portDS
  Then the system SHALL return coherent values updated by protocol events

Scenario: PTP Management Messages are disabled under gPTP
  Given profile = gPTP
  When a PTP Management Message is received
  Then the system SHALL ignore it and log a profile-incompatible management path
```

Dependencies:
- REQ-F-201 (profile selection)
- REQ-F-202 (datasets updated via Announce processing)

Constraints:
- API remains hardware/OS agnostic; remote transport for MIB is out-of-scope for standards layer.

Test Strategy:
- Unit: dataset read coherence, timeProperties flags/bounds
- Integration: simulated manager queries; negative test for PTP mgmt path

---

## Traceability Matrix (Excerpt)

| Stakeholder Req | System Req |
|-----------------|------------|
| StR-EXTS-022 | REQ-F-201 |
| StR-EXTS-003 | REQ-F-202 |
| StR-EXTS-004 | REQ-F-203 |
| StR-EXTS-001 | REQ-F-204 |
| StR-EXTS-009 | REQ-F-205 |

## Validation & Verification

- Automated schema check against `spec-kit-templates/schemas/requirements-spec.schema.json`
- Unit & integration tests per Test Strategy sections
- Conformance checks using profile PICS (to be added in verification artifacts)

## Open Questions

1. Do we expose profile selection at runtime per instance, or lock at build/config time for deterministic embedded targets?
2. Minimal dataset write operations allowed under gPTP (which fields are safely mutable without violating profile rules)?
3. (Resolved) Tolerance thresholds: P2P_TOLERANCE_NS fixed at 100ns per hop for initial tier; future performance tier may require tightening (<50ns).
