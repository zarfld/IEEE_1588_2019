---
specType: requirements
standard: "29148"
phase: "02-requirements"
version: "1.0.0"
author: "Requirements Engineering Team"
date: "2025-11-07"
status: "draft"
traceability:
  stakeholderRequirements:
    - StR-001  # STR-STD-001: IEEE 1588-2019 Protocol Compliance
    - StR-008  # STR-ARCH-001: Hardware Abstraction Layer
    - StR-012  # STR-MAINT-001: Platform Independence
---

# User Story: STORY-003 - Port PTP to Custom NIC

**Story ID**: STORY-003  
**Story Title**: Port PTP Stack to Custom Network Interface with Hardware Timestamping  
**Author**: Requirements Engineering Team  
**Date**: 2025-11-07  
**Version**: 1.0.0

---

## 1. User Story Statement

**As** a hardware vendor or NIC driver developer  
**I want** clear HAL interface contracts, reference implementations, and porting guidance  
**So that** I can port the PTP stack to my custom network interface card (NIC) with hardware timestamping support and achieve production-quality synchronization accuracy

---

## 2. Personas and Context

### Primary Persona: NIC Driver Developer

- **Name**: Kevin Zhao
- **Role**: Senior Embedded Driver Engineer
- **Company**: Semiconductor manufacturer developing automotive Ethernet PHY
- **Experience**: 10 years Ethernet drivers, 4 years hardware timestamping
- **Goals**:
  - Port PTP stack to custom Automotive Ethernet PHY (Broadcom BCM89811)
  - Achieve <100ns hardware timestamp accuracy
  - Pass IEEE 1588-2019 conformance tests
  - Enable customers to integrate PTP with minimal effort
  - Deliver reference driver implementation in SDK
- **Pain Points**:
  - Unclear HAL interface requirements and error handling expectations
  - No reference implementations for similar PHY architectures
  - Difficult to debug timestamp capture issues (PHY register access, interrupt timing)
  - Conformance test suites expensive and complex
- **Success Criteria**:
  - HAL implementation passes all unit tests (provided by PTP library)
  - Achieves <1µs synchronization accuracy (verified per STORY-002)
  - Reference driver code included in SDK release
  - Customer integration guide documented

### Secondary Persona: Technical Product Manager

- **Name**: Lisa Martinez
- **Role**: Product Manager for Ethernet PHY product line
- **Concerns**:
  - Time-to-market for PTP-enabled PHY (target: 6 months development)
  - Competitive differentiation (hardware timestamp accuracy, ease of integration)
  - Customer adoption barriers (complex integration scares customers away)
  - Long-term maintenance burden (PTP stack updates, bug fixes)
- **Success Criteria**:
  - PTP support becomes key selling point (mention in datasheets)
  - Customers successfully integrate within 1 week (vs. 4 weeks for competitors)
  - SDK includes working examples and automated tests

---

## 3. Acceptance Criteria (Gherkin Format)

```gherkin
Feature: Port PTP Stack to Custom NIC with Hardware Timestamping
  As a NIC driver developer
  I want to implement HAL interfaces for my custom NIC
  So that the PTP stack can leverage hardware timestamping for sub-microsecond accuracy

  Background:
    Given I have custom Ethernet PHY: Broadcom BCM89811 (automotive)
    And PHY supports hardware timestamping (ingress/egress timestamps)
    And I have access to:
      | Resource                       | Source                           |
      | PHY datasheet                  | Vendor (Broadcom)                |
      | PHY driver example code        | Vendor SDK                       |
      | PTP HAL interface spec         | docs/hal-specification.md        |
      | Reference HAL implementation   | examples/hal-intel-i210.c        |
      | HAL unit test suite            | tests/test-hal-compliance.c      |

  Scenario: Implement all 4 mandatory HAL interface functions
    Given HAL interface specification defines 4 required functions:
      | Function                     | Purpose                          |
      | hal_network_send_packet()    | Transmit PTP packet + egress TS  |
      | hal_network_receive_packet() | Receive PTP packet + ingress TS  |
      | hal_timestamp_get()          | Read current hardware timestamp  |
      | hal_clock_adjust_frequency() | Adjust PHY clock frequency       |
    When I implement all 4 functions for BCM89811 PHY:
      """c
      // File: hal_bcm89811.c
      
      int hal_network_send_packet(const uint8_t* packet, size_t length) {
          // 1. Write packet to PHY TX FIFO
          bcm89811_write_tx_fifo(packet, length);
          // 2. Trigger transmission
          bcm89811_trigger_tx();
          // 3. Wait for TX complete interrupt (capture egress timestamp)
          bcm89811_wait_tx_complete(1000);  // 1ms timeout
          // 4. Read egress timestamp from PHY register 0x1588_TX_TS
          uint64_t egress_ts = bcm89811_read_timestamp_reg(BCM89811_TX_TS_REG);
          // 5. Store for PTP stack to retrieve later
          hal_store_egress_timestamp(egress_ts);
          return PTP_HAL_SUCCESS;
      }
      
      int hal_network_receive_packet(uint8_t* buffer, size_t* length, uint32_t timeout_ms) {
          // 1. Poll RX FIFO or wait for RX interrupt
          if (bcm89811_rx_fifo_empty()) {
              bcm89811_wait_rx_interrupt(timeout_ms);
          }
          // 2. Read packet from PHY RX FIFO
          *length = bcm89811_read_rx_fifo(buffer, 1522);
          // 3. Read ingress timestamp from PHY register 0x1588_RX_TS
          uint64_t ingress_ts = bcm89811_read_timestamp_reg(BCM89811_RX_TS_REG);
          // 4. Store for PTP stack to retrieve
          hal_store_ingress_timestamp(ingress_ts);
          return PTP_HAL_SUCCESS;
      }
      
      int hal_timestamp_get(uint64_t* timestamp_ns) {
          // Read PHY free-running timestamp counter (48-bit, nanoseconds)
          *timestamp_ns = bcm89811_read_timestamp_reg(BCM89811_FREERUN_TS_REG);
          return PTP_HAL_SUCCESS;
      }
      
      int hal_clock_adjust_frequency(int64_t freq_adj_ppb) {
          // Calculate PHY addend adjustment (PHY-specific formula)
          uint32_t addend = bcm89811_calculate_addend(freq_adj_ppb);
          // Write to PHY clock control register
          bcm89811_write_reg(BCM89811_CLOCK_ADDEND_REG, addend);
          return PTP_HAL_SUCCESS;
      }
      """
    Then all 4 functions shall compile without errors or warnings
    And all functions shall follow HAL interface contracts (return codes, thread safety)

  Scenario: Pass HAL unit tests provided by PTP library
    Given PTP library includes hal-compliance test suite:
      """
      tests/test-hal-compliance.c:
      - test_hal_send_packet_basic()
      - test_hal_send_packet_timeout()
      - test_hal_receive_packet_basic()
      - test_hal_receive_packet_timeout()
      - test_hal_timestamp_monotonic()
      - test_hal_clock_adjust_frequency()
      """
    When I run unit tests with my BCM89811 HAL implementation:
      """bash
      $ cmake -DHAL_IMPLEMENTATION=bcm89811 ..
      $ make hal_unit_tests
      $ ./hal_unit_tests
      """
    Then all HAL unit tests shall pass (0 failures)
    And test report shall show:
      - send_packet: latency <100µs, success rate 100%
      - receive_packet: latency <100µs, timeout handling correct
      - timestamp_get: monotonic, no wraparound for 1 year
      - clock_adjust: frequency adjustment applies correctly

  Scenario: Achieve hardware timestamp accuracy <100ns
    Given I have calibrated BCM89811 PHY timestamp capture path
    And I have reference GPS-disciplined clock for validation
    When I measure timestamp capture accuracy using oscilloscope:
      """
      Setup:
      1. Inject 1PPS signal from GPS into PHY
      2. PHY captures timestamp on 1PPS rising edge
      3. Compare PHY timestamp to GPS reference
      4. Repeat 1000 times, calculate error distribution
      """
    Then P95 timestamp error shall be less than 100 nanoseconds
    And timestamp error shall have no systematic bias (mean ~0ns)
    And this accuracy shall be documented in PHY datasheet

  Scenario: Integrate HAL with PTP stack and achieve synchronization
    Given I have implemented and tested all 4 HAL functions
    When I initialize PTP stack with BCM89811 HAL:
      """c
      ptp_hal_t bcm89811_hal = {
          .network_send = hal_network_send_packet,
          .network_receive = hal_network_receive_packet,
          .timestamp_get = hal_timestamp_get,
          .clock_adjust = hal_clock_adjust_frequency
      };
      
      ptp_handle_t ptp_handle;
      int result = ptp_init(&ptp_handle, &config, &bcm89811_hal);
      """
    And I run PTP stack on hardware with BCM89811 PHY
    And Grandmaster is transmitting Sync messages
    Then PTP stack shall achieve SLAVE state within 60 seconds
    And clock offset shall converge to <1 microsecond (P95)
    And synchronization shall be verified per STORY-002 test procedures

  Scenario: Handle PHY-specific error conditions gracefully
    Given my HAL implementation may encounter PHY-specific errors:
      | Error Condition              | Expected HAL Behavior             |
      | TX FIFO full                 | Return PTP_HAL_ERROR_NO_RESOURCE  |
      | RX timeout (no packet)       | Return PTP_HAL_ERROR_TIMEOUT      |
      | PHY register access failure  | Return PTP_HAL_ERROR_HARDWARE     |
      | Timestamp counter wraparound | Handle 48-bit rollover correctly  |
    When these error conditions occur
    Then HAL shall return appropriate error codes (not crash)
    And PTP stack shall handle errors gracefully (log, retry, degrade)
    And system shall remain stable (no memory corruption, hangs)

  Scenario: Create reference driver example in vendor SDK
    Given HAL implementation is complete and tested
    When I create reference driver package for SDK:
      """
      sdk/ptp-examples/bcm89811-reference-driver/
      ├── hal_bcm89811.c              # HAL implementation
      ├── hal_bcm89811.h              # HAL header
      ├── example_ptp_slave.c         # Complete working example
      ├── Makefile                    # Build system
      ├── README.md                   # Integration guide
      └── tests/
          └── test_hal_bcm89811.c     # HAL-specific unit tests
      """
    Then reference driver shall include:
      - Complete HAL implementation (all 4 functions)
      - Working example application (PTP slave)
      - Build instructions (Makefile or CMake)
      - Integration guide (step-by-step for customers)
      - Unit tests (HAL validation)
    And README.md shall document:
      - Hardware requirements (BCM89811 PHY, development board)
      - Build and run instructions
      - Expected output (log showing synchronization)
      - Troubleshooting tips (common issues)

  Scenario: Pass IEEE 1588-2019 conformance tests
    Given IEEE conformance test suite is available (AVnu Milan or UNH-IOL)
    When I run conformance tests on BCM89811 PTP implementation:
      """
      Conformance Test Categories:
      1. Protocol Compliance (message formats, state machines)
      2. Timing Accuracy (offset <1µs requirement)
      3. Interoperability (with multiple grandmaster vendors)
      4. Robustness (network failures, packet loss)
      """
    Then all mandatory conformance tests shall pass
    And conformance report shall be generated for certification
    And optional tests should pass (best effort)
```

---

## 4. HAL Implementation Guidance

### 4.1 Hardware Timestamping Architecture

#### Timestamp Capture Points

```
Ethernet Frame Flow with Timestamp Capture:

   Application Layer
        |
        v
   [PTP Stack]  ◄── hal_timestamp_get() reads free-running counter
        |
        v
   [HAL Layer]  ◄── hal_network_send/receive()
        |
        v
   ┌───────────────────────────────┐
   │       PHY Hardware            │
   │                               │
   │  TX Path:                     │
   │  [TX FIFO] → [MAC] → [PHY]    │
   │              ▲                │
   │              │ Egress         │
   │          [Timestamp           │
   │           Capture]            │
   │         (Register)            │
   │                               │
   │  RX Path:                     │
   │  [PHY] → [MAC] → [RX FIFO]    │
   │      ▲                        │
   │      │ Ingress                │
   │  [Timestamp                   │
   │   Capture]                    │
   │ (Register)                    │
   └───────────────────────────────┘
```

**Key Points**:
1. **Ingress Timestamp**: Captured when Start of Frame Delimiter (SFD) arrives at PHY
2. **Egress Timestamp**: Captured when SFD leaves PHY transmitter
3. **Free-Running Counter**: Hardware counter incremented at 1 GHz (1ns resolution)
4. **Timestamp Registers**: PHY-specific registers store captured timestamps

#### Example PHY Register Map (Generic)

| Register Address | Name            | Purpose                          | Access |
|-----------------|-----------------|----------------------------------|--------|
| 0x1588_CTRL     | Control         | Enable/disable timestamping      | RW     |
| 0x1588_FREERUN  | Free-Run Counter| Current timestamp (48-bit)       | RO     |
| 0x1588_TX_TS    | TX Timestamp    | Egress timestamp (last packet)   | RO     |
| 0x1588_RX_TS    | RX Timestamp    | Ingress timestamp (last packet)  | RO     |
| 0x1588_ADDEND   | Clock Addend    | Frequency adjustment value       | RW     |

### 4.2 Reference Implementation: Intel i210 NIC

#### Intel i210 HAL Implementation (Simplified)

```c
/**
 * @file hal_intel_i210.c
 * @brief Reference HAL implementation for Intel i210 NIC
 * 
 * Intel i210 Features:
 * - Hardware timestamping support (IEEE 1588-2019)
 * - 96-bit timestamp counter (32.32.32 format)
 * - Timestamp accuracy: ±8ns typical
 */

#include <stdint.h>
#include <string.h>
#include "i210_registers.h"  // Intel i210 register definitions

// Intel i210 specific registers (base address varies)
#define I210_REG_SYSTIML   0xB600   // System Time Low (nanoseconds)
#define I210_REG_SYSTIMH   0xB604   // System Time High (seconds)
#define I210_REG_TXSTMPL   0x0420   // TX Timestamp Low
#define I210_REG_TXSTMPH   0x0424   // TX Timestamp High
#define I210_REG_RXSTMPL   0x0430   // RX Timestamp Low
#define I210_REG_RXSTMPH   0x0434   // RX Timestamp High
#define I210_REG_TIMINCA   0xB608   // Time Increment Adjustment

// HAL context for storing timestamps and state
typedef struct {
    uint64_t last_egress_ts_ns;
    uint64_t last_ingress_ts_ns;
    void* nic_base_addr;  // Memory-mapped I/O base address
} hal_i210_context_t;

static hal_i210_context_t g_hal_context;

// Low-level register access helpers
static inline void i210_write_reg(uint32_t reg_offset, uint32_t value) {
    volatile uint32_t* reg = (volatile uint32_t*)(g_hal_context.nic_base_addr + reg_offset);
    *reg = value;
}

static inline uint32_t i210_read_reg(uint32_t reg_offset) {
    volatile uint32_t* reg = (volatile uint32_t*)(g_hal_context.nic_base_addr + reg_offset);
    return *reg;
}

/**
 * @brief Transmit packet and capture egress timestamp
 */
int hal_network_send_packet(const uint8_t* packet, size_t length) {
    if (!packet || length > 1522) {
        return PTP_HAL_ERROR_INVALID_ARG;
    }
    
    // Step 1: Write packet to Intel i210 TX descriptor ring
    // (Simplified: actual implementation uses DMA descriptors)
    if (i210_tx_queue_full()) {
        return PTP_HAL_ERROR_NO_RESOURCE;
    }
    
    i210_write_tx_descriptor(packet, length);
    
    // Step 2: Trigger transmission (write to TX tail register)
    i210_kick_tx_queue();
    
    // Step 3: Wait for TX complete interrupt (timeout 1ms)
    if (!i210_wait_tx_complete(1000)) {
        return PTP_HAL_ERROR_TIMEOUT;
    }
    
    // Step 4: Read egress timestamp from i210 registers
    uint32_t ts_low = i210_read_reg(I210_REG_TXSTMPL);
    uint32_t ts_high = i210_read_reg(I210_REG_TXSTMPH);
    uint64_t egress_ts_ns = ((uint64_t)ts_high << 32) | ts_low;
    
    // Step 5: Store timestamp for PTP stack to retrieve
    g_hal_context.last_egress_ts_ns = egress_ts_ns;
    
    return PTP_HAL_SUCCESS;
}

/**
 * @brief Receive packet and capture ingress timestamp
 */
int hal_network_receive_packet(uint8_t* buffer, size_t* length, uint32_t timeout_ms) {
    if (!buffer || !length) {
        return PTP_HAL_ERROR_INVALID_ARG;
    }
    
    // Step 1: Poll RX descriptor ring or wait for interrupt
    if (i210_rx_queue_empty()) {
        if (!i210_wait_rx_interrupt(timeout_ms)) {
            return PTP_HAL_ERROR_TIMEOUT;
        }
    }
    
    // Step 2: Read packet from i210 RX descriptor
    *length = i210_read_rx_descriptor(buffer, 1522);
    
    // Step 3: Read ingress timestamp from i210 registers
    uint32_t ts_low = i210_read_reg(I210_REG_RXSTMPL);
    uint32_t ts_high = i210_read_reg(I210_REG_RXSTMPH);
    uint64_t ingress_ts_ns = ((uint64_t)ts_high << 32) | ts_low;
    
    // Step 4: Store timestamp
    g_hal_context.last_ingress_ts_ns = ingress_ts_ns;
    
    return PTP_HAL_SUCCESS;
}

/**
 * @brief Read current hardware timestamp
 */
int hal_timestamp_get(uint64_t* timestamp_ns) {
    if (!timestamp_ns) {
        return PTP_HAL_ERROR_INVALID_ARG;
    }
    
    // Read i210 system time registers (atomic read)
    uint32_t ts_low = i210_read_reg(I210_REG_SYSTIML);
    uint32_t ts_high = i210_read_reg(I210_REG_SYSTIMH);
    
    *timestamp_ns = ((uint64_t)ts_high << 32) | ts_low;
    
    return PTP_HAL_SUCCESS;
}

/**
 * @brief Adjust clock frequency
 * 
 * Intel i210 uses "time increment adjustment" method:
 * - Base increment: 1.000000000 (1 ns per cycle at 1 GHz)
 * - Adjusted increment: 1.000000000 + adjustment
 * - Adjustment range: ±100 ppm typical
 */
int hal_clock_adjust_frequency(int64_t freq_adj_ppb) {
    // i210 TIMINCA register format:
    // Bits [31:8]: Increment (24-bit, 1ns resolution)
    // Bits [7:0]: Fractional increment (8-bit, sub-ns)
    
    // Calculate adjustment (simplified, actual formula more complex)
    // Base: 1.000000000 ns = 0x01000000 (24.8 fixed-point)
    uint32_t base_incr = 0x01000000;
    
    // Adjust by ppb (parts per billion)
    // adjustment = base * (freq_adj_ppb / 1e9)
    int32_t delta = (int32_t)((int64_t)base_incr * freq_adj_ppb / 1000000000LL);
    uint32_t adjusted_incr = (uint32_t)((int32_t)base_incr + delta);
    
    // Write to i210 time increment register
    i210_write_reg(I210_REG_TIMINCA, adjusted_incr);
    
    return PTP_HAL_SUCCESS;
}
```

### 4.3 Common PHY Architectures and Porting Notes

| PHY Vendor | Example Part | Timestamp Mechanism | Porting Difficulty |
|------------|--------------|---------------------|-------------------|
| Intel      | i210, i225   | DMA descriptor + registers | Easy (reference available) |
| Broadcom   | BCM89811     | Registers + interrupts | Medium (vendor SDK required) |
| Marvell    | 88E6352      | Timestamping unit (TSU) | Medium (complex register map) |
| Microchip  | LAN9355      | FIFO-based timestamps | Hard (limited documentation) |
| TI         | DP83640      | SPI-accessible registers | Medium (SPI overhead) |

---

## 5. Testing and Validation

### 5.1 HAL Unit Test Suite

#### Test Cases Provided by PTP Library

```c
/**
 * @file tests/test-hal-compliance.c
 * @brief HAL compliance test suite
 * 
 * Validates HAL implementation meets interface contracts
 */

#include "unity.h"  // Unity test framework
#include "ptp/hal.h"

// Test: Verify hal_network_send_packet() basic functionality
void test_hal_send_packet_basic(void) {
    uint8_t test_packet[128];
    memset(test_packet, 0xAA, sizeof(test_packet));
    
    int result = hal_network_send_packet(test_packet, sizeof(test_packet));
    
    TEST_ASSERT_EQUAL_INT(PTP_HAL_SUCCESS, result);
    // Additional assertions: verify packet actually transmitted
}

// Test: Verify hal_network_send_packet() handles timeout correctly
void test_hal_send_packet_timeout(void) {
    uint8_t huge_packet[10000];  // Exceeds MTU
    
    int result = hal_network_send_packet(huge_packet, sizeof(huge_packet));
    
    TEST_ASSERT_NOT_EQUAL(PTP_HAL_SUCCESS, result);
    TEST_ASSERT_TRUE(result == PTP_HAL_ERROR_INVALID_ARG || 
                     result == PTP_HAL_ERROR_NO_RESOURCE);
}

// Test: Verify hal_timestamp_get() is monotonic
void test_hal_timestamp_monotonic(void) {
    uint64_t ts1, ts2, ts3;
    
    hal_timestamp_get(&ts1);
    usleep(1000);  // Wait 1ms
    hal_timestamp_get(&ts2);
    usleep(1000);
    hal_timestamp_get(&ts3);
    
    TEST_ASSERT_TRUE(ts2 > ts1);  // Timestamps increasing
    TEST_ASSERT_TRUE(ts3 > ts2);
    TEST_ASSERT_TRUE((ts2 - ts1) > 900000);  // ~1ms = 1,000,000 ns (allow jitter)
}

// Test: Verify hal_clock_adjust_frequency() applies correctly
void test_hal_clock_adjust_frequency(void) {
    uint64_t ts1, ts2;
    
    // Baseline: no adjustment
    hal_clock_adjust_frequency(0);
    hal_timestamp_get(&ts1);
    sleep(1);  // Wait 1 second
    hal_timestamp_get(&ts2);
    uint64_t baseline_delta = ts2 - ts1;
    
    // Adjust clock +1000 ppb (slow down by 1 µs per second)
    hal_clock_adjust_frequency(+1000);
    hal_timestamp_get(&ts1);
    sleep(1);
    hal_timestamp_get(&ts2);
    uint64_t adjusted_delta = ts2 - ts1;
    
    // Adjusted delta should be ~1µs less than baseline
    int64_t delta_diff = (int64_t)baseline_delta - (int64_t)adjusted_delta;
    TEST_ASSERT_TRUE(delta_diff > 500 && delta_diff < 1500);  // ~1000 ns (allow tolerance)
}

// Run all tests
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hal_send_packet_basic);
    RUN_TEST(test_hal_send_packet_timeout);
    RUN_TEST(test_hal_timestamp_monotonic);
    RUN_TEST(test_hal_clock_adjust_frequency);
    return UNITY_END();
}
```

### 5.2 Hardware-in-the-Loop (HIL) Testing

#### Test Setup

```
┌─────────────┐      ┌─────────────┐      ┌─────────────┐
│ Grandmaster │◄────►│   Switch    │◄────►│     DUT     │
│(GPS-locked) │      │             │      │(Custom NIC) │
└─────────────┘      └─────────────┘      └─────────────┘
       |                                          |
       └────────────────┬─────────────────────────┘
                        |
                  ┌─────▼──────┐
                  │Oscilloscope│ (1PPS validation)
                  └────────────┘
```

**HIL Test Cases**:
1. **Cold Start Sync**: Power on DUT, verify sync within 60s
2. **Master Failover**: Disconnect grandmaster, verify BMCA selects backup
3. **Packet Loss**: Inject 10% packet loss, verify graceful degradation
4. **1PPS Validation**: Compare DUT 1PPS output to GPS reference (oscilloscope)

---

## 6. Definition of Done

- [ ] **HAL implementation complete**: All 4 functions implemented for custom NIC
- [ ] **Unit tests pass**: PTP library HAL compliance tests pass (0 failures)
- [ ] **Hardware timestamp accuracy verified**: <100ns P95 error
- [ ] **Integration tested**: PTP stack achieves synchronization with custom NIC
- [ ] **Error handling validated**: All PHY error conditions handled gracefully
- [ ] **Reference driver created**: Complete example in vendor SDK
- [ ] **Documentation written**: Integration guide, HAL implementation notes
- [ ] **Conformance tests passed**: IEEE 1588-2019 mandatory tests pass
- [ ] **Code reviewed**: HAL implementation peer-reviewed
- [ ] **Performance validated**: Synchronization accuracy <1µs per STORY-002

---

## 7. Dependencies and Blockers

### Dependencies

- **PHY Datasheet**: Hardware timestamp register documentation
- **Vendor SDK**: Driver code examples and build system
- **Development Board**: Hardware with custom NIC for testing
- **Test Equipment**: GPS-disciplined grandmaster, oscilloscope

### Potential Blockers

- **PHY Documentation Incomplete**: Timestamp capture mechanism undocumented
- **Hardware Bugs**: PHY timestamp capture broken (silicon errata)
- **Register Access Latency**: SPI/MDIO access too slow for real-time requirements
- **Interrupt Latency**: OS interrupt handling delays timestamp capture

---

## 8. Traceability Matrix

| Story Element | Requirement ID | Description |
|---------------|----------------|-------------|
| HAL Implementation | REQ-F-005 | Hardware abstraction interfaces |
| Platform Independence | REQ-NF-M-001 | Portable across different NICs |
| Timestamp Accuracy | REQ-NF-P-001 | <100ns hardware timestamp, <1µs sync accuracy |
| Error Handling | REQ-NF-S-002 | Graceful handling of PHY errors |
| IEEE 1588-2019 Compliance | REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004 | Protocol correctness |

---

## 9. Related Use Cases and Stories

- **UC-001**: Synchronize as Ordinary Clock Slave (uses HAL for packet I/O)
- **UC-003**: Measure Clock Offset (uses HAL for timestamping)
- **UC-004**: Adjust Clock Frequency (uses HAL for clock adjustment)
- **STORY-001**: Integrate PTP into RTOS Application (customer integration scenario)
- **STORY-002**: Verify Synchronization Accuracy (validation after porting)

---

## 10. Notes and Comments

### Timestamp Capture Best Practices

1. **Capture Point**: Timestamp at PHY (closest to physical layer)
2. **Atomic Read**: Use hardware support to read multi-byte timestamps atomically
3. **Interrupt Latency**: Minimize ISR latency for timestamp capture (<10µs)
4. **Wraparound Handling**: Handle 32-bit or 48-bit counter rollover correctly

### Common Porting Mistakes

1. **Wrong Capture Point**: Timestamp at MAC instead of PHY (adds jitter)
2. **Software Timestamp Fallback**: Degraded accuracy without hardware support
3. **Register Access Blocking**: HAL functions block indefinitely (starve PTP task)
4. **Incorrect Frequency Adjustment**: Wrong formula for addend calculation
5. **Memory Corruption**: Buffer overflows in packet handling

### PHY Vendor Support

| Vendor    | SDK Quality | Documentation | Support Responsiveness |
|-----------|-------------|---------------|------------------------|
| Intel     | Excellent   | Excellent     | Good                   |
| Broadcom  | Good        | Fair          | Slow                   |
| Marvell   | Fair        | Fair          | Medium                 |
| Microchip | Good        | Good          | Good                   |
| TI        | Excellent   | Excellent     | Excellent              |

---

**Document Control**:
- **Created**: 2025-11-07
- **Last Updated**: 2025-11-07
- **Review Status**: Draft - Pending technical review
- **Approved By**: TBD (Requirements Review Board)
- **Next Review**: 2025-11-14
