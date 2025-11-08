# Corrective Action Package (CAP)

ID: CAP-20251108-02
Anomaly ID(s): ANOM-20251108-BMCA-TIE-001
Date Opened: 2025-11-08
Owner: BMCA Integration Stream
Integrity Level: 2
Status: AWAITING_REVERIFY

## 1. Summary

- Detected In Phase: 05-implementation (unit/integration maturation) transitioning to 07-verification-validation
- Detection Activity / Test ID: Planned TEST-UNIT-BMCA-TiePassive (to be added)
- Defect Description: BMCA does not yet implement passive role recommendation (RS_PASSIVE) when priority vectors tie; tie scenario currently treated as master/slave only.
- User Impact (If Released): Potential incorrect role arbitration in multi-master contention, reduced determinism in tie handling.
- Severity: Medium (protocol correctness edge case)
- Integrity Level Affected Components: BMCA engine, port state machine

## 2. Objective Evidence of Failure

| Evidence Type | Reference | Notes |
|--------------|-----------|-------|
| Failing Test | TEST-UNIT-BMCA-TiePassive (initial run) | Reproduced missing RS_PASSIVE; fixed in subsequent iteration |
| Log Extract | N/A | Current logs show selection without passive path |
| Metrics Snapshot | metrics.bmcaSelections only | No passive win counter present |

## 3. Root Cause Analysis

- Originating Lifecycle Phase: Implementation (tie handling not implemented in initial increment)
- Fault Class: Logic omission (missing branch for equal vectors)
- Why Introduced? Incremental delivery focused on master/slave path first.
- Why Not Detected Earlier? Tie scenarios not included in initial RED tests; absence of passive counter not flagged.
- Contributing Conditions: Scope limitation of first BMCA integration slice.

## 4. Impact & Change Scope

| Dimension | Assessment |
|-----------|------------|
| Affected Components | bmca.cpp, clocks.cpp (run_bmca), metrics & health telemetry |
| Coupled Elements | StateEvent enum, tests referencing BMCA role metrics |
| Requirements Impacted | REQ-F-002 (BMCA state machine completeness), REQ-NF-REL-001 (Telemetry) |
| Design Elements | DES-C-003 BMCA Engine, DES-C-010 Time Sync Component |
| Architecture Views | Logical PTP role arbitration view requires update (passive path) |
| SFMEA Impact | Minor: tie handling may affect failure mode FM-Clock-Arbitration |
| Reliability Gates | Negligible MTBF impact; correctness gate impacted |
| Security / Safety | None |

## 5. Change Plan

| Action ID | Type | Description | Owner | Phase Routed To | Preconditions |
|----------|------|-------------|-------|-----------------|---------------|
| ACT-01 | Requirement Clarification | Update REQ-F-002 to include passive recommendation behavior on tie | Requirements Lead | 02-requirements | Stakeholder review |
| ACT-02 | Design Update | Extend BMCA design spec with tie/passive decision branch & metrics | Design Lead | 04-design | ACT-01 approved |
| ACT-03 | Code Implementation | Add tie detection, RS_PASSIVE emission, BMCA_PassiveWins counter | Dev | 05-implementation | ACT-02 merged |
| ACT-04 | Test Addition | Add TEST-UNIT-BMCA-TiePassive (RED→GREEN) | QA/Dev | 05-implementation | ACT-03 in progress |
| ACT-05 | Integration Regression | Run BMCA suite + state machine tests | QA | 06-integration | ACT-04 GREEN |
| ACT-06 | Traceability Update | Update matrix & design references | QA | 07-verification-validation | ACT-05 PASS |
| ACT-07 | Closure Review | Risk & integrity reassessment | Reliability Eng | 07-verification-validation | Evidence compiled |

## 6. Verification & Validation Iteration Strategy

- Task Iteration Policy Triggered? Yes (logic omission)
- Re-Verification Scope: Impact-based (BMCA + port state state-machine tests)
- Regression Test Set: bmca_tdd_tests, bmca_tdd_selection, ptp_state_machine_tests
- Integrity Level Adjustment: None (remains Level 2)

## 7. Test Artifacts Added / Updated

| Test ID | Type | Purpose | New/Updated | Linked Requirement(s) |
|---------|------|---------|-------------|-----------------------|
| TEST-UNIT-BMCA-TiePassive | Unit | Reproduce tie leading to RS_PASSIVE recommendation | New | REQ-F-002 |
| TEST-UNIT-BMCA-RoleAssignment | Unit | Confirms master path (already GREEN) | Updated (adds metrics assertions) | REQ-F-002 |

## 8. Execution & Evidence (Results)

| Step | Description | Result | Evidence Ref |
|------|-------------|--------|--------------|
| 1 | Add failing TiePassive test | FAIL (as expected) | ctest -R bmca_tie_passive (pre-fix) |
| 2 | Implement tie/passive handling & BMCA_PassiveWins counter | Build PASS | MSBuild Debug build (2025-11-08) |
| 3 | Re-run BMCA suite (focused) | PASS | See test IDs 17,18,19,30,31 (all green) |
| 4 | Non-BMCA regression smoke | PASS | Test IDs 7,20,21,22 green |
| 5 | Update design & requirements | PASS | REQ-F-002 updated; DES-C-003, DES-C-010 updated |
| 6 | Update traceability matrix | PASS | Added REQ-F-002 row linking to tests in architecture-traceability-matrix.md |
| 7 | Risk review | Pending | — |

## 9. Traceability Updates (Planned)

| Artifact Type | ID | Updated? | Link |
|--------------|----|----------|------|
| Requirement | REQ-F-002 | Yes | 02-requirements/system-requirements-specification.md |
| Design | DES-C-003 | Yes | 04-design/components/ieee-1588-2019-bmca-design.md |
| Design | DES-C-010 | Yes | 04-design/components/ieee-1588-2019-state-machine-design.md |
| Code | bmca.cpp | Yes | PR link |
| Test | TEST-UNIT-BMCA-TiePassive | New | 05-implementation/tests/test_bmca_tie_passive.cpp |
| SFMEA | FM-Clock-Arbitration | Review | sfmea doc |

## 10. Risk & Integrity Review (Preliminary)

- Residual Risk Rating: Low
- SFMEA Adjusted? Pending
- CIL Item: N/A
- New Hazards Introduced? None anticipated

## 11. Closure Criteria

| Criterion | Status |
|-----------|--------|
| Failing tie test added | Done |
| Passive logic implemented | Done |
| Regression suite green | Done (focused + smoke) |
| Traceability updated | Done |
| Risk review complete | Pending |
| Stakeholder sign-off | Pending |

## 12. Lessons Learned (Preliminary)

- Prevention Opportunity: Include tie/passive scenarios in initial BMCA RED test set.
- Missing Control Identified: Role metrics did not include passive wins counter.
- Institutionalization: Add tie scenario to BMCA test template.

## 13. Sign-Off (Pending)

| Role | Name | Date | Approval |
|------|------|------|----------|
| QA Lead | | | |
| Reliability Engineer | | | |
| Product Owner | | | |
| Architect | | | |

Status: OPEN
