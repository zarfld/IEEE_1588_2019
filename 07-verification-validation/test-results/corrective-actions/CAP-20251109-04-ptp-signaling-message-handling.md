# Corrective Action Package (CAP)
ID: CAP-20251109-04
Anomaly ID(s): GAP-SIGNAL-001
Date Opened: 2025-11-09
Owner: timing-core
Integrity Level: 2
Status: OPEN

## 1. Summary
- Detected In Phase: Compliance checklist review (Phase 07) referencing earlier architecture/design phases
- Detection Artifact: `ieee1588-2019-compliance-matrix.md` row Signaling handling (Gap)
- Defect Description: Signaling message type (0xC) declared but no parsing, validation, dispatch, or tests. Optional feature negotiations (unicast, path trace, etc.) cannot proceed.
- User Impact: Reduced interoperability in environments relying on Signaling-based negotiations; incomplete feature set vs stakeholder requirement STR-PTP-MessageCoverage.
- Severity: S1 Medium (optional but unlocks further capabilities)
- Integrity Level Affected: Message handling pipeline (non-timestamped messages)

## 2. Objective Evidence of Failure
| Evidence Type | Reference | Notes |
|--------------|-----------|-------|
| Enum Presence | `include/IEEE/1588/PTP/2019/types.hpp` | `MessageType::Signaling` defined |
| Missing Handler | Search results (no signaling handler implementation) | No dispatch path |
| Compliance Matrix | `ieee1588-2019-compliance-matrix.md` | Marked Gap |

## 3. Root Cause Analysis
- Origin: Scope triage favored core timing & delay mechanisms first.
- Fault Class: Requirements / Implementation gap.
- Deferred due to prioritization on servo stability & dataset scaffolding.

## 4. Impact & Change Scope
| Dimension | Assessment |
|-----------|------------|
| Components | Message parser, dispatcher registry, optional TLV utilities |
| Coupled Elements | Future unicast/path trace/security modules |
| Requirements Impacted | REQ-PTP-SIGNALING (to add) |
| Design Impact | Need signaling dispatcher design stub |
| Security | Must validate TLV lengths to avoid parser abuse |
| Performance | Minimal; non-critical path |

## 5. Change Plan
| Action ID | Type | Description | Owner | Phase | Precondition |
|----------|------|-------------|-------|-------|-------------|
| ACT-SIG-01 | Requirement | Add REQ-PTP-SIGNALING to functional spec | BA Lead | 02 | Stakeholder alignment |
| ACT-SIG-02 | Design | Create signaling dispatcher design stub | Dev Lead | 04 | ACT-SIG-01 merged |
| ACT-SIG-03 | Tests (Failing) | Add parsing & dispatch unit test (TEST-SIGNAL-001) | QA | 05 | ACT-SIG-02 draft |
| ACT-SIG-04 | Implementation | Minimal parse + dispatch + unknown-TLV safe handling | Dev Team | 05 | ACT-SIG-03 failing test present |
| ACT-SIG-05 | Integration | Add acceptance multi-instance signaling negotiation placeholder | QA | 06 | ACT-SIG-04 implemented |
| ACT-SIG-06 | Compliance Update | Update matrix & traceability | Dev Lead | 07 | Tests green |

## 6. Verification & Validation Strategy
- Add failing unit test first (parse & dispatch).
- Implement minimal feature to make test pass; then extend for negotiation TLVs.
- Regression: Run full message handler suite.

## 7. Test Artifacts
| Test ID | Type | Purpose | New/Updated | Requirement |
|---------|------|---------|-------------|-------------|
| TEST-SIGNAL-001 | Unit | Parse & validate signaling header + safe dispatch | New | REQ-PTP-SIGNALING |
| TEST-SIGNAL-NEG-001 | Integration | Placeholder: simple negotiation path (future) | Planned | REQ-PTP-SIGNALING-NEG |

## 8. Execution & Evidence (Planned)
| Step | Description | Expected | Evidence |
|------|-------------|----------|---------|
| 1 | Add failing test | FAIL | CI run |
| 2 | Implement minimal parsing | PASS | Commit SHA |
| 3 | Add unknown TLV handling test | PASS | CI run |
| 4 | Update compliance matrix | Updated | Diff |

## 9. Traceability Updates
| Artifact | ID | Action |
|---------|----|-------|
| Requirement | REQ-PTP-SIGNALING | Add |
| Design | DES-PTP-SIGNALING | Add |
| Code | signaling_dispatcher.* | Add |
| Test | TEST-SIGNAL-001 | Add |
| Compliance Matrix | GAP-SIGNAL-001 row | Update to Partial after implementation |

## 10. Risk Review
- Parsing security: Validate lengths; drop malformed.
- Residual risk after fix: Low.

## 11. Closure Criteria
| Criterion | Status |
|-----------|--------|
| Requirement added | ☐ |
| Design stub committed | ☐ |
| Tests passing | ☐ |
| Compliance matrix updated | ☐ |
| Traceability matrix updated | ☐ |
| Stakeholder sign-off | ☐ |

## 12. Lessons Learned
Prioritize minimal parsing stubs for all declared message types to avoid late-stage compliance gaps.

## 13. Sign-Off
| Role | Name | Date | Approval |
|------|------|------|----------|
| QA Lead | | | |
| Product Owner | | | |
| Security Review | | | |
| Timing Architect | | | |

Status: OPEN
