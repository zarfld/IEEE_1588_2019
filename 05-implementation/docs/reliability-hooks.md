---
title: "Reliability Hooks Overview"
phase: 05-implementation
traceability:
  design: [DES-REL-001, DES-REL-002]
  requirements: [REQ-NF-Reliability-001, REQ-NF-Observability-002]
  tests: [TEST-UNIT-bmca-basic, TEST-UNIT-offset-robustness, TEST-UNIT-fault-injection-offset, TEST-UNIT-fault-injection-bmca, TEST-UNIT-health-selftest]
---

## Reliability Hooks (Phase 05)

This document summarizes the reliability/observability mechanisms integrated during Phase 05 implementation per IEEE 1633 guidance and Phase 05 exit criteria. These hooks provide structured logging, metrics, fault injection capabilities, and a health/self-test API for downstream integration, verification, and reliability modeling.

## Objectives

1. Enable collection of operational evidence (counts, state transitions, selections) without hardware dependencies.
2. Provide deterministic fault injection triggers for controlled negative/edge testing.
3. Surface internal synchronization and BMCA state via a health/self-test interface.
4. Maintain strict hardware/platform agnosticism while supporting reliability analysis.

## Components

| Component | File | Purpose |
|-----------|------|---------|
| Logger | `include/Common/utils/logger.hpp` | Lightweight structured logging (severity + context) for offset and BMCA decision paths |
| Metrics | `include/Common/utils/metrics.hpp` | Atomic counters for BMCA selections, offset computations, parse errors |
| Fault Injection | `include/Common/utils/fault_injection.hpp` | Controlled jitter injection and BMCA tie forcing for test-only scenarios |
| Health/Self-Test | `include/Common/utils/health.hpp` | Aggregated report (last offset, BMCA selection, metrics snapshot) + observer callback |

## Metrics Catalog

| CounterId | Semantic |
|-----------|---------|
| `OffsetsComputed` | Successful offset calculations executed |
| `BMCA_CandidateUpdates` | BMCA candidate update operations performed |
| `BMCA_Selections` | Final BMCA grandmaster selections completed |
| (Extensible) | Additional parse/error counters can be added using enum extension |

Access pattern:

```cpp
using namespace Common::utils::metrics;
reset_all(); // test setup
increment(CounterId::OffsetsComputed);
auto val = get(CounterId::OffsetsComputed);
auto snap = snapshot(); // bulk read for reporting
```

## Fault Injection Controls

| Feature | API | Description |
|---------|-----|-------------|
| Offset jitter | `enable_offset_jitter(ns_delta)` | Adds deterministic jitter to computed offset for robustness tests |
| BMCA tie token | `issue_bmca_tie_token()` | Forces next BMCA comparison tie path to exercise tie resolution logic |
| Reset | `reset_fault_injection()` | Clears all toggles/tokens between tests |

Usage pattern in tests:

```cpp
reset_fault_injection();
enable_offset_jitter(500); // +500ns artificial jitter
// invoke offset calculation, assert adjusted value

issue_bmca_tie_token();
// invoke BMCA selection, assert tie-handling path executed
```

## Health/Self-Test Interface

Provides a consolidated view of recent internal state for integration monitoring.

```cpp
SelfTestReport report = self_test();
// Fields contain last offset ns, last BMCA selection index, metrics snapshot, fault injection flags
```

Observer pattern (optional):

```cpp
set_observer([](const SelfTestReport& rpt){
    // Push to external collector or log sink
});
// Emitted automatically after offset computation and BMCA selection
```

## Integration Points

| Location | Hook |
|----------|------|
| `SynchronizationData::calculateOffset` | Applies offset jitter FI, increments `OffsetsComputed`, records last offset, emits health observer |
| `selectBestIndex` (BMCA) | Consumes tie token FI, increments `BMCA_Selections` and `BMCA_CandidateUpdates`, records selection, emits health observer |

## Testing Coverage

| Test | Purpose |
|------|---------|
| `test_offset_calculation.cpp` | Validates offset math + metrics increment |
| `test_bmca_basic.cpp` | Checks BMCA selection + metrics increments |
| `test_fault_injection_offset.cpp` | Verifies jitter injection behavior |
| `test_fault_injection_bmca.cpp` | Verifies tie token forcing path |
| `test_health_selftest.cpp` | Ensures health report aggregation correctness |

## Extensibility Guidelines

1. Add new counters by extending `enum class CounterId` (preserve existing ordinal order; append only).
2. Keep fault injection strictly test-only; never enable in production builds (guard via configuration if needed).
3. Emit health reports only on state transitions or calculations to minimize overhead.
4. Avoid dynamic allocation in hot paths; all hooks remain header-only and atomic-backed.

## Phase 06/07 Usage

| Artifact | Usage in Later Phases |
|----------|----------------------|
| Metrics snapshot | Feed into reliability trend and failure rate modeling |
| Health reports | Integration monitoring, baseline for operational profile validation |
| Fault injection | Stress/failure scenario generation for integration/V&V suites |
| Logger output | Time-correlated event sequencing for defect triage |

## Compliance Notes

- Implements reliability hooks per Phase 05 exit criteria (metrics, health checks, fault injection, logging).
- Hardware agnostic; no platform-specific headers.
- No copyrighted IEEE/AES/AVnu specification text reproduced; references encoded via semantic identifiers only.

## Next Steps

1. Add parse error counters (e.g., `MessageParseErrors`).
2. Extend health report with moving averages if needed (ensure O(1) cost per update).
3. Wire metrics export into integration test harness for automated baseline capture.
4. Include reliability hooks section in standards compliance report.
