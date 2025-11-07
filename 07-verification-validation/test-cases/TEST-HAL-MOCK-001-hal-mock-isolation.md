# Test Case: TEST-HAL-MOCK-001 HAL Mock Isolation

Trace to: REQ-F-005; ADR-001

## Test Information

- Test ID: TEST-HAL-MOCK-001
- Test Type: Isolation
- Test Level: Unit
- Priority: P1
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Mock HAL compiled independently of platform headers

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Build mock in isolation | Build succeeds |
| 2 | Run tests with mock only | All HAL-dependent tests pass |
| 3 | Swap alternate mock implementation | Tests still pass (interface stability) |

## Expected Results

- Mock portability confirmed

## Metrics

- Build time < threshold; no platform linkage

## Notes

- Supports portability requirements.
