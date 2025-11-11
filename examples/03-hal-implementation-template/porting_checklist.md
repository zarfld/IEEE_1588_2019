# HAL Implementation Porting Checklist

## Overview

This checklist guides you through porting the IEEE 1588-2019 PTP library to a new platform by implementing the Hardware Abstraction Layer (HAL).

**Estimated Time**: 16-25 days depending on platform complexity and hardware timestamping support

---

## Phase 1: Environment Setup (Days 1-2)

### Day 1: Initial Setup
- [ ] **Clone repository**
  ```bash
  git clone <repository-url>
  cd IEEE_1588_2019
  ```

- [ ] **Build library and examples**
  ```bash
  mkdir build && cd build
  cmake ..
  cmake --build .
  ```

- [ ] **Run simulated examples**
  - [ ] Example 1: Basic PTP Slave (simulated HAL)
  - [ ] Example 2: BMCA Integration
  - [ ] Verify all tests pass: `ctest`

- [ ] **Study HAL interfaces**
  - [ ] Read `examples/03-hal-implementation-template/README.md`
  - [ ] Read `hal_template.hpp` carefully
  - [ ] Understand function contracts and requirements

### Day 2: Platform Analysis
- [ ] **Document your platform**
  - Platform name: ________________
  - Operating system: ________________
  - CPU architecture: ________________
  - RAM available: ________________
  - Flash/ROM available: ________________

- [ ] **Identify platform capabilities**
  - [ ] OS or bare-metal? ________________
  - [ ] Network stack available? ________________
  - [ ] Which network APIs? ________________
  - [ ] Timer resolution? ________________
  - [ ] Clock adjustment support? ________________

- [ ] **Check hardware specifications**
  - [ ] Ethernet controller model: ________________
  - [ ] Hardware timestamping support? ☐ Yes ☐ No
  - [ ] Maximum timer resolution: ________________
  - [ ] CPU frequency: ________________

---

## Phase 2: HAL Skeleton (Days 3-7)

### Day 3: Setup Platform HAL Files
- [ ] **Copy template files**
  ```bash
  cd examples
  mkdir my-platform-hal
  cp 03-hal-implementation-template/hal_template.hpp my-platform-hal/my_platform_hal.hpp
  cp 03-hal-implementation-template/hal_template.cpp my-platform-hal/my_platform_hal.cpp
  ```

- [ ] **Update file headers**
  - [ ] Replace "Template" with platform name
  - [ ] Update copyright/author information
  - [ ] Update namespace name

- [ ] **Create CMakeLists.txt**
  ```cmake
  add_library(my_platform_hal STATIC
      my_platform_hal.cpp
  )
  
  target_link_libraries(my_platform_hal
      ieee1588_2019_ptp
      # Add platform-specific libraries
  )
  ```

### Day 4-5: NetworkHAL Implementation
- [ ] **NetworkHAL Constructor**
  - [ ] Open network socket
    - Layer 2: `socket(AF_PACKET, SOCK_RAW, htons(0x88F7))`
    - UDP/IP: `socket(AF_INET, SOCK_DGRAM, 0)`
  - [ ] Bind to interface
  - [ ] Configure for multicast
  - [ ] Enable hardware timestamping (if available)
  - [ ] Set non-blocking mode or timeout

- [ ] **NetworkHAL::send_packet()**
  - [ ] Implement packet transmission
  - [ ] Verify destination address setup
  - [ ] Test with known packet
  - [ ] Measure transmission latency (<100μs target)

- [ ] **NetworkHAL::receive_packet()**
  - [ ] Implement packet reception
  - [ ] Extract hardware timestamp (if available)
  - [ ] Fallback to software timestamp
  - [ ] Handle timeout correctly

- [ ] **NetworkHAL::has_packet()**
  - [ ] Implement non-blocking check
  - [ ] Use select/poll/hardware status

- [ ] **NetworkHAL Destructor**
  - [ ] Close sockets
  - [ ] Release resources

### Day 6: TimestampHAL Implementation
- [ ] **TimestampHAL Constructor**
  - [ ] Initialize hardware timer (if needed)
  - [ ] Configure for maximum resolution
  - [ ] Calibrate if necessary

- [ ] **TimestampHAL::get_time_ns()**
  - [ ] Implement high-resolution time capture
  - [ ] Verify nanosecond conversion
  - [ ] Test for monotonicity
  - [ ] Measure capture latency (<1μs target)

- [ ] **TimestampHAL::get_resolution_ns()**
  - [ ] Return actual hardware resolution
  - [ ] Document in comments

### Day 7: ClockHAL Implementation
- [ ] **ClockHAL Constructor**
  - [ ] Verify permission to adjust clock
  - [ ] Initialize discipline state

- [ ] **ClockHAL::adjust_clock() - STEP mode**
  - [ ] Implement immediate adjustment
  - [ ] Test with small offset (100μs)
  - [ ] Test with large offset (100ms)
  - [ ] Verify system time changed

- [ ] **ClockHAL::adjust_clock() - SLEW mode**
  - [ ] Implement gradual adjustment
  - [ ] Understand slew rate (~0.5ms/sec on Linux)
  - [ ] Test with moderate offset (10ms)

- [ ] **ClockHAL::adjust_frequency()**
  - [ ] Implement frequency adjustment
  - [ ] Test with ±100 PPB
  - [ ] Document limitations if not supported

---

## Phase 3: Unit Testing (Days 8-10)

### Day 8: Network Tests
- [ ] **Test send_packet()**
  ```cpp
  uint8_t test_packet[64] = {0x01, 0x02, 0x03, ...};
  int result = hal.network().send_packet(test_packet, 64);
  assert(result == 0);
  ```

- [ ] **Test receive_packet() with loopback**
  ```cpp
  hal.network().send_packet(send_data, 64);
  uint8_t recv_data[64];
  size_t len = 64;
  uint64_t ts;
  int result = hal.network().receive_packet(recv_data, &len, &ts);
  assert(result == 0);
  assert(memcmp(send_data, recv_data, 64) == 0);
  ```

- [ ] **Test has_packet()**
  ```cpp
  assert(!hal.network().has_packet());  // Initially empty
  hal.network().send_packet(data, 64);
  assert(hal.network().has_packet());   // Now available
  ```

### Day 9: Timestamp Tests
- [ ] **Test get_time_ns() monotonicity**
  ```cpp
  uint64_t t1 = hal.timestamp().get_time_ns();
  usleep(1000);  // 1ms
  uint64_t t2 = hal.timestamp().get_time_ns();
  assert(t2 > t1);
  assert(t2 - t1 > 900000);   // At least 0.9ms
  assert(t2 - t1 < 1100000);  // At most 1.1ms
  ```

- [ ] **Test timestamp accuracy**
  - [ ] Compare against GPS reference (if available)
  - [ ] Measure jitter over 1000 samples
  - [ ] Target: <100ns jitter

### Day 10: Clock Adjustment Tests
- [ ] **Test clock adjustment**
  ```cpp
  uint64_t t1 = hal.timestamp().get_time_ns();
  hal.clock().adjust_clock(1000000, AdjustMode::STEP);  // +1ms
  uint64_t t2 = hal.timestamp().get_time_ns();
  int64_t diff = t2 - t1;
  assert(diff > 900000 && diff < 1100000);  // ~1ms
  ```

- [ ] **Test frequency adjustment**
  ```cpp
  hal.clock().adjust_frequency(100);  // +100 PPB
  // Measure frequency over time
  ```

---

## Phase 4: Integration Testing (Days 11-15)

### Day 11: Create Test Application
- [ ] **Create simple PTP slave**
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

- [ ] **Build test application**
  - [ ] Add to CMakeLists.txt
  - [ ] Link with library and HAL
  - [ ] Resolve any link errors

### Day 12: Network Integration Test
- [ ] **Setup PTP master**
  - Linux: `sudo ptp4l -i eth0 -m`
  - Or use hardware PTP grandmaster

- [ ] **Test Announce reception**
  - [ ] Run slave application
  - [ ] Verify Announce messages received
  - [ ] Check BMCA selection

- [ ] **Test Sync/Follow_Up**
  - [ ] Verify Sync messages processed
  - [ ] Verify timestamps captured
  - [ ] Calculate offset

### Day 13: Synchronization Test
- [ ] **Test full sync sequence**
  - [ ] Announce → Sync → Follow_Up → Delay_Req → Delay_Resp
  - [ ] Verify offset calculation
  - [ ] Verify path delay measurement

- [ ] **Test clock adjustment**
  - [ ] Verify clock adjusts toward master
  - [ ] Monitor offset over time
  - [ ] Target: <1μs after convergence (hardware timestamps)
  - [ ] Target: <10μs after convergence (software timestamps)

### Day 14: Stress Testing
- [ ] **Test under load**
  - [ ] Run with high network traffic
  - [ ] Verify PTP messages not lost
  - [ ] Measure CPU utilization (target <5%)

- [ ] **Test error conditions**
  - [ ] Master disconnection
  - [ ] Network cable unplug/replug
  - [ ] Multiple masters (BMCA)

### Day 15: Performance Characterization
- [ ] **Measure synchronization accuracy**
  - [ ] Use GPS-disciplined master if available
  - [ ] Log offset over 24 hours
  - [ ] Calculate mean, std dev, max deviation

- [ ] **Measure resource usage**
  - [ ] CPU utilization: ______%
  - [ ] Memory footprint: ______ KB
  - [ ] Network bandwidth: ______ packets/sec

---

## Phase 5: Hardware Timestamping (Days 16-18)

**Note**: Skip this phase if hardware timestamping not available

### Day 16: Enable Hardware Timestamps
- [ ] **Configure Ethernet controller**
  - [ ] Read controller datasheet
  - [ ] Enable PTP timestamping mode
  - [ ] Configure timestamp format

- [ ] **Verify timestamp registers**
  - [ ] Read timestamp after TX
  - [ ] Read timestamp after RX
  - [ ] Verify values are reasonable

### Day 17: Integrate Hardware Timestamps
- [ ] **Update send_packet()**
  - [ ] Capture TX timestamp after transmission
  - [ ] Return timestamp to library

- [ ] **Update receive_packet()**
  - [ ] Extract RX timestamp from controller
  - [ ] Return timestamp to library

### Day 18: Validate Hardware Timestamps
- [ ] **Compare hardware vs software**
  - [ ] Measure both timestamp types
  - [ ] Calculate difference
  - [ ] Verify hardware is more accurate

- [ ] **Measure improvement**
  - Before (software): ______ μs accuracy
  - After (hardware): ______ μs accuracy
  - Improvement factor: ______x

---

## Phase 6: Documentation and Release (Days 19-20)

### Day 19: Documentation
- [ ] **Create platform-specific README**
  - [ ] Platform requirements
  - [ ] Build instructions
  - [ ] Configuration options
  - [ ] Known limitations

- [ ] **Document build process**
  ```markdown
  # Building for [Platform Name]
  
  ## Prerequisites
  - [List required tools]
  - [List required libraries]
  
  ## Build Steps
  1. [Step 1]
  2. [Step 2]
  ...
  ```

- [ ] **Create troubleshooting guide**
  - [ ] Common issues
  - [ ] Solutions
  - [ ] Debug tips

### Day 20: Final Validation
- [ ] **Run full test suite**
  - [ ] All unit tests pass
  - [ ] Integration tests pass
  - [ ] No memory leaks (valgrind/sanitizers)

- [ ] **Performance meets targets**
  - [ ] Sync accuracy: _____ (target: <1μs with HW, <10μs SW)
  - [ ] CPU utilization: _____ (target: <5%)
  - [ ] Memory: _____ (target: <1MB)

- [ ] **Create release package**
  - [ ] Source code
  - [ ] Build scripts
  - [ ] Documentation
  - [ ] Example applications

---

## Success Criteria

Mark complete when ALL criteria met:

- [ ] **Functionality**
  - ✓ All HAL functions implemented
  - ✓ No stub code remaining
  - ✓ All unit tests passing
  - ✓ Integration tests passing

- [ ] **Performance**
  - ✓ Synchronization accuracy: <1μs (hardware) or <10μs (software)
  - ✓ CPU utilization: <5%
  - ✓ Memory footprint: <1MB
  - ✓ Packet latency: <100μs

- [ ] **Quality**
  - ✓ No memory leaks
  - ✓ No compiler warnings
  - ✓ No static analysis issues
  - ✓ Code reviewed

- [ ] **Documentation**
  - ✓ Platform README complete
  - ✓ Build instructions verified
  - ✓ Troubleshooting guide written
  - ✓ API documentation updated

---

## Platform-Specific Notes

### Your Platform: ________________

**Challenges Encountered**:
1. ________________
2. ________________
3. ________________

**Solutions**:
1. ________________
2. ________________
3. ________________

**Performance Results**:
- Sync accuracy: ________________
- CPU usage: ________________
- Memory usage: ________________

**Recommendations**:
- ________________
- ________________
- ________________

---

## Next Steps After Porting

1. **Submit HAL implementation** to project repository
2. **Share results** with community
3. **Maintain** as platform evolves
4. **Contribute improvements** back to library

---

**Congratulations on completing your HAL implementation!** 🎉

Your work enables IEEE 1588-2019 PTP on a new platform, benefiting the entire community.
