# Requirements Traceability Repair Plan

**Date**: 2025-11-11  
**Issue**: CI traceability validation failing - 53 requirements have no linked architecture/design/test elements  
**Priority**: HIGH (Blocking CI)  
**Estimated Effort**: 4-6 hours

---

## Executive Summary

**Problem**: The traceability validation script (`scripts/validate-traceability.py`) is failing because 53 requirements from `system-requirements-specification.md` have `(none)` in the "Linked Elements" column of `reports/traceability-matrix.md`.

**Root Cause**: The traceability matrix generation script (`scripts/generate-traceability-matrix.py`) is not finding links between:
- Requirements in `02-requirements/system-requirements-specification.md`
- Design elements in `03-architecture/` and `04-design/`
- Test cases in `07-verification-validation/test-cases/`

**Solution**: Add proper traceability metadata to design documents and test cases to link them to requirements.

---

## Failing Requirements Analysis

### Category 1: Functional Requirements (19 requirements)

**Requirements with no links**:
- REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-012, REQ-F-015
- REQ-FUNC-001 through REQ-FUNC-010
- REQ-FUNC-101 through REQ-FUNC-105

**Expected Links** (based on system-requirements-specification.md):
- `REQ-F-001` to `REQ-F-005` → Already linked (✅ Working example)
- `REQ-F-006` to `REQ-F-015` → Need design/test links
- `REQ-FUNC-001` to `REQ-FUNC-010` → Core PTP protocol requirements
- `REQ-FUNC-101` to `REQ-FUNC-105` → Clock synchronization requirements

### Category 2: Portability Requirements (6 requirements)

**Requirements with no links**:
- REQ-PORT-010, REQ-PORT-012, REQ-PORT-020, REQ-PORT-025, REQ-PORT-030, REQ-PORT-032

**Expected Links**:
- Should link to `ieee-1588-2019-hal-interface-design.md`
- Should link to portability test cases (TEST-PORT-*, TEST-HAL-*)

### Category 3: Reliability Requirements (5 requirements)

**Requirements with no links**:
- REQ-REL-001, REQ-REL-002, REQ-REL-003, REQ-REL-004, REQ-REL-005

**Expected Links**:
- Should link to reliability test results
- Should link to operational profile validation
- Should link to SRG analysis reports

### Category 4: Security Requirements (7 requirements)

**Requirements with no links**:
- REQ-S-002, REQ-SEC-001, REQ-SEC-002, REQ-SEC-003, REQ-SEC-005, REQ-SEC-010, REQ-SEC-015, REQ-SEC-020

**Expected Links**:
- Should link to security test cases (TEST-SEC-*)
- Should link to security design elements

### Category 5: Standards Compliance Requirements (4 requirements)

**Requirements with no links**:
- REQ-STD-001, REQ-STD-010, REQ-STD-020, REQ-STD-025

**Expected Links**:
- Should link to IEEE 1588-2019 compliance test cases
- Should link to standards compliance design documents

### Category 6: Usability Requirements (10 requirements)

**Requirements with no links**:
- REQ-USE-001, REQ-USE-005, REQ-USE-010, REQ-USE-015, REQ-USE-020, REQ-USE-022, REQ-USE-030, REQ-USE-035

**Expected Links**:
- Should link to documentation test cases (TEST-DOC-*)
- Should link to API design documents
- Should link to example applications

### Category 7: Architecture Constraint Requirements (2 requirements)

**Requirements with no links**:
- REQ-CONST-ARCH-001, REQ-CONST-TECH-001

**Expected Links**:
- Should link to architecture decisions (ADRs)
- Should link to architecture constraints documents

### Category 8: Verification Baseline Requirement (1 requirement)

**Requirements with no links**:
- REQ-VER-BASELINE-001

**Expected Links**:
- Should link to requirements verification baseline document
- Should link to verification test cases

---

## How Traceability Works (Current System)

### 1. Traceability Matrix Generation

**Script**: `scripts/generate-traceability-matrix.py`

**Logic** (heuristic-based):
```python
# Scans markdown files for patterns like:
- "Trace to: REQ-XXX" or "Implements: REQ-XXX"
- "satisfies REQ-XXX"
- "verifies REQ-XXX"
- Links in YAML front matter: traceability.requirements: [REQ-XXX]
```

**Output**: `reports/traceability-matrix.md`

```markdown
| Requirement | Linked Elements (ADR / Component / Scenario / Test) |
|-------------|----------------------------------------------------|
| REQ-F-001   | ADR-001, ADR-002, ..., TEST-MSG-001                 |
| REQ-FUNC-001 | (none)                                             |
```

### 2. Traceability Validation

**Script**: `scripts/validate-traceability.py`

**Logic**:
```python
# Reads traceability-matrix.md
# Fails if any requirement has linked = "(none)"
# Outputs: ❌ Requirement REQ-XXX has no linked architecture/design/test elements
```

**Requirement for Pass**:
Every requirement MUST link to at least ONE of:
- Architecture decision (ADR-XXX)
- Design component (ARC-C-XXX, DES-XXX)
- Test case (TEST-XXX, TC-XXX)
- Quality scenario (QA-SC-XXX)

---

## Solution: Add Traceability Metadata

### Approach 1: Add Traceability to Existing Test Cases ✅ **RECOMMENDED**

**Effort**: 2-3 hours  
**Impact**: Links 80%+ of failing requirements

**Action**: Update test case files in `07-verification-validation/test-cases/` to include requirement traces.

**Example** - `TEST-MSG-001-message-parsing.md`:

```markdown
---
testId: TEST-MSG-001
requirementId: REQ-FUNC-001  # ← ADD THIS
trace:
  - REQ-FUNC-001  # Message parsing requirement
  - REQ-F-001     # IEEE 1588-2019 message type support
  - REQ-STD-001   # Standards compliance
---

# Test Case: TEST-MSG-001 - Message Parsing Validation

**Traces to**: REQ-FUNC-001, REQ-F-001, REQ-STD-001  # ← ADD THIS

[Rest of test case content]
```

**Files to Update** (24 test cases):

1. **Message/Protocol Tests** → REQ-FUNC-001 to REQ-FUNC-003, REQ-F-001, REQ-F-002, REQ-STD-001:
   - `TEST-MSG-001-message-parsing.md`
   - `TEST-MSG-NEG-001-negative-parsing.md`
   - `TEST-MSG-HANDLERS-001-message-handlers-coverage.md`

2. **BMCA Tests** → REQ-FUNC-004 to REQ-FUNC-007, REQ-F-002, REQ-STD-010:
   - `TEST-BMCA-001-master-selection.md`
   - `TEST-BMCA-DATASET-001-bmca-dataset-integrity.md`
   - `TEST-BMCA-TIMEOUT-001-reselection.md`
   - `TEST-BMCA-TRANSITION-001-bmca-transition.md`

3. **Sync/Offset Tests** → REQ-FUNC-008 to REQ-FUNC-010, REQ-FUNC-101 to REQ-FUNC-105, REQ-F-003, REQ-F-004:
   - `TEST-SYNC-001-offset-calculation.md`
   - `TEST-SYNC-OFFSET-DETAIL-001-sync-offset-detail.md`
   - `TEST-SYNC-OUTLIER-001-offset-outlier-handling.md`

4. **Servo Tests** → REQ-FUNC-102, REQ-FUNC-103, REQ-F-004:
   - `TEST-SERVO-001-pi-controller-convergence.md`
   - `TEST-SERVO-OUTLIER-001-servo-disturbance.md`

5. **Portability Tests** → REQ-PORT-010, REQ-PORT-012, REQ-PORT-020, REQ-PORT-025, REQ-PORT-030, REQ-PORT-032:
   - `TEST-PORT-BUILD-MULTI-001-portability-builds.md`
   - `TEST-HAL-001-hal-interface.md`
   - `TEST-HAL-MOCK-001-hal-mock-isolation.md`

6. **Security Tests** → REQ-SEC-001, REQ-SEC-002, REQ-SEC-003, REQ-SEC-005, REQ-SEC-010, REQ-SEC-015, REQ-SEC-020:
   - `TEST-SEC-INPUT-FUZZ-001-security-fuzzing.md`
   - `TEST-SEC-MEM-SAFETY-001-memory-safety.md`

7. **Performance Tests** → REQ-PERF-*, REQ-F-006 to REQ-F-015:
   - `TEST-PERF-OFFSET-P95-001-performance-offset.md`
   - `TEST-WCET-CRITPATH-001-wcet-critical-path.md`
   - `TEST-RESOURCE-FOOTPRINT-001-resource-footprint.md`

8. **Documentation Tests** → REQ-USE-001, REQ-USE-005, REQ-USE-010, REQ-USE-015, REQ-USE-020, REQ-USE-022, REQ-USE-030, REQ-USE-035:
   - `TEST-DOC-QUICKSTART-001-documentation-quickstart.md`

9. **Build System Tests** → REQ-F-012, REQ-F-015, REQ-STD-020, REQ-STD-025:
   - `TEST-CMAKE-OPTIONS-001-cmake-options.md`

### Approach 2: Add Traceability to Design Documents ✅ **COMPLEMENTARY**

**Effort**: 1-2 hours  
**Impact**: Provides architecture → requirements mapping

**Action**: Update design documents in `04-design/components/` to include requirement traces.

**Example** - `ieee-1588-2019-message-processing-design.md`:

```markdown
---
designId: DES-MSG-001
phase: "04-design"
trace:
  requirements:
    - REQ-FUNC-001  # ← ADD THIS
    - REQ-F-001
    - REQ-STD-001
  architecture:
    - ADR-001
    - ARC-C-001
---

# IEEE 1588-2019 Message Processing Design

**Implements**: REQ-FUNC-001, REQ-F-001, REQ-STD-001  # ← ADD THIS

[Rest of design content]
```

**Files to Update** (14 design documents):

1. `ieee-1588-2019-message-processing-design.md` → REQ-FUNC-001 to REQ-FUNC-003, REQ-F-001
2. `ieee-1588-2019-bmca-design.md` → REQ-FUNC-004 to REQ-FUNC-007, REQ-F-002
3. `ieee-1588-2019-state-machine-design.md` → REQ-FUNC-008, REQ-F-003
4. `ieee-1588-2019-hal-interface-design.md` → REQ-PORT-010 to REQ-PORT-032
5. `sdd-core-protocol.md` → REQ-FUNC-001 to REQ-FUNC-010
6. `sdd-bmca.md` → REQ-FUNC-004 to REQ-FUNC-007
7. `sdd-servo.md` → REQ-FUNC-102, REQ-FUNC-103, REQ-F-004
8. `sdd-state-machine.md` → REQ-FUNC-008 to REQ-FUNC-010
9. `sdd-transport.md` → REQ-F-005, REQ-F-006

### Approach 3: Create Reliability Evidence Documents ✅ **HIGH PRIORITY**

**Effort**: 1-2 hours  
**Impact**: Links REQ-REL-* requirements

**Action**: Create reliability evidence documents that trace to REQ-REL-001 to REQ-REL-005.

**Files to Create**:

1. **`07-verification-validation/test-results/reliability-evidence.md`**

```markdown
---
evidenceType: reliability
phase: "07-verification-validation"
trace:
  requirements:
    - REQ-REL-001  # MTBF target
    - REQ-REL-002  # Failure rate
    - REQ-REL-003  # Availability
    - REQ-REL-004  # Fault tolerance
    - REQ-REL-005  # Recovery mechanisms
---

# Reliability Evidence Package

**Verifies**: REQ-REL-001, REQ-REL-002, REQ-REL-003, REQ-REL-004, REQ-REL-005

## 1. Operational Profile Validation

[Evidence of REQ-REL-001 compliance]

## 2. Failure Rate Analysis

[Evidence of REQ-REL-002 compliance - current λ(t)]

## 3. Availability Metrics

[Evidence of REQ-REL-003 compliance - uptime %]

## 4. Fault Tolerance Testing

[Evidence of REQ-REL-004 compliance - recovery from failures]

## 5. Recovery Mechanism Validation

[Evidence of REQ-REL-005 compliance - restore times]
```

### Approach 4: Link Architecture Constraints ✅ **QUICK WIN**

**Effort**: 15-30 minutes  
**Impact**: Links REQ-CONST-* requirements

**Action**: Update architecture documents to trace constraint requirements.

**Files to Update**:

1. **`03-architecture/constraints/architectural-constraints.md`** (if exists, else create):

```markdown
---
phase: "03-architecture"
trace:
  requirements:
    - REQ-CONST-ARCH-001
    - REQ-CONST-TECH-001
---

# Architectural Constraints

**Satisfies**: REQ-CONST-ARCH-001, REQ-CONST-TECH-001

[Constraint definitions]
```

2. **`03-architecture/decisions/ADR-001-hardware-abstraction.md`** (update):

Add to existing ADR:
```markdown
**Satisfies**: REQ-CONST-ARCH-001 (Hardware-agnostic principle)
```

### Approach 5: Link Verification Baseline Requirement ✅ **TRIVIAL**

**Effort**: 5 minutes  
**Impact**: Links REQ-VER-BASELINE-001

**Action**: Update requirements verification baseline document.

**File**: `07-verification-validation/requirements-verification-baseline.md`

Add to existing file front matter:
```markdown
---
trace:
  requirements:
    - REQ-VER-BASELINE-001
---

# Requirements Verification Baseline

**Implements**: REQ-VER-BASELINE-001
```

---

## Execution Plan

### Phase 1: Quick Wins (30 minutes)

**Objective**: Fix easy requirements to reduce error count

**Actions**:
1. ✅ Update `requirements-verification-baseline.md` → Links REQ-VER-BASELINE-001 (5 min)
2. ✅ Create/update `architectural-constraints.md` → Links REQ-CONST-* (15 min)
3. ✅ Update ADR-001 with REQ-CONST-ARCH-001 trace (10 min)

**Impact**: Fixes 3/53 requirements (5.7%)

---

### Phase 2: Test Case Traceability (2-3 hours)

**Objective**: Link test cases to requirements (highest impact)

**Priority 1** - Critical test cases (1 hour):
1. Message parsing tests → REQ-FUNC-001 to REQ-FUNC-003, REQ-F-001, REQ-STD-001
2. BMCA tests → REQ-FUNC-004 to REQ-FUNC-007, REQ-F-002, REQ-STD-010
3. HAL tests → REQ-PORT-010 to REQ-PORT-032

**Priority 2** - Important test cases (1 hour):
4. Sync/offset tests → REQ-FUNC-008 to REQ-FUNC-010, REQ-FUNC-101 to REQ-FUNC-105
5. Servo tests → REQ-FUNC-102, REQ-FUNC-103, REQ-F-004
6. Security tests → REQ-SEC-*
7. Performance tests → REQ-F-006 to REQ-F-015

**Priority 3** - Documentation tests (30 minutes):
8. Documentation tests → REQ-USE-*
9. Build system tests → REQ-STD-020, REQ-STD-025

**Impact**: Fixes 40-45/53 requirements (75-85%)

---

### Phase 3: Design Document Traceability (1-2 hours)

**Objective**: Link design documents to requirements

**Actions**:
1. Update message processing design → REQ-FUNC-001 to REQ-FUNC-003
2. Update BMCA design → REQ-FUNC-004 to REQ-FUNC-007
3. Update HAL design → REQ-PORT-*
4. Update SDD documents → REQ-FUNC-*, REQ-F-*

**Impact**: Strengthens traceability (redundant links for confidence)

---

### Phase 4: Reliability Evidence (1-2 hours)

**Objective**: Create reliability evidence documents

**Actions**:
1. Create `reliability-evidence.md` → REQ-REL-001 to REQ-REL-005
2. Link to SRG analysis reports
3. Link to operational profile validation
4. Link to reliability test results

**Impact**: Fixes 5/53 requirements (9.4%)

---

## Total Effort Summary

| Phase | Effort | Impact | Priority |
|-------|--------|--------|----------|
| Phase 1: Quick Wins | 30 min | 3 reqs (5.7%) | HIGH |
| Phase 2: Test Traceability | 2-3 hours | 40-45 reqs (75-85%) | CRITICAL |
| Phase 3: Design Traceability | 1-2 hours | Redundant links | MEDIUM |
| Phase 4: Reliability Evidence | 1-2 hours | 5 reqs (9.4%) | MEDIUM |
| **TOTAL** | **4.5-7.5 hours** | **48-53 reqs (91-100%)** | |

**Critical Path**: Phase 1 + Phase 2 = 2.5-3.5 hours → Fixes 43-48/53 requirements (81-91%)

---

## Expected Outcomes

### After Phase 1 (30 minutes):
- CI error count: 53 → 50 (5.7% improvement)
- Status: Still failing but progress visible

### After Phase 2 (3-3.5 hours total):
- CI error count: 53 → 5-13 (81-91% improvement)
- Status: Most requirements linked, CI may pass or near-pass

### After Phase 3 (4.5-5.5 hours total):
- CI error count: 53 → 0-8 (85-100% improvement)
- Status: Strong traceability, CI likely passing

### After Phase 4 (5.5-7.5 hours total):
- CI error count: 53 → 0 (100% improvement)
- Status: ✅ **CI PASSING** - Complete traceability

---

## Acceptance Criteria

✅ **CI Traceability Validation Passes**:
```bash
python scripts/validate-traceability.py
# Output: ✅ Traceability validation passed (basic).
# Exit code: 0
```

✅ **All 53 Requirements Linked**:
- Every REQ-* in `traceability-matrix.md` has at least one linked element
- No `(none)` entries in "Linked Elements" column

✅ **Bi-directional Traceability**:
- Requirements → Design: Every requirement links to at least one design element
- Requirements → Tests: Every requirement links to at least one test case
- Design → Requirements: Every design document traces back to requirements
- Tests → Requirements: Every test case traces back to requirements

✅ **Documentation Updated**:
- Test case files include `trace:` metadata in front matter
- Design documents include `trace:` metadata in front matter
- Reliability evidence document created

---

## Risks and Mitigations

### Risk 1: Script Doesn't Detect New Links ⚠️ MEDIUM

**Scenario**: After adding traceability metadata, script still shows `(none)`

**Root Cause**: Script's heuristics don't match our metadata format

**Mitigation**:
1. Review `generate-traceability-matrix.py` to understand search patterns
2. Test with one file first before updating all files
3. Run `python scripts/generate-traceability-matrix.py` after each update
4. Adjust metadata format to match script expectations

### Risk 2: Requirements Don't Actually Exist 🟢 LOW

**Scenario**: Some REQ-* IDs in error list don't exist in system-requirements-specification.md

**Root Cause**: Requirements removed or renamed

**Mitigation**:
1. Verify each REQ-* exists in `02-requirements/system-requirements-specification.md`
2. If requirement doesn't exist, it's an orphan in architecture/design docs
3. Remove orphan references or update to correct REQ-* ID

### Risk 3: Time Estimate Too Optimistic ⚠️ MEDIUM

**Scenario**: Effort exceeds 4-6 hours estimate

**Root Cause**: More test cases need updating than expected

**Mitigation**:
1. Execute Phase 1 + Phase 2 Priority 1 first (critical path: 1.5 hours)
2. Re-evaluate after first pass: how many requirements fixed?
3. If >80% fixed after Priority 1, continue
4. If <80% fixed, escalate and re-plan

---

## Next Steps

**Immediate** (now):
1. ✅ Read `generate-traceability-matrix.py` to understand link detection patterns
2. ✅ Verify script search patterns (e.g., "Trace to:", "Implements:", YAML front matter)
3. ✅ Create one test file with traceability as proof of concept
4. ✅ Run traceability generation and validation to confirm fix works

**Execute** (today):
1. Phase 1: Quick Wins (30 minutes) - Fix 3 requirements
2. Phase 2 Priority 1: Critical test cases (1 hour) - Fix ~20 requirements
3. Regenerate traceability matrix and validate
4. Check CI status: Expected 53 → ~30 errors remaining

**Continue** (tomorrow if needed):
1. Phase 2 Priority 2-3: Remaining test cases (1.5-2 hours)
2. Phase 4: Reliability evidence (1-2 hours)
3. Phase 3: Design documents (optional redundancy)

**Verify** (after all changes):
1. Run `python scripts/generate-traceability-matrix.py`
2. Run `python scripts/validate-traceability.py`
3. Commit changes and push to trigger CI
4. Verify CI traceability check passes ✅

---

**Document Control**:

- **Created**: 2025-11-11 by AI Assistant
- **Version**: 1.0 (Repair Plan)
- **Status**: READY TO EXECUTE
- **Owner**: V&V Lead
- **Next Action**: Execute Phase 1 (Quick Wins)

---

**End of Requirements Traceability Repair Plan**
