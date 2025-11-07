# Test Case: TEST-BMCA-TIMEOUT-001 BMCA Timeout Reselection

Trace to: REQ-F-002; ADR-002

## Test Information

- Test ID: TEST-BMCA-TIMEOUT-001
- Test Type: Resilience
- Test Level: Integration
- Priority: P1
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Active master selected
- Announce timeout interval configured

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Simulate absence of Announce messages past timeout | Master considered lost |
| 2 | Trigger BMCA re-run | Next best candidate elected |
| 3 | Log transition event | Transition recorded with timestamp |

## Expected Results

- Proper timeout detection and reselection

## Metrics

- Reselection latency ≤ configured timeout + 1 cycle

## Notes

- Ensures stability under master loss.
