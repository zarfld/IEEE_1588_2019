# Corrective Action Package (CAP)
ID: CAP-20251109-03
Anomaly ID(s): ANOM-20251109-PTP-STATE-MACHINE
Date Opened: 2025-11-09
Owner: timing-core
Integrity Level: 3
Status: OPEN

## 1. Summary
- Detected In Phase: Feature audit during 07-verification-validation
- Detection Activity / Test ID: TEST-PTP-StateMachine-Transition-Suite (planned)
- Defect Description: PTP port state machine (Section 9.2) not fully covered; missing transitions and guard condition validations for FAULTY, PASSIVE, UNCALIBRATED, DISABLED states; limited event handling.
- User Impact: Reduced synchronization robustness; inability to correctly recover from fault or passive scenarios; conformance risk.
- Severity: S2 High
- Integrity Level Affected Components: Port state machine logic, BMCA interaction, event dispatcher.

## 2. Objective Evidence of Failure
| Evidence Type | Reference | Notes |
|--------------|-----------|-------|
| Gap Annotation | README Feature Summary | "Full state machine coverage – Partial transitions" |
| Planned Test | TEST-PTP-StateMachine-Transition-Suite | Will initially fail |

## 3. Root Cause Analysis
- Originating Lifecycle Phase: Implementation (incremental approach)
- Fault Class: Incomplete Algorithmic State Coverage
- Why Introduced?: Focus on core MASTER/SLAVE path first.
- Why Not Detected Earlier?: No exhaustive transition test harness present.
- Contributing Conditions: Deferred implementation of rarely exercised states in early prototypes.

## 4. Impact & Change Scope
| Dimension | Assessment |
|-----------|------------|
| Affected Components | State machine module, BMCA hooks, event enumerations |
| Coupled Elements | Servo activation logic, dataset updates |
| Requirements Impacted | REQ-PTP-STATE-MACHINE (to add) |
| Design Elements | State transition table, guard condition definitions |
| Architecture Views | Behavioral/state view update |
| SFMEA Impact | Fault recovery risk reduced after completion |
| Reliability Gates | Deterministic bounded transition evaluation |
| Safety/Security | Correct handling prevents invalid time propagation |

## 5. Change Plan
| Action ID | Type | Description | Owner | Phase Routed To | Preconditions |
|----------|------|-------------|-------|-----------------|---------------|
| ACT-01 | Requirement Update | Define REQ-PTP-STATE-MACHINE (Sec 9.2) | BA Lead | 02-requirements | Stakeholder review |
| ACT-02 | Design | Transition table + guard design spec | Dev Lead | 04-design | ACT-01 complete |
| ACT-03 | Tests | Add failing exhaustive transition suite | QA | 06-integration | ACT-02 approved |
| ACT-04 | Code | Implement missing transitions + guards | Dev Team | 05-implementation | ACT-03 failing tests present |
| ACT-05 | Regression | Validate servo/BMCA unaffected | QA | 07-verification-validation | ACT-04 merged |

## 6. Verification & Validation Iteration Strategy
- Iteration Trigger: New algorithmic coverage
- Re-Verification Scope: Full state machine transitions + existing BMCA + servo convergence
- Regression Tests: Multi-instance sync, resource efficiency
- Integrity Level Adjustment: No

## 7. Test Artifacts Added / Updated
| Test ID | Type | Purpose | New/Updated | Linked Requirement(s) |
|---------|------|---------|-------------|-----------------------|
| TEST-PTP-StateMachine-Transition-Suite | Unit | Exhaustive transition verification | New | REQ-PTP-STATE-MACHINE |
| TEST-PTP-StateMachine-Fault-Recovery | Scenario | Validate recovery from FAULTY to LISTENING | New | REQ-PTP-STATE-MACHINE |
| TEST-PTP-StateMachine-Passive-Behavior | Unit | Ensure PASSIVE state rules enforced | New | REQ-PTP-STATE-MACHINE |

## 8. Execution & Evidence (Planned)
| Step | Description | Result | Evidence Ref |
|------|-------------|--------|--------------|
| 1 | Add failing tests | FAIL | CI run |
| 2 | Implement transitions & guards | PASS unit | Commit |
| 3 | Regression suite | PASS | CI run |
| 4 | Coverage measurement | PASS threshold | Report |

## 9. Traceability Updates
| Artifact Type | ID | Updated? | Link |
|--------------|----|----------|------|
| Requirement | REQ-PTP-STATE-MACHINE | New | 02-requirements/functional/ptp-state-machine.md |
| Design | DES-PTP-STATE-MACHINE | New | 04-design/components/ptp-state-machine.md |
| Code | src/ptp/state_machine.* | Updated | PR |
| Test | TEST-PTP-StateMachine-* | New | tests path |
| SFMEA | FM-PTP-STATE | New | 07-verification-validation/sfmea.md |

## 10. Risk & Integrity Review
- Residual Risk: Low post full coverage
- SFMEA Adjusted?: Yes
- New Hazards?: None

## 11. Closure Criteria
| Criterion | Status |
|-----------|--------|
| All planned actions complete | ☐ |
| All new/updated tests passing | ☐ |
| Regression suite green | ☐ |
| Traceability matrix updated | ☐ |
| Integrity level re-assessed | ☐ |
| Stakeholder sign-off obtained | ☐ |

## 12. Lessons Learned
- Introduce exhaustive state transition tests early to prevent latent coverage gaps.

## 13. Sign-Off
| Role | Name | Date | Approval |
|------|------|------|----------|
| QA Lead | | | |
| Reliability Engineer | | | |
| Product Owner | | | |
| Security (if applicable) | | | |

Status: OPEN
