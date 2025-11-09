# Corrective Action Package (CAP)
ID: CAP-20251109-02
Anomaly ID(s): ANOM-20251109-PTP-DELAY
Date Opened: 2025-11-09
Owner: timing-core
Integrity Level: 3
Status: OPEN

 
## 1. Summary

- Detected In Phase: 07-verification-validation (feature gap review)
- Detection Activity / Test ID: TEST-PTP-Delay-E2E-Peer (planned)
- Defect Description: Peer-to-peer and end-to-end delay mechanisms (Sections 11.3 & 11.4) not implemented; only abstraction points exist.
- User Impact: Cannot compute path delay or correctionField; synchronization accuracy limited; interoperability with profile consumers (802.1AS, telecom) reduced.
- Severity: S2 High
- Integrity Level Affected Components: Delay request/response processing, timestamp capture integration, correctionField updates.

 
## 3. Root Cause Analysis

- Originating Lifecycle Phase: Implementation (deferred feature)
- Fault Class: Incomplete Algorithmic Feature
- Why Introduced?: Prioritized BMCA & servo scaffolding first.
- Why Not Detected Earlier?: Feature intentionally staged; no failing test yet.
- Contributing Conditions: Need for hardware abstraction of timestamps before full path calc.

| Planned Test | TEST-PTP-Delay-E2E-Peer | Will fail until algorithms present |

## 6. Verification & Validation Iteration Strategy

- Task Iteration Policy: Yes
- Re-Verification Scope: Delay algorithm tests + servo regression + BMCA unaffected check
- Regression Tests: Existing BMCA, servo convergence, multi-instance sync
- Integrity Level Adjustment: No

 
## 4. Impact & Change Scope
 
| Dimension | Assessment |
|-----------|------------|
| Affected Components | Delay message handling, timestamp retrieval interfaces, correctionField logic |
| Coupled Elements | Servo update calculations; BMCA indirectly via quality metrics |
| Requirements Impacted | REQ-PTP-DELAY-E2E, REQ-PTP-DELAY-P2P (to add) |
| Design Elements | Delay algorithm module, path delay accumulator, validation checks |
| Architecture Views | Timing algorithm view update |
| SFMEA Impact | Potential accuracy degradation risk mitigated post implementation |
| Reliability Gates | Must show deterministic bounded calc time |
| Security / Safety | Validate sequence ID / timestamp correlation; bounds on correctionField |

 
## 5. Change Plan
 
| Action ID | Type | Description | Owner | Phase Routed To | Preconditions |
|----------|------|-------------|-------|-----------------|---------------|
| ACT-01 | Requirement Update | Define REQ-PTP-DELAY-E2E & REQ-PTP-DELAY-P2P | BA Lead | 02-requirements | Stakeholder review |
| ACT-02 | Design | Specify algorithm flow & data structures | Dev Lead | 04-design | ACT-01 complete |
| ACT-03 | Code | Implement request/response handling & path delay computation | Dev Team | 05-implementation | ACT-02 merged |
| ACT-04 | Tests | Add failing unit + scenario tests | QA | 06-integration | Code scaffolding present |
| ACT-05 | Performance Evidence | Measure execution time & memory footprint | Reliability Eng | 07-verification-validation | Implementation complete |

 
## 6. Verification & Validation Iteration Strategy
- Task Iteration Policy: Yes
- Re-Verification Scope: Delay algorithm tests + servo regression + BMCA unaffected check
- Regression Tests: Existing BMCA, servo convergence, multi-instance sync
- Integrity Level Adjustment: No

 
## 7. Test Artifacts Added / Updated
 
| Test ID | Type | Purpose | New/Updated | Linked Requirement(s) |
|---------|------|---------|-------------|-----------------------|
| TEST-PTP-Delay-E2E-Peer | Unit | Compute end-to-end & peer path delay from synthetic timestamps | New | REQ-PTP-DELAY-E2E, REQ-PTP-DELAY-P2P |
| TEST-PTP-CorrectionField-Accum | Unit | Validate correctionField aggregation | New | REQ-PTP-DELAY-E2E |
| TEST-PTP-Servo-Delay-Integration | Integration | Ensure servo uses computed delay gracefully | New | REQ-PTP-DELAY-E2E |

 
## 8. Execution & Evidence (Planned)
 
| Step | Description | Result | Evidence Ref |
|------|-------------|--------|--------------|
| 1 | Add failing tests | FAIL | CI run |
| 2 | Implement algorithms | PASS unit | Commit |
| 3 | Run regression suite | PASS | CI run |
| 4 | Performance profiling | PASS thresholds | Report |

 
## 9. Traceability Updates
 
| Artifact Type | ID | Updated? | Link |
|--------------|----|----------|------|
| Requirement | REQ-PTP-DELAY-E2E | New | 02-requirements/functional/ptp-delay-e2e.md |
| Requirement | REQ-PTP-DELAY-P2P | New | 02-requirements/functional/ptp-delay-p2p.md |
| Design | DES-PTP-DELAY | New | 04-design/components/ptp-delay.md |
| Code | src/ptp/delay_mechanisms.* | New | PR |
| Test | TEST-PTP-Delay-* | New | tests path |
| SFMEA | FM-PTP-DELAY | New | sfmea.md |

## 10. Risk & Integrity Review
- Residual Risk: Low (algorithmic correctness validated via test cases)
- SFMEA Adjusted?: Yes
- CIL Item: N/A
- New Hazards Introduced?: No

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
- Delay algorithms should be implemented alongside servo to reduce rework.

## 13. Sign-Off
| Role | Name | Date | Approval |
|------|------|------|----------|
| QA Lead | | | |
| Reliability Engineer | | | |
| Product Owner | | | |
| Security (if applicable) | | | |

Status: OPEN
