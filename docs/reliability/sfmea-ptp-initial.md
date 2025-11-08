---
id: REL-SFMEA-001
phase: 05-implementation
status: draft
schema: internal/sfmea/v1
related:
  requirements:
    - REQ-F-002   # BMCA state machine
    - REQ-F-003   # Offset/delay computation
    - REQ-NF-REL-004 # Health/self-test API
  design:
    - DES-C-003   # BMCA Engine Component
    - DES-I-007   # Health/self-test interface design
    - DES-C-031   # BMCA comparison sequence
  tests:
    - TEST-UNIT-BMCA-BASIC
    - TEST-UNIT-OffsetCalculation
    - TEST-UNIT-HealthSelfTest
    - TEST-UNIT-FaultInjectionOffset
    - TEST-UNIT-FaultInjectionBMCA
revision:
  created: 2025-11-08
  updated: 2025-11-08
  version: 0.1.0
review:
  owner: reliability
  approvers: []
---

# SFMEA – IEEE 1588-2019 PTP Core Library (Initial Sweep)

This System/Software Failure Mode & Effects Analysis focuses on the currently implemented reliability-relevant slices: BMCA selection, offset/delay computation, metrics, fault injection, and health reporting.

## Scope & Operating Profile Linkage

Operational profile reference: `operational-profile-ptp-lib.md` (REL-OP-001)

Weighted usage (approx):

- processSync + processFollowUp: 44%
- processAnnounce: 18%
- calculateOffsetDelay: 4.5%
- runBMCA: 4.5%
- tick / housekeeping: remainder

Likelihood (L) heuristics scale 1 (rare) – 10 (frequent) derived from frequency, complexity, and data variability.
Severity (S) scale 1 (negligible) – 10 (catastrophic time sync loss / silent divergence).
Detectability (D) scale 1 (highly detectable) – 10 (latent / silent).
RPN = S × L × D. Initial mitigations reflect current code (tests, assertions pending), residual RPN shown after planned mitigations.

## Failure Mode Table (Initial Top Candidates)

| ID | Subsystem | Failure Mode | Cause | Effect | S | L | D | RPN | Existing Controls | Planned Mitigations | Residual RPN (Est) |
|----|-----------|--------------|-------|--------|---|---|---|-----|-------------------|---------------------|--------------------|
| FM-001 | OffsetCalc | Incorrect offset sign (swap T1/T2 or T3/T4) | Timestamp ordering defect | System drifts opposite direction | 8 | 4 | 6 | 192 | Unit tests basic cases | Add assertion ordering invariants; add negative path test with reversed times | 80 |
| FM-002 | OffsetCalc | Overflow in scaled arithmetic | Extreme interval / malformed timestamp | Wrap yields huge offset -> large adjustment | 9 | 3 | 7 | 189 | Type width (int64) | Range clamp + validation counters + test for max bounds | 81 |
| FM-003 | OffsetCalc | Jitter FI left enabled in production path | Test harness oversight | Persistent bias / instability | 7 | 5 | 5 | 175 | FI toggle API manual | Add health flag gating + build config guard | 60 |
| FM-004 | BMCA | Tie token misuse selects non-optimal master silently | Fault injection not reset | Suboptimal sync path / increased jitter | 7 | 5 | 6 | 210 | FI token single-consume logic | Add explicit test ensuring token count zero afterward + health event for forced tie | 90 |
| FM-005 | BMCA | Priority comparison ordering defect | Field sequence regression | Wrong master selection | 9 | 3 | 6 | 162 | comparePriorityVectors tests | Add golden vector regression test & static_assert doc on order | 63 |
| FM-006 | BMCA | Empty foreign master list unhandled | Logic path omission | Crash / invalid index | 8 | 2 | 3 | 48 | selectBestIndex returns -1 | Add health validation flag increment when -1; test for propagation | 36 |
| FM-007 | Health | Health report stale (emit not called) | Missed emit call path | Operators rely on outdated status | 6 | 4 | 7 | 168 | emit in key functions (BMCA, offset) | Add periodic heartbeat tick emit + observer presence check test | 72 |
| FM-008 | Health | Incorrect basicSynchronizedLikely heuristic | Heuristic too permissive | False sense of sync correctness | 6 | 5 | 6 | 180 | Simple heuristic only | Calibrate with additional validation counters & min sample count threshold | 72 |
| FM-009 | Metrics | Counter overflow (OffsetsComputed) | Long uptime (wrap) | Misleading monitoring / false alarms | 5 | 2 | 9 | 90 | 64-bit counters | Add wrap detection threshold & health flag | 40 |
| FM-010 | Metrics | Lost increments under concurrency | Non-atomic update (future expansion) | Under-reported activity -> missed anomalies | 7 | 3 | 4 | 84 | Atomic increments | Add stress test w/ simulated concurrency | 60 |
| FM-011 | FI | Fault injection API used in release build | Build flag misconfig | Latent test hooks enabling unintended behavior | 7 | 2 | 7 | 98 | Manual process | CMake option OFF by default + runtime assert if enabled in Release | 40 |
| FM-012 | BMCA | Health last index not updated on error | Early return path added later | Stale master reference | 6 | 3 | 6 | 108 | Single return point | Add regression test forcing early exit path | 54 |
| FM-013 | OffsetCalc | Negative offset extreme (underflow scaling) | Malformed timestamps unnatural diff | Large negative causing frequency clamp issues | 8 | 3 | 6 | 144 | Basic bounds via int64 | Clamp + log + validationFailed increment | 60 |
| FM-014 | OffsetCalc | Division rounding bias (integer /2) | Odd delta values | Systematic 0.5 scaled unit bias | 4 | 6 | 8 | 192 | Acceptable tolerance | Add rounding test harness & optional compensation flag | 96 |
| FM-015 | BMCA | StepsRemoved misinterpretation (unsigned wrap) | Large steps causing wrap | Prefer worse path master | 8 | 2 | 7 | 112 | uint16_t type | Add max steps guard & validationFailed increment | 48 |
| FM-016 | Common | Timestamp validation missing in callers | Skipped validate() use | Propagation of bad timestamps | 8 | 4 | 5 | 160 | validate() available | Enforce validate() checks + counter & tests | 72 |
| FM-017 | Health | Emission storm (observer heavy cost) | Observer slow or reentrant | Latency spikes | 7 | 3 | 6 | 126 | Simple direct call | Add rate limiting (min interval) & doc contract | 54 |
| FM-018 | BMCA | Foreign master list overflow (silent overwrite) | Missing bounds check expansion | Loss of candidate -> wrong selection | 8 | 3 | 7 | 168 | Static limit constant | Add explicit bounds check + overflow counter + test | 70 |
| FM-019 | BMCA | Forced tie path not telemetry-tagged | FI path silent in logs | Difficult post-mortem | 5 | 5 | 7 | 175 | Generic log only | Add distinct log code + health flag for forced tie | 70 |
| FM-020 | OffsetCalc | Health not recording last offset on failure path (future) | Early error returns added later | Stale offset in report | 5 | 3 | 6 | 90 | Current always success | Add unit test covering failure return path | 45 |

## Immediate Critical Items (Preliminary CIL Extraction)

Threshold for CIL inclusion (initial): RPN >= 170.

CIL Candidates: FM-001, FM-002, FM-004, FM-007, FM-008, FM-014, FM-018, FM-019.

Planned next step: Create `critical-items-list.md` with mitigation owners & schedule.

## Mitigation Strategy Overview

1. Add invariant & range assertions (compile-time + runtime in debug) for timestamp ordering, stepsRemoved max, scaled arithmetic bounds.
2. Extend tests: edge arithmetic (overflow/underflow), forced tie logging uniqueness, heartbeat emission, heuristic calibration cases.
3. Introduce validation counters (validationsFailed/Passed) pathways in offset & BMCA list update functions.
4. Implement health emission rate limit (configurable minimal interval) and distinct FI usage flag.
5. Add build configuration guard: fault injection APIs assert if enabled in Release unless explicitly overridden.
6. Logging: dedicated codes for forced tie (BMCA) and clamped offsets.

## Assumptions

- Concurrency model currently single-threaded for core algorithm; future concurrency will re-evaluate atomic coverage.
- All timestamps provided are post basic field integrity checks; deeper semantic ordering still needs assertions.
- Operational profile frequencies stable for this iteration; large changes will trigger SFMEA revision.

## Next Actions

- [ ] Approve initial SFMEA (reviewer sign-off)
- [ ] Commit mitigation issues linking to IDs FM-001 etc.
- [ ] Implement assertions & tests (Phase 05 completion gate)
- [ ] Produce `critical-items-list.md` with RPN rationale
- [ ] Update after mitigation to record residual RPN empirically

---

(End of initial SFMEA draft)
