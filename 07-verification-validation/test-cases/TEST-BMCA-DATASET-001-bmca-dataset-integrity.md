# Test Case: TEST-BMCA-DATASET-001 BMCA Dataset Integrity

Trace to: REQ-FR-PTPA-002; ADR-002

## Test Information

- Test ID: TEST-BMCA-DATASET-001
- Test Type: Functional / Data integrity
- Test Level: Unit / Integration
- Priority: P2
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Accessors for DefaultDS/CurrentDS/ParentDS/PortDS
- Known-good dataset fixtures

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Populate datasets from Announce | Fields mapped correctly |
| 2 | Serialize/deserialize datasets | Round-trip preserves values |
| 3 | Validate comparison keys used by BMCA | Keys match spec order |

## Expected Results

- Datasets remain consistent and adhere to expected constraints

## Metrics

- Round-trip mismatch count (0)

## Notes

- Strengthens BMCA correctness.
