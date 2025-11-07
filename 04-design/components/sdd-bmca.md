---
title: "BMCA (ARC-C-003) - Software Design Description"
specType: design
version: 0.1.0
status: draft
author: AI Standards Implementation Agent
date: 2025-11-07
relatedArchitecture:
  - ARC-C-003-BMCA
  - ARC-C-002-StateMachine
relatedRequirements:
  - REQ-F-002
  - REQ-F-010
  - REQ-SYS-PTP-001
---

## BMCA Software Design Description

### Overview

Detailed design for the Best Master Clock Algorithm (BMCA) implementation: dataset management, clock quality comparison, state recommendations, and interactions with the Port/Clock state machine.

### Traceability

- Architecture: ARC-C-003-BMCA; collaborates with ARC-C-002-StateMachine
- ADRs: ADR-002 (BMCA layering), ADR-003 (implementation strategy)
- Requirements: REQ-F-002 (BMCA core), REQ-F-010 (failover), REQ-SYS-PTP-001 (system).
- Tests: TEST-BMCA-001, TEST-BMCA-DATASET-001, TEST-BMCA-TRANSITION-001.

### Design Elements

| ID        | Type      | Responsibility                              | Realization Reference |
|-----------|-----------|----------------------------------------------|-----------------------|
| DES-C-031 | Component | BMCA Engine                                   | `ieee-1588-2019-bmca-design.md` (DES-C-005-BMCAEngineImpl) |
| DES-I-032 | Interface | IBMCA (process announce, state decision, get best) | `ieee-1588-2019-bmca-design.md` (DES-I-003-BMCAInterface) |
| DES-D-033 | Data      | Datasets (default, parent, current) and decision types | `ieee-1588-2019-bmca-design.md` (DES-D-004..DES-D-008) |

### Contracts

- Inputs: Announce-derived datasets, current candidates, timeouts.
- Outputs: Recommended state, selected best master, reasons (decision vector).
- Errors: malformed datasets → reject; incomplete announce → ignore; tie-break resolves by identity.
- Performance: candidate evaluation ≤ 15 µs per update (typical), with O(N) compare; N bounded by config.

### Algorithms (Summary)

- Lexicographic comparison chain (priority1, clockClass, accuracy, variance, priority2, identity).
- Ageing and eviction of foreign masters based on announce intervals/timeouts.
- Decision memoization to avoid recomputation when inputs unchanged.

### TDD Mapping

- TEST-BMCA-001: basic best master selection.
- TEST-BMCA-DATASET-001: dataset integrity and validation rules.
- TEST-BMCA-TRANSITION-001: integration with state machine transitions.

### References

- IEEE 1588-2019 BMCA rules (section references only)
- 03-architecture constraints/performance-modeling
- ADR-002 for layering
