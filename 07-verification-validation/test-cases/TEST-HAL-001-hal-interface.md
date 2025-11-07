# Test Case: TEST-HAL-001 HAL Interface Compliance

Trace to: REQ-F-005; ADR-001

## Test Information

- Test ID: TEST-HAL-001
- Test Type: Interface / Functional
- Test Level: Unit
- Priority: P0
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Mock network/time interface providing deterministic responses

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Initialize PTP with mock HAL | Returns success |
| 2 | Send packet via HAL callback | Packet recorded in mock log |
| 3 | Retrieve time via HAL callback | Time monotonic increasing |
| 4 | Simulate error path (send failure) | Proper error code propagated |

## Expected Results

- HAL contract upheld; no hardware references

## Metrics

- 0 forbidden includes detected (CI static check)

## Notes

- Ensures abstraction purity.
