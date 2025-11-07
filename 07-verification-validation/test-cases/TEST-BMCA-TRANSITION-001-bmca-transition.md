# Test Case: TEST-BMCA-TRANSITION-001 BMCA State Transition

Trace to: REQ-S-001; ADR-002, ADR-003

## Test Information

- Test ID: TEST-BMCA-TRANSITION-001
- Test Type: System / Integration
- Test Level: System
- Priority: P0
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- System running with selected Grandmaster
- Transition criteria and timers configured (announceInterval, timeout)

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Degrade current master (priority bump) | BMCA selects new best master |
| 2 | Restore original priorities | BMCA re-evaluates; stable selection |
| 3 | Verify state change notifications | Proper event/log emitted |

## Expected Results

- BMCA transitions occur deterministically with no oscillation

## Metrics

- Transition count matches scenario
- No flapping (min dwell time respected)

## Notes

- Aligns with state handling around PRE_MASTER/MASTER/PASSIVE/SLAVE.
