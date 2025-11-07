---
title: "Implementation Plan - State Machine (DES-C-021)"
specType: implementation-plan
version: 0.1.0
status: draft
author: AI Standards Implementation Agent
date: 2025-11-07
relatedDesign:
  - DES-C-021
  - DES-I-022
  - DES-D-023
  - DES-I-024
relatedRequirements:
  - REQ-F-001
  - REQ-F-010
  - REQ-SYS-PTP-001
  - REQ-SYS-PTP-005
  - REQ-SYS-PTP-006
integrityLevel: high
traceStatus: establishing
---

## Objective
Deliver a deterministic IEEE 1588-2019 port state machine implementation with event-driven transitions, BMCA integration hooks, timeout processing, and initial synchronization path (UNCALIBRATED→SLAVE) enabling servo coupling.

## Scope (Increment 1)
 
1. Basic lifecycle: initialize(), start() → LISTENING, stop()
2. Event handling for: RS_MASTER, QUALIFICATION_TIMEOUT, RS_SLAVE, ANNOUNCE_RECEIPT_TIMEOUT, FAULT_DETECTED/CLEARED
3. Foreign master list add/update (bounded capacity)
4. Simplified BMCA trigger path (select first foreign master; later replace with full compare logic)
5. Sync + Follow_Up minimal handling to achieve UNCALIBRATED→SLAVE transition
6. Delay_Req / Delay_Resp placeholders (offset/delay calculation stub)
7. Announce receipt timeout detection

Excluded (Future): full BMCA ranking, servo algorithm, multi-domain, security TLVs, unicast negotiation.

## TDD Strategy
 
Red-Green-Refactor sequence:
 
1. Failing test: initialization & start transitions (TEST-BMCA-TRANSITION-001 subset)
2. Failing test: RS_MASTER→PreMaster→QUALIFICATION_TIMEOUT→Master path
3. Failing test: Master tick sends Announce & Sync (counters increment)
4. Failing test: RS_SLAVE path and Follow_Up to Slave
5. Failing test: Announce timeout back to Listening
Refactor after each green: isolate entry/exit logic; introduce TransitionMatrix later.

## Acceptance Criteria
 
1. All transitions execute ≤ 50 µs (instrumentation hooks placeholder)
2. No dynamic memory; foreign master list fixed size; overflow → Resource_Unavailable
3. Public API returns deterministic PTPResult
4. Tests pass in CI with BUILD_TESTING=ON
5. Traceability: test references DES and TEST IDs

## Risks & Mitigations
 
| Risk | Impact | Mitigation |
|------|--------|-----------|
| BMCA stub hides selection issues | Incorrect master role early | Clearly marked TODO; add BMCA test placeholders |
| Timestamp math incomplete | Offset/delay not validated | Stub function with success, schedule later increment |
| Concurrency absent | Future race conditions | Single-thread invariant; plan message queue interface |

## Refactoring Targets
 
Post Increment 1: introduce TransitionMatrix, extract BMCA strategy object, unify timeout calculations.

## Instrumentation Hooks (Planned)
 
Macro-based timing capture around transition_to_state and tick; disabled by default.

## Future Increments Outline
 
Increment 2: Full BMCA vector comparison.
Increment 3: Servo integration & stable sync criteria.
Increment 4: Two-step vs one-step completion; correctionField usage.
Increment 5: Expanded fault model & management dataset updates.

## References
 
Design: sdd-state-machine.md (DES-C-021/022/023/024).
Architecture: ARC-C-002-StateMachine.
Requirements: REQ-F-001, REQ-F-010, REQ-SYS-PTP-005, REQ-SYS-PTP-006.
IEEE 1588-2019: Section 9.2 (States), Section 9.3 (BMCA overview).

## Traceability Status
 
Initial test (test_state_machine_basic.cpp) references DES and TEST IDs; further test IDs to be added in subsequent increments.
