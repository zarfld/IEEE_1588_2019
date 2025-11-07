---
title: "State Machine (ARC-C-002) - Software Design Description"
specType: design
version: 0.1.0
status: draft
author: AI Standards Implementation Agent
date: 2025-11-07
relatedArchitecture:
  - ARC-C-002-StateMachine
  - ARC-C-005-HardwareAbstraction
relatedRequirements:
  - REQ-F-001
  - REQ-F-010
  - REQ-SYS-PTP-001
  - REQ-SYS-PTP-005
  - REQ-SYS-PTP-006
---

## State Machine Software Design Description

## Overview

Detailed design for the IEEE 1588-2019 Port / Clock State Machine governing protocol role transitions (INITIALIZING → FAULTY → LISTENING → PRE_MASTER → MASTER → PASSIVE → UNCALIBRATED → SLAVE) and timeout/event processing.

## Traceability

- Architecture: ARC-C-002-StateMachine
- Requirements: REQ-F-001 (core behavior), REQ-F-010 (failover), system PTP requirements REQ-SYS-PTP-001/005/006.
- ADRs: ADR-002 (BMCA layering), ADR-003 (implementation strategy), ADR-004 (servo integration), ADR-001 (HAL boundaries).
- Tests: TEST-BMCA-TRANSITION-001, TEST-BMCA-TIMEOUT-001, TEST-SYNC-001, TEST-SYNC-OFFSET-DETAIL-001.

## Design Elements

| ID        | Type       | Responsibility                              | Realization Reference |
|-----------|------------|----------------------------------------------|-----------------------|
| DES-C-021 | Component  | PortStateMachine implementation               | `ieee-1588-2019-state-machine-design.md` (DES-C-002-PortStateMachineImpl) |
| DES-I-022 | Interface  | IPortStateMachine API                        | `ieee-1588-2019-state-machine-design.md` (DES-I-001-StateMachineInterface) |
| DES-D-023 | Data       | PortStates / Events / TransitionMatrix       | `ieee-1588-2019-state-machine-design.md` (DES-D-001 / DES-D-002 / DES-D-003) |
| DES-I-024 | Interface  | Timing & Timer integration (HAL)             | `ieee-1588-2019-state-machine-design.md` (DES-I-002-TimingInterface) |

## Contracts

- Inputs: events (enum), current state, timestamps, timeout signals.
- Outputs: new state, scheduled timers, callbacks to BMCA/servo modules.
- Error Modes: invalid transition → error code; expired timers rescheduled; hardware time unavailable → fallback to software time.
- Timing: transition execution ≤ 50 µs WCET; normal tick ≤ 15 µs typical.
- Concurrency: single-threaded state mutations; timer callbacks synchronized via message queue abstraction.

## Algorithms (Summary)

- Event-driven dispatch: state + event lookup in TransitionMatrix (DES-D-003) with O(1) indexed table.
- Timeout processing: creation of periodic and one-shot timers mapped to state durations (e.g. Announce receive timeout).
- Fallback flow: FAULTY → LISTENING after recovery checks; PASSIVE path ensures no master announcements while subordinate domain present.

## Performance & Complexity

- Cyclomatic complexity targets: each handler < 10; transition validator < 12.
- Memory footprint: states table < 4 KB; timer metadata < 2 KB.

## TDD Mapping

- TEST-BMCA-TRANSITION-001 covers master selection interaction (BMCA decision injection).
- TEST-BMCA-TIMEOUT-001 validates timeout-driven reselection sequences.
- TEST-SYNC-001 / TEST-SYNC-OFFSET-DETAIL-001 ensure servo interaction readiness and offset propagation hooks.

## Implementation Guidance

- Keep state machine pure (no direct network I/O); side-effects via injected interfaces.
- Represent timestamps as int64 nanoseconds; avoid floating types.
- Provide deterministic ordering for simultaneous events (priority: FAULT → TIMEOUT → ANNOUNCE → SYNC).

## Open Items

- Finalize jitter mitigation strategy for timer drift.
- Confirm ordering with multi-domain design when domains share physical port.
- Instrument benchmarking points for transition latency.

## References

- IEEE 1588-2019 Section 9.2 (States), Section 9.3 (BMCA interaction)
- Architecture constraints performance-modeling
- ADR-002, ADR-003, ADR-004 for layering context
