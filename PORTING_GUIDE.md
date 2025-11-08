---
id: PORTING-GUIDE
specType: documentation
phase: 08-transition
version: 0.1.0
status: draft
traceability:
  requirements:
    - STR-USE-004
---
# Porting Guide (Draft)

This guide provides step-by-step instructions for porting the IEEE 1588-2019 protocol core to a new platform while preserving hardware/OS agnosticism.

## 1. Scope
Target audience: platform integrators creating a HAL for a new OS, NIC, or embedded target (e.g., NXP i.MX RT, STM32H7, RISC-V SoC).

## 2. Prerequisites
- C/C++ toolchain supporting C++14 and C99 interoperability
- Access to platform timer and network driver APIs
- Ability to capture hardware timestamps (optional but recommended)

## 3. HAL Implementation Steps
1. Copy the HAL interface header from `lib/Standards/Common/interfaces/` (future location) into your adapter project include path.
2. Implement the required function pointer callbacks:
   - `send_packet`, `receive_packet`
   - `get_time_ns` (monotonic source)
   - `set_timer` (one-shot / periodic timers)
   - Optional: hardware timestamp retrieval if NIC supports it
3. Initialize capability flags (e.g., `NETWORK_CAP_HARDWARE_TIMESTAMP`).
4. Provide a factory function returning a fully populated HAL struct.
5. Write unit tests using mocks/fakes around each function pointer to validate error paths and edge cases.

## 4. Integration
```text
App/Service Layer --> HAL Adapter --> IEEE 1588-2019 Protocol Core
```
Inject the HAL into the protocol initialization routine (e.g., `ptp_init(&hal)`).

## 5. Validation Checklist
- [ ] All required callbacks implemented and return success codes
- [ ] No direct inclusion of OS or vendor headers in Standards layer
- [ ] Timestamp precision validated (<100 ns resolution preferred)
- [ ] No dynamic allocation in time-critical paths
- [ ] Endianness conversions verified with sample packets

## 6. Performance Considerations
- Use static or pooled buffers for packet I/O.
- Avoid blocking calls inside callbacks.
- Leverage hardware timestamping to reduce jitter when available.

## 7. Testing Strategy
- Unit: mock HAL functions to force error codes and boundary cases.
- Integration: loopback or virtual NIC tests for send/receive.
- Determinism: measure path latency of Sync processing (see `timing_determinism` test pattern).

## 8. Common Pitfalls
- Forgetting to zero-initialize the HAL struct (leads to null function pointers)
- Mixing clock domains (system time vs PTP timescale) without conversion
- Not handling sequence ID rollover

## 9. Next Steps
- Provide a reference Linux HAL adapter.
- Add a CI job to compile with a cross-toolchain (ARM Cortex-M).
- Expand this guide with real code snippets once reference HAL lands.

*Copyright note: Implementation guidance is original; references IEEE 1588-2019 conceptually without reproducing specification text.*
