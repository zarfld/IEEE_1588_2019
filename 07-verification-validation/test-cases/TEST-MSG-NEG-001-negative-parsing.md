# Test Case: TEST-MSG-NEG-001 Negative Message Parsing

Trace to: REQ-F-001; ADR-001, ADR-003

## Test Information

- Test ID: TEST-MSG-NEG-001
- Test Type: Robustness / Negative
- Test Level: Unit
- Priority: P1
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- Parser initialized
- Corpus of malformed frames prepared (truncated, bad version, wrong length, invalid TLVs)

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Feed truncated header | Error: ERR_PTP_TRUNCATED |
| 2 | Feed unsupported version | Error: ERR_PTP_VERSION |
| 3 | Feed incorrect length field | Error: ERR_PTP_LENGTH |
| 4 | Feed invalid messageType | Error: ERR_PTP_TYPE |
| 5 | Feed malformed TLV list | Error: ERR_PTP_TLV |

## Expected Results

- All malformed inputs rejected; no memory corruption
- No undefined behavior (verified under sanitizers)

## Metrics

- 0 crashes / aborts
- 100% detection of malformed patterns

## Notes

- Negative cases strengthen fuzz harness basis.
