---
title: "Operational Profile - IEEE 1588-2019 PTP Library"
specType: operational-profile
version: 0.1.0
author: reliability-eng
phase: 05-implementation
status: draft
standard: IEEE 1633-2016
component: ptp-core-lib
relatedRequirements:
  - REQ-NF-Reliability-001
  - REQ-NF-Observability-002
  - REQ-F-003   # Offset calculation
  - REQ-F-010   # BMCA selection correctness
relatedDesign:
  - DES-CLOCK-STATE-MACHINES
  - DES-BMCA-ALGO
  - DES-OFFSET-CALC
traceabilityTests:
  - TEST-UNIT-offset-calculation
  - TEST-UNIT-bmca-basic
  - TEST-UNIT-bmca-edges
  - TEST-UNIT-fault-injection-offset
  - TEST-UNIT-fault-injection-bmca
  - TEST-UNIT-health-selftest
---

## 1. Actors and User Segments

| Actor / Segment | Description | Usage Share |
|-----------------|-------------|-------------|
| Library Integrator | Embeds PTP in system/service; configures ports & callbacks | 35% |
| QA / Test Harness | Executes deterministic simulation runs, fault injection scenarios | 25% |
| Time Sync Monitor | Polls health/self-test, inspects BMCA status & offset metrics | 20% |
| Reliability Analytics Tool | Collects metrics snapshots for trend / MTBF modeling | 10% |
| Diagnostic / Debug User | Enables detailed logging, performs edge-case message injections | 10% |

## 2. Operations and Modes

| Operation | Description | Relative Frequency |
|-----------|-------------|--------------------|
| initLibrary | Initialize data sets, state machines, clear metrics | 2.0% |
| configurePort | Apply port configuration (intervals, version) | 1.0% |
| processAnnounce | Parse + evaluate Announce, foreign master list update | 18.0% |
| processSync | Handle Sync message, capture T2 timestamp | 22.0% |
| processFollowUp | Update origin timestamp (T1) for offset calculation | 22.0% |
| processDelayReq | Record T3 transmit timestamp | 6.0% |
| processDelayResp | Record T4 master receive timestamp, complete cycle | 6.0% |
| runBMCA | Evaluate foreign masters, select best clock | 4.5% |
| calculateOffsetDelay | Compute offset_from_master & mean_path_delay | 4.5% |
| tick | Periodic housekeeping, timeout checks | 6.0% |
| snapshotMetrics | Produce metrics snapshot for external collector | 2.0% |
| healthSelfTest | Generate SelfTestReport (offset, selection, counters) | 2.0% |
| faultInjectJitter | Enable offset jitter for robustness scenario | 0.5% |
| faultInjectTie | Force BMCA tie path for test scenario | 0.5% |


## 3. Mission Profiles

| Mission Profile | Duration | Description |
|-----------------|----------|-------------|
| Standard Synchronization Session | 8h | Normal operation with periodic Sync/Follow_Up/Delay cycles |
| High-Load Test Session | 2h | Elevated message rate to stress BMCA & offset logic |
| Fault Injection Campaign | 1h | Interleaved jitter and tie tokens to exercise resilience paths |
| Monitoring/Analytics Window | 24h | Continuous metric & health polling for reliability trend data |

Sequence Archetype (Standard Session):

1. initLibrary
2. configurePort
3. Loop: { processAnnounce?, processSync, processFollowUp, processDelayReq, processDelayResp, calculateOffsetDelay, runBMCA?, tick }
4. Periodically: snapshotMetrics, healthSelfTest

## 4. Behavioral Model (States & Transitions)

States (high-level clock/port operation):

- S0: Uninitialized
- S1: Initialized
- S2: Listening (Awaiting Announce)
- S3: Master
- S4: Slave
- S5: Passive
- S6: Fault

Transitions (subset):

| Transition | From | To | Trigger | Notes |
|------------|------|----|---------|-------|
| T0 | S0 | S1 | initLibrary | Allocation complete |
| T1 | S1 | S2 | start() | Port starts listening |
| T2 | S2 | S4 | processAnnounce (better GM) | BMCA selects foreign master |
| T3 | S2 | S3 | processAnnounce (local best) | Local clock wins BMCA |
| T4 | S3 | S4 | foreign master better emerges | BMCA reevaluation |
| T5 | S4 | S3 | local becomes better | BMCA reevaluation |
| T6 | * | S6 | fault detected | Fault path |
| T7 | S6 | prior state | fault cleared | Recovery |

Transition frequency normalization: For each state, outgoing transitions sum to 1.0 (maintained in modeling tool).

## 5. Transition Frequencies (Indicative)

| State | Transition | Relative Probability |
|-------|------------|----------------------|
| S2 | T2 | 0.55 |
| S2 | T3 | 0.40 |
| S2 | T6 | 0.05 |
| S4 | T4 | 0.08 |
| S4 | T6 | 0.02 |
| S3 | T5 | 0.06 |
| S3 | T6 | 0.02 |
| S6 | T7 | 0.95 |
| S6 | (remain) | 0.05 |

## 6. Coverage Targets

| Coverage Type | Target |
|---------------|--------|
| State Coverage | ≥ 95% |
| Transition Coverage | ≥ 90% |
| Statement Coverage | ≥ 80% |
| Branch Coverage | ≥ 80% |

## 7. Test Generation Mapping

| Abstract Action | Adapter / Mapping | Verifications |
|-----------------|-------------------|---------------|
| processSync | test helper invoking Sync parse/handler | T2 timestamp captured; metrics increment |
| processFollowUp | test helper for Follow_Up acquisition | T1 stored; sequence association maintained |
| processDelayResp | test helper for Delay_Resp finalize | T4 stored; offset path readiness |
| calculateOffsetDelay | direct call to offset calc wrapper | Offset sign & magnitude sanity |
| runBMCA | BMCA test harness | Selection metrics increment; tie path reachable |
| faultInjectTie | FI token issuance utility | Next BMCA compare returns equality path |
| healthSelfTest | self_test() invocation | Report fields populated & consistent |

## 8. Data Collection for Reliability Estimation

| Data Item | Source | Collection Interval | Purpose |
|-----------|--------|---------------------|---------|
| OffsetsComputed counter | metrics snapshot | 5 min | Synchronization activity rate |
| BMCA_Selections counter | metrics snapshot | 5 min | Clock stability / churn indicator |
| Last offset ns | health report | 5 min | Drift / correction trend |
| Fault injection flags | health report | on change | Segregate artificial failures |
| Foreign master count | BMCA context | 5 min | Topology volatility |

Failure logging format: structured log entry `{ ts, code, component, details }` (logger.hpp). Severity classification (info/debug only now; extend for error/fault).

Trend analysis: Apply Laplace test on inter-failure times when error severity integrated; monitor stability of offset variance.

## 9. Variants & Evolution

| Variant | Difference | Impact on Reliability Tests |
|---------|-----------|-----------------------------|
| Profile-A (Default) | Standard message intervals | Baseline coverage |
| Profile-B (High Load) | Reduced intervals (higher rate) | Stress timing & capacity |
| Profile-C (Fault Campaign) | FI toggles active | Exercise resilience paths |

Versioning: Increment `version` field and archive profile under `docs/reliability/archive/` when major operation frequency shifts (>10% change) occur.

## 10. References

- IEEE 1588-2019 Sections 9, 11 (state machine and delay mechanisms)
- Requirements: REQ-F-003, REQ-F-010, REQ-NF-Reliability-001, REQ-NF-Observability-002
- Design Specs: DES-CLOCK-STATE-MACHINES, DES-BMCA-ALGO, DES-OFFSET-CALC
- SRPP (software-reliability-program-plan.md)
- Reliability Hooks: `05-implementation/docs/reliability-hooks.md`

## 11. Notes & Next Steps

- Calibrate transition probabilities with empirical test run data (Phase 06 integration).
- Add error-severity classification once failure logging extended beyond debug/info.
- Feed metrics snapshots + health reports into reliability estimation scripts.
- Derive workload weighting for offset variance trending.

---
Generated from `spec-kit-templates/operational-profile.md` template and adapted for PTP core library.
