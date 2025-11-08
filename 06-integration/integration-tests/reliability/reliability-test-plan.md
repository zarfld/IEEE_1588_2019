---
# NOTE: This document follows Phase 06 integration reliability guidance (ISO/IEC/IEEE 12207:2017; IEEE 1633)
# It references the Operational Profile (OP) created in Phase 05.
---

# Reliability Test Plan (RTP) — Phase 06

References:

- OP source: 05-implementation/docs/operational-profile.md (or spec-kit-templates/operational-profile.md template)
- Standards: IEEE 1633 Section 5.4 (Reliability estimation during/after testing), ISO/IEC/IEEE 12207 (Integration)

## Objectives

- Exercise PTP operations according to the Operational Profile (usage-weighted).
- Capture reliability data: duty time, failures with severity and timestamps, restore times.
- Export SRG-ready CSV for model fitting in Phase 07.
- Enforce a reliability quality gate in CI (pass rate ≥ 95%, 0 critical failures, non-decreasing MTBF trend).

## Scope and System Under Test (SUT)

- Library: IEEE 1588-2019 PTP protocol (standards layer only; hardware-agnostic).
- Components under test: `PtpPort`, `BoundaryClock` (multi-port), offset/delay computation, BMCA reaction to Announce.

## Model Coverage Targets

- States: 100% of PTP port states reachable in tests (Initializing, Listening, PreMaster, Master, Passive, Uncalibrated, Slave, Faulty, Disabled).
- Transitions: 100% for main-line transitions used in OP-driven scenarios.
- Usage-weighted execution: ≥ 80% of total probability mass covered across transitions.

## Structural Coverage Targets (advisory)

- Statement: ≥ 80% (overall project quality gate already enforced).
- Branch/decision: ≥ 70% on reliability-adapter code paths.

## Failure Definition and Scoring Criteria (FDSC)

- Severity scale 1–10. Critical = 10 (protocol invariant violation, crash, memory corruption).
- Major = 7–9 (incorrect offset/delay result outside spec bounds, illegal state transition).
- Minor = 4–6 (counter/metric mismatch, transient timeout under threshold).
- Cosmetic = 1–3 (logging only).

## OP to Test Adapter Mapping

- OP Operation OP-001: Receive Announce and run BMCA → Adapter: `run_bmca_cycle(port)`
- OP Operation OP-002: Slave offset/delay sample (Sync + Follow_Up + Delay_Req + Delay_Resp) → Adapter: `run_offset_cycle(port)`
- OP Operation OP-003: Health heartbeat emission (tick-driven) → Adapter: `run_health_heartbeat(port)`
- OP Operation OP-004: Multi-port BoundaryClock routing → Adapter: `run_boundary_routing(clock)`

Relative frequencies (example; refine from OP):

- OP-002: 50%
- OP-001: 25%
- OP-003: 15%
- OP-004: 10%

## Test Adapters (C++)

Adapters are self-contained functions returning pass/fail and capturing metadata. Implemented in `06-integration/integration-tests/reliability/`.

Return contract (conceptual):

- Inputs: iteration index, random seed, SUT handle.
- Outputs: `passed` (bool), `operation` (OP-XXX), `state`, `severity`, `executionTimeMs`, `errorMessage`.
- Side effects: Append failure record to in-memory buffer for CSV export.

## Data Collection and Export

- Duty time (seconds) accumulated over reliability run.
- Sequential failure numbering with occurrence time (seconds from start).
- CSV format: `FailureNumber,FailureTime,Severity,Operation,State,Fixed` saved to build output (uploaded by CI).

## Reliability Quality Gate

- Pass rate ≥ 95% across N iterations (default N = 200 on CI to keep runtime < 1 min).
- Critical failures (severity = 10): 0.
- MTBF trend non-decreasing versus previous run (best-effort; advisory on first run).

## Execution Plan

- Harness: `reliability_harness` CTest labeled `reliability`.
- Parameters: iterations (default 200), csv path (default: `${CMAKE_BINARY_DIR}/reliability/srg_failures.csv`).
- CI: Run after integration tests on Windows and Ubuntu; upload CSV artifact.

## Evidence and Traceability

- Test sources: `06-integration/integration-tests/reliability/*.cpp`.
- Results: `${build}/reliability/*.csv` artifacts.
- Linkage: RTP ↔ OP ↔ adapters ↔ CTest.

## Model Fitting (Phase 07 preview)

- Candidate SRG models: Musa-Okumoto, Goel-Okumoto, AMSAA (Crow). Select via AIC/BIC on collected data.
- Accuracy verification: Compare model-estimated MTBF to recent observed MTBF per IEEE 1633 5.4.7.
