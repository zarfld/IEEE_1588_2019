# Corrective Action Package (CAP)

ID: CAP-20251109-06

Anomaly ID(s): GAP-UNICAST-001

Date Opened: 2025-11-09

Owner: timing-core

Integrity Level: 2

Status: OPEN

## 1. Summary

- Detected In Phase: Compliance cross-check (Phase 07)
- Defect Description: Optional unicast negotiation (clause referenced below) not implemented; no negotiation state, timers, or tests present.
- User Impact: Unicast-only or mixed networks cannot request/maintain unicast message flows; limits deployability.
- Severity: S1 Medium
- Integrity Level Affected: Message control path and session state

## 2. Objective Evidence of Failure

| Evidence Type | Reference | Notes |
|--------------|-----------|-------|
| Missing Code | Search: no unicast negotiation state/handlers | Not found |
| Requirements | Stakeholder/requirements mention unicast; no concrete acceptance tests | Gap |

## 3. Root Cause Analysis

- Deferred optional feature in favor of multicast baseline.
- Fault Class: Requirements/feature gap.

## 4. Impact & Change Scope

| Dimension | Assessment |
|-----------|------------|
| Components | Negotiation state machine, timers, message encode/parse |
| Coupling | Signaling handling baseline; transport abstraction |
| Requirements | REQ-PTP-UNICAST-NEG | Add |
| Design | DES-PTP-UNICAST-NEG | Add |
| Performance | Timer-driven; ensure bounded processing |

## 5. Change Plan

| Action ID | Type | Description | Owner | Phase | Precondition |
|----------|------|-------------|-------|-------|-------------|
| ACT-U-01 | Requirement | Add REQ-PTP-UNICAST-NEG | BA | 02 | Stakeholder confirm |
| ACT-U-02 | Design | Negotiation state + timers design | Dev Lead | 04 | ACT-U-01 |
| ACT-U-03 | Tests (Failing) | Add TEST-UNICAST-001 (request→grant→active→timeout) | QA | 05 | ACT-U-02 |
| ACT-U-04 | Implementation | Minimal negotiation path via abstract transport | Dev | 05 | ACT-U-03 |
| ACT-U-05 | Acceptance | 2-instance negotiation smoke (abstract wiring) | QA | 06 | ACT-U-04 |

## 6. Verification Strategy

- Start with unit tests on negotiation transitions and timers.
- Add acceptance test using existing multi-instance harness.

## 7. Traceability Updates

| Artifact | ID | Action |
|---------|----|-------|
| Requirement | REQ-PTP-UNICAST-NEG | Add |
| Design | DES-PTP-UNICAST-NEG | Add |
| Test | TEST-UNICAST-001 | Add |
| Compliance Matrix | GAP-UNICAST-001 | Update post-merge |

## 8. References (by clause number only)

- IEEE 1588-2019: Clause 16.1 (unicast negotiation)

Status: OPEN
