# Time Source Integration Guide - How to Keep PTP Clock Synchronized

## Overview

The adapters (NTP, DCF77, GPS) **DO NOT** directly control the PTP clock. They are **external time sources** that provide:
1. **Time information** (timestamps from external reference)
2. **Quality information** (how good is the time source)

The **IEEE 1588-2019 PTP library** handles the actual clock synchronization via the PTP protocol.

## Architecture: External Time Source → PTP Clock

```
┌──────────────────────────────────────────────────────────────────┐
│                    PTP Network Topology                          │
│                                                                  │
│  ┌─────────────────┐         ┌─────────────────┐              │
│  │ Grandmaster     │         │  Boundary       │              │
│  │ (Your Device)   │────────►│  Clock          │────┐         │
│  │                 │  PTP    │                 │    │         │
│  └────────┬────────┘ Protocol└─────────────────┘    │ PTP     │
│           │                                          │Protocol │
│           │ External Time Source                    │         │
│           │ (NTP/DCF77/GPS)                         ▼         │
│           │                                   ┌──────────────┐ │
│           │                                   │   Slave      │ │
│           ▼                                   │   Clock      │ │
│  ┌──────────────────┐                        └──────────────┘ │
│  │  Time Adapter    │                                          │
│  │  (Your Code)     │                                          │
│  └──────────────────┘                                          │
└──────────────────────────────────────────────────────────────────┘
```

## The Correct Integration Pattern

### Step 1: Time Source Adapter (External)

Your adapter queries the external time source and returns **quality information**:

```cpp
// examples/05-ntp-sntp-sync/ntp_adapter.cpp
#include "ntp_adapter.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"

bool NTPAdapter::update() {
    // Query NTP server via SNTP
    query_ntp_server(last_query_result_);
    
    // Adapter's job: Determine quality based on NTP stratum
    // Does NOT set system time!
    return last_query_result_.valid;
}

Types::ClockQuality NTPAdapter::get_clock_quality() const {
    // Convert NTP metrics → IEEE 1588-2019 quality
    Types::ClockQuality quality;
    
    if (last_query_result_.stratum == 1) {
        quality.clock_class = 6;      // Primary reference
        quality.clock_accuracy = 0x2B; // ±10ms
    } else if (last_query_result_.stratum == 2) {
        quality.clock_class = 6;
        quality.clock_accuracy = 0x2C; // ±25ms  
    } else {
        quality.clock_class = 13;     // Application-specific
        quality.clock_accuracy = 0x2D; // ±100ms
    }
    
    return quality;  // Just returns quality, doesn't modify clock!
}
```

### Step 2: PTP Clock Configuration (Library)

The PTP clock uses this quality for **BMCA** (Best Master Clock Algorithm):

```cpp
// Your main application
#include "clocks.hpp"
#include "ntp_adapter.hpp"

using namespace IEEE::_1588::PTP::_2019::Clocks;

int main() {
    // Create IEEE 1588-2019 PTP clock (from library)
    OrdinaryClock ptp_clock(clock_identity, port_number);
    ptp_clock.initialize();
    
    // Create external time source adapter
    Examples::NTP::NTPAdapter ntp("pool.ntp.org");
    ntp.initialize();
    
    // Main synchronization loop
    while (running) {
        // 1. Query external time source
        if (ntp.update()) {
            // 2. Get quality from external source
            Types::ClockQuality quality = ntp.get_clock_quality();
            
            // 3. Update PTP clock's advertised quality
            // NOTE: This updates what the clock ADVERTISES, not the time itself!
            auto& ds = ptp_clock.get_default_data_set();
            ds.clockQuality = quality;  // For BMCA
            
            auto& tp = ptp_clock.get_time_properties_data_set();
            tp.timeSource = static_cast<uint8_t>(Types::TimeSource::NTP);
        }
        
        // 4. PTP clock handles actual time synchronization
        auto current_time = get_system_time();
        ptp_clock.tick(current_time);  // Process PTP protocol
        
        std::this_thread::sleep_for(std::chrono::seconds(64));
    }
}
```

### Step 3: What the PTP Library Does

The IEEE 1588-2019 library handles:
- **Sending Announce messages** with your clock quality
- **Running BMCA** to determine if this clock should be master/slave
- **Sync/Follow_Up messages** for time distribution
- **Delay_Req/Delay_Resp** for path delay measurement
- **Actually adjusting system time** via hardware timestamping

## Key Concepts

### 1. Time Source Adapter's Job

✅ **DOES**:
- Query external time source (NTP server, DCF77 receiver, GPS module)
- Compute IEEE 1588-2019 clock quality based on source status
- Return `Types::ClockQuality` struct

❌ **DOES NOT**:
- Set system time directly
- Implement PTP protocol
- Synchronize clocks over network
- Manage PTP state machines

### 2. PTP Library's Job

✅ **DOES**:
- Implement IEEE 1588-2019 PTP protocol
- Send/receive Sync, Announce, Delay messages
- Run BMCA to elect best master
- Synchronize slave clocks to master
- Adjust system time via hardware timestamps

❌ **DOES NOT**:
- Query NTP servers
- Decode DCF77 signals
- Parse GPS NMEA sentences
- Know about external time sources

## Two Synchronization Scenarios

### Scenario A: PTP Grandmaster with External Reference

**Your device = Grandmaster, using NTP/DCF77/GPS as reference**

```cpp
// You are the time source for the PTP network
OrdinaryClock grandmaster(identity, port);

// External reference improves your quality
NTPAdapter ntp("time.google.com");

while (running) {
    if (ntp.update()) {
        // Update quality so slaves know you're traceable
        grandmaster.get_default_data_set().clockQuality = 
            ntp.get_clock_quality();
        
        // Optionally: Steer your local clock to match NTP
        // (This is application-specific, not part of PTP library)
        int64_t offset_ns = ntp.get_last_result().offset_ns;
        adjust_system_clock(offset_ns);  // Your implementation
    }
    
    // PTP library distributes YOUR time to slaves
    grandmaster.tick(get_system_time());
}
```

**Result**: Slave clocks sync to YOU, you sync to NTP/DCF77/GPS

### Scenario B: PTP Slave with External Reference Validation

**Your device = Slave, validating PTP time against NTP/DCF77/GPS**

```cpp
// You are synchronizing TO another PTP master
OrdinaryClock slave(identity, port);

// External reference for validation
NTPAdapter ntp("pool.ntp.org");

while (running) {
    // PTP library synchronizes to master
    slave.tick(get_system_time());
    
    if (slave.is_synchronized()) {
        // Optionally: Validate PTP time against NTP
        if (ntp.update()) {
            int64_t ptp_time = get_system_time();
            int64_t ntp_time = ntp.get_last_result().timestamp;
            int64_t difference = std::abs(ptp_time - ntp_time);
            
            if (difference > 1000000000) {  // >1 second
                std::cerr << "WARNING: PTP and NTP disagree by " 
                         << (difference / 1000000) << "ms\n";
            }
        }
    }
}
```

**Result**: You sync to PTP master, NTP validates correctness

## Complete Example: Grandmaster with NTP Reference

```cpp
#include "clocks.hpp"
#include "ntp_adapter.hpp"
#include <iostream>
#include <thread>

using namespace IEEE::_1588::PTP::_2019;
using namespace Examples::NTP;

class PTPGrandmasterWithNTP {
public:
    PTPGrandmasterWithNTP(const std::string& ntp_server)
        : ntp_(ntp_server, 123, 64)
    {
        // Initialize PTP clock as grandmaster
        Types::ClockIdentity my_id = generate_clock_identity();
        ptp_clock_ = std::make_unique<Clocks::OrdinaryClock>(my_id, 1);
        
        // Set initial quality (will be updated by NTP)
        auto& ds = ptp_clock_->get_default_data_set();
        ds.priority1 = 128;  // Default priority
        ds.clockQuality.clock_class = 248;  // Not synchronized yet
    }
    
    bool initialize() {
        // Initialize NTP adapter
        if (!ntp_.initialize()) {
            std::cerr << "Failed to initialize NTP\n";
            return false;
        }
        
        // Initialize PTP clock
        auto result = ptp_clock_->initialize();
        if (!result) {
            std::cerr << "Failed to initialize PTP clock\n";
            return false;
        }
        
        // Start PTP protocol
        result = ptp_clock_->start();
        if (!result) {
            std::cerr << "Failed to start PTP clock\n";
            return false;
        }
        
        return true;
    }
    
    void run() {
        std::cout << "PTP Grandmaster with NTP reference running...\n";
        
        auto last_ntp_query = std::chrono::steady_clock::now();
        
        while (running_) {
            auto now = std::chrono::steady_clock::now();
            
            // Query NTP periodically (every 64 seconds)
            if (std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_ntp_query).count() >= 64) {
                
                if (ntp_.update()) {
                    // Update PTP clock quality based on NTP
                    Types::ClockQuality quality = ntp_.get_clock_quality();
                    
                    auto& ds = ptp_clock_->get_default_data_set();
                    ds.clockQuality = quality;
                    
                    auto& tp = ptp_clock_->get_time_properties_data_set();
                    tp.timeSource = static_cast<uint8_t>(
                        Types::TimeSource::NTP);
                    
                    std::cout << "Updated clock quality from NTP: "
                              << "clockClass=" << (int)quality.clock_class
                              << ", accuracy=0x" << std::hex 
                              << (int)quality.clock_accuracy << std::dec << "\n";
                    
                    // Optionally: Adjust local clock to NTP
                    int64_t offset_ns = ntp_.get_last_result().offset_ns;
                    if (std::abs(offset_ns) > 10000000) {  // >10ms
                        std::cout << "NTP offset: " 
                                 << (offset_ns / 1000000.0) << "ms\n";
                        // adjust_local_clock(offset_ns);  // Your implementation
                    }
                }
                
                last_ntp_query = now;
            }
            
            // Process PTP protocol (sends Announce, Sync, etc.)
            Types::Timestamp current_time = get_current_ptp_time();
            ptp_clock_->tick(current_time);
            
            // Sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void stop() {
        running_ = false;
        ptp_clock_->stop();
    }
    
private:
    std::unique_ptr<Clocks::OrdinaryClock> ptp_clock_;
    NTPAdapter ntp_;
    bool running_ = true;
    
    Types::ClockIdentity generate_clock_identity() {
        // Generate unique clock ID (typically from MAC address)
        Types::ClockIdentity id{};
        // ... implementation ...
        return id;
    }
    
    Types::Timestamp get_current_ptp_time() {
        // Get current time with nanosecond precision
        auto now = std::chrono::system_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        
        Types::Timestamp ts;
        ts.seconds = ns / 1000000000ULL;
        ts.nanoseconds = ns % 1000000000ULL;
        return ts;
    }
};

int main() {
    PTPGrandmasterWithNTP gm("pool.ntp.org");
    
    if (!gm.initialize()) {
        return 1;
    }
    
    gm.run();
    return 0;
}
```

## Summary

| Component | Responsibility | Interaction |
|-----------|---------------|-------------|
| **NTP/DCF77/GPS Adapter** | Query external time source, compute quality | Reads from network/hardware, returns `Types::ClockQuality` |
| **Your Application** | Connect adapter → PTP clock | Periodically updates `ptp_clock.get_default_data_set().clockQuality` |
| **PTP Library** | Implement IEEE 1588-2019 protocol | Sends Announce with quality, distributes time to slaves |
| **System Clock** | Actual time value | PTP library adjusts via hardware timestamps (not shown here) |

The key insight: **Adapters provide QUALITY information, PTP library handles TIME synchronization**.
