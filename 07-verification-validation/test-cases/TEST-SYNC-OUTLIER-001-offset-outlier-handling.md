# Test Case: TEST-SYNC-OUTLIER-001 Offset Outlier Handling

Trace to: REQ-F-003; ADR-003

## Test Information

- Test ID: TEST-SYNC-OUTLIER-001
- Test Type: Robustness
- Test Level: Unit
- Priority: P1
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Offset filter / detection logic implemented

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Inject large anomalous delay sample | Sample flagged / excluded |
| 2 | Continue with normal samples | Offset returns to stable range |
| 3 | Track rejected sample count | Counter increments |

## Expected Results

- Outliers excluded; no long-term drift introduced

## Metrics

- Recovery within M cycles (define M)

## Notes

- Precursor to servo robustness tests.
