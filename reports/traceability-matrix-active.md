# Active Requirements Traceability Matrix (Link-Now)

Generated: 2025-11-07
Source: 02-requirements/traceability/linkage-plan-2025-11-07.md
Scope: Only Link-Now requirements for IEEE 1588-2019 MVP; Post-MVP (cross-arch) excluded.

## Core Functional Requirements

| Requirement | Use Cases | User Stories | ADRs | Tests (Planned) |
|-------------|----------|--------------|------|-----------------|
| REQ-F-001 | UC-001, UC-003 | STORY-001 | ADR-001, ADR-002, ADR-003 | TEST-MSG-001, TEST-MSG-NEG-001 |
| REQ-F-002 | UC-002 | STORY-002 | ADR-002 | TEST-BMCA-001, TEST-BMCA-TIMEOUT-001 |
| REQ-F-003 | UC-003 | STORY-002 | ADR-003 | TEST-SYNC-001, TEST-SYNC-OUTLIER-001 |
| REQ-F-004 | UC-004 | STORY-002 | ADR-004 | TEST-SERVO-001, TEST-SERVO-OUTLIER-001 |
| REQ-F-005 | UC-001 (init), UC-004 | STORY-001, STORY-003 | ADR-001 | TEST-HAL-001, TEST-HAL-MOCK-001 |

## System Behavior

| Requirement | Use Cases | User Stories | ADRs | Tests (Planned) |
|-------------|----------|--------------|------|-----------------|
| REQ-S-001 | UC-002 | STORY-002 | ADR-002, ADR-003 | TEST-BMCA-TRANSITION-001 |
| REQ-S-004 | UC-002 | STORY-002, STORY-003 | ADR-002, ADR-003 | TEST-ANNOUNCE-INTEROP-001 |

## Performance & Determinism

| Requirement | Use Cases | User Stories | ADRs | Tests |
|-------------|----------|--------------|------|-------|
| REQ-NF-P-001 | UC-003 | STORY-002 | ADR-003, ADR-004 | TEST-PERF-OFFSET-P95-001 |
| REQ-NF-P-002 | UC-003, UC-004 | STORY-002 | ADR-004 | TEST-WCET-CRITPATH-001 |
| REQ-NF-P-003 | UC-001..004 | STORY-001, STORY-002, STORY-003 | ADR-001, ADR-004 | TEST-RESOURCE-FOOTPRINT-001 |

## Security & Safety

| Requirement | Use Cases | User Stories | ADRs | Tests |
|-------------|----------|--------------|------|-------|
| REQ-NF-S-001 | UC-001..004 | STORY-001 | ADR-003 | TEST-SEC-INPUT-FUZZ-001 |
| REQ-NF-S-002 | UC-001..004 | STORY-001 | ADR-003 | TEST-SEC-MEM-SAFETY-001 |

## Portability & Build

| Requirement | Use Cases | User Stories | ADRs | Tests |
|-------------|----------|--------------|------|-------|
| REQ-NF-M-001 | UC-001 | STORY-001, STORY-003 | ADR-001 | TEST-PORT-BUILD-MULTI-001 |
| REQ-NF-M-002 | UC-001 | STORY-001 | ADR-001 | TEST-CMAKE-OPTIONS-001 |

## Usability

| Requirement | Use Cases | User Stories | ADRs | Tests |
|-------------|----------|--------------|------|-------|
| REQ-NF-U-001 | UC-001 | STORY-001 | ADR-001 (indirect via HAL clarity) | TEST-DOC-QUICKSTART-001 |

## PTP Analysis (Deepening Functional Granularity)

| Requirement | Maps To (Baseline) | Use Cases | ADRs | Tests |
|-------------|--------------------|----------|------|-------|
| REQ-FR-PTPA-001 | REQ-F-003 | UC-003 | ADR-003 | TEST-SYNC-OFFSET-DETAIL-001 |
| REQ-FR-PTPA-002 | REQ-F-002 | UC-002 | ADR-002 | TEST-BMCA-DATASET-001 |
| REQ-FR-PTPA-003 | REQ-F-001 | UC-001 | ADR-003 | TEST-MSG-HANDLERS-001 |
| REQ-FR-PTPA-004 | REQ-F-001, REQ-F-005 | UC-001 | ADR-001 | TEST-TRANSPORT-L2-001 |
| REQ-FR-PTPA-005 | REQ-FUN-PTP-033..040 | (Future UC-MGMT) | ADR-003 | TEST-MGMT-TLV-001 (deferred) |
| REQ-FR-PTPA-006 | REQ-FUN-PTP-025..032 | (Future UC-SEC) | ADR-003 | TEST-SEC-TLV-001 (deferred) |

Notes:

- This active subset is a convenience view to support Phase 04 detailed design and V&V planning. The authoritative full matrix remains in `reports/traceability-matrix.md`.
- “Deferred” test labels indicate test IDs pre-reserved but intentionally scheduled post-MVP.
