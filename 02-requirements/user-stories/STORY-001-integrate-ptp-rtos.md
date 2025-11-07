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

# User Story: STORY-001 - Integrate PTP into RTOS Application

**Story ID**: STORY-001  
**Story Title**: Integrate PTP Stack into Real-Time Operating System Application  
**Author**: Requirements Engineering Team  
**Date**: 2025-11-07  
**Version**: 1.0.0

---

## 1. User Story Statement

**As** an embedded software developer  
**I want** a simple, well-documented API to initialize and integrate the PTP stack into my RTOS application (FreeRTOS, Zephyr, custom RTOS)  
**So that** my device achieves nanosecond-precision time synchronization with minimal integration effort and deterministic real-time behavior

---

## 2. Personas and Context

### Primary Persona: Embedded Software Developer

- **Name**: Alex Chen
- **Role**: Senior Embedded Software Engineer
- **Company**: Industrial automation equipment manufacturer
- **Experience**: 8 years embedded C/C++, 3 years RTOS development
- **Goals**:
  - Integrate IEEE 1588-2019 PTP into FreeRTOS-based motor controller
  - Achieve <1µs synchronization accuracy for distributed motion control
  - Minimize integration time (target: 2 days from download to working sync)
  - Ensure deterministic real-time behavior (no malloc in critical paths)
- **Pain Points**:
  - Previous PTP stacks required weeks of porting effort
  - Poor documentation, unclear HAL requirements
  - Memory leaks and non-deterministic behavior in existing solutions
  - Difficult to debug timing issues

### Secondary Persona: Technical Project Manager

- **Name**: Maria Rodriguez
- **Role**: Engineering Manager overseeing 5-person firmware team
- **Concerns**:
  - Integration risk to project schedule
  - Technical debt from poor abstractions
  - Long-term maintainability and platform portability
- **Success Criteria**:
  - PTP integration completed within sprint (2 weeks)
  - Zero production bugs related to PTP after 6 months deployment
  - Easy migration to future hardware platforms

---

## 3. Acceptance Criteria (Gherkin Format)

```gherkin
Feature: Integrate PTP Stack into RTOS Application
  As an embedded developer
  I want to initialize PTP with minimal code and clear HAL requirements
  So that my device achieves time synchronization with deterministic behavior

  Background:
    Given I have FreeRTOS v10.4 running on ARM Cortex-M7 (STM32H7)
    And I have Ethernet PHY with hardware timestamping support (LAN8742)
    And I have downloaded IEEE 1588-2019 PTP library source code
    And I have read integration documentation in docs/integration-guide.md

  Scenario: Initialize PTP stack successfully on first attempt
    Given I have implemented 4 HAL interface functions:
      | Interface Function          | Purpose                        |
      | hal_network_send_packet()   | Transmit PTP packet            |
      | hal_network_receive_packet()| Receive PTP packet             |
      | hal_timestamp_get()         | Capture current timestamp      |
      | hal_clock_adjust_frequency()| Adjust local clock frequency   |
    When I call ptp_init() with HAL function pointers and configuration:
      """c
      ptp_config_t config = {
          .device_type = PTP_DEVICE_ORDINARY_CLOCK,
          .clock_id = {0x00, 0x11, 0x22, 0xFF, 0xFE, 0x33, 0x44, 0x55},
          .domain_number = 0,
          .priority1 = 128,
          .priority2 = 128,
          .sync_interval = -3  // 8 Hz (2^-3 seconds)
      };
      
      ptp_hal_t hal = {
          .network_send = hal_network_send_packet,
          .network_receive = hal_network_receive_packet,
          .timestamp_get = hal_timestamp_get,
          .clock_adjust = hal_clock_adjust_frequency
      };
      
      ptp_handle_t ptp_handle;
      int result = ptp_init(&ptp_handle, &config, &hal);
      """
    Then ptp_init() shall return 0 (success)
    And PTP stack shall allocate statically (zero malloc/free calls)
    And PTP stack shall be in INITIALIZING state
    And integration shall compile without errors or warnings

  Scenario: Create PTP task in FreeRTOS with correct priority
    Given PTP stack is initialized successfully
    And I have motor control task at priority 5 (high priority)
    And I have network stack task at priority 3 (medium priority)
    When I create PTP task at priority 4 (between network and motion):
      """c
      TaskHandle_t ptp_task_handle;
      BaseType_t result = xTaskCreate(
          ptp_task_function,      // Task function
          "PTP",                  // Task name
          2048,                   // Stack size (words)
          &ptp_handle,            // Task parameter (PTP handle)
          4,                      // Priority (between network and motion)
          &ptp_task_handle        // Task handle output
      );
      """
    Then xTaskCreate() shall return pdPASS
    And PTP task shall run without stack overflow
    And PTP task shall not starve lower-priority tasks
    And PTP task shall yield CPU properly when waiting for packets

  Scenario: Achieve synchronization within 60 seconds
    Given PTP task is running in FreeRTOS
    And Grandmaster is transmitting Sync messages at 8 Hz
    And Network path delay is approximately 50 microseconds
    When 60 seconds elapse after PTP initialization
    Then PTP stack shall transition to SLAVE state
    And BMCA shall select grandmaster successfully (UC-002)
    And Clock offset shall converge to less than 1 microsecond (UC-001, UC-003, UC-004)
    And Offset measurements shall be available via ptp_get_status() API

  Scenario: Verify zero dynamic allocation in steady state
    Given PTP stack has achieved SLAVE state and is synchronized
    When system runs for 1 hour processing 28,800 Sync messages
    Then malloc() shall be called 0 times during steady state
    And free() shall be called 0 times during steady state
    And all packet buffers shall be statically allocated or from pre-allocated pool
    And PTP stack shall use no more than 32 KB RAM total

  Scenario: Measure CPU overhead meets budget
    Given PTP stack is synchronized and processing messages at 8 Hz
    And CPU load measurement task is monitoring execution time
    When CPU overhead is measured over 10 minutes:
      """c
      uint32_t ptp_cpu_us = 0;  // Microseconds spent in PTP task
      uint32_t total_cpu_us = 600000000;  // 10 minutes total
      
      // Measure PTP task execution time using DWT cycle counter
      float ptp_cpu_percent = (float)ptp_cpu_us / (float)total_cpu_us * 100.0;
      """
    Then PTP CPU overhead shall be less than 5% at 8 Hz sync rate
    And Peak PTP execution time shall be less than 500 microseconds per sync interval
    And Average PTP execution time shall be less than 100 microseconds per sync interval

  Scenario: Handle graceful shutdown on task deletion
    Given PTP task is running and synchronized
    When FreeRTOS task deletion is requested:
      """c
      vTaskDelete(ptp_task_handle);
      """
    Then PTP task shall clean up resources before terminating
    And No memory leaks shall occur (verified with heap tracing)
    And No dangling pointers or corrupt data structures
    And System shall remain stable after PTP task deleted

  Scenario: Recover from transient network failure
    Given PTP stack is synchronized with offset < 1 microsecond
    When Ethernet cable is unplugged for 10 seconds
    And Ethernet cable is reconnected
    Then PTP stack shall detect Sync timeout (no Announce for 5 seconds)
    And PTP stack shall enter LISTENING state
    And PTP stack shall re-run BMCA when Announce messages resume
    And PTP stack shall resynchronize within 60 seconds of cable reconnection

  Scenario: Integrate with existing network stack (lwIP)
    Given lwIP network stack is running in FreeRTOS
    And Ethernet driver uses lwIP pbuf for packet buffers
    When I implement HAL using lwIP APIs:
      """c
      int hal_network_send_packet(const uint8_t* packet, size_t length) {
          struct pbuf* p = pbuf_alloc(PBUF_RAW, length, PBUF_RAM);
          memcpy(p->payload, packet, length);
          return ethernet_output(netif, p);  // lwIP function
      }
      """
    Then PTP packets shall be transmitted correctly via lwIP
    And No packet corruption or buffer overflows
    And PTP coexists peacefully with other network protocols (TCP/IP, UDP)
```

---

## 4. Implementation Guidance

### 4.1 HAL Interface Specification

All HAL functions follow this error code convention:

```c
// Success and error codes
#define PTP_HAL_SUCCESS            0
#define PTP_HAL_ERROR_TIMEOUT     -1
#define PTP_HAL_ERROR_NO_RESOURCE -2
#define PTP_HAL_ERROR_INVALID_ARG -3
#define PTP_HAL_ERROR_HARDWARE    -4
```

#### Network HAL Interface

```c
/**
 * @brief Transmit PTP packet via Ethernet
 * 
 * @param packet Pointer to PTP packet data (Ethernet frame)
 * @param length Packet length in bytes (60-1522 typical)
 * @return 0 on success, negative error code on failure
 * 
 * REQUIREMENTS:
 * - Must capture egress timestamp if possible (hardware timestamping)
 * - Must not block indefinitely (timeout <1ms preferred)
 * - Must handle PTP multicast MAC addresses correctly
 * - Thread-safe: may be called from PTP task or ISR context
 * 
 * PERFORMANCE:
 * - WCET: <100 microseconds
 * - No dynamic allocation
 */
int hal_network_send_packet(const uint8_t* packet, size_t length);

/**
 * @brief Receive PTP packet from Ethernet
 * 
 * @param buffer Pointer to buffer for received packet
 * @param length Pointer to variable updated with received length
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return 0 on success, negative error code on timeout/failure
 * 
 * REQUIREMENTS:
 * - Must provide ingress timestamp (hardware or software)
 * - Must filter PTP packets (Ethertype 0x88F7 or UDP ports 319/320)
 * - Buffer size must be at least 1522 bytes (max Ethernet frame)
 * - Thread-safe: called from PTP task only
 * 
 * PERFORMANCE:
 * - WCET: <100 microseconds (excluding wait time)
 * - No dynamic allocation
 */
int hal_network_receive_packet(uint8_t* buffer, size_t* length, uint32_t timeout_ms);
```

#### Timestamp HAL Interface

```c
/**
 * @brief Get current timestamp with nanosecond resolution
 * 
 * @param timestamp_ns Pointer to 64-bit timestamp output (nanoseconds since epoch)
 * @return 0 on success, negative error code on failure
 * 
 * REQUIREMENTS:
 * - Monotonically increasing (no wraparound for at least 1 year)
 * - Nanosecond resolution (64-bit unsigned integer)
 * - Hardware timestamping preferred (±8ns accuracy)
 * - Software timestamping acceptable (±1µs accuracy)
 * - Must align with packet ingress/egress timestamps
 * 
 * PERFORMANCE:
 * - WCET: <10 microseconds
 * - No dynamic allocation
 * - Thread-safe: may be called from any context
 */
int hal_timestamp_get(uint64_t* timestamp_ns);
```

#### Clock Adjustment HAL Interface

```c
/**
 * @brief Adjust local clock frequency
 * 
 * @param freq_adj_ppb Frequency adjustment in parts per billion
 *                     Positive: slow down clock, Negative: speed up clock
 * @return 0 on success, negative error code on failure
 * 
 * REQUIREMENTS:
 * - Adjustment range: at least ±100 ppm (±100,000 ppb)
 * - Resolution: 1 ppb preferred, 10 ppb acceptable
 * - Persistent: adjustment remains until next call
 * - Monotonic: clock must not jump or wrap
 * 
 * EXAMPLE:
 * - freq_adj_ppb = +1000 → slow clock by 1 µs per second
 * - freq_adj_ppb = -1000 → speed up clock by 1 µs per second
 * 
 * PERFORMANCE:
 * - WCET: <50 microseconds
 * - No dynamic allocation
 * - Thread-safe: called from PTP task only
 */
int hal_clock_adjust_frequency(int64_t freq_adj_ppb);
```

### 4.2 FreeRTOS Task Integration

#### PTP Task Function Template

```c
/**
 * @brief PTP protocol processing task
 * 
 * Runs continuously processing PTP packets and updating clock servo.
 * Task priority should be between network stack and time-critical tasks.
 */
void ptp_task_function(void* pvParameters) {
    ptp_handle_t* ptp_handle = (ptp_handle_t*)pvParameters;
    
    // Task loop: process PTP protocol indefinitely
    while (1) {
        // Process incoming PTP packets (blocks with timeout)
        int result = ptp_process_packets(ptp_handle, 100);  // 100ms timeout
        
        // Check for PTP state machine updates
        if (ptp_state_machine_tick(ptp_handle) != 0) {
            // State machine error, log and continue
            LOG_ERROR("PTP state machine error");
        }
        
        // Optional: yield to lower priority tasks if no work
        if (result == PTP_ERROR_TIMEOUT) {
            taskYIELD();  // Allow other tasks to run
        }
    }
}
```

#### Recommended Task Priorities (FreeRTOS)

```
Priority Level | Task Type                  | Example Tasks
---------------|----------------------------|---------------------------
7 (Highest)    | Hard real-time control     | Motor PWM, sensor sampling
6              | Time-critical I/O          | CAN bus, fieldbus
5              | Deterministic application  | Motion control algorithm
4              | PTP synchronization        | PTP task (THIS STORY)
3              | Network stack              | lwIP TCP/IP, Ethernet driver
2              | Application logic          | User interface, logging
1              | Idle and background        | Housekeeping, statistics
0 (Lowest)     | Idle task                  | FreeRTOS idle (power saving)
```

**Rationale**: PTP must run below time-critical control but above general network traffic to ensure timely packet processing without impacting deterministic control loops.

### 4.3 Memory Configuration

#### Static Memory Allocation Example

```c
// PTP stack configuration: all buffers statically allocated
#define PTP_MAX_PACKET_SIZE   1522    // Max Ethernet frame
#define PTP_RX_BUFFER_COUNT   8       // Receive packet buffers
#define PTP_TX_BUFFER_COUNT   4       // Transmit packet buffers

// Static buffer pools (no malloc/free)
static uint8_t ptp_rx_buffers[PTP_RX_BUFFER_COUNT][PTP_MAX_PACKET_SIZE];
static uint8_t ptp_tx_buffers[PTP_TX_BUFFER_COUNT][PTP_MAX_PACKET_SIZE];

// Total PTP stack memory footprint
// - Code: ~24 KB (depends on features enabled)
// - Data: ~8 KB (state machines, statistics)
// - Buffers: ~24 KB (8×1522 + 4×1522)
// Total: ~56 KB ROM, ~32 KB RAM
```

### 4.4 Example Integration Code

#### Complete Integration Example (FreeRTOS + STM32H7 + LAN8742)

```c
#include "ptp/ptp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32h7xx_hal.h"

// HAL implementations (platform-specific)
extern ETH_HandleTypeDef heth;  // STM32 Ethernet HAL handle

int hal_network_send_packet(const uint8_t* packet, size_t length) {
    // Use STM32 HAL to transmit Ethernet frame
    HAL_StatusTypeDef status = HAL_ETH_Transmit(&heth, (uint8_t*)packet, length, 1000);
    return (status == HAL_OK) ? PTP_HAL_SUCCESS : PTP_HAL_ERROR_HARDWARE;
}

int hal_network_receive_packet(uint8_t* buffer, size_t* length, uint32_t timeout_ms) {
    // Use STM32 HAL to receive Ethernet frame (blocking with timeout)
    HAL_StatusTypeDef status = HAL_ETH_ReadData(&heth, buffer, 1522);
    if (status == HAL_OK) {
        *length = HAL_ETH_GetReceivedFrameLength(&heth);
        return PTP_HAL_SUCCESS;
    }
    return (status == HAL_TIMEOUT) ? PTP_HAL_ERROR_TIMEOUT : PTP_HAL_ERROR_HARDWARE;
}

int hal_timestamp_get(uint64_t* timestamp_ns) {
    // Use ARM Cortex-M7 DWT cycle counter (hardware timestamp)
    uint32_t cycles = DWT->CYCCNT;  // 32-bit cycle counter at CPU frequency
    uint64_t ns = (uint64_t)cycles * 1000000000ULL / SystemCoreClock;
    *timestamp_ns = ns;
    return PTP_HAL_SUCCESS;
}

int hal_clock_adjust_frequency(int64_t freq_adj_ppb) {
    // Adjust STM32H7 PTP hardware clock frequency
    // (Simplified: actual implementation uses ETH_PTP registers)
    int32_t addend_adjustment = (int32_t)(freq_adj_ppb / 1000);  // ppb to ppm
    // Write to ETH->PTPTSAR register...
    return PTP_HAL_SUCCESS;
}

// Main application: initialize and start PTP
void app_main(void) {
    // Configure PTP stack
    ptp_config_t config = {
        .device_type = PTP_DEVICE_ORDINARY_CLOCK,
        .clock_id = {0x00, 0x80, 0xE1, 0xFF, 0xFE, 0x12, 0x34, 0x56},  // MAC-based
        .domain_number = 0,
        .priority1 = 128,  // Default priority
        .priority2 = 128,
        .sync_interval = -3  // 8 Hz (2^-3 = 0.125 seconds)
    };
    
    // Set up HAL
    ptp_hal_t hal = {
        .network_send = hal_network_send_packet,
        .network_receive = hal_network_receive_packet,
        .timestamp_get = hal_timestamp_get,
        .clock_adjust = hal_clock_adjust_frequency
    };
    
    // Initialize PTP stack
    ptp_handle_t ptp_handle;
    if (ptp_init(&ptp_handle, &config, &hal) != 0) {
        // Initialization failed
        Error_Handler();
    }
    
    // Create PTP task in FreeRTOS
    TaskHandle_t ptp_task_handle;
    BaseType_t result = xTaskCreate(
        ptp_task_function,     // Task function
        "PTP",                 // Task name for debugging
        2048,                  // Stack size: 2048 words = 8 KB
        &ptp_handle,           // Pass PTP handle to task
        4,                     // Priority (between network and motion control)
        &ptp_task_handle       // Task handle output
    );
    
    if (result != pdPASS) {
        // Task creation failed
        Error_Handler();
    }
    
    // Start FreeRTOS scheduler (other tasks also created here)
    vTaskStartScheduler();
}
```

---

## 5. Definition of Done

- [ ] **Integration compiles**: PTP library builds with RTOS without errors/warnings
- [ ] **HAL implemented**: All 4 HAL functions implemented and tested
- [ ] **Task created**: PTP task running at correct priority in RTOS
- [ ] **Synchronization achieved**: Device achieves SLAVE state, offset <1µs within 60 seconds
- [ ] **Zero malloc**: No dynamic allocation during steady-state operation (verified with heap tracing)
- [ ] **CPU overhead acceptable**: PTP task uses <5% CPU at configured sync rate
- [ ] **Graceful shutdown**: Task deletion does not leak memory or corrupt state
- [ ] **Documentation**: Integration code documented, HAL requirements clear
- [ ] **Code reviewed**: Integration code passes peer review
- [ ] **Tested on hardware**: Integration validated on target RTOS and MCU

---

## 6. Dependencies and Blockers

### Dependencies

- **PTP Library**: IEEE 1588-2019 protocol implementation available
- **RTOS**: FreeRTOS, Zephyr, or custom RTOS with task/thread support
- **Hardware**: MCU with Ethernet MAC/PHY, timestamp capability
- **Toolchain**: ARM GCC or equivalent C compiler with C99 support
- **Documentation**: Integration guide, HAL specification, API reference

### Potential Blockers

- **Missing HAL Docs**: Unclear HAL requirements delay implementation
- **Hardware Limitations**: MCU lacks sufficient RAM or clock adjustment capability
- **RTOS Incompatibility**: PTP library assumes features not available in target RTOS
- **Network Stack Conflicts**: PTP integration interferes with existing network stack (lwIP, uIP)

---

## 7. Traceability Matrix

| Story Element | Requirement ID | Description |
|---------------|----------------|-------------|
| HAL Interface | REQ-F-005 | Hardware abstraction interfaces per REQ-F-005 |
| Platform Independence | REQ-NF-M-001 | RTOS integration without platform-specific code in PTP core |
| Build System | REQ-NF-M-002 | CMake builds PTP library for RTOS targets |
| IEEE 1588-2019 Compliance | REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004 | PTP protocol implemented correctly |
| Synchronization Accuracy | REQ-NF-P-001 | Achieve <1µs offset in integrated system |
| Deterministic Behavior | REQ-NF-P-002 | Zero malloc, bounded WCET |
| Resource Efficiency | REQ-NF-P-003 | <5% CPU, <32KB RAM |
| Documentation | REQ-NF-M-001 | Clear integration guide and HAL specification |

---

## 8. Related Use Cases and Stories

- **UC-001**: Synchronize as Ordinary Clock Slave (high-level synchronization workflow)
- **UC-002**: Select Best Master via BMCA (BMCA algorithm)
- **UC-003**: Measure Clock Offset (offset measurement mechanism)
- **UC-004**: Adjust Clock Frequency (PI servo controller)
- **STORY-002**: Verify Synchronization Accuracy (QA validation of integration)
- **STORY-003**: Port PTP to Custom NIC (hardware vendor porting)

---

## 9. Notes and Comments

### Integration Time Estimate

Based on experience with similar integrations:

- **Experienced developer** (familiar with RTOS, Ethernet): 2 days
  - Day 1: Implement HAL (4 hours), integrate build system (2 hours), create PTP task (2 hours)
  - Day 2: Test, debug, verify synchronization (8 hours)
- **Intermediate developer**: 1 week
- **Junior developer** (learning RTOS/PTP): 2 weeks

### Common Integration Mistakes

1. **Wrong Task Priority**: PTP task priority too low causes packet loss
2. **Insufficient Stack**: PTP task stack overflow crashes system
3. **No Timestamping**: Software timestamps without hardware support degrades accuracy to ~10µs
4. **Blocking HAL**: hal_network_send() blocks indefinitely, starves PTP task
5. **Memory Leaks**: HAL uses malloc() without corresponding free(), exhausts heap

### Hardware Recommendations

| MCU Family | Timestamp Capability | PTP Support | Notes |
|------------|---------------------|-------------|-------|
| STM32H7    | Hardware (±8ns)     | Excellent   | Dedicated PTP hardware in Ethernet MAC |
| STM32F7    | Hardware (±20ns)    | Good        | Basic PTP support |
| STM32F4    | Software (±1µs)     | Fair        | No hardware timestamping |
| Cortex-M7  | DWT cycle counter   | Good        | Generic ARM core with precise timer |
| Cortex-M4  | SysTick (±10µs)     | Fair        | Limited timestamp precision |

---

**Document Control**:
- **Created**: 2025-11-07
- **Last Updated**: 2025-11-07
- **Review Status**: Draft - Pending technical review
- **Approved By**: TBD (Requirements Review Board)
- **Next Review**: 2025-11-14
