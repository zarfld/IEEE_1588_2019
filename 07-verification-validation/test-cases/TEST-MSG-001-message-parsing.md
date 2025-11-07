# Test Case: TEST-MSG-001 Message Parsing Validation

Trace to: REQ-F-001; ADR-001, ADR-002, ADR-003

## Test Information

- Test ID: TEST-MSG-001
- Test Type: Functional
- Test Level: Unit → Integration
- Priority: P0 (Critical)
- Author: TBD
- Date Created: 2025-11-07

## Preconditions

- PTP message header and payload structures defined
- Valid sample frames for Sync, Follow_Up, Delay_Req, Delay_Resp, Announce available
- Parser entry points exposed in headers

## Test Steps

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Provide a valid Sync frame to parser | Parsed struct fields populated; no error |
| 2 | Provide a valid Follow_Up frame | Parsed; timestamps extracted |
| 3 | Provide a valid Announce frame | Parsed; dataset fields validated |
| 4 | Validate endianness handling on multi-byte fields | All fields match expected host representation |
| 5 | Validate header sanity checks (version, messageType) | Invalid values rejected with specific error code |

## Expected Results

- All supported PTP message types parse successfully with correct field values
- Invalid inputs are rejected gracefully with deterministic error codes

## Automation

- Add unit tests in tests/ targeting message parse functions
- Include golden vectors for regression

## Notes

- Reference: Implements parsing behavior per IEEE 1588-2019 Section 13 (no text reproduction)

