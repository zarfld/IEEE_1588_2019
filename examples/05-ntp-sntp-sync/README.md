# NTP/SNTP Time Source for IEEE 1588-2019 PTP

## Overview

This example demonstrates synchronizing an IEEE 1588-2019 PTP clock with NTP/SNTP servers.

**IEEE 1588-2019 Classification**:
- **timeSource**: 0x50 (NTP - Network Time Protocol)
- **clockClass**: 6 (Primary reference when synchronized to stratum 1/2)
- **clockAccuracy**: 0x2B to 0x2F (10ms to 10s, depending on network conditions)

## NTP vs SNTP

- **NTP (Network Time Protocol)**: Full-featured time synchronization with accuracy ~1-50ms over LAN
- **SNTP (Simple Network Time Protocol)**: Lightweight subset, suitable for clients

This implementation uses SNTP for simplicity while supporting NTP servers.

## Hardware Requirements

### Option 1: Windows PC with Network Connection
- Windows 10/11 with network access
- Access to NTP servers (local or internet)
- **No additional hardware needed**

### Option 2: ESP32 with Ethernet/WiFi
- ESP32 DevKit (WiFi built-in)
- or ESP32 with W5500/LAN8720 Ethernet module
- Network connection to NTP servers

## Network Requirements

### Local NTP Server (Recommended)
- Windows Time Service (default on Windows)
- Linux NTP daemon (ntpd, chronyd)
- pfSense/OPNsense NTP server
- Dedicated NTP appliance

### Public NTP Servers (Fallback)
- `pool.ntp.org` - NTP Pool Project
- `time.windows.com` - Microsoft
- `time.google.com` - Google
- `time.cloudflare.com` - Cloudflare

## Expected Performance

| NTP Server Type | Typical Accuracy | clockAccuracy | Clock Class |
|-----------------|------------------|---------------|-------------|
| Local stratum 1 (GPS) | ±1-10 ms | 0x2B (10ms) | 6 |
| Local stratum 2 | ±10-50 ms | 0x2C (25ms) | 6 |
| Internet stratum 2 | ±50-200 ms | 0x2D (100ms) | 13 |
| Internet stratum 3+ | ±100ms-1s | 0x2F (1s) | 13 |

**Note**: NTP is less accurate than GPS but requires no additional hardware!

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│          IEEE 1588-2019 PTP Grandmaster                 │
│                                                          │
│  ┌────────────────────────────────────────────────┐    │
│  │       NTP Time Source (timeSource=0x50)        │    │
│  │                                                 │    │
│  │  ┌─────────────┐      ┌──────────────────┐   │    │
│  │  │ NTP Client  │◄────►│  NTP Server(s)   │   │    │
│  │  │   (SNTP)    │      │ (pool.ntp.org)   │   │    │
│  │  └─────────────┘      └──────────────────┘   │    │
│  │         │                                      │    │
│  │         ▼                                      │    │
│  │  ┌─────────────────┐                          │    │
│  │  │ Quality Manager │                          │    │
│  │  │ - Poll interval │                          │    │
│  │  │ - Offset/jitter │                          │    │
│  │  │ - Stratum level │                          │    │
│  │  └─────────────────┘                          │    │
│  └────────────────────────────────────────────────┘    │
│                       │                                 │
│                       ▼                                 │
│  ┌────────────────────────────────────────────────┐    │
│  │     PTP Clock Quality Attributes              │    │
│  │  clockClass: 6 (if stratum 1/2)               │    │
│  │  clockAccuracy: 0x2B-0x2F (10ms-10s)          │    │
│  │  timeSource: 0x50 (NTP)                       │    │
│  └────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

## Files

- `ntp_adapter.hpp` - NTP adapter interface (USES library's Types::ClockQuality!)
- `ntp_adapter.cpp` - SNTP client implementation
- `ntp_ptp_sync_example.cpp` - Example program
- `CMakeLists.txt` - Build configuration
- `README.md` - This file

## Building

```bash
cd d:\Repos\IEEE_1588_2019\examples\05-ntp-sntp-sync
cmake -S . -B build
cmake --build build --config Release
```

Or add to main project:
```bash
cd d:\Repos\IEEE_1588_2019
cmake --build build --config Release --target ntp_ptp_sync_example
```

## Usage

### Basic Example

```cpp
#include "ntp_adapter.hpp"
#include "clocks.hpp"

using namespace Examples::NTP;

// Create NTP adapter (USES library types!)
NTPAdapter ntp("pool.ntp.org", 123, 64);

// Initialize
if (!ntp.initialize()) {
    std::cerr << "NTP initialization failed!\n";
    return 1;
}

// Main loop
while (running) {
    // Query NTP server
    if (ntp.update()) {
        // Get clock quality using LIBRARY's Types::ClockQuality
        Types::ClockQuality quality = ntp.get_clock_quality();
        
        // Update PTP clock with library types (not duplicated!)
        auto& ds = ptp_clock.get_default_data_set();
        ds.clockQuality = quality;  // Direct assignment of library type
        
        auto& tp = ptp_clock.get_time_properties_data_set();
        tp.timeSource = static_cast<uint8_t>(Types::TimeSource::NTP);
    }
    
    // Wait for next poll interval
    std::this_thread::sleep_for(std::chrono::seconds(64));
}
```

### Integration with PTP Clock

```cpp
// This adapter USES library types, does NOT recreate them!
// ✅ CORRECT - using library's Types::ClockQuality
Types::ClockQuality quality = ntp.get_clock_quality();

// ✅ CORRECT - using library's Types::TimeSource enum
Types::TimeSource source = NTPAdapter::get_time_source();  // Returns Types::TimeSource::NTP

// Update PTP clock directly with library types
auto& ds = ptp_clock.get_default_data_set();
ds.clockQuality = quality;  // No conversion needed!

auto& tp = ptp_clock.get_time_properties_data_set();
tp.timeSource = static_cast<uint8_t>(source);  // 0x50
```

## NTP Quality Determination

Clock quality depends on NTP server stratum and network conditions:

```cpp
ClockQualityAttributes determine_quality(uint8_t stratum, int32_t offset_ms) {
    ClockQualityAttributes quality;
    
    if (stratum == 1) {
        // Stratum 1 (GPS, atomic clock)
        quality.clock_class = 6;
        quality.clock_accuracy = 0x2B;  // 10ms
        quality.time_source = 0x50;     // NTP
    } else if (stratum == 2) {
        // Stratum 2 (synced to stratum 1)
        quality.clock_class = 6;
        quality.clock_accuracy = 0x2C;  // 25ms
    } else {
        // Stratum 3+ (internet NTP)
        quality.clock_class = 13;       // Application-specific
        quality.clock_accuracy = 0x2D;  // 100ms
    }
    
    // Adjust for poor network conditions
    if (abs(offset_ms) > 100) {
        quality.clock_accuracy = 0x2F;  // 1 second
        quality.clock_class = 248;      // Not traceable
    }
    
    return quality;
}
```

## Monitoring

Monitor NTP synchronization:

```cpp
auto status = ntp_source.get_status();

std::cout << "NTP Status:\n";
std::cout << "  State: " << status.status_message << "\n";
std::cout << "  Quality: " << status.sync_quality_percent << "%\n";
std::cout << "  Last update: " << status.time_since_update_ms << "ms ago\n";
```

## Advantages vs GPS

✅ **No additional hardware** - uses existing network  
✅ **Easy deployment** - works anywhere with network  
✅ **Multiple servers** - redundancy built-in  
✅ **Low cost** - free public NTP servers available  
✅ **Indoor operation** - no antenna/sky view needed  

❌ **Lower accuracy** - typically ±10-100ms (vs GPS ±100ns)  
❌ **Network dependent** - requires stable network  
❌ **Stratum matters** - accuracy depends on NTP server quality  

## Use Cases

**Ideal for**:
- Indoor installations without GPS
- Temporary/development setups
- Backup time source when GPS unavailable
- Applications with ±10-100ms accuracy requirements
- Cost-sensitive deployments

**Not suitable for**:
- Sub-millisecond timing requirements
- Financial trading (need GPS/PPS)
- 5G telecom synchronization
- High-precision industrial control

## Troubleshooting

### No NTP response
- Check firewall (UDP port 123)
- Verify NTP server address
- Try public NTP servers as fallback

### High offset/jitter
- Check network latency
- Use local NTP server instead of internet
- Increase poll interval

### Wrong stratum
- NTP server not synchronized
- Use different NTP server
- Check server configuration

## References

- **IEEE 1588-2019**: Section 8.6.2.7, Table 6
- **RFC 5905**: Network Time Protocol Version 4
- **RFC 4330**: Simple Network Time Protocol (SNTP)
- **NTP Pool Project**: https://www.ntppool.org/

---

**Status**: Implementation ready for development  
**Accuracy**: ±10-100ms (typical)  
**Hardware**: No additional hardware required  
**Standards**: IEEE 1588-2019 compliant (timeSource=0x50)
