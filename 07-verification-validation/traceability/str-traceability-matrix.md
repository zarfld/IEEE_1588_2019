---
title: Stakeholder Requirements Traceability Matrix (StR → Impl/Test/CAP)
version: 0.1.0
date: 2025-11-09
status: draft
---

# Stakeholder Requirements Traceability Matrix

This matrix links normalized stakeholder requirements (StR-IDs) to current implementation artifacts, tests, and corrective action plans (CAPs). It supports ISO/IEC/IEEE 29148 traceability and auditability.

Legend: Impl (Implemented/Partial/Gap), Tests (CTest name or file), CAP (CAP-YYYYMMDD-xx), Notes (scope/assumptions).

| StR | Summary | Impl | Tests | CAP | Notes |
|-----|---------|------|-------|-----|-------|
| StR-001 | Use P2P delay on full-duplex 802.3 | Gap | — | CAP-20251109-02 (delay mechanisms) | Operational Pdelay not implemented |
| StR-002 | Full-duplex p2p, untagged frames | Partial | — | — | Transport constraints tracked; tagging rules to verify via tests |
| StR-003 | BMCA per 802.1AS; ext cfg FALSE (Domain 0) | Gap | — | CAP-20251109-03 (state machine/BMCA) | Full BMCA pending |
| StR-004 | Path Trace TLV mandatory | Gap | — | CAP-20251109-04 (signaling/TLVs) | TLV parsing pending |
| StR-005 | Exclude FAULTY/UNCALIBRATED/LISTENING/PRE_MASTER | Partial | state machine tests | CAP-20251109-03 | Ensure transitions cannot enter excluded states |
| StR-006 | No foreign master feature | Partial | — | CAP-20251109-03 | Pruning/foreign master DS behavior to align |
| StR-007 | Use 802.1AS mgmt (no 1588 mgmt msgs) | Gap | — | CAP-20251109-01 (management TLV/framework) | Mgmt via datasets/MIB not implemented |
| StR-008 | Exclude integrated security (1588 16.14, Annex P) | N/A | — | — | Out of scope; explicit exclusion documented |
| StR-009 | Disable PAUSE/PFC; capability to disable | Gap | — | New CAP suggested | Add management toggle and enforcement tests |
| StR-010 | ±100 ppm; ≤40 ns granularity | Partial | resource/clock tests | — | Clock model constraints partially asserted |
| StR-011 | Optional multi-domain support | Gap | — | New CAP suggested | Requires instance/domain scaffolding |
| StR-012 | CMLDS req’d if multi-domain | Gap | — | New CAP suggested | Depends on StR-011 |
| StR-013 | External port config optional | Gap | — | New CAP suggested | Management + state control required |
| StR-014 | One-step modes optional, managed | Gap | — | New CAP suggested | Managed objects + Sync/FUP wiring needed |
| StR-015 | Asymmetry modeling optional | Gap | — | New CAP suggested | Managed objects + compensation path |
| StR-016 | 4 domains for industrial profile | Gap | — | New CAP suggested | Overlay on multi-domain |
| StR-017 | CMLDS mandatory (industrial) | Gap | — | New CAP suggested | As above |
| StR-018 | ≤8 ns timestamp accuracy | Gap | — | New CAP suggested | Performance acceptance harness |
| StR-019 | <1 s to <1 µs sync | Gap | servo convergence sim | New CAP suggested | Tighten convergence test target |
| StR-020 | Capability to disable EEE | Gap | — | New CAP suggested | Mgmt toggle and enforcement |

Initial focus: StR-001, StR-004, StR-003 to unlock core profile compliance; then StR-011/012 for industrial profile readiness.
