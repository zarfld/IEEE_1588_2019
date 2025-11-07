# Test Case: TEST-SYNC-001 Offset Calculation Accuracy

Trace to: REQ-F-003; ADR-003

## Test Information

- Test ID: TEST-SYNC-001
- Test Type: Functional / Numeric
- Test Level: Unit
- Priority: P0
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Timestamp arithmetic functions implemented
- Known send/receive timestamps fixture

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Compute offset with symmetric path delay sample | Offset within ±1 µs target |
| 2 | Compute offset with varied delay samples | Stable convergence within N cycles (specify N) |
| 3 | Verify correctionField application | Corrected offset matches expected |

## Expected Results

- Accurate offset per Section 11.3 formulas (conceptual)

## Metrics

- Offset error P95 ≤ 1 µs (software baseline)

## Notes

- Serves performance evidence tie-in.
