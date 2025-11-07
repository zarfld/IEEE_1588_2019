---
title: "Core Protocol (ARC-C-001) - Software Design Description"
specType: design
version: 0.1.0
status: draft
author: AI Standards Implementation Agent
date: 2025-11-07
relatedArchitecture:
  - ARC-C-001-CoreProtocol
relatedRequirements:
  - REQ-F-001
  - REQ-F-002
  - REQ-F-003
---

## Core Protocol Software Design Description

This SDD describes the detailed design for the Core Protocol component, covering PTP message structures, parsing/serialization, TLV handling, and validation for IEEE 1588-2019.

## Scope

- Message formats (header + per-type payloads)
- Byte order conversions and bounds-checked buffer I/O
- TLV processing and registry
- Validation rules and compliance checks
- Contracts for latency, memory, and error handling

## Traceability

- Architecture: ARC-C-001-CoreProtocol
- Requirements: REQ-F-001, REQ-F-002, REQ-F-003
- Related ADRs: ADR-001, ADR-002, ADR-003 (layering, BMCA dependency, implementation strategy)
- Planned tests: TEST-MSG-001, TEST-MSG-NEG-001, TEST-MSG-HANDLERS-001, TEST-TRANSPORT-L2-001

## Design elements

| ID         | Type      | Name/Responsibility                         | Realization                                                                 |
|------------|-----------|----------------------------------------------|------------------------------------------------------------------------------|
| DES-C-010  | Component | Core Message Processing                      | Refer to `ieee-1588-2019-message-processing-design.md` (DES-1588-MSG-001)    |
| DES-I-011  | Interface | Message interfaces (PTP headers, TLVs)       | Defined in `ieee-1588-2019-message-processing-design.md`                     |
| DES-D-012  | Data      | Message data models (headers, payloads, TLV) | Defined in `ieee-1588-2019-message-processing-design.md`                     |

Notes:

- The existing component design file uses an internal ID `DES-1588-MSG-001`. This SDD assigns canonical project-wide IDs `DES-C-010`, `DES-I-011`, `DES-D-012` for traceability without renaming existing files. Scripts can resolve both.

## Contracts (inputs/outputs, error modes)

- Inputs: contiguous buffers (uint8_t*, length), message structs; all pointers non-null; lengths validated.
- Outputs: populated message structs, serialized buffers, TLV vectors; exact byte counts returned.
- Error modes: negative error codes for invalid length, invalid fields, unsupported type, overflow risk.
- Performance: parse ≤ 5 µs; serialize ≤ 10 µs per message (typical target, see performance model).
- Memory: no dynamic allocation in hot paths; pre-sized buffers; TLV vector growth bounded.

## Algorithms and checks (summary)

- Header field masks and ranges validated before use
- Network byte order conversions isolated in ByteOrderConverter
- Correction field preserved and propagated per message type rules
- TLV encode/decode uses size calculator to prevent overruns

## Test-driven design links

- TEST-MSG-001 validates happy-path parsing/serialization for key types
- TEST-MSG-NEG-001 exercises malformed inputs and boundary conditions
- TEST-MSG-HANDLERS-001 ensures handler coverage and registry behavior
- TEST-TRANSPORT-L2-001 validates L2 framing alignment with transport design

## Implementation guidance

- Keep message structs POD and packed as needed; document alignment assumptions
- Avoid copying buffers; operate on spans/pointers with explicit length
- Separate validation from transformation to enable targeted testing
- Do not introduce hardware/OS dependencies in this component

## Open items & risks

- Ensure mapping to datasets/timeProperties handled consistently across state machine design
- Confirm endianness coverage for all multi-field structures
- Align performance measurements with 03-architecture/constraints/performance-modeling.md
