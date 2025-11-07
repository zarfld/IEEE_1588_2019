# Test Case: TEST-BMCA-001 BMCA Master Selection

Trace to: REQ-F-002; ADR-002

## Test Information

- Test ID: TEST-BMCA-001
- Test Type: Functional
- Test Level: Unit
- Priority: P0
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- BMCA dataset structure implemented
- Candidate clocks list with varied priority1/priority2/clockQuality

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Provide candidate set with one highest priority1 | That candidate selected |
| 2 | Tie on priority1; vary clockClass | Lower clockClass (better) chosen |
| 3 | Tie on class; vary accuracy | Better accuracy chosen |
| 4 | Tie on accuracy; vary offsetScaledLogVariance | Lower variance chosen |
| 5 | All equal; compare clockIdentity | Lexicographic lowest chosen |

## Expected Results

- Deterministic selection per IEEE 1588-2019 Section 9.3 comparison order

## Metrics

- 100% rule adherence
- Execution time < 10 µs (baseline target)

## Notes

- Supports later transition tests.
