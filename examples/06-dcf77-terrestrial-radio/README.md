# DCF77 Terrestrial Radio Time Source for IEEE 1588-2019 PTP

## Overview

This example demonstrates synchronizing an IEEE 1588-2019 PTP clock with DCF77 longwave time signals from Germany.

**IEEE 1588-2019 Classification**:
- **timeSource**: 0x30 (TERRESTRIAL_RADIO)
- **clockClass**: 6 (Primary reference when synchronized)
- **clockAccuracy**: 0x29 (±1ms - synchronized to PTB atomic clocks)

## DCF77 Background

- **Frequency**: 77.5 kHz longwave
- **Location**: Mainflingen, Germany (50°01'N, 9°00'E)
- **Operator**: Physikalisch-Technische Bundesanstalt (PTB)
- **Coverage**: ~2000 km radius (Central Europe)
- **Accuracy**: ±1 millisecond to atomic time
- **Encoding**: Amplitude modulation, pulse-width encoding
- **Frame**: 59 bits per minute, minute marker

## Coverage Area

DCF77 can be received in:
- 🇩🇪 Germany (excellent)
- 🇦🇹 Austria (excellent) 
- 🇨🇭 Switzerland (excellent)
- 🇳🇱 Netherlands (good)
- 🇧🇪 Belgium (good)
- 🇫🇷 France (northern/eastern regions)
- 🇱🇺 Luxembourg (excellent)
- 🇨🇿 Czech Republic (western regions)
- 🇵🇱 Poland (western regions)
- 🇩🇰 Denmark (southern regions)

**Note**: Signal strength depends on distance, local interference, and time of day.

## Similar Systems Worldwide

Other terrestrial radio time signals:
- **WWVB** (USA): 60 kHz, Fort Collins, Colorado
- **MSF** (UK): 60 kHz, Anthorn, Cumbria
- **JJY** (Japan): 40 kHz / 60 kHz, dual stations
- **BPC** (China): 68.5 kHz, Shangqiu

This implementation can be adapted for these systems with protocol modifications.

## Hardware Requirements

### DCF77 Receiver Module

Popular modules:
1. **Pollin DCF1** - Low cost, good sensitivity
2. **Conrad DCF77** - Reliable, widely available
3. **HKW DCF77** - Industrial grade
4. **ELV DCF77** - High quality ferrite antenna

### Microcontroller

Compatible platforms:
- **ESP32** (recommended) - WiFi + GPIO
- **Arduino Uno/Mega** - Classic option
- **Raspberry Pi** - With GPIO access
- **STM32** - Industrial applications

### Wiring

```
DCF77 Module          Microcontroller
┌──────────┐         ┌──────────┐
│   VCC    │────────►│   3.3V   │  (or 5V depending on module)
│   GND    │────────►│   GND    │
│   PON    │────────►│   3.3V   │  (Power-on, tie high)
│   DATA   │────────►│   GPIO4  │  (with 10kΩ pull-up if needed)
└──────────┘         └──────────┘
```

**Important**: Some modules have open-collector output and need external 10kΩ pull-up resistor!

## Expected Performance

| Condition | Signal Quality | Accuracy | clockClass | clockAccuracy |
|-----------|---------------|----------|------------|---------------|
| Near transmitter (<500km) | Excellent | ±1ms | 6 | 0x29 |
| Medium distance (500-1000km) | Good | ±1ms | 6 | 0x29 |
| Far distance (1000-2000km) | Fair | ±1-5ms | 13 | 0x29 |
| Poor reception / holdover | Weak | Degraded | 52-187 | 0xFE |
| No signal | None | Not synced | 248 | 0xFE |

**Note**: Nighttime reception is typically better than daytime due to ionospheric propagation.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│          IEEE 1588-2019 PTP Grandmaster                 │
│                                                          │
│  ┌────────────────────────────────────────────────┐    │
│  │   DCF77 Time Source (timeSource=0x30)          │    │
│  │                                                 │    │
│  │  ┌─────────────┐      ┌──────────────────┐   │    │
│  │  │ DCF77 RX    │◄─────│  77.5 kHz Signal │   │    │
│  │  │  Module     │      │  from Mainflingen│   │    │
│  │  └─────────────┘      └──────────────────┘   │    │
│  │         │                                      │    │
│  │         ▼                                      │    │
│  │  ┌─────────────────┐                          │    │
│  │  │  Bit Decoder    │                          │    │
│  │  │ - Pulse width   │                          │    │
│  │  │ - Frame sync    │                          │    │
│  │  │ - BCD decode    │                          │    │
│  │  │ - Parity check  │                          │    │
│  │  └─────────────────┘                          │    │
│  │         │                                      │    │
│  │         ▼                                      │    │
│  │  ┌─────────────────┐                          │    │
│  │  │ Quality Manager │                          │    │
│  │  │ - Signal level  │                          │    │
│  │  │ - Decode errors │                          │    │
│  │  │ - Holdover time │                          │    │
│  │  └─────────────────┘                          │    │
│  └────────────────────────────────────────────────┘    │
│                       │                                 │
│                       ▼                                 │
│  ┌────────────────────────────────────────────────┐    │
│  │     PTP Clock Quality Attributes              │    │
│  │  clockClass: 6 (synchronized to atomic time)  │    │
│  │  clockAccuracy: 0x29 (±1ms)                   │    │
│  │  timeSource: 0x30 (TERRESTRIAL_RADIO)         │    │
│  └────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

## DCF77 Protocol Details

### Bit Encoding

- **Logic 0**: 100ms carrier reduction (10% or 25%)
- **Logic 1**: 200ms carrier reduction
- **Minute marker**: No reduction at second 59 (~2 seconds)

### Frame Structure (59 bits)

```
Bit 0: Start marker (always 0)
Bits 1-14: Weather data, call bit
Bit 15: Antenna fault indicator
Bit 16: Reserved
Bit 17-18: CET/CEST indicators
Bit 19: Leap second announcement
Bit 20: Start of time code (always 1)
Bits 21-27: Minutes (BCD)
Bit 28: Even parity for minutes
Bits 29-34: Hours (BCD)
Bit 35: Even parity for hours
Bits 36-41: Day of month (BCD)
Bits 42-44: Day of week (BCD)
Bits 45-49: Month (BCD)
Bits 50-57: Year (BCD, within century)
Bit 58: Even parity for date
Bit 59: (Minute marker, no pulse)
```

## Files

- `dcf77_adapter.hpp` - DCF77 adapter interface (USES library's Types::ClockQuality!)
- `dcf77_adapter.cpp` - DCF77 decoder implementation
- `dcf77_ptp_sync_example.cpp` - Example program
- `CMakeLists.txt` - Build configuration
- `README.md` - This file

## Building

```bash
cd d:\Repos\IEEE_1588_2019\examples\06-dcf77-terrestrial-radio
cmake -S . -B build
cmake --build build --config Release
```

Or add to main project:

```bash
cd d:\Repos\IEEE_1588_2019
cmake --build build --config Release --target dcf77_ptp_sync_example
```

## Usage

### Basic Example

```cpp
#include "dcf77_adapter.hpp"
#include "clocks.hpp"

using namespace Examples::DCF77;

// Create DCF77 adapter (USES library types!)
DCF77Adapter dcf77(4, false);  // GPIO4, normal polarity

// Initialize
if (!dcf77.initialize()) {
    std::cerr << "DCF77 initialization failed!\n";
    return 1;
}

// Main loop (call frequently!)
while (running) {
    // Process DCF77 signal
    if (dcf77.update()) {
        // New frame decoded!
        auto frame = dcf77.get_last_frame();
        
        // Get clock quality using LIBRARY's Types::ClockQuality
        Types::ClockQuality quality = dcf77.get_clock_quality();
        
        // Update PTP clock with library types (not duplicated!)
        auto& ds = ptp_clock.get_default_data_set();
        ds.clockQuality = quality;  // Direct assignment
        
        auto& tp = ptp_clock.get_time_properties_data_set();
        tp.timeSource = static_cast<uint8_t>(
            Types::TimeSource::Terrestrial_Radio);
    }
    
    // Call update() frequently (every 50ms or faster)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
```

### Integration with PTP Clock

```cpp
// This adapter USES library types, does NOT recreate them!
// ✅ CORRECT - using library's Types::ClockQuality
Types::ClockQuality quality = dcf77.get_clock_quality();

// ✅ CORRECT - using library's Types::TimeSource enum
Types::TimeSource source = DCF77Adapter::get_time_source();  
// Returns Types::TimeSource::Terrestrial_Radio

// Update PTP clock directly with library types
auto& ds = ptp_clock.get_default_data_set();
ds.clockQuality = quality;  // No conversion needed!

auto& tp = ptp_clock.get_time_properties_data_set();
tp.timeSource = static_cast<uint8_t>(source);  // 0x30
```

## Advantages vs GPS/NTP

✅ **No network required** - works offline  
✅ **Legal time source** - official government time  
✅ **Low cost hardware** - ~$10-20 for receiver  
✅ **Indoor operation** - small ferrite antenna  
✅ **Atomic clock accuracy** - ±1ms to PTB  
✅ **Automatic DST** - CET/CEST switching encoded  
✅ **Leap second warning** - 1 month advance notice  

❌ **Regional coverage** - only Central Europe  
❌ **Slower acquisition** - 1-2 minutes for first sync  
❌ **Interference sensitive** - affected by electrical noise  
❌ **Night/day variation** - signal strength varies  

## Use Cases

**Ideal for**:
- Central European installations
- Offline/air-gapped systems
- Legal time source requirement
- Indoor locations (with good antenna)
- Cost-sensitive projects

**Not suitable for**:
- Locations outside ~2000km range
- High-interference industrial environments
- Sub-millisecond timing (use GPS+PPS)
- Mobile/vehicle applications

## Troubleshooting

### No signal received

- Check antenna orientation (rotate for best signal)
- Move away from electronics/monitors (interference)
- Try different location (near window better)
- Check if within coverage area (~2000km)
- Wait for nighttime (better propagation)

### Poor decode rate

- Improve antenna placement
- Add ferrite core to receiver power supply
- Ground the receiver module properly
- Check for correct GPIO pull-up/pull-down

### Wrong polarity

- Some modules output active-low signal
- Set `invert_signal=true` in constructor
- Check datasheet for your specific module

### Intermittent sync

- Normal behavior during daytime at distance
- Implement holdover mode (use local oscillator)
- Monitor signal strength statistics

## Antenna Optimization

### Placement

- **Best**: Near window, away from electronics
- **Good**: Indoor, away from metal structures
- **Avoid**: Near computers, monitors, switch-mode PSUs
- **Avoid**: Metal enclosures without external antenna

### Orientation

- DCF77 antenna is directional (ferrite bar)
- Rotate for maximum signal strength
- Typically perpendicular to Mainflingen direction

## References

- **IEEE 1588-2019**: Section 8.6.2.7, Table 6
- **PTB DCF77 Specification**: Physikalisch-Technische Bundesanstalt
- **DCF77 Protocol**: Technical documentation available from PTB
- **Coverage Map**: <https://www.ptb.de/cms/en/ptb/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77.html>

---

**Status**: Implementation ready for development  
**Accuracy**: ±1ms (synchronized to PTB atomic clocks)  
**Hardware**: DCF77 receiver module (~$10-20) + microcontroller  
**Coverage**: Central Europe (~2000 km from Mainflingen, Germany)  
**Standards**: IEEE 1588-2019 compliant (timeSource=0x30)
