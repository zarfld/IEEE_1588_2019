# Example 3: HAL Implementation Template - Porting Guide

## Overview

This example provides a **complete template** for implementing the Hardware Abstraction Layer (HAL) required to port the IEEE 1588-2019 PTP library to your specific platform. Whether you're targeting **Linux**, **Windows**, **FreeRTOS**, **bare-metal embedded**, or another platform, this guide provides the structure and examples you need.

**What is the HAL?**  
The HAL (Hardware Abstraction Layer) is the bridge between the platform-independent IEEE 1588-2019 PTP protocol implementation and your specific hardware/operating system. It provides network access, hardware timestamping, and clock control.

**What You'll Learn:**
- Complete HAL interface requirements
- Platform-specific implementation strategies
- Step-by-step porting process
- Testing and validation approaches
- Performance optimization techniques

## Why HAL Abstraction?

The IEEE 1588-2019 library core is **completely hardware-agnostic**:
- ✅ **Portable**: Compiles without platform-specific headers
- ✅ **Testable**: Runs with simulated HAL (no hardware required)
- ✅ **Reusable**: Same core logic across all platforms
- ✅ **Maintainable**: Hardware changes don't affect protocol logic

**Your responsibility**: Implement the HAL interface for your target platform.

## Architecture

```
┌──────────────────────────────────────────┐
│   Application Code                       │
│   (Your PTP-enabled application)         │
└─────────────┬────────────────────────────┘
              │
              │ Uses library API
              ▼
┌──────────────────────────────────────────┐
│   IEEE 1588-2019 PTP Library             │
│   (Hardware-agnostic protocol logic)     │
│   ├─ Message parsing                     │
│   ├─ State machines                      │
│   ├─ BMCA                                │
│   └─ Time calculations                   │
└─────────────┬────────────────────────────┘
              │
              │ Dependency Injection
              │ (Function pointers or interfaces)
              ▼
┌──────────────────────────────────────────┐
│   HAL Implementation (YOU IMPLEMENT)     │
│   ├─ NetworkHAL                          │
│   │   ├─ send_packet()                   │
│   │   └─ receive_packet()                │
│   ├─ TimestampHAL                        │
│   │   └─ get_time_ns()                   │
│   └─ ClockHAL                            │
│       └─ adjust_clock()                  │
└─────────────┬────────────────────────────┘
              │
              │ Platform APIs
              ▼
┌──────────────────────────────────────────┐
│   Platform-Specific APIs                 │
│   Linux: socket(), clock_gettime()       │
│   Windows: Winsock2, QPC()               │
│   RTOS: Task APIs, hardware timers       │
│   Bare-metal: Register access            │
└──────────────────────────────────────────┘
```

## Files in This Example

| File | Description |
|------|-------------|
| `README.md` | This comprehensive porting guide |
| `hal_template.hpp` | Complete HAL interface definitions with documentation |
| `hal_template.cpp` | Skeleton implementations with platform-specific examples |
| `porting_checklist.md` | Step-by-step checklist for porting process |
| `CMakeLists.txt` | Build configuration with platform detection |

## HAL Interface Requirements

### 1. NetworkHAL - Network Communication

**Required Functions:**

```cpp
class NetworkHAL {
public:
    // Send PTP packet
    // - data: Packet bytes (IEEE 1588-2019 format)
    // - length: Packet size
    // Returns: 0 on success, negative error code on failure
    int send_packet(const uint8_t* data, size_t length);
    
    // Receive PTP packet (blocking or non-blocking)
    // - buffer: Destination buffer
    // - length: Buffer size (in), received size (out)
    // - timestamp_ns: Hardware timestamp of reception (out)
    // Returns: 0 on success, negative error code on failure/timeout
    int receive_packet(uint8_t* buffer, size_t* length, uint64_t* timestamp_ns);
    
    // Check if packet available (non-blocking)
    // Returns: true if packet ready, false otherwise
    bool has_packet();
};
```

**Platform Implementations:**

| Platform | Network API | Notes |
|----------|-------------|-------|
| **Linux** | `socket()`, `sendto()`, `recvfrom()` | Use `AF_PACKET` for Layer 2, `AF_INET` for UDP/IP |
| **Windows** | Winsock2 `sendto()`, `recvfrom()` | Use `SOCK_RAW` for Layer 2 access |
| **FreeRTOS** | lwIP `sendto()`, `recvfrom()` | May need raw socket support |
| **Bare-metal** | Ethernet driver direct access | Access TX/RX buffers directly |

**Key Considerations:**
- **Layer 2 vs UDP/IP**: PTP can use Ethernet Layer 2 (Ethertype 0x88F7) or UDP/IP (ports 319/320)
- **Multicast handling**: PTP uses multicast addresses (01:1B:19:00:00:00 for Layer 2, 224.0.1.129 for IPv4)
- **Hardware timestamping**: Extract timestamp from NIC if available (see section below)

### 2. TimestampHAL - Time Capture

**Required Functions:**

```cpp
class TimestampHAL {
public:
    // Get current time in nanoseconds since epoch
    // Must provide sub-microsecond resolution for PTP accuracy
    // Returns: Current time in nanoseconds
    uint64_t get_time_ns();
    
    // Get resolution of timestamp hardware
    // Returns: Resolution in nanoseconds (e.g., 1 for 1ns, 8 for 8ns)
    uint32_t get_resolution_ns();
};
```

**Platform Implementations:**

| Platform | Time API | Resolution | Notes |
|----------|----------|------------|-------|
| **Linux** | `clock_gettime(CLOCK_REALTIME)` | ~1ns | Use `CLOCK_TAI` for TAI timescale |
| **Windows** | `QueryPerformanceCounter()` | ~100ns | Convert using `QueryPerformanceFrequency()` |
| **FreeRTOS** | `xTaskGetTickCount()` + hardware timer | Varies | Combine system tick with high-res timer |
| **Bare-metal** | Hardware timer register | Varies | Read timer counter, convert to nanoseconds |

**Hardware Timestamping (Critical for PTP accuracy):**

Most modern Ethernet controllers support hardware timestamping:
- **Intel i210/i211**: Supports IEEE 1588 timestamping
- **TI DP83640**: Dedicated PTP PHY with timestamping
- **Xilinx Zynq**: Hardware timestamp support in GEM

**Hardware timestamp extraction (Linux example):**
```cpp
// Enable hardware timestamping on socket
int flags = SOF_TIMESTAMPING_RX_HARDWARE |
            SOF_TIMESTAMPING_TX_HARDWARE |
            SOF_TIMESTAMPING_RAW_HARDWARE;
setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof(flags));

// Receive with timestamp
struct msghdr msg;
struct cmsghdr *cmsg;
// ... setup msg ...

recvmsg(sock, &msg, 0);

// Extract hardware timestamp from control message
for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
        struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
        uint64_t timestamp_ns = ts[2].tv_sec * 1000000000ULL + ts[2].tv_nsec;
        // ts[2] contains hardware timestamp
    }
}
```

### 3. ClockHAL - Clock Discipline

**Required Functions:**

```cpp
class ClockHAL {
public:
    // Adjust system clock by offset
    // - offset_ns: Adjustment in nanoseconds (positive = speed up, negative = slow down)
    // - mode: STEP (immediate) or SLEW (gradual)
    // Returns: 0 on success, negative error code on failure
    int adjust_clock(int64_t offset_ns, AdjustMode mode);
    
    // Set clock frequency adjustment (for disciplining)
    // - ppb: Parts-per-billion adjustment
    // Returns: 0 on success, negative error code on failure
    int adjust_frequency(int32_t ppb);
};

enum class AdjustMode {
    STEP,   // Immediate jump (for large offsets)
    SLEW    // Gradual adjustment (for small offsets)
};
```

**Platform Implementations:**

| Platform | Clock Control API | Notes |
|----------|-------------------|-------|
| **Linux** | `adjtimex()`, `clock_adjtime()` | Use `ADJ_OFFSET_SINGLESHOT` for step, `ADJ_OFFSET` for slew |
| **Windows** | `SetSystemTime()`, `GetSystemTime()` | Limited to millisecond resolution |
| **FreeRTOS** | Software PLL + hardware timer | Implement software clock discipline |
| **Bare-metal** | RTC register access | Direct hardware manipulation |

**Linux example:**
```cpp
int adjust_clock(int64_t offset_ns, AdjustMode mode) {
    struct timex tx;
    memset(&tx, 0, sizeof(tx));
    
    if (mode == AdjustMode::STEP) {
        // Immediate adjustment
        tx.modes = ADJ_OFFSET_SINGLESHOT | ADJ_NANO;
        tx.offset = offset_ns;
    } else {
        // Gradual adjustment
        tx.modes = ADJ_OFFSET | ADJ_NANO;
        tx.offset = offset_ns;
    }
    
    return adjtimex(&tx);
}
```

## Step-by-Step Porting Process

### Phase 1: Environment Setup (1-2 days)

1. ✅ **Clone repository and build examples**
   ```bash
   git clone <repo>
   cd IEEE_1588_2019
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```

2. ✅ **Run simulated examples**
   - Example 1: Basic PTP Slave (with simulated HAL)
   - Example 2: BMCA Integration
   - Verify library compiles and tests pass

3. ✅ **Study HAL interfaces**
   - Read `hal_template.hpp` carefully
   - Understand function contracts
   - Review platform-specific examples

### Phase 2: Platform Analysis (2-3 days)

1. ✅ **Identify platform capabilities**
   - Operating system (Linux, Windows, RTOS, bare-metal)?
   - Network stack available?
   - Hardware timestamping support?
   - Clock control mechanisms?

2. ✅ **Document platform APIs**
   - Network: How to send/receive packets?
   - Time: How to get high-resolution timestamps?
   - Clock: How to adjust system time?

3. ✅ **Check hardware specifications**
   - Ethernet controller model?
   - Does it support IEEE 1588 hardware timestamping?
   - Timer resolution?
   - CPU frequency and capabilities?

### Phase 3: HAL Skeleton Implementation (3-5 days)

1. ✅ **Copy template files**
   ```bash
   cp examples/03-hal-implementation-template/hal_template.hpp my_platform_hal.hpp
   cp examples/03-hal-implementation-template/hal_template.cpp my_platform_hal.cpp
   ```

2. ✅ **Implement NetworkHAL**
   - Start with `send_packet()` (easier)
   - Then `receive_packet()` (may need threading)
   - Add `has_packet()` for non-blocking operation

3. ✅ **Implement TimestampHAL**
   - Start with software timestamps (system time)
   - Later add hardware timestamping if available

4. ✅ **Implement ClockHAL (simplified first)**
   - Start with STEP mode only
   - Add SLEW and frequency adjustment later

### Phase 4: Integration and Testing (5-7 days)

1. ✅ **Create platform-specific example**
   ```cpp
   #include "my_platform_hal.hpp"
   #include <IEEE/1588/2019/ptp_slave.hpp>
   
   int main() {
       MyPlatformHAL hal;
       hal.initialize();
       
       PTPSlave slave(&hal);
       slave.run();
       
       return 0;
   }
   ```

2. ✅ **Unit test each HAL function**
   - Test `send_packet()` with known packet
   - Test `receive_packet()` with loopback
   - Test `get_time_ns()` for monotonicity
   - Test `adjust_clock()` with small offset

3. ✅ **Integration test with PTP master**
   - Use `ptp4l` (Linux PTP) as master
   - Or hardware PTP grandmaster clock
   - Verify Announce message reception
   - Verify Sync/Follow_Up processing
   - Measure synchronization accuracy

### Phase 5: Optimization (3-5 days)

1. ✅ **Enable hardware timestamping**
   - Configure Ethernet controller
   - Extract timestamps from driver
   - Verify improved accuracy

2. ✅ **Optimize critical paths**
   - Minimize latency in `receive_packet()`
   - Use DMA for packet transfer if available
   - Avoid dynamic allocation in hot paths

3. ✅ **Add performance monitoring**
   - Measure timestamp jitter
   - Track offset/delay statistics
   - Log synchronization quality

### Phase 6: Validation (2-3 days)

1. ✅ **Conformance testing**
   - Run library test suite
   - Verify IEEE 1588-2019 compliance
   - Test with multiple masters (BMCA)

2. ✅ **Performance characterization**
   - Measure synchronization accuracy (target: <1μs)
   - Measure CPU utilization
   - Measure memory footprint

3. ✅ **Documentation**
   - Document platform-specific considerations
   - Create build instructions
   - Write troubleshooting guide

**Total Estimated Time**: 16-25 days (varies by platform complexity)

## Platform-Specific Examples

### Linux Implementation (Production-Ready)

See `08-transition/deployment-plans/integration-guide.md` for complete Linux HAL implementation.

**Key Linux APIs:**
- **Network**: `socket(AF_PACKET, SOCK_RAW)` for Layer 2, `socket(AF_INET, SOCK_DGRAM)` for UDP
- **Timestamping**: `SO_TIMESTAMPING` socket option, extract from `recvmsg()` control messages
- **Clock**: `adjtimex()` with `ADJ_OFFSET_SINGLESHOT` or `ADJ_OFFSET` modes

### Windows Implementation

**Key Windows APIs:**
- **Network**: Winsock2 `socket()`, `sendto()`, `recvfrom()`
- **Timestamping**: `QueryPerformanceCounter()`, convert using `QueryPerformanceFrequency()`
- **Clock**: `SetSystemTime()` (millisecond resolution only)

**Windows challenge**: Limited clock control resolution (1ms). Consider:
- Software PLL for sub-millisecond discipline
- Third-party timestamping drivers (e.g., Intel PTP)

### FreeRTOS Implementation

**Key FreeRTOS APIs:**
- **Network**: lwIP stack `sendto()`, `recvfrom()` or raw API
- **Timestamping**: Combine `xTaskGetTickCount()` with hardware timer for high resolution
- **Clock**: Software PLL adjusting timer reload values

**FreeRTOS pattern:**
```cpp
class FreeRTOSTimestampHAL {
public:
    uint64_t get_time_ns() {
        // Combine system tick (coarse) with hardware timer (fine)
        uint32_t ticks = xTaskGetTickCount();
        uint32_t timer_count = READ_HW_TIMER();
        
        uint64_t coarse_ns = ticks * (1000000000ULL / configTICK_RATE_HZ);
        uint64_t fine_ns = timer_count * HW_TIMER_NS_PER_TICK;
        
        return coarse_ns + fine_ns;
    }
};
```

### Bare-Metal Embedded Implementation

**Direct hardware access:**
```cpp
class BareMetalNetworkHAL {
public:
    int send_packet(const uint8_t* data, size_t length) {
        // Access Ethernet controller TX buffer
        volatile uint32_t* tx_buf = (uint32_t*)ETHERNET_TX_BUFFER_ADDR;
        memcpy((void*)tx_buf, data, length);
        
        // Trigger transmission
        ETHERNET_TX_CTRL_REG = TX_START | length;
        
        // Wait for completion (or use interrupt)
        while (ETHERNET_TX_STATUS_REG & TX_BUSY);
        
        return 0;
    }
};
```

## Testing Your HAL Implementation

### Unit Tests

Create unit tests for each HAL component:

```cpp
// test_hal.cpp
#include "my_platform_hal.hpp"
#include <gtest/gtest.h>

TEST(NetworkHAL, SendReceiveLoopback) {
    MyPlatformNetworkHAL hal;
    
    uint8_t send_data[64] = {0x01, 0x02, 0x03};
    ASSERT_EQ(0, hal.send_packet(send_data, 64));
    
    uint8_t recv_data[64];
    size_t recv_len = 64;
    uint64_t timestamp;
    ASSERT_EQ(0, hal.receive_packet(recv_data, &recv_len, &timestamp));
    
    ASSERT_EQ(64, recv_len);
    ASSERT_EQ(0, memcmp(send_data, recv_data, 64));
}

TEST(TimestampHAL, Monotonicity) {
    MyPlatformTimestampHAL hal;
    
    uint64_t t1 = hal.get_time_ns();
    usleep(1000);  // Wait 1ms
    uint64_t t2 = hal.get_time_ns();
    
    ASSERT_GT(t2, t1);  // Time must increase
    ASSERT_LT(t2 - t1, 10000000);  // Should be ~1ms (not wildly off)
}
```

### Integration Tests

Test with real PTP master:

```bash
# On Linux master
sudo ptp4l -i eth0 -m

# On your slave platform
./my_ptp_slave
```

**Expected output:**
- Announce messages received
- Sync/Follow_Up messages processed
- Offset converging to <1μs

### Performance Validation

Measure key metrics:
- **Sync accuracy**: Measure against GPS-disciplined master
- **Timestamp jitter**: Log timestamp differences over time
- **CPU utilization**: Profile HAL functions
- **Memory usage**: Check stack/heap consumption

## Troubleshooting

### Issue 1: No packets received

**Symptoms**: `receive_packet()` always times out

**Diagnosis**:
- Check network connectivity (ping)
- Verify multicast group membership
- Check firewall rules
- Ensure correct Ethernet interface

**Solution**:
- Linux: Join multicast group with `setsockopt(IP_ADD_MEMBERSHIP)`
- Verify interface with `ip link show`

### Issue 2: Large synchronization offsets

**Symptoms**: Offset >100ms, not converging

**Diagnosis**:
- Check if timestamps are in correct units (nanoseconds?)
- Verify timestamp epoch (Unix epoch?)
- Check for timestamp wrap-around

**Solution**:
- Print raw timestamps and verify format
- Ensure 64-bit values for nanoseconds

### Issue 3: Hardware timestamping not working

**Symptoms**: Timestamps identical for different packets

**Diagnosis**:
- Check if hardware supports timestamping
- Verify driver supports `SO_TIMESTAMPING`
- Check if kernel compiled with PTP support

**Solution**:
- Linux: `ethtool -T eth0` shows timestamping capabilities
- May need to enable in hardware/driver

### Issue 4: Clock adjustment not effective

**Symptoms**: Clock adjusts but offset doesn't improve

**Diagnosis**:
- Check if system allows clock adjustment (permissions)
- Verify adjustment direction (sign)
- Check if adjustment magnitude is reasonable

**Solution**:
- Linux: Run as root or with `CAP_SYS_TIME` capability
- Verify adjustment with `adjtimex` directly

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| **Sync Accuracy** | <1μs | With hardware timestamping |
| **Sync Accuracy** | <10μs | Software timestamping only |
| **Timestamp Resolution** | ≤100ns | For sub-microsecond sync |
| **Packet Latency** | <100μs | From reception to timestamp capture |
| **CPU Utilization** | <5% | On moderate embedded CPU |
| **Memory Footprint** | <1MB | Including library + HAL |

## Next Steps

1. **Complete porting checklist** in `porting_checklist.md`
2. **Review integration guide** in `08-transition/deployment-plans/integration-guide.md`
3. **Study IEEE 1588-2019** Section 20 (Conformance requirements)
4. **Join community** - Share your HAL implementation!

## Additional Resources

- **IEEE 1588-2019 Standard**: Authoritative specification
- **Linux PTP Project**: Reference implementation (`ptp4l`, `phc2sys`)
- **Intel i210 Datasheet**: Hardware timestamping details
- **Library Documentation**: `docs/doxygen/html/index.html`
- **Integration Guide**: `08-transition/deployment-plans/integration-guide.md`

---

**Example Status**: Complete HAL porting guide and template  
**IEEE 1588-2019 Compliance**: HAL interface requirements  
**Last Updated**: 2025-11-11
