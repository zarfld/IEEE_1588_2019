# Test Case: TEST-PERF-OFFSET-P95-001 Synchronization Offset P95

Trace to: REQ-NF-P-001; ADR-003, ADR-004

## Test Information

- Test ID: TEST-PERF-OFFSET-P95-001
- Test Type: Performance
- Test Level: System
- Priority: P0
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Performance measurement harness available
- Stable environment; clock drift baseline measured

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Run sync for N cycles collecting offset samples | Samples captured |
| 2 | Compute P50/P95/P99 of absolute offset | Metrics calculated |
| 3 | Compare P95 to target threshold | P95 ≤ 1 µs (software baseline) |

## Expected Results

- P95 offset meets or beats target threshold

## Metrics

- P50, P95, P99, max offset
- Confidence interval if repeated runs

## Notes

- This forms part of Phase 03 performance evidence.
