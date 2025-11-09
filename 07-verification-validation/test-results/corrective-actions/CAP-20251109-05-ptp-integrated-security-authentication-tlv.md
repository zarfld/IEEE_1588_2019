# Corrective Action Package (CAP)

ID: CAP-20251109-05

Anomaly ID(s): GAP-SEC-001

Date Opened: 2025-11-09

Owner: security-review, timing-core

Integrity Level: 3

Status: OPEN

## 1. Summary

- Detected In Phase: Architecture/design review and compliance cross-check
- Defect Description: Integrated security (Authentication TLV) per IEEE 1588-2019 optional feature (clause reference below) is not implemented; only design mentions exist. No tests.
- User Impact: Security-conscious environments cannot validate provenance/integrity of PTP messages; deployments may require this feature for acceptance.
- Severity: S2 High (strategic, optional)
- Integrity Level Affected: Message parsing pipeline; management/security hooks

## 2. Objective Evidence of Failure

| Evidence Type | Reference | Notes |
|--------------|-----------|-------|
| Design Mentions | 04-design/components/ieee-1588-2019-management-design.md | Security TLV noted |
| Missing Code | No security/auth TLV encode/validate | Not found |
| Missing Tests | No positive/negative authentication tests | Not found |

## 3. Root Cause Analysis

- Deferred feature; complexity and optional nature led to postponement.
- Fault Class: Requirements gap.

## 4. Impact & Change Scope

| Dimension | Assessment |
|-----------|------------|
| Components | Security manager interface, TLV encode/validate path |
| Coupling | Management path, message parsing, configuration |
| Requirements | REQ-PTP-SEC-AUTH (to add) |
| Design | DES-PTP-SEC-AUTH (to add) |
| Performance | Cryptographic validation cost; make optional via feature flag |

## 5. Change Plan (Staged)

| Stage | Type | Description | Owner | Phase | Preconditions |
|------|------|-------------|-------|-------|---------------|
| S1 | Requirement | Add REQ-PTP-SEC-AUTH (clause ref); define policy & feature flag | BA Lead | 02 | Stakeholder sign-off |
| S2 | Design | Security interface + TLV validation flow | Dev Lead | 04 | S1 |
| S3 | Tests (Failing) | Add TEST-SEC-001/002 (valid/invalid auth) | QA | 05 | S2 |
| S4 | Implementation | Minimal HMAC-based validation (configurable) | Dev Team | 05 | S3 |
| S5 | Docs/Config | Security policy configuration doc & examples | Dev Lead | 08 | S4 |

## 6. Verification Strategy

- Add unit tests for TLV acceptance/rejection.
- Fuzz malformed lengths; ensure safe drops, no crashes.
- Performance check: Ensure validation bounded and optional.

## 7. Test Artifacts

| Test ID | Type | Purpose |
|---------|------|---------|
| TEST-SEC-001 | Unit | Accept valid Authentication TLV |
| TEST-SEC-002 | Unit | Reject invalid/missing Authentication TLV |
| TEST-SEC-003 | Negative | Malformed TLV length handling |

## 8. Traceability Updates

| Artifact | ID | Action |
|---------|----|-------|
| Requirement | REQ-PTP-SEC-AUTH | Add |
| Design | DES-PTP-SEC-AUTH | Add |
| Code | security_tlv.*; security_manager.* | Add |
| Tests | TEST-SEC-* | Add |
| Compliance Matrix | GAP-SEC-001 | Update when tests pass |

## 9. Risk Review

- Crypto errors must not stall timing pipeline; enforce timeouts and safe-fail drop.
- Key management out of scope; accept injected key material via abstract interface.

## 10. Closure Criteria

| Criterion | Status |
|-----------|--------|
| Requirements & design merged | ☐ |
| Tests passing (unit + negative) | ☐ |
| Feature flag documented | ☐ |
| Compliance matrix updated | ☐ |
| Security review sign-off | ☐ |

## 11. References (by clause number only)

- IEEE 1588-2019: Clause 16.14 (integrated security)

Status: OPEN
