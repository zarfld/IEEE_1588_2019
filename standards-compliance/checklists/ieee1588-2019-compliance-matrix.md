# IEEE 1588-2019 Compliance Matrix

Metadata:
```yaml
matrixId: CMP-IEEE1588-2019-BASE
version: 0.1.0
status: DRAFT
created: 2025-11-08
updated: 2025-11-08
scope: Baseline mandatory protocol features (types, datasets, state machine, messages, E2E delay)
assumptions:
  - Minor version fixed to 1 (header.minorVersionPTP = 1)
  - BMCA simplified: selects first foreign master; full priority vector comparison TBD
  - Security (Annex P) not yet implemented (out of baseline scope)
  - Management messages declared but not functionally processed
  - Signaling minimal parsing/dispatch implemented (stub) with dedicated test; TLV parsing pending
  - Peer delay mechanism partially represented (structures present, no full path delay calc)
references:
  standard: IEEE 1588-2019
  sections: [5,8,9,11,13,20]
```

## Legend

Status values: Compliant | Partial | Gap | N/A
Evidence references use file paths and test case IDs.

## Section 13 Message Formats

| Clause | Element | Artifact | Tests / Evidence | Status | Notes |
|--------|---------|----------|------------------|--------|-------|
| 13.3 | Common Header fields (messageType, version, length, domain, flags, correctionField, sourcePortIdentity, sequenceId, controlField, logMessageInterval) | `include/IEEE/1588/PTP/2019/messages.hpp` struct CommonHeader | `tests/test_messages_validate.cpp` (TEST-MSG-VALIDATE-001) header.validate() version/length checks | Compliant | Size static_assert deferred to serialization layer |
| 13.5 | Announce message body (originTimestamp, UTC offset, priorities, clock quality, identity, stepsRemoved, timeSource) | AnnounceBody | TEST-MSG-VALIDATE-001 body.validate() stepsRemoved range | Compliant | Origin timestamp placeholder (no hardware timestamp yet) |
| 13.6 | Sync message body (originTimestamp) | SyncBody | TEST-MSG-VALIDATE-001 boundary nanoseconds; clocks.cpp process_sync | Compliant | Two-step mode handled via Follow_Up combination |
| 13.6 | Delay_Req message body | DelayReqBody | clocks.cpp process_delay_req (slave emit path) | Partial | Master transmit path implemented; originTimestamp always zero (one-step assumption) |
| 13.8 | Delay_Resp message body (receiveTimestamp, requestingPortIdentity) | DelayRespBody | clocks.cpp process_delay_resp identity match, offset calc triggers | Compliant | Validation ensures identity match |
| 13.9 | Pdelay_Req body (originTimestamp + reserved) | PdelayReqBody | messages.hpp validate() reserved zero loop | Partial | No processing logic yet (not used by E2E profile) |
| 13.10 | Pdelay_Resp body | PdelayRespBody | messages.hpp validate() + struct presence | Partial | No processing logic yet |
| 13.11 | Pdelay_Resp_Follow_Up body | PdelayRespFollowUpBody | messages.hpp validate() + struct presence | Partial | No processing logic yet |
| 13 (General) | Message template & initialization (`PTPMessage<T>`) | `PTPMessage<T>`, `initialize()` sets fields | TEST-MSG-VALIDATE-001 initialization used; `clocks.cpp` `send_*` functions | Compliant | Minor version set to 1 consistently |

 
## Section 8 Data Sets

| Clause | Element | Artifact | Tests / Evidence | Status | Notes |
| 8.2.2 | currentDS (stepsRemoved, offsetFromMaster, meanPathDelay) | `clocks.hpp` CurrentDataSet; updated in calculate_offset_and_delay | `test_offset_calc_red.cpp` (stub), `test_offset_clamp_boundary.cpp`, `test_state_machine_basic.cpp` transitions | Partial | Offset & path delay updated; stepsRemoved static; needs tests for positive path & offset range |
| 8.2.3 | parentDS (grandmaster identity, quality, priorities, variance, phase change rate) | ParentDataSet in `clocks.hpp` initial defaults | Initialization code in `clocks.cpp` PtpPort ctor | Partial | Static defaults; update logic from Announce not implemented |
| 8.2.5 | portDS (identity, state, intervals, delayMechanism, version) | PortDataSet; transitions in `clocks.cpp` | `test_state_machine_basic.cpp`, `test_state_machine_heuristic_negative.cpp` | Compliant | Intervals configurable via setters |
| 8 (Foreign Master Records) | Foreign master list maintenance | `foreign_masters_` array & `update_foreign_master_list()` | `clocks.cpp` `update_foreign_master_list` telemetry on overflow | Partial | Selection heuristic simplistic; timeout pruning missing |

 
## Section 9 State Machine & BMCA

| Clause | Element | Artifact | Tests / Evidence | Status | Notes |
| 9.2 | Port states enumeration & transitions | PortState enum, process_event(), transition_to_state() | `test_state_machine_basic.cpp` basic transitions; negative heuristic test | Compliant | Transition coverage executed in reliability harness (Phase 06) |
| 9.2.6 | State events | StateEvent enum & switch logic | Same as above | Compliant | All defined events mapped to transitions |
| 9.3 | BMCA algorithm (priority vector comparison, dataset evaluation) | run_bmca() simplified; PriorityVector struct, comparePriorityVectors() | `clocks.cpp` run_bmca() selects first foreign master | Gap | Full comparison & role assignment not implemented |
| 9.3.4 | Priority vector comparison semantics | comparePriorityVectors() rootSystemIdentity & stepsRemoved comparison | Declared only | Partial | Missing complete field ordering (clockClass, accuracy, variance, priority1/2) |
| 9.3.3 | Port role designation (Master/Slave/Passive/Disabled) | PortRole enum (declared) | Not referenced | Gap | Role assignment logic absent |

 
## Section 11 Delay Mechanisms

| Clause | Element | Artifact | Tests / Evidence | Status | Notes |
| 11.3 | End-to-end delay offset & mean path delay formula | calculate_offset_and_delay() (port), SynchronizationData::calculateOffset() | `test_offset_clamp_boundary.cpp`, `test_rounding_bias.cpp` (rounding), RED offset test stub | Compliant | Rounding compensation & clamps implemented; test for numeric offset result pending (RED) |
| 11.4 | Peer delay mechanism timestamps (Pdelay sequence handling) | Pdelay* structures | None | Gap | Peer delay operational logic not implemented |
| 11.5 | Correction field accumulation (transparent clock residence time) | TransparentClock::forward_message(), update_correction_field() | `clocks.cpp` TransparentClock implementations | Partial | Residence time adds correction; upstream/downstream path delay aggregation incomplete |

 
## Section 5 Basic Types

| Clause | Element | Artifact | Tests / Evidence | Status | Notes |
| 5.x | Basic integer & composite types (ClockIdentity, PortIdentity, Timestamp, CorrectionField) | `types.hpp` definitions & validation methods | `test_types_timestamp.cpp`, `test_messages_validate.cpp` | Compliant | Validation covers nanoseconds range & port number non-zero |

 
## Section 20 Conformance

| Clause | Element | Artifact | Tests / Evidence | Status | Notes |
| 20.2 | Mandatory protocol elements implemented (messages, state machine, datasets, offset calc) | Aggregated across messages.hpp, clocks.hpp/cpp | All listed tests; VERIFICATION_EVIDENCE.md | Partial | Missing: full BMCA, management & signaling handling, peer delay operational flow, security |
| 20.x | Management message handling | MessageType::Management declared | None | Gap | Parsing & processing absent |
| 20.x | Signaling message handling | Minimal stub in `include/IEEE/1588/PTP/2019/messages.hpp` and `src/clocks.cpp` | CTest: `ptp_signaling_message_handling` (05-implementation/tests/test_signaling_message.cpp); CAP-20251109-04 | Partial | Minimal parsing/dispatch implemented; TLVs and behaviors pending |
| Annex P | Security TLVs & processing | Flags::SECURITY constant | None | Gap | Security features out of current scope |

 
## Gap Remediation Plan

| Gap ID | Description | Priority | Proposed Action | Target Version |
|--------|-------------|----------|-----------------|----------------|
| GAP-BMCA-001 | Full BMCA decision hierarchy absent | High | Implement dataset extraction from Announce & complete priority vector comparison | 0.2.0 |
| GAP-PDELAY-001 | Peer delay operational logic missing | Medium | Add Pdelay request/response timestamp handling & path delay calc | 0.3.0 |
| GAP-MGMT-001 | Management message handling absent | Medium | Implement parsing & response framework with TLV support | 0.4.0 |
| GAP-SIGNAL-001 | Signaling message handling absent | Low | Add minimal signaling parsing & dispatch | 0.4.0 |
| GAP-PARENT-001 | ParentDS not updated from Announce | High | Parse announce fields into ParentDataSet & grandmaster updates | 0.2.0 |
| GAP-FOREIGN-001 | Foreign master pruning & advanced selection | Medium | Add timeout pruning & quality comparison | 0.2.0 |
| GAP-SEC-001 | Security (Annex P) not implemented | Deferrable | Evaluate scope & add security TLV parsing if required | TBD |
| GAP-OFFSET-TEST-001 | Numeric offset calc test incomplete (RED) | High | Implement green test verifying expected offset & path delay | 0.1.1 |

 
## Traceability Links

| Requirement ID | Matrix Rows | Tests | Notes |
|----------------|------------|-------|-------|
| REQ-F-001 (Message Types) | Section 13 rows | TEST-MSG-VALIDATE-001 | All mandatory event & general messages structured |
| REQ-F-003 (E2E Offset Calc) | Section 11.3 row | test_offset_clamp_boundary.cpp; RED offset calc test | Formula implemented, validation heuristics added |
| REQ-NF-REL-003 (Observability) | Various (state transitions, timeout) | health_snapshot, metrics counters | Telemetry & metrics increment on validations |
| REQ-NF-P-001 (Performance deterministic) | Types/Message header rows | test_messages_validate.cpp timing (bounded) | All validations O(1) |

 
## Summary

Baseline elements of IEEE 1588-2019 (types, primary messages, datasets scaffold, port state machine, E2E offset/path delay formula, transparent clock correction handling) are present and tested. Significant gaps remain in full BMCA implementation, parentDS dynamic updates, peer delay mechanism, management & signaling message handling, and security features.

Next immediate action: complete numeric offset verification test (GAP-OFFSET-TEST-001) and begin BMCA enhancement (GAP-BMCA-001).
