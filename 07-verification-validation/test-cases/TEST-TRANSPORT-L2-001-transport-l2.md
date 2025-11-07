# Test Case: TEST-TRANSPORT-L2-001 Layer 2 Transport Mapping

Trace to: REQ-FR-PTPA-004; ADR-001

## Test Information

- Test ID: TEST-TRANSPORT-L2-001
- Test Type: Functional / Transport
- Test Level: Unit / Integration
- Priority: P2
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Ethernet L2 frame construction utilities
- Ethertype and destination MAC constants defined

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Build PTP L2 frame for Sync | Fields set per mapping; length correct |
| 2 | Build PTP L2 frame for Announce | Fields set per mapping; length correct |
| 3 | Parse back constructed frames | Round-trip matches inputs |

## Expected Results

- L2 mapping correct for supported message types; round-trip success

## Metrics

- Round-trip mismatch count (0)

## Notes

- Validates Annex E mapping without hardware dependencies.
