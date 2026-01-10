# CI Traceability Fix - Action Plan

**Date**: 2025-12-02  
**CI Failure**: Missing "Traces to: #N" links causing <95% coverage  
**Root Cause**: Incorrect StR-005 mapping propagated across multiple issues

---

## 🚨 CRITICAL FINDING: Issue #197 Mapping Error

### The Problem

**stakeholder-requirements-spec.md Definition**:
```markdown
#### STR-PERF-001: Synchronization Accuracy
- Requirement: Clock offset <1 microsecond from master
- Success Criteria: 95% <1µs, 99% <2µs
```

**Multiple Issues INCORRECTLY Claim**:
- "StR-005 (Maps to STR-PERF-001: Synchronization Accuracy)"

**GitHub API Truth (Issue #197 Title)**:
```
StR-005: Exclusion of Specific PTP Port States
Category: IEEE 802.1AS Profile Exclusions
Description: PTP Port states FAULTY, UNCALIBRATED, LISTENING, PRE_MASTER SHALL NOT be used
```

**Conclusion**: **StR-005 ≠ Synchronization Accuracy**. It's about **Port State Exclusions**.

---

## 📊 Verified Issue Inventory (All 217 Issues)

| Range | Count | Category | Notes |
|-------|-------|----------|-------|
| #193-#217 | 25 | Stakeholder Requirements (StR-*) | StR-001 through StR-020 (gaps exist) |
| #118-#192 | 75 | Functional/System (REQ-F-*, REQ-S-*) | Many claim incorrect StR-005 mapping |
| #94-#117 | 24 | Non-Functional (REQ-NFR-*) | Performance, reliability, security |
| #46-#93 | 48 | Functional (REQ-FUN-*) | Core PTP protocol functions |
| #34-#45 | 12 | System (REQ-SYS-*) | Architecture, cross-platform |
| #18-#33 | 16 | Stakeholder (REQ-STK-*) | High-level business needs |
| #9-#17 | 9 | GPS-PPS Requirements | Many incorrectly reference StR-005 |
| #1-#8 | 8 | Roadmap/Features | Enhancements and milestones |

**Total**: 217 issues (all OPEN, 0 CLOSED)

---

## ❌ Issues with INCORRECT "Traces to" Links

### Category 1: StR-005 Misattribution (7+ issues)

| Issue # | Current (WRONG) | Correct Mapping | Action |
|---------|-----------------|-----------------|--------|
| #9 | Traces to: #197 (claims synchronization) | **TBD** - find actual STR-PERF-001 | Update body |
| #11 | Traces to: StR-005 (claims synchronization) | **TBD** - find actual STR-PERF-001 | Update body |
| #12 | Traces to: StR-005 (claims synchronization) | **TBD** - find actual STR-PERF-001 | Update body |
| #15 | Traces to: StR-005 (claims synchronization) | **TBD** - find actual STR-PERF-001 | Update body |
| #180 | StR-005 = Synchronization | **TBD** - find actual STR-PERF-001 | Update body |
| #184 | StR-005 = Synchronization | **TBD** - find actual STR-PERF-001 | Update body |
| #189 | StR-005 = Synchronization | **TBD** - find actual STR-PERF-001 | Update body |

### Category 2: Missing "Traces to: #N" Pattern (CI Validation Failure)

**CI Script Pattern**: `[Tt]races?\s+to:?\s*#(\d+)`

**Total Requirements Needing Links**: ~209 issues (excluding roadmap/feature issues #1-#8)

**Current Coverage**: < 95% (causing CI failure)

---

## 🔍 URGENT: Find STR-PERF-001 GitHub Issue

**Question**: Which GitHub issue represents **STR-PERF-001 (Synchronization Accuracy)**?

**Search Candidates**:

### Hypothesis 1: Check StR-* issues (#193-#217)
- **None** of issues #193-#217 have "Synchronization Accuracy" in title
- StR-001 through StR-020 are defined, but NO "Synchronization Accuracy" StR-* issue exists

### Hypothesis 2: Check REQ-NFR-* Performance Issues (#94-#117)
- #95: REQ-NFR-PTP-001: **Microsecond-level timing accuracy (±1μs typical)** ← **CANDIDATE**
- #94: REQ-NFR-PTP-002: Sub-microsecond accuracy with hardware timestamping (±100ns) ← **CANDIDATE**
- #98: REQ-NFR-PTP-003: Timing accuracy under network load and jitter ← **CANDIDATE**

**LIKELY ANSWER**: Issue #95 (REQ-NFR-PTP-001) is the non-functional requirement for accuracy

### Hypothesis 3: Check REQ-STK-* Stakeholder Issues (#18-#33)
- #18: REQ-STK-PTP-001: **Enterprise-grade timing precision beyond basic gPTP** ← **CANDIDATE**

**FINDING**: **STR-PERF-001** from stakeholder spec **may not have a corresponding GitHub issue**!

---

## 🎯 Naming Convention Analysis

| Source | Format | Example | Notes |
|--------|--------|---------|-------|
| **stakeholder-requirements-spec.md** | STR-CATEGORY-NNN | STR-PERF-001 | Spec document format |
| **GitHub Issues (#193-#217)** | StR-NNN | StR-005 | GitHub issue title format |
| **Issue Bodies (WRONG)** | "StR-005 (Maps to STR-PERF-001)" | - | **INCORRECT CLAIM** |

**DISCREPANCY**: 
- **Spec uses**: STR-PERF-001, STR-STD-001, STR-ARCH-001 (category-based)
- **GitHub uses**: StR-001, StR-002, ... StR-020 (sequential numbering)
- **No direct mapping exists** between STR-CATEGORY-NNN and StR-NNN!

**CONCLUSION**: The mapping claim "StR-005 (Maps to STR-PERF-001)" was **incorrect speculation**, not verified fact.

---

## ✅ Correct Mappings (Verified from GitHub API)

### StR-* Issues (Sequential, #193-#217)

| Issue # | StR-ID | Title | Category |
|---------|--------|-------|----------|
| #195 | StR-001 | P2P Path Delay Mechanism on Full-Duplex 802.3 Links | Delay |
| #193 | StR-002 | Full-Duplex Point-to-Point 802.3 with Untagged Frames | Network |
| #196 | StR-003 | BMCA Implementation per 802.1AS with Domain 0 Constraints | BMCA |
| #194 | StR-004 | Path Trace TLV Processing and Transmission | Path Trace |
| **#197** | **StR-005** | **Exclusion of Specific PTP Port States** | **Exclusions** ← **NOT SYNC** |
| #199 | StR-006 | Exclusion of Foreign Master Feature | Exclusions |
| #202 | StR-007 | Management via 802.1AS Data Sets and MIB | Management |
| #201 | StR-008 | Exclusion of IEEE 1588 Integrated Security | Security |
| #198 | StR-009 | Prohibition of MAC PAUSE and PFC on 802.1AS Traffic | Network |
| #200 | StR-010 | LocalClock Frequency Offset and Measurement Granularity | Clock |
| #205 | StR-011 | Optional Support for Multiple PTP Domains (1-127) | Optional |
| #207 | StR-012 | CMLDS Mandatory for Multi-Domain Support | Multi-Domain |
| #204 | StR-013 | Optional External Port Configuration Support | Optional |
| #203 | StR-014 | Optional One-Step Transmit/Receive Mode Support | Optional |
| #206 | StR-015 | Optional Delay Asymmetry Modeling and Compensation | Optional |
| #212 | StR-016 | IEC/IEEE 60802 Four Synchronization Domains | IEC 60802 |
| #210 | StR-017 | IEC/IEEE 60802 CMLDS Mandatory Requirement | IEC 60802 |
| #208 | StR-018 | IEC/IEEE 60802 Timestamp Accuracy ≤8 ns | IEC 60802 |
| #209 | StR-019 | IEC/IEEE 60802 Convergence <1 µs in <1 Second per Hop | IEC 60802 |
| #211 | StR-020 | IEC/IEEE 60802 Capability to Disable EEE | IEC 60802 |

**No StR-* issue for "Synchronization Accuracy" exists!**

---

## 📝 Manual Correction Plan

### Phase 1: Identify Correct Mappings (BEFORE updating issues)

1. ✅ **COMPLETED**: Verified all 217 GitHub issues via API
2. ✅ **COMPLETED**: Confirmed Issue #197 = StR-005 = "Port State Exclusions"
3. 🚧 **IN PROGRESS**: Determine which issue(s) represent STR-PERF-001 accuracy requirement
   - **Candidate**: Issue #95 (REQ-NFR-PTP-001: Microsecond-level timing accuracy)
   - **Candidate**: Issue #18 (REQ-STK-PTP-001: Enterprise-grade timing precision)
4. ⏳ **PENDING**: Create mapping table: STR-CATEGORY-NNN → GitHub Issue #

### Phase 2: Update Issue Bodies (ONE BY ONE per user requirement)

**Batch 1: Fix StR-005 Misattributions** (7 issues):
1. Issue #9: Update "Traces to: #197" → "Traces to: #95" (or correct issue)
2. Issue #11: Update "Traces to: StR-005" → "Traces to: #95" (or correct issue)
3. Issue #12: Update "Traces to: StR-005" → "Traces to: #95" (or correct issue)
4. Issue #15: Update "Traces to: StR-005" → "Traces to: #95" (or correct issue)
5. Issue #180: Update body removing StR-005 synchronization claim
6. Issue #184: Update body removing StR-005 synchronization claim
7. Issue #189: Update body removing StR-005 synchronization claim

**Batch 2: Add Missing "Traces to: #N" Links** (~202 remaining issues):
- Systematically add "Traces to: #N" for each requirement issue
- Use verified mapping table from Phase 1
- Target: ≥95% coverage for CI validation

### Phase 3: Validation

1. Re-run CI: `.github/workflows/traceability-validation.yml`
2. Verify ≥95% coverage achieved
3. Confirm no broken links
4. Document final mapping in `reports/tracability-verified-github-extraction-2025-12-02.md`

---

## ⏱️ Time Estimates

| Phase | Task | Time | Status |
|-------|------|------|--------|
| 1.1 | Fetch all 217 issues | 10 min | ✅ DONE |
| 1.2 | Create verified mapping table | 30 min | ✅ DONE |
| 1.3 | Identify STR-PERF-001 mapping | 30 min | 🚧 IN PROGRESS |
| 1.4 | Complete mapping table | 60 min | ⏳ TODO |
| 2.1 | Fix 7 StR-005 misattributions | 30 min | ⏳ TODO |
| 2.2 | Add ~202 "Traces to" links | 7-10 hours | ⏳ TODO |
| 3.1 | Re-run CI validation | 5 min | ⏳ TODO |
| 3.2 | Document final results | 30 min | ⏳ TODO |
| **Total** | | **9-12 hours** | **~15% done** |

---

## 🎯 Next Immediate Actions

1. **URGENT**: Search issue #95, #94, #98, #18 bodies to confirm which represents STR-PERF-001
2. **DECISION**: Choose correct parent issue for accuracy requirements
3. **UPDATE**: Fix Issue #9 first (known error, easy to verify)
4. **VALIDATE**: Run CI after first fix to confirm pattern works
5. **CONTINUE**: Systematically update remaining 206+ issues

---

## 📚 Reference Documents

- **Verified GitHub Extraction**: `reports/tracability-verified-github-extraction-2025-12-02.md`
- **Stakeholder Requirements Spec**: `01-stakeholder-requirements/stakeholder-requirements-spec.md`
- **CI Validation Script**: `scripts/github-traceability-report.py`
- **CI Workflow**: `.github/workflows/traceability-validation.yml`

---

*This action plan provides the roadmap to fix CI traceability validation failure through systematic, manual correction of all "Traces to" links based on verified GitHub API data.*
