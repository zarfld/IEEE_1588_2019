# Test Case: TEST-SYNC-OFFSET-DETAIL-001 Detailed Sync Offset Analysis

Trace to: REQ-FR-PTPA-001; ADR-003

## Test Information

- Test ID: TEST-SYNC-OFFSET-DETAIL-001
- Test Type: Analysis / Functional
- Test Level: System
- Priority: P2
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Instrumentation to log raw timestamps (t1, t2, t3, t4) and correctionField
- Dataset export facility

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Collect M cycles of timestamp exchanges | Raw dataset complete |
| 2 | Compute per-cycle offset using implemented formula | Offsets array stored |
| 3 | Independently compute offset via reference script | Values match within tolerance |

## Expected Results

- Library offset computation matches reference implementation

## Metrics

- Mean absolute difference reference vs implementation (≈0)

## Notes

- Serves traceability for offset correctness beyond aggregate stats.
