---
specType: traceability-workbook
title: Orphan Triage Workbook
date: 2025-11-07
status: draft
---

# Orphan Triage Workbook (2025-11-07)

Status: Draft
Owner: Traceability Working Group
Standard: ISO/IEC/IEEE 29148:2018 (traceability), IEEE 1016-2009 (design references)

## Scope
This workbook classifies orphaned requirement IDs from `reports/orphans.md` and defines linkage actions to reduce the orphan count below 50 before Phase 03 architecture baseline.

## Classification Legend
- Link-Now: Can be safely traced to existing SyRS (REQ-F-*, REQ-NF-*) and/or ADR/UC/Story today
- Defer (Post-MVP): Valid future scope, trace to Post-MVP doc and ADR; mark status: deferred
- Deprecate: Superseded/duplicative; mark status: deprecated with superseded-by reference

## Group A — PTP Functional (REQ-FUN-PTP-001..048)
Action: Link-Now
Rationale: These granular PTP items map cleanly to SyRS core requirements and ADRs.
Mapping rules:
- 001–008 (data types, headers, message types, TLVs) → REQ-F-001; ADR-003
- 009–012 (OC/BC/TC/E2E TC) → REQ-F-001, REQ-F-002; ADR-003
- 013–016 (BMCA family) → REQ-F-002; ADR-003
- 017–018 (offset/delay) → REQ-F-003; ADR-003
- 019–020 (frequency/servo) → REQ-F-004; ADR-003
- 021–024 (multi-domain) → Defer (Post-MVP). Trace to Post-MVP cross-standards integration, ADR-013
- 041–048 (HAL/network abstraction) → REQ-F-005, REQ-NF-M-001; ADR-001

## Group B — PTP Non-Functional (REQ-NFR-PTP-001..048)
Action: Link-Now (most); a few Defer
Mapping rules:
- 001–004 (accuracy/determinism) → REQ-NF-P-001/002
- 005–008 (WCET/CPU/frequency) → REQ-NF-P-002/003
- 009–016 (fault tolerance/availability) → REQ-NF-P-001/002
- 017–024 (scale/resource) → REQ-NF-P-003
- 025–028 (maintainability/devex) → REQ-NF-M-002
- 029–032 (config mgmt) → REQ-NF-M-002
- 033–040 (security) → REQ-NF-S-001/002
- 041–048 (portability/HW ind.) → REQ-NF-M-001

## Group C — Architectural Compliance (REQ-FUNC-ARCH-001..006, REQ-NFR-ARCH-001..004)
Action: Link-Now
- Functional: map to REQ-F-001..005 per topic; ADR-002, ADR-013
- NFR: map to REQ-NF-P-001..003, REQ-NF-M-001/002

## Group D — Cross-Standards (REQ-F-CROSSARCH-001..008, REQ-NF-CROSSARCH-001..029)
Action: Defer (Post-MVP)
- Trace to: `02-requirements/post-mvp/cross-standards-architecture-integration-requirements.md` and ADR-013
- Add status: deferred; keep backward traceability

## Group E — System PTP and Architecture System (REQ-SYS-PTP-*, REQ-SYS-ARCH-*)
Action: Link-Now
- Map to SyRS (REQ-F-*, REQ-NF-*) and ADR-003/ADR-013

## Group F — Legacy Stakeholder IDs (REQ-STK-PTP-002..020)
Action: Link-Now
- Add mapping to Phase 01 StR-### where known; also trace to appropriate SyRS
- If StR mapping missing, keep descriptive link to UC/Story and ADR-003 as interim

## Execution Plan
1) Apply mapping in source files via "Traces to:" inline annotations under each requirement bullet (A, B).
2) For D, add front-matter `status: deferred` and a Traceability section linking ADR-013 and Post-MVP spec.
3) For C/E/F, add concise trace tables at section end.
4) Re-run orphan analysis; iterate until <50.

## Audit Log
- 2025-11-07: Workbook created; approved mapping rules A–F.
