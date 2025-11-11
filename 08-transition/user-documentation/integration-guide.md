# IEEE 1588-2019 PTP Library - Integration Guide

**Version**: 1.0.0-MVP  
**Date**: 2025-11-11  
**Audience**: Software Engineers integrating PTP library into embedded systems  
**Compliance**: IEEE 1588-2019 Precision Time Protocol v2.1

---

## Table of Contents

1. [Integration Overview](#1-integration-overview)
2. [Build System Integration](#2-build-system-integration)
3. [Hardware Abstraction Layer (HAL) Implementation](#3-hardware-abstraction-layer-hal-implementation)
4. [Platform-Specific Considerations](#4-platform-specific-considerations)
5. [Threading and Concurrency](#5-threading-and-concurrency)
6. [Network Integration](#6-network-integration)
7. [Timing and Timestamping](#7-timing-and-timestamping)
8. [Configuration and Tuning](#8-configuration-and-tuning)
9. [Testing Your Integration](#9-testing-your-integration)
10. [Common Integration Patterns](#10-common-integration-patterns)

---

## 1. Integration Overview

### 1.1 Integration Architecture

```
┌──────────────────────────────────────────────────────┐
│                  Your Application                     │
│  (Industrial Controller, Audio Device, Telecom, etc.)│
└────────────────┬─────────────────────────────────────┘
                 │ Application API
┌────────────────▼─────────────────────────────────────┐
│         IEEE 1588-2019 PTP Library                    │
│  ┌────────────┐  ┌────────────┐  ┌────────────────┐  │
│  │   Clocks   │  │  Messages  │  │      BMCA      │  │
│  │ State      │  │  Parsing/  │  │   Algorithm    │  │
│  │ Machines   │  │  Serialize │  │                │  │
│  └────────────┘  └────────────┘  └────────────────┘  │
└────────────────┬─────────────────────────────────────┘
                 │ HAL Interface (Function Pointers)
┌────────────────▼─────────────────────────────────────┐
│            Your HAL Implementation                    │
│  ┌────────────┐  ┌────────────┐  ┌────────────────┐  │
│  │  Network   │  │  Timestamp │  │  Clock Adjust  │  │
│  │   Tx/Rx    │  │  Capture   │  │  (Servo Loop)  │  │
│  └────────────┘  └────────────┘  └────────────────┘  │
└────────────────┬─────────────────────────────────────┘
                 │ Hardware/OS API
┌────────────────▼─────────────────────────────────────┐
│     Hardware / Operating System                       │
│  (Ethernet NIC, Timestamp Unit, System Clock, etc.)   │
└──────────────────────────────────────────────────────┘
```

### 1.2 Integration Steps

1. **Add library to build system** (CMake, Make, etc.)
2. **Implement HAL callbacks** (network, timestamping, clock control)
3. **Configure PTP port** (domain, intervals, priority)
4. **Initialize and start clock** (OrdinaryClock, BoundaryClock, etc.)
5. **Integrate into main loop** (tick() and message processing)
6. **Test with PTP master/slave** (verify synchronization)

### 1.3 Prerequisites

**Required**:
- C++17 or later compiler
- CMake 3.15 or later
- Network interface with PTP support (hardware timestamping recommended)
- Real-time or time-sensitive operating system (RTOS, Linux RT, etc.)

**Recommended**:
- Hardware timestamping support (sub-microsecond accuracy)
- IEEE 1588 capable Ethernet PHY/NIC
- GPS or atomic clock reference (for grandmaster)

---

## 2. Build System Integration

### 2.1 CMake Integration (find_package)

**Step 1**: Install IEEE 1588-2019 library (after building):

```bash
# Clone repository
git clone https://github.com/[organization]/IEEE_1588_2019.git
cd IEEE_1588_2019

# Build and install
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .  # Installs to /usr/local by default
```

**Step 2**: Use in your project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyPTPApplication)

# Find installed IEEE 1588-2019 library
find_package(IEEE1588_2019 REQUIRED)

# Add your application
add_executable(my_ptp_app
    main.cpp
    hal_implementation.cpp
    network_interface.cpp
)

# Link PTP library
target_link_libraries(my_ptp_app
    PRIVATE
        IEEE1588_2019::PTP
)

# Include directories (automatically added by target_link_libraries)
# No need for manual include_directories()

# Set C++17 standard
target_compile_features(my_ptp_app PRIVATE cxx_std_17)
```

### 2.2 CMake Integration (FetchContent)

For header-only or source-integrated builds:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyPTPApplication)

# Fetch IEEE 1588-2019 library directly from GitHub
include(FetchContent)

FetchContent_Declare(
    IEEE1588_2019
    GIT_REPOSITORY https://github.com/[organization]/IEEE_1588_2019.git
    GIT_TAG v1.0.0-MVP  # Use specific version tag
)

# Make library available
FetchContent_MakeAvailable(IEEE1588_2019)

# Add your application
add_executable(my_ptp_app
    main.cpp
    hal_implementation.cpp
)

# Link PTP library (automatically downloads and builds)
target_link_libraries(my_ptp_app
    PRIVATE
        IEEE1588_2019::PTP
)
```

### 2.3 CMake Configuration Options

Available CMake options for customizing the build:

```cmake
# Enable/disable optional features
option(IEEE1588_ENABLE_LOGGING "Enable debug logging" ON)
option(IEEE1588_ENABLE_METRICS "Enable performance metrics" ON)
option(IEEE1588_ENABLE_HEALTH "Enable health monitoring" ON)
option(IEEE1588_BUILD_TESTS "Build unit tests" OFF)
option(IEEE1588_BUILD_EXAMPLES "Build example applications" OFF)

# Pass options when configuring
cmake -DIEEE1588_ENABLE_LOGGING=ON -DIEEE1588_BUILD_EXAMPLES=ON ..
```

### 2.4 Manual Integration (Without CMake)

For custom build systems (Make, SCons, etc.):

```makefile
# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

# Include paths
INCLUDES = -I/path/to/IEEE_1588_2019/include

# Library paths (if using pre-built library)
LDFLAGS = -L/path/to/IEEE_1588_2019/lib
LIBS = -lieee1588_2019_ptp

# Your application sources
SOURCES = main.cpp hal_implementation.cpp network_interface.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = my_ptp_app

# Build rules
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
```

**Note**: Header-only usage is also supported - just include headers directly without linking.

---

## 3. Hardware Abstraction Layer (HAL) Implementation

### 3.1 HAL Interface Overview

The PTP library requires implementation of `StateCallbacks` struct:

```cpp
struct StateCallbacks {
    // Message transmission (non-blocking)
    PTPError (*send_announce)(const AnnounceMessage& msg);
    PTPError (*send_sync)(const SyncMessage& msg);
    PTPError (*send_follow_up)(const FollowUpMessage& msg);
    PTPError (*send_delay_req)(const DelayReqMessage& msg);
    PTPError (*send_delay_resp)(const DelayRespMessage& msg);
    
    // Timestamping (deterministic)
    Timestamp (*get_timestamp)();
    PTPError (*get_tx_timestamp)(uint16_t sequence_id, Timestamp* timestamp);
    
    // Clock control (bounded execution)
    PTPError (*adjust_clock)(int64_t adjustment_ns);
    PTPError (*adjust_frequency)(double ppb_adjustment);
    
    // Event notifications
    void (*on_state_change)(PortState old_state, PortState new_state);
    void (*on_fault)(const char* fault_description);
};
```

### 3.2 Network Transmission Implementation

**Example for Linux raw sockets**:

```cpp
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <unistd.h>

// Global socket for PTP messages
static int g_ptp_socket = -1;
static struct sockaddr_ll g_dest_addr;

// Initialize network interface
int init_network_interface(const char* interface_name) {
    // Create raw socket for PTP (Ethertype 0x88F7)
    g_ptp_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_PTP));
    if (g_ptp_socket < 0) {
        return -1;
    }
    
    // Get interface index
    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ-1);
    if (ioctl(g_ptp_socket, SIOCGIFINDEX, &ifr) < 0) {
        close(g_ptp_socket);
        return -1;
    }
    
    // Setup destination address
    memset(&g_dest_addr, 0, sizeof(g_dest_addr));
    g_dest_addr.sll_family = AF_PACKET;
    g_dest_addr.sll_ifindex = ifr.ifr_ifindex;
    g_dest_addr.sll_halen = ETH_ALEN;
    
    // PTP multicast MAC: 01:1B:19:00:00:00
    g_dest_addr.sll_addr[0] = 0x01;
    g_dest_addr.sll_addr[1] = 0x1B;
    g_dest_addr.sll_addr[2] = 0x19;
    g_dest_addr.sll_addr[3] = 0x00;
    g_dest_addr.sll_addr[4] = 0x00;
    g_dest_addr.sll_addr[5] = 0x00;
    
    return 0;
}

// Send Sync message implementation
static PTPError hal_send_sync(const SyncMessage& msg) {
    // Serialize message to buffer
    uint8_t buffer[128];
    auto result = msg.serialize(buffer, sizeof(buffer));
    if (!result.is_success()) {
        return PTPError::SERIALIZATION_FAILED;
    }
    
    size_t msg_size = result.unwrap();
    
    // Send via raw socket
    ssize_t sent = sendto(g_ptp_socket, buffer, msg_size, 0,
                         (struct sockaddr*)&g_dest_addr, 
                         sizeof(g_dest_addr));
    if (sent < 0) {
        perror("sendto");
        return PTPError::NETWORK_ERROR;
    }
    
    return PTPError::SUCCESS;
}

// Similar implementations for other message types
static PTPError hal_send_announce(const AnnounceMessage& msg) {
    uint8_t buffer[256];
    auto result = msg.serialize(buffer, sizeof(buffer));
    if (!result.is_success()) return PTPError::SERIALIZATION_FAILED;
    
    ssize_t sent = sendto(g_ptp_socket, buffer, result.unwrap(), 0,
                         (struct sockaddr*)&g_dest_addr, sizeof(g_dest_addr));
    return (sent < 0) ? PTPError::NETWORK_ERROR : PTPError::SUCCESS;
}

static PTPError hal_send_follow_up(const FollowUpMessage& msg) {
    uint8_t buffer[128];
    auto result = msg.serialize(buffer, sizeof(buffer));
    if (!result.is_success()) return PTPError::SERIALIZATION_FAILED;
    
    ssize_t sent = sendto(g_ptp_socket, buffer, result.unwrap(), 0,
                         (struct sockaddr*)&g_dest_addr, sizeof(g_dest_addr));
    return (sent < 0) ? PTPError::NETWORK_ERROR : PTPError::SUCCESS;
}

static PTPError hal_send_delay_req(const DelayReqMessage& msg) {
    uint8_t buffer[128];
    auto result = msg.serialize(buffer, sizeof(buffer));
    if (!result.is_success()) return PTPError::SERIALIZATION_FAILED;
    
    ssize_t sent = sendto(g_ptp_socket, buffer, result.unwrap(), 0,
                         (struct sockaddr*)&g_dest_addr, sizeof(g_dest_addr));
    return (sent < 0) ? PTPError::NETWORK_ERROR : PTPError::SUCCESS;
}

static PTPError hal_send_delay_resp(const DelayRespMessage& msg) {
    uint8_t buffer[128];
    auto result = msg.serialize(buffer, sizeof(buffer));
    if (!result.is_success()) return PTPError::SERIALIZATION_FAILED;
    
    ssize_t sent = sendto(g_ptp_socket, buffer, result.unwrap(), 0,
                         (struct sockaddr*)&g_dest_addr, sizeof(g_dest_addr));
    return (sent < 0) ? PTPError::NETWORK_ERROR : PTPError::SUCCESS;
}
```

**Example for embedded RTOS (FreeRTOS + lwIP)**:

```cpp
#include "lwip/sockets.h"
#include "lwip/udp.h"

// UDP socket for PTP event messages
static struct udp_pcb* g_event_pcb = nullptr;
static struct udp_pcb* g_general_pcb = nullptr;

// Initialize network for embedded system
int init_embedded_network() {
    // Create UDP PCBs for PTP
    g_event_pcb = udp_new();
    g_general_pcb = udp_new();
    
    if (!g_event_pcb || !g_general_pcb) {
        return -1;
    }
    
    // Bind to PTP ports (319 for event, 320 for general)
    ip_addr_t any_addr = IPADDR4_INIT(IPADDR_ANY);
    udp_bind(g_event_pcb, &any_addr, 319);
    udp_bind(g_general_pcb, &any_addr, 320);
    
    // Join PTP multicast group (224.0.1.129)
    ip4_addr_t multicast_addr;
    IP4_ADDR(&multicast_addr, 224, 0, 1, 129);
    igmp_joingroup_netif(netif_default, &multicast_addr);
    
    return 0;
}

static PTPError hal_send_sync(const SyncMessage& msg) {
    uint8_t buffer[128];
    auto result = msg.serialize(buffer, sizeof(buffer));
    if (!result.is_success()) return PTPError::SERIALIZATION_FAILED;
    
    // Send via lwIP UDP
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, result.unwrap(), PBUF_RAM);
    if (!p) return PTPError::RESOURCE_UNAVAILABLE;
    
    memcpy(p->payload, buffer, result.unwrap());
    
    ip_addr_t dest_addr = IPADDR4_INIT_BYTES(224, 0, 1, 129);
    err_t err = udp_sendto(g_event_pcb, p, &dest_addr, 319);
    pbuf_free(p);
    
    return (err == ERR_OK) ? PTPError::SUCCESS : PTPError::NETWORK_ERROR;
}
```

### 3.3 Timestamping Implementation

**Hardware timestamping (Linux PTP Hardware Clock)**:

```cpp
#include <linux/ptp_clock.h>
#include <sys/ioctl.h>
#include <fcntl.h>

static int g_ptp_clock_fd = -1;

// Initialize PTP hardware clock
int init_ptp_hardware_clock(const char* ptp_device) {
    g_ptp_clock_fd = open(ptp_device, O_RDWR);
    if (g_ptp_clock_fd < 0) {
        perror("open ptp device");
        return -1;
    }
    
    // Enable hardware timestamping on network interface
    struct hwtstamp_config hwts_config;
    hwts_config.flags = 0;
    hwts_config.tx_type = HWTSTAMP_TX_ON;
    hwts_config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
    
    struct ifreq ifr;
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
    ifr.ifr_data = (char*)&hwts_config;
    
    if (ioctl(g_ptp_socket, SIOCSHWTSTAMP, &ifr) < 0) {
        perror("SIOCSHWTSTAMP");
        return -1;
    }
    
    return 0;
}

// Get current timestamp from hardware
static Timestamp hal_get_timestamp() {
    struct ptp_clock_time ptp_time;
    
    if (ioctl(g_ptp_clock_fd, PTP_CLOCK_GETTIME, &ptp_time) < 0) {
        // Fallback to system time
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return Timestamp(ts.tv_sec, ts.tv_nsec);
    }
    
    return Timestamp(ptp_time.sec, ptp_time.nsec);
}

// Get TX timestamp for specific message
static PTPError hal_get_tx_timestamp(uint16_t sequence_id, Timestamp* timestamp) {
    // Read TX timestamp from socket error queue
    uint8_t control[256];
    struct msghdr msg;
    struct iovec iov;
    
    memset(&msg, 0, sizeof(msg));
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    
    // Receive timestamp from error queue (MSG_ERRQUEUE)
    ssize_t len = recvmsg(g_ptp_socket, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PTPError::TIMESTAMP_UNAVAILABLE;
        }
        return PTPError::HARDWARE_ERROR;
    }
    
    // Parse control message for timestamp
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && 
            cmsg->cmsg_type == SO_TIMESTAMPING) {
            struct timespec* ts = (struct timespec*)CMSG_DATA(cmsg);
            *timestamp = Timestamp(ts[2].tv_sec, ts[2].tv_nsec);  // HW timestamp
            return PTPError::SUCCESS;
        }
    }
    
    return PTPError::TIMESTAMP_UNAVAILABLE;
}
```

**Software timestamping (fallback)**:

```cpp
#include <time.h>

static Timestamp hal_get_timestamp_software() {
    struct timespec ts;
    
    // Use monotonic clock for stability
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // Fallback to realtime clock
        clock_gettime(CLOCK_REALTIME, &ts);
    }
    
    return Timestamp(ts.tv_sec, ts.tv_nsec);
}

static PTPError hal_get_tx_timestamp_software(uint16_t sequence_id, 
                                               Timestamp* timestamp) {
    // Software timestamping: capture timestamp immediately after send
    // This is less accurate but works without hardware support
    *timestamp = hal_get_timestamp_software();
    return PTPError::SUCCESS;
}
```

### 3.4 Clock Adjustment Implementation

**Linux adjtime() API**:

```cpp
#include <sys/timex.h>

static PTPError hal_adjust_clock(int64_t adjustment_ns) {
    struct timex tx;
    memset(&tx, 0, sizeof(tx));
    
    // Convert nanoseconds to microseconds for adjtime
    tx.modes = ADJ_OFFSET_SINGLESHOT;
    tx.offset = adjustment_ns / 1000;  // Convert ns to us
    
    if (adjtimex(&tx) < 0) {
        perror("adjtimex");
        return PTPError::HARDWARE_ERROR;
    }
    
    return PTPError::SUCCESS;
}

static PTPError hal_adjust_frequency(double ppb_adjustment) {
    struct timex tx;
    memset(&tx, 0, sizeof(tx));
    
    // Set frequency adjustment
    tx.modes = ADJ_FREQUENCY;
    tx.freq = (long)(ppb_adjustment * 65536.0 / 1000.0);  // Convert ppb to scaled ppm
    
    if (adjtimex(&tx) < 0) {
        perror("adjtimex");
        return PTPError::HARDWARE_ERROR;
    }
    
    return PTPError::SUCCESS;
}
```

**PTP Hardware Clock adjustment (Linux)**:

```cpp
static PTPError hal_adjust_clock_phc(int64_t adjustment_ns) {
    struct ptp_clock_time ptp_offset;
    ptp_offset.sec = adjustment_ns / 1000000000LL;
    ptp_offset.nsec = adjustment_ns % 1000000000LL;
    
    if (ioctl(g_ptp_clock_fd, PTP_CLOCK_ADJTIME, &ptp_offset) < 0) {
        perror("PTP_CLOCK_ADJTIME");
        return PTPError::HARDWARE_ERROR;
    }
    
    return PTPError::SUCCESS;
}

static PTPError hal_adjust_frequency_phc(double ppb_adjustment) {
    struct ptp_clock_time ptp_freq;
    ptp_freq.sec = 0;
    ptp_freq.nsec = (int32_t)(ppb_adjustment * 1000.0);  // Convert ppb to ppt
    
    if (ioctl(g_ptp_clock_fd, PTP_CLOCK_ADJFREQ, &ptp_freq) < 0) {
        perror("PTP_CLOCK_ADJFREQ");
        return PTPError::HARDWARE_ERROR;
    }
    
    return PTPError::SUCCESS;
}
```

**Embedded RTOS (custom timer peripheral)**:

```cpp
// Example for custom hardware timer control
extern void timer_set_offset(int64_t offset_ns);
extern void timer_set_frequency(double ppb);

static PTPError hal_adjust_clock_embedded(int64_t adjustment_ns) {
    // Apply offset to hardware timer
    timer_set_offset(adjustment_ns);
    return PTPError::SUCCESS;
}

static PTPError hal_adjust_frequency_embedded(double ppb_adjustment) {
    // Apply frequency adjustment to timer
    timer_set_frequency(ppb_adjustment);
    return PTPError::SUCCESS;
}
```

### 3.5 Event Notification Implementation

```cpp
// State change handler
static void hal_on_state_change(PortState old_state, PortState new_state) {
    const char* state_names[] = {
        "INITIALIZING", "FAULTY", "DISABLED", "LISTENING",
        "PRE_MASTER", "MASTER", "PASSIVE", "UNCALIBRATED", "SLAVE"
    };
    
    printf("[PTP] State change: %s -> %s\n",
           state_names[(int)old_state],
           state_names[(int)new_state]);
    
    // Application-specific actions
    if (new_state == PortState::Slave) {
        // Synchronized! Enable time-sensitive operations
        enable_tsn_applications();
    } else if (old_state == PortState::Slave && new_state != PortState::Slave) {
        // Lost synchronization! Disable time-sensitive operations
        disable_tsn_applications();
    }
}

// Fault handler
static void hal_on_fault(const char* fault_description) {
    fprintf(stderr, "[PTP ERROR] %s\n", fault_description);
    
    // Log to system logger
    syslog(LOG_ERR, "PTP fault: %s", fault_description);
    
    // Application-specific fault handling
    increment_error_counter();
}
```

### 3.6 Complete HAL Setup

```cpp
// Assemble all callbacks into StateCallbacks struct
StateCallbacks create_hal_callbacks() {
    StateCallbacks callbacks;
    
    // Message transmission
    callbacks.send_announce = hal_send_announce;
    callbacks.send_sync = hal_send_sync;
    callbacks.send_follow_up = hal_send_follow_up;
    callbacks.send_delay_req = hal_send_delay_req;
    callbacks.send_delay_resp = hal_send_delay_resp;
    
    // Timestamping
    callbacks.get_timestamp = hal_get_timestamp;
    callbacks.get_tx_timestamp = hal_get_tx_timestamp;
    
    // Clock control
    callbacks.adjust_clock = hal_adjust_clock;
    callbacks.adjust_frequency = hal_adjust_frequency;
    
    // Event notifications
    callbacks.on_state_change = hal_on_state_change;
    callbacks.on_fault = hal_on_fault;
    
    return callbacks;
}
```

---

## 4. Platform-Specific Considerations

### 4.1 Linux

**Requirements**:
- Kernel 3.0+ with PTP support (`CONFIG_PTP_1588_CLOCK`)
- Network interface with hardware timestamping (`ethtool -T eth0`)
- Root privileges or `CAP_NET_RAW` capability

**Configuration**:

```bash
# Check hardware timestamping support
ethtool -T eth0

# Enable PTP on network interface
sudo ifconfig eth0 up
sudo ip link set eth0 promisc on

# Disable NTP to avoid conflicts
sudo systemctl stop ntp
sudo systemctl disable ntp
```

**Compile flags**:

```cmake
if(UNIX AND NOT APPLE)
    target_compile_definitions(my_ptp_app PRIVATE
        _GNU_SOURCE
        __LINUX__
    )
    target_link_libraries(my_ptp_app PRIVATE pthread rt)
endif()
```

### 4.2 Windows

**Requirements**:
- Windows 10 1809+ with Precision Time Protocol support
- WinPcap or Npcap for raw packet access
- Administrator privileges

**Winsock implementation**:

```cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Initialize Winsock
WSADATA wsa_data;
WSAStartup(MAKEWORD(2, 2), &wsa_data);

// Create raw socket (requires admin)
SOCKET sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
```

**Compile flags**:

```cmake
if(WIN32)
    target_compile_definitions(my_ptp_app PRIVATE
        _WIN32_WINNT=0x0A00  # Windows 10
        NOMINMAX
    )
    target_link_libraries(my_ptp_app PRIVATE ws2_32 iphlpapi)
endif()
```

### 4.3 Embedded RTOS (FreeRTOS, Zephyr, etc.)

**Memory considerations**:
- Static allocation only (no malloc/free in critical paths)
- Stack size: minimum 4KB for PTP task
- Heap: not required for core PTP logic

**Task priorities**:

```c
// FreeRTOS example
#define PTP_TASK_PRIORITY    (configMAX_PRIORITIES - 2)  // High priority
#define PTP_STACK_SIZE       (4096 / sizeof(StackType_t))

TaskHandle_t ptp_task_handle;
xTaskCreate(ptp_task_function, "PTP", PTP_STACK_SIZE, NULL, 
            PTP_TASK_PRIORITY, &ptp_task_handle);
```

**Timing considerations**:
- Call `tick()` every 1-125ms (configurable)
- Use hardware timer for precise intervals
- Disable interrupts during critical timestamp capture

### 4.4 ARM Cortex-M (Bare Metal)

**DWT cycle counter for timestamps**:

```cpp
// Enable DWT cycle counter (ARM Cortex-M)
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

static Timestamp hal_get_timestamp_cortex_m() {
    uint32_t cycles = DWT->CYCCNT;
    uint64_t ns = (cycles * 1000000000ULL) / SystemCoreClock;
    return Timestamp::fromNanoseconds(ns);
}
```

**Ethernet MAC timestamping (STM32)**:

```cpp
// Enable PTP on STM32 Ethernet MAC
ETH->PTPTSCR |= ETH_PTPTSCR_TSE;  // Enable timestamping
ETH->PTPTSCR |= ETH_PTPTSCR_TSFCU;  // Fine correction method

static Timestamp hal_get_timestamp_stm32() {
    uint32_t sec_high = ETH->PTPTSHR;
    uint32_t nsec = ETH->PTPTSLR;
    return Timestamp(sec_high, nsec);
}
```

---

## 5. Threading and Concurrency

### 5.1 Single-Threaded Integration

Simplest approach - all PTP processing in one thread/task:

```cpp
void ptp_main_loop() {
    OrdinaryClock clock(config, callbacks);
    clock.initialize();
    clock.start();
    
    while (running) {
        // 1. Periodic processing
        Timestamp now = get_timestamp();
        clock.tick(now);
        
        // 2. Process received messages
        while (has_pending_message()) {
            Message msg = receive_message();
            clock.process_message(msg.type, msg.data, msg.size, msg.timestamp);
        }
        
        // 3. Sleep until next tick (1ms typical)
        usleep(1000);
    }
    
    clock.stop();
}
```

### 5.2 Multi-Threaded Integration

Separate threads for network RX, PTP processing, and application:

```cpp
// Network RX thread (high priority)
void network_rx_thread() {
    while (running) {
        Message msg = receive_blocking();  // Blocks until message arrives
        message_queue.push(msg);  // Thread-safe queue
    }
}

// PTP processing thread (high priority)
void ptp_processing_thread() {
    OrdinaryClock clock(config, callbacks);
    clock.initialize();
    clock.start();
    
    auto last_tick = std::chrono::steady_clock::now();
    
    while (running) {
        // Periodic tick every 1ms
        auto now_chrono = std::chrono::steady_clock::now();
        if (now_chrono - last_tick >= std::chrono::milliseconds(1)) {
            Timestamp now = get_timestamp();
            clock.tick(now);
            last_tick = now_chrono;
        }
        
        // Process messages from queue
        Message msg;
        while (message_queue.try_pop(msg)) {  // Non-blocking
            clock.process_message(msg.type, msg.data, msg.size, msg.timestamp);
        }
        
        // Small sleep to avoid busy-wait
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    clock.stop();
}

// Application thread (normal priority)
void application_thread() {
    while (running) {
        if (clock.is_synchronized()) {
            // Do time-sensitive work
            perform_synchronized_operation();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main() {
    // Start threads
    std::thread net_rx(network_rx_thread);
    std::thread ptp_proc(ptp_processing_thread);
    std::thread app(application_thread);
    
    // Set thread priorities (platform-specific)
    set_thread_priority(net_rx, PRIORITY_HIGH);
    set_thread_priority(ptp_proc, PRIORITY_HIGH);
    set_thread_priority(app, PRIORITY_NORMAL);
    
    // Wait for threads
    net_rx.join();
    ptp_proc.join();
    app.join();
    
    return 0;
}
```

### 5.3 Thread Safety Considerations

**PTP library is NOT thread-safe by design** (for determinism). Use these patterns:

1. **Single-threaded access**: Only one thread calls PTP library methods
2. **External synchronization**: Use mutex if multiple threads need access
3. **Message queue**: Pass messages to PTP thread via thread-safe queue

```cpp
// Thread-safe wrapper (if multi-threaded access needed)
class ThreadSafePTPClock {
private:
    OrdinaryClock clock_;
    std::mutex mutex_;
    
public:
    void tick(const Timestamp& now) {
        std::lock_guard<std::mutex> lock(mutex_);
        clock_.tick(now);
    }
    
    void process_message(uint8_t type, const void* data, 
                        size_t size, const Timestamp& ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        clock_.process_message(type, data, size, ts);
    }
    
    bool is_synchronized() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return clock_.is_synchronized();
    }
};
```

---

## 6. Network Integration

### 6.1 PTP Network Addressing

**Ethernet Layer 2 (recommended)**:
- Ethertype: `0x88F7`
- Multicast MAC: `01:1B:19:00:00:00` (general messages)
- Multicast MAC: `01:80:C2:00:00:0E` (peer delay)

**UDP/IPv4 Layer 3**:
- Event messages (Sync, Delay_Req): Port 319, Multicast 224.0.1.129
- General messages (Announce, Follow_Up): Port 320, Multicast 224.0.1.129

**UDP/IPv6 Layer 3**:
- Event messages: Port 319, Multicast FF0X::181 (X=scope)
- General messages: Port 320, Multicast FF0X::181

### 6.2 Message Reception Loop

```cpp
void receive_ptp_messages(OrdinaryClock& clock) {
    uint8_t buffer[2048];
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    while (running) {
        // Receive packet (blocking or non-blocking with timeout)
        ssize_t len = recvfrom(g_ptp_socket, buffer, sizeof(buffer), 0,
                              (struct sockaddr*)&src_addr, &addr_len);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // Timeout, try again
            }
            perror("recvfrom");
            break;
        }
        
        // Capture RX timestamp immediately
        Timestamp rx_timestamp = get_timestamp();
        
        // Extract message type from PTP header
        uint8_t message_type = buffer[0] & 0x0F;
        
        // Process message
        auto result = clock.process_message(message_type, buffer, len, rx_timestamp);
        if (!result.is_success()) {
            fprintf(stderr, "Failed to process PTP message: %d\n", 
                   (int)result.error());
        }
    }
}
```

### 6.3 Multicast Group Management

```cpp
// Join PTP multicast group (IPv4)
void join_ptp_multicast(int sock) {
    struct ip_mreqn mreq;
    
    // PTP multicast address 224.0.1.129
    inet_pton(AF_INET, "224.0.1.129", &mreq.imr_multiaddr);
    
    // Get interface address
    struct ifreq ifr;
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
    ioctl(sock, SIOCGIFADDR, &ifr);
    mreq.imr_address = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
    mreq.imr_ifindex = 0;
    
    // Join multicast group
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                   &mreq, sizeof(mreq)) < 0) {
        perror("IP_ADD_MEMBERSHIP");
    }
}
```

---

## 7. Timing and Timestamping

### 7.1 Timestamp Accuracy Requirements

| Accuracy Level | Hardware Support | Typical Use Case |
|----------------|------------------|------------------|
| < 1 μs | Hardware timestamping required | Industrial control, TSN |
| 1-10 μs | Hardware timestamping recommended | Professional audio/video |
| 10-100 μs | Software timestamping acceptable | General synchronization |
| > 100 μs | NTP may be sufficient | Non-critical applications |

### 7.2 Hardware Timestamping Setup

**Verify hardware support**:

```bash
# Check if network interface supports hardware timestamping
ethtool -T eth0

# Expected output:
# Time stamping parameters for eth0:
# Capabilities:
#   hardware-transmit     (SOF_TIMESTAMPING_TX_HARDWARE)
#   software-transmit     (SOF_TIMESTAMPING_TX_SOFTWARE)
#   hardware-receive      (SOF_TIMESTAMPING_RX_HARDWARE)
#   software-receive      (SOF_TIMESTAMPING_RX_SOFTWARE)
#   software-system-clock (SOF_TIMESTAMPING_SOFTWARE)
#   hardware-raw-clock    (SOF_TIMESTAMPING_RAW_HARDWARE)
```

**Enable hardware timestamping**:

```cpp
int enable_hardware_timestamping(int sock) {
    // Enable hardware TX/RX timestamping
    int flags = SOF_TIMESTAMPING_TX_HARDWARE |
                SOF_TIMESTAMPING_RX_HARDWARE |
                SOF_TIMESTAMPING_RAW_HARDWARE;
    
    if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING,
                   &flags, sizeof(flags)) < 0) {
        perror("SO_TIMESTAMPING");
        return -1;
    }
    
    // Request timestamps on transmitted packets
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPNS,
                   &enable, sizeof(enable)) < 0) {
        perror("SO_TIMESTAMPNS");
        return -1;
    }
    
    return 0;
}
```

### 7.3 Software Timestamping (Fallback)

```cpp
// Capture timestamp as close to send/receive as possible
Timestamp capture_tx_timestamp_software() {
    // Get timestamp immediately before/after send
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return Timestamp(ts.tv_sec, ts.tv_nsec);
}

Timestamp capture_rx_timestamp_software() {
    // Get timestamp immediately after receive
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return Timestamp(ts.tv_sec, ts.tv_nsec);
}
```

---

## 8. Configuration and Tuning

### 8.1 Port Configuration

```cpp
PortConfiguration create_slave_config() {
    PortConfiguration config;
    
    // Basic settings
    config.port_number = 1;
    config.domain_number = 0;  // Default PTP domain
    
    // Message intervals (log2 seconds)
    config.announce_interval = 1;      // 2^1 = 2 seconds
    config.sync_interval = 0;          // 2^0 = 1 second
    config.delay_req_interval = 0;     // 2^0 = 1 second
    
    // Timeouts (multiplier of interval)
    config.announce_receipt_timeout = 3;  // 3 x announce_interval
    config.sync_receipt_timeout = 3;      // 3 x sync_interval
    
    // Delay mechanism
    config.delay_mechanism_p2p = false;  // Use E2E delay
    
    // PTP version
    config.version_number = 2;  // PTPv2
    
    return config;
}

PortConfiguration create_master_config() {
    PortConfiguration config = create_slave_config();
    
    // Masters typically send more frequent Sync
    config.sync_interval = -3;  // 2^-3 = 125ms (8 Hz)
    
    return config;
}
```

### 8.2 Clock Quality Configuration

```cpp
void configure_clock_quality(OrdinaryClock& clock, ClockClass clock_class) {
    auto& port = clock.get_port();
    auto& defaults = const_cast<DefaultDataSet&>(port.get_default_data_set());
    
    // Set clock class
    defaults.clockQuality.clockClass = clock_class;
    
    // Examples:
    // 6 = GPS-locked primary reference
    // 13 = Application-specific, external source
    // 248 = Default, no external source
    
    // Set accuracy
    if (clock_class <= 127) {
        defaults.clockQuality.clockAccuracy = 0x20;  // 25 ns
    } else {
        defaults.clockQuality.clockAccuracy = 0xFE;  // Unknown
    }
    
    // Set variance
    defaults.clockQuality.offsetScaledLogVariance = 0x4E5D;  // Typical value
}

void configure_priority(OrdinaryClock& clock, uint8_t priority1, 
                       uint8_t priority2) {
    auto& port = clock.get_port();
    auto& defaults = const_cast<DefaultDataSet&>(port.get_default_data_set());
    
    // Lower value = higher priority
    defaults.priority1 = priority1;  // 0-255, default 128
    defaults.priority2 = priority2;  // 0-255, default 128
}
```

### 8.3 Performance Tuning

**Linux kernel tuning**:

```bash
# Increase network buffer sizes
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216

# Reduce interrupt coalescing for lower latency
sudo ethtool -C eth0 rx-usecs 0 tx-usecs 0

# Set CPU governor to performance
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Isolate CPU cores for PTP (cpuset)
sudo cset shield -c 2,3 -k on
sudo cset shield --exec -- ./my_ptp_app
```

**RTOS tuning**:

```c
// Increase PTP task priority
#define PTP_TASK_PRIORITY (configMAX_PRIORITIES - 1)

// Use high-resolution timer
configTICK_RATE_HZ 10000  // 10 kHz tick rate = 100 μs resolution

// Disable unnecessary features
configUSE_TICK_HOOK 0
configUSE_MALLOC_FAILED_HOOK 0
```

---

## 9. Testing Your Integration

### 9.1 Loopback Test (Self-Test)

```cpp
// Test HAL callbacks without network
void test_hal_loopback() {
    // Test timestamping
    Timestamp t1 = callbacks.get_timestamp();
    usleep(1000);  // 1ms
    Timestamp t2 = callbacks.get_timestamp();
    
    assert(t2 > t1);
    double elapsed_ms = (t2 - t1).toNanoseconds() / 1e6;
    assert(elapsed_ms >= 1.0 && elapsed_ms < 2.0);  // Should be ~1ms
    
    // Test clock adjustment
    PTPError err = callbacks.adjust_clock(1000);  // +1 μs
    assert(err == PTPError::SUCCESS);
    
    printf("HAL loopback test PASSED\n");
}
```

### 9.2 Integration with PTP Master

**Setup test environment**:

```bash
# On Linux, use ptp4l as reference master
sudo ptp4l -i eth0 -m -S

# Or use a hardware PTP grandmaster
```

**Monitor synchronization**:

```cpp
void monitor_synchronization(OrdinaryClock& clock) {
    while (true) {
        sleep(1);
        
        if (!clock.is_synchronized()) {
            printf("Not synchronized\n");
            continue;
        }
        
        const auto& current = clock.get_port().get_current_data_set();
        const auto& parent = clock.get_port().get_parent_data_set();
        
        printf("Synchronized to GM: ");
        for (int i = 0; i < 8; i++) {
            printf("%02X", parent.grandmasterIdentity[i]);
        }
        printf("\n");
        
        printf("Offset: %+.1f ns\n", current.offsetFromMaster.toNanoseconds());
        printf("Delay:  %.1f ns\n", current.meanPathDelay.toNanoseconds());
        printf("Steps:  %u\n", current.stepsRemoved);
    }
}
```

### 9.3 Accuracy Verification

**Compare PTP time with reference**:

```cpp
void verify_accuracy() {
    // Get PTP synchronized time
    Timestamp ptp_time = callbacks.get_timestamp();
    
    // Get system time (should match PTP time if synchronized)
    struct timespec sys_time;
    clock_gettime(CLOCK_REALTIME, &sys_time);
    Timestamp sys_timestamp(sys_time.tv_sec, sys_time.tv_nsec);
    
    // Calculate difference
    TimeInterval diff = ptp_time - sys_timestamp;
    double diff_ns = diff.toNanoseconds();
    
    printf("PTP vs System time: %+.1f ns\n", diff_ns);
    
    // Should be within expected accuracy (< 1 μs for hardware timestamping)
    if (fabs(diff_ns) > 1000.0) {
        printf("WARNING: Large time difference detected!\n");
    }
}
```

---

## 10. Common Integration Patterns

### 10.1 Slave-Only Clock

```cpp
// Simplest pattern: always slave, never master
PortConfiguration config = create_slave_config();

// Configure as slave-only
auto& defaults = const_cast<DefaultDataSet&>(port.get_default_data_set());
defaults.slaveOnly = true;  // Never become master

// Minimal callbacks (no master functionality)
StateCallbacks callbacks = {
    .send_announce = nullptr,      // Slaves don't send Announce
    .send_sync = nullptr,           // Slaves don't send Sync
    .send_follow_up = nullptr,      // Slaves don't send Follow_Up
    .send_delay_req = hal_send_delay_req,  // Slaves send Delay_Req
    .send_delay_resp = nullptr,     // Slaves don't send Delay_Resp
    .get_timestamp = hal_get_timestamp,
    .get_tx_timestamp = hal_get_tx_timestamp,
    .adjust_clock = hal_adjust_clock,
    .adjust_frequency = hal_adjust_frequency,
    .on_state_change = hal_on_state_change,
    .on_fault = hal_on_fault
};

OrdinaryClock clock(config, callbacks);
```

### 10.2 Grandmaster Clock (GPS-Locked)

```cpp
// Configure as grandmaster with GPS reference
PortConfiguration config = create_master_config();

StateCallbacks callbacks = {
    .send_announce = hal_send_announce,
    .send_sync = hal_send_sync,
    .send_follow_up = hal_send_follow_up,
    .send_delay_req = nullptr,      // Masters don't send Delay_Req
    .send_delay_resp = hal_send_delay_resp,
    .get_timestamp = hal_get_gps_timestamp,  // GPS-disciplined clock
    .get_tx_timestamp = hal_get_tx_timestamp,
    .adjust_clock = nullptr,        // GM doesn't adjust its clock
    .adjust_frequency = nullptr,    // GM doesn't adjust frequency
    .on_state_change = hal_on_state_change,
    .on_fault = hal_on_fault
};

OrdinaryClock clock(config, callbacks);

// Configure as GPS-locked grandmaster
auto& port = clock.get_port();
auto& defaults = const_cast<DefaultDataSet&>(port.get_default_data_set());
defaults.priority1 = 100;  // High priority
defaults.clockQuality.clockClass = 6;  // GPS primary reference
defaults.clockQuality.clockAccuracy = 0x20;  // 25 ns accuracy
defaults.slaveOnly = false;  // Can be master
```

### 10.3 Boundary Clock (Network Switch)

```cpp
// Multi-port clock for network switches/routers
std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs;

// Configure ports
for (size_t i = 0; i < 4; i++) {
    configs[i].port_number = i + 1;
    configs[i].domain_number = 0;
    configs[i].announce_interval = 1;
    configs[i].sync_interval = 0;
}

// Create boundary clock with 4 ports
BoundaryClock bc(configs, 4, callbacks);
bc.initialize();
bc.start();

// Process messages on specific ports
while (running) {
    // Receive message on port 1
    if (message_available(1)) {
        bc.process_message(1, msg_type, msg_data, msg_size, rx_timestamp);
    }
    
    // Tick all ports
    bc.tick(get_timestamp());
}
```

---

## Appendix A: Troubleshooting Checklist

### Build Issues

- [ ] CMake version >= 3.15
- [ ] C++17 compiler support enabled
- [ ] All required headers accessible
- [ ] Link errors: check library path

### Network Issues

- [ ] Network interface UP and configured
- [ ] Multicast group joined correctly
- [ ] Firewall allows PTP traffic (UDP 319/320, Ethertype 0x88F7)
- [ ] Promiscuous mode enabled (for Layer 2)
- [ ] Raw socket privileges (root or CAP_NET_RAW)

### Timestamping Issues

- [ ] Hardware timestamping supported (`ethtool -T`)
- [ ] PTP clock device accessible (`/dev/ptp0`)
- [ ] SO_TIMESTAMPING enabled correctly
- [ ] TX timestamp retrieved before next send

### Synchronization Issues

- [ ] PTP master reachable on network
- [ ] Announce messages received (check `get_statistics()`)
- [ ] Sync/Follow_Up messages received
- [ ] Delay_Req/Delay_Resp exchange working
- [ ] Clock adjustment callbacks implemented correctly

---

## Appendix B: Example Projects

See `examples/` directory for complete working examples:

1. **basic-slave**: Minimal PTP slave clock
2. **gps-grandmaster**: GPS-locked grandmaster clock
3. **boundary-clock**: Multi-port boundary clock
4. **hal-template**: HAL implementation template

---

**End of Integration Guide**

For API details, see [API Reference Guide](api-reference.md).  
For getting started, see [Getting Started Tutorial](../training-materials/getting-started.md).  
For troubleshooting, see [Troubleshooting Guide](troubleshooting.md).

---

**Document Version**: 1.0.0  
**Last Updated**: 2025-11-11  
**Status**: Complete for MVP release
