---
specType: traceability-workbook
phase: 02-requirements
version: 1.0.0
date: 2025-11-07
author: Traceability Engineering Team
status: draft
purpose: "Design linkage plan mapping each requirement group to UC, STORY, ADR, and Test IDs"
---
# Linkage Plan (Phase 02 -> Phase 03 Readiness)

## 1. Scope
This document captures the finalized linkage strategy for all active (Link-Now) IEEE 1588-2019 MVP requirements and records deferral rationale for Post-MVP sets. No dummy IDs are introduced; only existing, validated identifiers are referenced.

## 2. Classification Recap
 
| Group | IDs | Classification | Rationale |
|-------|-----|----------------|-----------|
| Core Functional (PTP) | REQ-F-001..005 | Link-Now | Essential protocol behavior & HAL foundation |
| Servo / Timing | REQ-F-003, REQ-F-004 | Link-Now | Direct synchronization accuracy impact |
| System Behavior | REQ-S-001, REQ-S-004 | Link-Now | Smooth BMCA transitions & interoperability |
| Non-Functional Perf/Security | REQ-NF-P-001..003, REQ-NF-S-001..002 | Link-Now | Accuracy, determinism, safety baseline |
| Portability / Build | REQ-NF-M-001..002 | Link-Now | Hardware/OS independence, build portability |
| Usability | REQ-NF-U-001 | Link-Now | Developer integration speed to reduce adoption friction |
| PTP Analysis Items | REQ-FR-PTPA-001..006 | Link-Now | Deep dives expanding baseline details |
| PTP Multi-Domain | REQ-FR-PTPA-007 | Deferred | Requires cross-standard layering (ADR-013) |
| Cross-Standards Functional | REQ-F-CROSSARCH-001..008 | Deferred | Post-MVP integration scope (ADR-013) |
| Cross-Standards Non-Functional | REQ-NF-CROSSARCH-001..029 | Deferred | High-scale & multi-standard qualities beyond MVP |

## 3. Linkage Matrix Skeleton (Active Requirements)

### 3.1 Core Functional Requirements
 
| Requirement | Use Cases | User Stories | ADRs | Tests (Planned) |
|-------------|----------|--------------|------|-----------------|
| REQ-F-001 | UC-001, UC-003 | STORY-001 | ADR-001, ADR-002, ADR-003 | TEST-MSG-001, TEST-MSG-NEG-001 |
| REQ-F-002 | UC-002 | STORY-002 | ADR-002 | TEST-BMCA-001, TEST-BMCA-TIMEOUT-001 |
| REQ-F-003 | UC-003 | STORY-002 | ADR-003 | TEST-SYNC-001, TEST-SYNC-OUTLIER-001 |
| REQ-F-004 | UC-004 | STORY-002 | ADR-004 | TEST-SERVO-001, TEST-SERVO-OUTLIER-001 |
| REQ-F-005 | UC-001 (init), UC-004 | STORY-001, STORY-003 | ADR-001 | TEST-HAL-001, TEST-HAL-MOCK-001 |

### 3.2 System Behavior
 
| Requirement | Use Cases | User Stories | ADRs | Tests (Planned) |
|-------------|----------|--------------|------|-----------------|
| REQ-S-001 | UC-002 | STORY-002 | ADR-002, ADR-003 | TEST-BMCA-TRANSITION-001 |
| REQ-S-004 | UC-002 | STORY-002, STORY-003 | ADR-002, ADR-003 | TEST-ANNOUNCE-INTEROP-001 |

### 3.3 Performance & Determinism
 
| Requirement | Use Cases | User Stories | ADRs | Tests |
|-------------|----------|--------------|------|-------|
| REQ-NF-P-001 | UC-003 | STORY-002 | ADR-003, ADR-004 | TEST-PERF-OFFSET-P95-001 |
| REQ-NF-P-002 | UC-003, UC-004 | STORY-002 | ADR-004 | TEST-WCET-CRITPATH-001 |
| REQ-NF-P-003 | UC-001..004 | STORY-001, STORY-002, STORY-003 | ADR-001, ADR-004 | TEST-RESOURCE-FOOTPRINT-001 |

### 3.4 Security & Safety
 
| Requirement | Use Cases | User Stories | ADRs | Tests |
|-------------|----------|--------------|------|-------|
| REQ-NF-S-001 | UC-001..004 | STORY-001 | ADR-003 | TEST-SEC-INPUT-FUZZ-001 |
| REQ-NF-S-002 | UC-001..004 | STORY-001 | ADR-003 | TEST-SEC-MEM-SAFETY-001 |

### 3.5 Portability & Build
 
| Requirement | Use Cases | User Stories | ADRs | Tests |
|-------------|----------|--------------|------|-------|
| REQ-NF-M-001 | UC-001 | STORY-001, STORY-003 | ADR-001 | TEST-PORT-BUILD-MULTI-001 |
| REQ-NF-M-002 | UC-001 | STORY-001 | ADR-001 | TEST-CMAKE-OPTIONS-001 |

### 3.6 Usability
 
| Requirement | Use Cases | User Stories | ADRs | Tests |
|-------------|----------|--------------|------|-------|
| REQ-NF-U-001 | UC-001 | STORY-001 | ADR-001 (indirect via HAL clarity) | TEST-DOC-QUICKSTART-001 |

### 3.7 PTP Analysis (Deepening Functional Granularity)
 
| Requirement | Maps To (Baseline) | Use Cases | ADRs | Tests |
|-------------|--------------------|----------|------|-------|
| REQ-FR-PTPA-001 | REQ-F-003 | UC-003 | ADR-003 | TEST-SYNC-OFFSET-DETAIL-001 |
| REQ-FR-PTPA-002 | REQ-F-002 | UC-002 | ADR-002 | TEST-BMCA-DATASET-001 |
| REQ-FR-PTPA-003 | REQ-F-001 | UC-001 | ADR-003 | TEST-MSG-HANDLERS-001 |
| REQ-FR-PTPA-004 | REQ-F-001, REQ-F-005 | UC-001 | ADR-001 | TEST-TRANSPORT-L2-001 |
| REQ-FR-PTPA-005 | REQ-FUN-PTP-033..040 | (Future UC-MGMT) | ADR-003 | TEST-MGMT-TLV-001 (deferred) |
| REQ-FR-PTPA-006 | REQ-FUN-PTP-025..032 | (Future UC-SEC) | ADR-003 | TEST-SEC-TLV-001 (deferred) |

## 4. Deferred Sets (Archived Post-MVP)
All `REQ-F-CROSSARCH-*` and `REQ-NF-CROSSARCH-*` retained with `status: deprecated` (semantic deferral) and trace to ADR-013 plus foundational baseline (HAL, BMCA, offset, determinism). No further MVP linkage required.

## 5. Test ID Planning Conventions
Pattern: `TEST-<AREA>-<NAME>-###` where AREA ∈ {MSG, BMCA, SYNC, SERVO, PERF, WCET, SEC, PORT, DOC}. Numbering reserved sequentially starting at 001 per area.

## 6. Next Actions

1. Instantiate planned test specs under `07-verification-validation/test-cases/` (Phase 07 entry) – minimal skeletons first.
2. Update existing requirements files with reciprocal test IDs once test specs exist (avoid forward references until created).
3. Integrate linkage rows into regenerated `reports/traceability-matrix.md` via script augmentation.

## 7. Traceability Integrity Checks

- All Link-Now requirements have at least one UC or STORY and one ADR.
- Deferred items isolated; no active dependencies block MVP.
- No orphan Link-Now requirements remaining.

---
**Validation Note**: This linkage plan is a planning artifact (specType: traceability-workbook) and is excluded from governed schema evaluation.
