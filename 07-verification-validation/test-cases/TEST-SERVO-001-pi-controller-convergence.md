# Test Case: TEST-SERVO-001 PI Controller Convergence

Trace to: REQ-F-004; ADR-004

## Test Information

- Test ID: TEST-SERVO-001
- Test Type: Functional / Control
- Test Level: Unit
- Priority: P0
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- PI servo implemented with configurable gains
- Synthetic offset sequence available

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Apply step offset input | Control output drives offset toward zero |
| 2 | Measure settling time | Settling time within target window |
| 3 | Verify no integral windup (bounded) | Integral term capped |

## Expected Results

- Convergence without oscillation within design parameters

## Metrics

- Settling time ≤ T_settle target
- Overshoot < X%

## Notes

- Links to performance REQ-NF-P-001 indirectly.
