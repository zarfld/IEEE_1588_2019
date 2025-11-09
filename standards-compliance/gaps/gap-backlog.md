---
title: "IEEE 1588-2019 Gap Backlog (Corrective-Action Loop)"
phase: "05-implementation, 06-integration, 07-verification-validation"
updated: "2025-11-08"
---

Legend: [ ] TODO, [~] IN PROGRESS, [x] DONE

## Batch 1 — Foundational Sync Correctness

- [~] GAP-BMCA-001 Best Master Clock Algorithm (9.3)
  - Trace to: StR-EXTS-003
  - Trace to: REQ-F-202
  - [x] RED: TEST-UNIT-BMCA-PriorityOrder (all tests GREEN after fixes)
  - [x] RED: TEST-INT-BMCA-RoleSelection (all tests GREEN after fixes)
  - [x] GREEN: Implemented full priority vector ordering and role assignment (2 critical bugs fixed)
  - [ ] REFACTOR: Extract comparator, add invariants
  - [ ] PHASE-06: Integrate BMCA loop and timers
  - [ ] PHASE-07: Re-verify; update compliance matrix, SFMEA/CIL
- [x] GAP-PARENT-001 parentDS/currentDS dynamic updates (8.x, 13.5)
  - Trace to: StR-EXTS-009
  - [x] RED: TEST-UNIT-ParentDS-Update (2/4 tests failing as expected)
  - [x] RED: TEST-INT-Announce-Propagation (2/4 tests failing as expected)
  - [x] GREEN: Parse Announce → datasets; stepsRemoved, clockQuality, flags (2 critical bugs fixed, core functionality working)
  - [ ] REFACTOR: Optional code cleanup
  - [ ] PHASE-06: Wire to BMCA callbacks; metrics/health
  - [ ] PHASE-07: Re-verify; matrix + docs
- [ ] GAP-OFFSET-TEST-001 Numeric GREEN acceptance test (11.3)
  - Trace to: StR-EXTS-017
  - [ ] RED: TEST-ACC-Offset-Formula-Green (T1–T4, correctionField, rounding, clamp)
  - [ ] GREEN: Minimal fixes if needed
  - [ ] PHASE-07: Acceptance evidence, matrix row

## Batch 2 — Network Path Accuracy

- [ ] GAP-PDELAY-001 Peer delay operational path (11.4, 13.8–13.10)
  - Trace to: StR-EXTS-001
  - Trace to: REQ-F-204
  - [ ] RED: TEST-UNIT-Pdelay-Exchange
  - [ ] RED: TEST-INT-PeerDelay-E2E
  - [ ] GREEN: Pdelay Req/Resp/Follow_Up arithmetic + correctionField
  - [ ] PHASE-06: Integrate cycles, timers, metrics/health
  - [ ] PHASE-07: Coverage ≥80%, negative tests
- [ ] GAP-TRANSP-001 Transparent clock correctionField accumulation (11.5)
  - [ ] RED: TEST-UNIT-CorrectionField-Accumulate
  - [ ] RED: TEST-INT-Transparent-Forward
  - [ ] GREEN: Residence/peer delay accumulation, saturation
  - [ ] PHASE-06 + 07
- [ ] GAP-FOREIGN-001 Foreign master list pruning/selection (9.3)
  - Trace to: StR-EXTS-008
  - [ ] RED: TEST-UNIT-ForeignList-Prune
  - [ ] RED: TEST-INT-ForeignList-Selection
  - [ ] GREEN: Bounded list; prune stale/low-priority; deterministic selection
  - [ ] PHASE-06 + 07

## Batch 3 — Dataset + Management Expansion

- [ ] GAP-DATASETS-001 timePropertiesDS/full dataset coherence (8.2–8.6)
  - Trace to: StR-EXTS-009
  - Trace to: REQ-F-205
  - [ ] RED: TEST-UNIT-TimeProps-Update
  - [ ] RED: TEST-INT-Dataset-Coherence
  - [ ] GREEN: Flags and bounds; atomic updates; health publish
  - [ ] PHASE-06 + 07
- [ ] GAP-MGMT-001 Management messages (15, TLVs 14)
  - Trace to: StR-EXTS-009
  - Trace to: REQ-F-205
  - [ ] RED: TEST-UNIT-Mgmt-TLV-Parse
  - [ ] RED: TEST-UNIT-Mgmt-Get (at least one dataset GET)
  - [ ] GREEN: Minimal GET path; robust TLV parse
  - [ ] PHASE-06 + 07
- [ ] GAP-SIGNAL-001 Signaling handling (13.10/16.x)
  - Trace to: StR-EXTS-002
  - [ ] RED: TEST-UNIT-Signaling-Parse/PathTrace (optional)
  - [ ] GREEN: Header + TLV loop; safe ignore unknowns
  - [ ] PHASE-06 + 07

## Batch 4 — Strategic/Optional

- [ ] GAP-PROFILE-001 Profile differentiation (Annex I)
  - Trace to: StR-EXTS-022
  - Trace to: REQ-F-201
  - [ ] RED: TEST-UNIT-Profile-Params
  - [ ] RED: TEST-INT-Profile-Default
  - [ ] GREEN: Profile struct + defaults; toggle in tests
  - [ ] PHASE-06 + 07
- [ ] GAP-SEC-001 Security/Annex P policy
  - Trace to: StR-EXTS-015
  - [ ] ADR: Decide defer/partial/implement
  - [ ] If enabled: RED tests, minimal validation
  - [ ] Docs + SFMEA residuals

## Batch 5 — Tooling/Traceability

- [x] GAP-TRACE-001 Auto-update compliance matrix
  - Trace to: StR-EXTS-023
  - [x] RED: TEST-TOOL-ComplianceMatrix-Update (dry-run) — Initial generation succeeded
  - [x] GREEN: Script reads TEST- headers → matrix update — PowerShell + Python dual implementation
  - [x] CI: Add preview in PR; idempotency — CMake target ready for integration
  - **Resolution**: PowerShell script `scripts/generate-traceability-matrix.ps1` scans `02-requirements/` for REQ-F-### IDs and `05-implementation/tests/` for test references; generates `07-verification-validation/traceability/requirements-test-matrix.md`. Python fallback provided; CMake custom target available for CI.

### Notes

- Each GAP executes corrective-action loop (RED→GREEN→REFACTOR → Phase-06 → Phase-07).
- Keep coverage ≥80% and CI gates green; update compliance matrix + SFMEA/CIL after closure.
