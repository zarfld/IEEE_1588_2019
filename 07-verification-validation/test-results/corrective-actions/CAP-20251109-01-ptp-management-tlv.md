# Corrective Action Package (CAP)
ID: CAP-20251109-01
Anomaly ID(s): ANOM-20251109-PTP-MGMT-TLV
Date Opened: 2025-11-09
Owner: timing-core
Integrity Level: 3
Status: OPEN

## 1. Summary
- Detected In Phase: 03-architecture → 07-verification-validation (review + feature audit)
- Detection Activity / Test ID: TEST-PTP-MGMT-TLV-Roundtrip (to be added)
- Defect Description (Concise): Management TLV framework is not implemented; structural placeholders exist without end-to-end TLV handling and dataset operations per IEEE 1588-2019.
- User Impact (If Released): Inability to configure/monitor PTP datasets via management messages; limited interoperability with management tooling.
- Severity (FDSC Scale / Business Impact): S2 High
- Integrity Level Affected Components: Core management message path and datasets

## 2. Objective Evidence of Failure
| Evidence Type | Reference | Notes |
|--------------|-----------|-------|
| Gap Annotation | README Feature Summary | "Management TLV framework – Structural placeholders, logic pending" |
| Test (planned) | TEST-PTP-MGMT-TLV-Roundtrip | Will fail until implemented |

## 3. Root Cause Analysis
- Originating Lifecycle Phase: Design / Implementation (scope deferred)
- Fault Class: Requirements Gap / Incomplete Feature
- Why Introduced?: Prioritized core datasets/servo and BMCA; deferred management path
- Why Not Detected Earlier?: Not a runtime defect; surfaced during feature inventory & compliance review
- Contributing Conditions: Focus on hardware-agnostic core, limited bandwidth

## 4. Impact & Change Scope
| Dimension | Assessment |
|-----------|------------|
| Affected Components | Message parsing/serialization, dataset accessors, TLV handlers |
| Coupled Elements | Test harness, serialization utilities |
| Requirements Impacted | REQ-PTP-MGMT-TLV (to be added in 02-requirements) |
| Design Elements | TLV base types, TLV registry/dispatcher, dataset ops |
| Architecture Views | Interface view for management path |
| SFMEA Impact | Potential misconfiguration risk reduced after implementation |
| Reliability Gates | No performance degradation expected; add tests |
| Security / Safety | Validate bounds and type/length consistency |

## 5. Change Plan
| Action ID | Type | Description | Owner | Phase Routed To | Preconditions |
|----------|------|-------------|-------|-----------------|---------------|
| ACT-01 | Requirement Update | Specify REQ-PTP-MGMT-TLV (Sec 15) | BA Lead | 02-requirements | Stakeholder sign-off |
| ACT-02 | Design | TLV base + handlers design | Dev Lead | 04-design | ACT-01 complete |
| ACT-03 | Code Fix | Implement TLV parse/serialize, dataset ops | Dev Team | 05-implementation | ACT-02 merged |
| ACT-04 | Tests | Add failing unit/integration tests | QA | 06-integration | Code path scaffold present |

## 6. Verification & Validation Iteration Strategy
- Task Iteration Policy Triggered? Yes
- Re-Verification Scope: Impact-based (management path) + smoke regression
- Regression Test Set: All parsing/serialization suites + new management tests
- Integrity Level Adjustment? No

## 7. Test Artifacts Added / Updated
| Test ID | Type | Purpose | New/Updated | Linked Requirement(s) |
|---------|------|---------|-------------|-----------------------|
| TEST-PTP-MGMT-TLV-Roundtrip | Unit | TLV build/parse roundtrip | New | REQ-PTP-MGMT-TLV |
| TEST-PTP-MGMT-GetSetDataset | Integration | Validate dataset get/set via mgmt | New | REQ-PTP-MGMT-TLV |

## 8. Execution & Evidence
| Step | Description | Result | Evidence Ref |
|------|-------------|--------|--------------|
| 1 | Add failing tests | FAIL (expected) | CI link |
| 2 | Implement TLV framework | PASS locally | Commit SHA |
| 3 | Regression | PASS | CI run |

## 9. Traceability Updates
| Artifact Type | ID | Updated? | Link |
|--------------|----|----------|------|
| Requirement | REQ-PTP-MGMT-TLV | New | 02-requirements/functional/ptp-management-tlv.md |
| Design | DES-PTP-MGMT-TLV | New | 04-design/components/ptp-management-tlv.md |
| Code | src/ptp/management_tlv.* | New | PR link |
| Test | TEST-PTP-MGMT-* | New | tests path |
| SFMEA | FM-PTP-MGMT | New | 07-verification-validation/sfmea.md |

## 10. Risk & Integrity Review
- Residual Risk Rating: Low
- SFMEA Adjusted? Yes
- CIL Item Created/Closed? N/A
- New Hazards Introduced? No

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
- Capture management message path early to avoid late integration

## 13. Sign-Off
| Role | Name | Date | Approval |
|------|------|------|----------|
| QA Lead | | | |
| Reliability Engineer | | | |
| Product Owner | | | |
| Security (if applicable) | | | |

Status: OPEN
