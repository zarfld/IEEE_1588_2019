---
title: "Transport (ARC-C-004) - Software Design Description"
specType: design
version: 0.1.0
status: draft
author: AI Standards Implementation Agent
date: 2025-11-07
relatedArchitecture:
  - ARC-C-004-Transport
  - ARC-C-005-HardwareAbstraction
relatedRequirements:
  - REQ-F-004
  - REQ-NFR-158
---

## Transport Software Design Description

### Overview

Defines the transport layer design for IEEE 1588-2019 messages over Ethernet (Annex E) and UDP/IPv4/IPv6 (Annex C/D), including framing/deframing, multicast addressing, and timestamp propagation via HAL.

### Traceability

- Architecture: ARC-C-004-Transport; depends on HAL (ARC-C-005)
- ADRs: ADR-001 (HAL boundary), ADR-003 (implementation strategy)
- Requirements: REQ-F-004 (transport handling), REQ-NFR-158 (resource/performance envelope)
- Tests: TEST-TRANSPORT-L2-001; plus message handler coverage tests referencing transport API.

### Design Elements

| ID        | Type      | Responsibility                              | Realization Reference |
|-----------|-----------|----------------------------------------------|-----------------------|
| DES-C-041 | Component | Transport Manager (multi-transport coordination) | `ieee-1588-2019-transport-design.md` (DES-C-009-TransportManager) |
| DES-I-042 | Interface | ITransport (send/receive, capabilities, ts config) | `ieee-1588-2019-transport-design.md` (DES-I-005-TransportInterface) |
| DES-D-043 | Data      | TransportAddress, capabilities & timestamp modes | `ieee-1588-2019-transport-design.md` (DES-D-009) |

### Contracts

- Inputs: raw ptp payloads, transport options (L2 vs UDP), multicast groups.
- Outputs: fully framed L2/Ethernet or UDP packets; timestamps forwarded from HAL.
- Errors: oversized payload → error; invalid address params → error; HAL unsupported ts mode → capability false.
- Performance: framing ≤ 5 µs; deframing ≤ 5 µs typical; zero-copy where practical.

### Algorithms (Summary)

- L2 framing: build eth header with PTP EtherType; multicast filters via HAL.
- UDP framing: build IP/UDP headers; choose ports/multicast per profile.
- Capability probing: query HAL for hardware timestamping; fall back to software.

### TDD Mapping

- TEST-TRANSPORT-L2-001 validates L2 framing/deframing and multicast.
- Message handler tests ensure correct transport API usage and byte alignment.

### References

- IEEE 1588-2019 transport annexes (C/D/E)
- 03-architecture constraints/performance-modeling
- ADR-001 for HAL boundary
