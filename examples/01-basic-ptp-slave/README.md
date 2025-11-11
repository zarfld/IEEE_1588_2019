# Example 1: Basic PTP Slave Implementation

**Difficulty**: Beginner  
**Time**: 15-20 minutes  
**Concepts**: PTP Slave Clock, Message Handling, Time Synchronization

## Overview

This example demonstrates a complete, working PTP slave clock implementation. A **PTP slave** synchronizes its local time to a master clock on the network by:

1. Listening for **Announce** messages to discover the best master
2. Processing **Sync** and **Follow_Up** messages to calculate offset
3. Using **Delay_Req** and **Delay_Resp** to measure path delay
4. Adjusting the local clock to match the master

This is the **most common PTP use case** - synchronizing a device's clock to a network time source.

## What You'll Learn

- ✅ How to implement a PTP slave clock
- ✅ How to handle PTP messages (Announce, Sync, Follow_Up, Delay_Req, Delay_Resp)
- ✅ How to calculate time offset and path delay
- ✅ How to use a minimal Hardware Abstraction Layer (HAL)
- ✅ How the library handles clock synchronization

## Architecture

```
┌─────────────────────────────────────────┐
│         PTP Master Clock                │
│   (Provides accurate time reference)    │
└─────────────────┬───────────────────────┘
                  │
        Network   │   PTP Protocol Messages
        (Ethernet)│   (Announce, Sync, etc.)
                  │
┌─────────────────▼───────────────────────┐
│      This Example: PTP Slave            │
│                                         │
│  ┌─────────────────────────────────┐   │
│  │  PTP Slave State Machine        │   │
│  │  - Listens for master           │   │
│  │  - Calculates offset & delay    │   │
│  │  - Adjusts local clock          │   │
│  └─────────────────────────────────┘   │
│              ▲                          │
│              │                          │
│  ┌───────────┴──────────────────┐      │
│  │  Minimal HAL (Simulated)     │      │
│  │  - Mock network send/receive │      │
│  │  - Simulated timestamps      │      │
│  │  - Clock adjustment stub     │      │
│  └──────────────────────────────┘      │
└─────────────────────────────────────────┘
```

## Files in This Example

| File | Description |
|------|-------------|
| `basic_ptp_slave.cpp` | Main slave implementation (200+ lines) |
| `minimal_hal.hpp` | Minimal HAL interface definitions |
| `minimal_hal.cpp` | Simulated HAL implementation |
| `CMakeLists.txt` | Build configuration |
| `README.md` | This file |

## Building the Example

### Prerequisites

- CMake 3.20+
- C++17 compiler
- IEEE 1588-2019 PTP Library (parent project)

### Build Steps

From the repository root:

```bash
# Configure
cmake -S . -B build

# Build just this example
cmake --build build --target basic_ptp_slave

# Or build all examples
cmake --build build
```

The executable will be in:
- **Windows**: `build/examples/01-basic-ptp-slave/Release/basic_ptp_slave.exe`
- **Linux/macOS**: `build/examples/01-basic-ptp-slave/basic_ptp_slave`

## Running the Example

```bash
# Windows
.\build\examples\01-basic-ptp-slave\Release\basic_ptp_slave.exe

# Linux/macOS
./build/examples/01-basic-ptp-slave/basic_ptp_slave
```

## Expected Output

```
=====================================
  Basic PTP Slave Example
  IEEE 1588-2019 Implementation
=====================================

Initializing PTP Slave...
Clock Identity: 00:11:22:ff:fe:33:44:55
Port Number: 1
Initial Clock State: LISTENING

--- Starting Synchronization Sequence ---

[Step 1] Receiving Announce message from Master...
  Master Clock ID: aa:bb:cc:ff:fe:dd:ee:ff
  Master Priority1: 128
  Master Priority2: 128
  Master Clock Class: 248
  → Best Master Clock Algorithm (BMCA) Result: ACCEPT
  → State Transition: LISTENING → UNCALIBRATED

[Step 2] Receiving Sync message...
  Sync Timestamp: 1699564800.500000000
  Received at (local): 1699564800.501234567
  → Calculated raw offset: 1234567 ns

[Step 3] Receiving Follow_Up message...
  Precise Timestamp: 1699564800.500000000
  → Updated offset calculation

[Step 4] Sending Delay_Req to measure path delay...
  Sent at (local): 1699564800.502000000

[Step 5] Receiving Delay_Resp from Master...
  Master received Delay_Req at: 1699564800.502050000
  → Calculated path delay: 50000 ns (50 μs)

[Synchronization Results]
  Time Offset from Master: 1234567 ns (1.23 ms)
  Path Delay: 50000 ns (50 μs)
  Corrected Offset: 1184567 ns (1.18 ms)
  → Adjusting clock by: -1184567 ns

Clock synchronized successfully!
Final State: SLAVE

=====================================
  Example Complete!
=====================================

Summary:
  ✓ Discovered and selected master clock
  ✓ Calculated time offset (1.18 ms)
  ✓ Measured path delay (50 μs)
  ✓ Synchronized local clock to master
  ✓ Achieved SLAVE state

In a real system:
  • Network HAL would use actual Ethernet/UDP sockets
  • Timestamps would come from hardware timestamping
  • Clock adjustment would use system time APIs
  • Process would repeat continuously for ongoing sync

Next Steps:
  → Study the source code in basic_ptp_slave.cpp
  → Examine minimal_hal.cpp for HAL patterns
  → See integration-guide.md for production HAL implementation
  → Try example 2: BMCA Integration (multi-clock scenario)
```

## Key Concepts Demonstrated

### 1. PTP Message Flow (Delay Request-Response Mechanism)

```
Master                          Slave
  │                              │
  ├──── Announce ────────────────>│  (Master advertisement)
  │                              │
  ├──── Sync (t1) ───────────────>│  (Reference timestamp)
  │                              │  Received at t2 (local)
  ├──── Follow_Up (t1 precise) ──>│  (Corrected timestamp)
  │                              │
  │<──── Delay_Req (t3) ──────────┤  (Path delay probe)
  │                              │  Sent at t3 (local)
  ├──── Delay_Resp (t4) ─────────>│  (When Delay_Req arrived)
  │                              │
  │                              │  Calculate:
  │                              │  offset = t2 - t1
  │                              │  delay = (t4 - t3)
  │                              │  corrected_offset = offset - delay/2
```

### 2. BMCA (Best Master Clock Algorithm)

When multiple masters exist, the slave uses BMCA to select the best one:

```cpp
// Compare master quality attributes
if (announce.grandmaster_priority1 != local.priority1) {
    return announce.grandmaster_priority1 < local.priority1 ? ACCEPT : REJECT;
}
if (announce.clock_class != local.clock_class) {
    return announce.clock_class < local.clock_class ? ACCEPT : REJECT;
}
// ... continue comparison
```

### 3. Time Offset Calculation

```cpp
// Basic offset calculation
int64_t offset_ns = t2_receive_time - t1_sync_timestamp;

// Path delay measurement
int64_t path_delay_ns = t4_delay_resp - t3_delay_req;

// Corrected offset (accounting for one-way delay)
int64_t corrected_offset = offset_ns - (path_delay_ns / 2);

// Apply correction to local clock
adjust_clock(-corrected_offset);
```

### 4. State Machine Transitions

```
INITIALIZING → LISTENING → UNCALIBRATED → SLAVE
                    ↑            │
                    └────────────┘
                  (if sync lost)
```

## Code Structure

### main() Flow

1. **Initialize** - Create slave clock, setup HAL
2. **Listen** - Wait for Announce messages
3. **Select Master** - Apply BMCA
4. **Synchronize** - Process Sync/Follow_Up/Delay messages
5. **Adjust Clock** - Apply offset correction
6. **Maintain** - Continuous synchronization (simulated once here)

### HAL Interface

The minimal HAL provides:

```cpp
// Network operations
int send_packet(const uint8_t* data, size_t length);
int receive_packet(uint8_t* buffer, size_t* length);

// Timestamp capture
uint64_t get_timestamp_ns();

// Clock adjustment
int adjust_clock(int64_t offset_ns);
```

## Adapting for Production

To use this in a real system:

### 1. Replace Minimal HAL with Real Implementation

```cpp
// Instead of simulated network:
#ifdef _WIN32
    // Use Windows Winsock2 for UDP/Ethernet
#else
    // Use Linux socket API with SO_TIMESTAMPING
#endif
```

### 2. Enable Hardware Timestamping

```cpp
// Linux example
int sock = socket(AF_INET, SOCK_DGRAM, 0);
int flags = SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RX_HARDWARE;
setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof(flags));
```

### 3. Implement Continuous Operation

```cpp
while (running) {
    // Receive PTP messages
    // Process based on message type
    // Update clock periodically
    // Handle state transitions
}
```

### 4. Add Error Handling and Logging

```cpp
if (result.has_error()) {
    log_error("Sync failed: %s", result.error().message());
    // Transition to FAULTY state
}
```

## Troubleshooting

### Problem: Example doesn't compile

**Solution**: Make sure you've built the main library first:

```bash
cmake -S . -B build
cmake --build build
```

### Problem: Can't find the executable

**Solution**: Check the correct path for your platform:

```bash
# Windows uses Release/Debug subdirectory
dir build\examples\01-basic-ptp-slave\Release\

# Linux/macOS is in the example directory
ls build/examples/01-basic-ptp-slave/
```

### Problem: Want to see actual network traffic

**Solution**: This example uses simulated HAL. For real network traffic, see:
- `integration-guide.md` Section 3: HAL Implementation
- Example 3: HAL Implementation Template
- Production system would use actual sockets

## Next Steps

1. **Study the Code**: Read through `basic_ptp_slave.cpp` with comments
2. **Try Example 2**: BMCA Integration - multiple clocks competing
3. **Try Example 3**: HAL Implementation Template - porting guide
4. **Read Integration Guide**: Production HAL implementation details
5. **Build a Real Slave**: Implement with actual network/timestamping

## References

- **IEEE 1588-2019**: Section 9.2 (State Protocol), Section 11.3 (Delay Mechanisms)
- **API Reference**: `api-reference.md` in docs
- **Integration Guide**: `integration-guide.md` in docs
- **Getting Started**: `getting-started.md` in training-materials

## Questions?

If you have questions about this example:
1. Check the source code comments
2. Review the Getting Started tutorial
3. See the Integration Guide for production patterns
4. Open an issue on GitHub

---

**Happy PTP Coding!** 🚀⏱️
