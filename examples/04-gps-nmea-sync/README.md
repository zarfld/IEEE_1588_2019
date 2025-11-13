# GPS NMEA Time Synchronization Example

This example demonstrates IEEE 1588-2019 PTP clock synchronization using GPS as an external time reference via NMEA-0183 serial protocol.

## Overview

The GPS NMEA synchronization example:
- Reads NMEA sentences from GPS module over serial connection
- Parses GPRMC and GPGGA sentences with checksum validation
- Converts GPS UTC time to IEEE 1588-2019 PTP timestamps (TAI)
- Calculates clock offset between system time and GPS reference
- Displays synchronization accuracy in real-time

**Target Accuracy**: ±100 microseconds synchronization with GPS time

## Hardware Requirements

### GPS Module
- **Recommended**: u-blox NEO-6M, NEO-7M, or similar GPS module
- **Output**: NMEA-0183 sentences at 9600 baud
- **Antenna**: GPS antenna with clear sky view (outdoor or window-mounted)
- **Power**: 3.3V or 5V (check module specifications)

### Serial Interface
- **Windows**: USB-to-TTL serial adapter (FTDI FT232, CP2102, CH340, etc.)
- **Linux**: USB-to-TTL adapter or direct UART connection

### Wiring Diagram

```
GPS Module (NEO-6M)         USB-to-TTL Adapter
┌────────────────┐          ┌──────────────────┐
│                │          │                  │
│  VCC  ────────────────────  VCC (3.3V/5V)   │
│  GND  ────────────────────  GND              │
│  TX   ────────────────────  RX               │
│  RX   (not connected)       TX               │
│                │          │                  │
└────────────────┘          └──────────────────┘
                                    │
                                    │ USB
                                    │
                              ┌─────▼──────┐
                              │ Computer   │
                              │ COM3 (Win) │
                              │ /dev/ttyUSB0│
                              └────────────┘
```

**Important Notes**:
- Connect GPS module **TX** to adapter **RX** (data flows from GPS to computer)
- GPS module **RX** can remain unconnected (one-way communication)
- Ensure voltage compatibility (3.3V vs 5V) between GPS module and adapter
- Place GPS antenna with clear view of sky for best results (outdoors or near window)

## Software Requirements

### Build Dependencies
- CMake 3.16 or later
- C++14 compatible compiler
- IEEE 1588-2019 PTP library (included in parent project)

### Runtime Requirements
- **Windows**: Windows 7 or later
- **Linux**: Any modern Linux distribution with termios support

## Building

The GPS NMEA example is built as part of the main IEEE 1588-2019 project:

```bash
# From repository root
cd build
cmake ..
cmake --build . --config Release --target gps_nmea_sync_example

# Executable location:
# Windows: build/examples/04-gps-nmea-sync/Release/gps_nmea_sync_example.exe
# Linux: build/examples/04-gps-nmea-sync/gps_nmea_sync_example
```

### Building Tests

```bash
# Build all GPS NMEA tests
cmake --build . --config Release --target test_nmea_parser
cmake --build . --config Release --target test_gps_time_converter  
cmake --build . --config Release --target test_integration

# Run tests
ctest -C Release --output-on-failure
```

## Usage

### Basic Usage

**Windows**:
```bash
cd build\examples\04-gps-nmea-sync\Release
gps_nmea_sync_example.exe COM3
```

**Linux**:
```bash
cd build/examples/04-gps-nmea-sync
./gps_nmea_sync_example /dev/ttyUSB0
```

### Command-Line Options

```
gps_nmea_sync_example <serial_port> [options]

Arguments:
  serial_port    Serial port name
                 Windows: COM1, COM3, COM10, etc.
                 Linux: /dev/ttyUSB0, /dev/ttyS0, etc.

Options:
  --baud <rate>  Baud rate (default: 9600)
  --help         Show help message
```

### Example Output

```
GPS NMEA Time Synchronization Example
======================================

Serial Port: COM3
Baud Rate:   9600

Opening serial port...
Serial port opened successfully
Waiting for GPS NMEA sentences...

========================================
GPS Synchronization Status
========================================
GPS Time (UTC): 08:32:18.00
GPS Date:       2025-11-13
Fix Status:     TIME_ONLY
Satellites:     3
GPS PTP Time:   1731483138.000000000 (TAI)
System Time:    1731483138.000045300 (TAI)
Clock Offset:   -45.300 μs (system ahead of GPS)
Sync Quality:   EXCELLENT (within ±100 μs target)
========================================
```

## Hardware Setup Guide

### Step 1: Connect GPS Module

1. **Identify GPS Module Pins**:
   - VCC (power): 3.3V or 5V
   - GND (ground)
   - TX (transmit data from GPS)
   - RX (receive data to GPS) - not needed for this example

2. **Connect to USB-to-TTL Adapter**:
   - GPS VCC → Adapter VCC (match voltage: 3.3V or 5V)
   - GPS GND → Adapter GND
   - GPS TX → Adapter RX (⚠️ cross connection!)
   - GPS RX → Leave disconnected (one-way communication)

3. **Connect Antenna**:
   - Attach GPS antenna to GPS module
   - Place antenna with clear sky view (outdoor or window-mounted)
   - **Important**: GPS requires direct view of satellites

### Step 2: Install USB Serial Driver

**Windows**:
- Download driver for your USB-to-TTL adapter:
  - FTDI FT232: https://ftdichip.com/drivers/
  - CP2102: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
  - CH340: Search "CH340 driver" online
- Install driver and reboot if required
- Check Device Manager → Ports (COM & LPT) for COM port number

**Linux**:
- Most USB-to-TTL adapters work out-of-the-box
- Check for device: `ls /dev/ttyUSB*` or `dmesg | grep tty`
- Add user to dialout group if needed: `sudo usermod -a -G dialout $USER`

### Step 3: Test GPS Module

Before running the example, verify GPS module is working:

**Windows** (PowerShell):
```powershell
# Open serial port and display data
mode COM3 BAUD=9600 PARITY=n DATA=8
Get-Content -Path COM3
# You should see NMEA sentences like:
# $GPRMC,083218.00,V,,,,,,,131125,,,N*78
# $GPGGA,083217.00,,,,,0,00,99.99,,,,,,*69
# Press Ctrl+C to stop
```

**Linux**:
```bash
# Install screen utility
sudo apt-get install screen

# Open serial port
screen /dev/ttyUSB0 9600

# You should see NMEA sentences
# Press Ctrl+A, K to exit
```

### Step 4: Wait for GPS Fix

- GPS modules require 30-60 seconds for initial fix (cold start)
- With antenna indoors, GPS may only achieve "time-only" mode (sufficient for this example)
- Outdoor placement provides best results (full fix with position)

**NMEA Status Indicators**:
- `$GPRMC,...,V,...` - Time-only mode (valid for PTP sync)
- `$GPRMC,...,A,...` - Full GPS fix with position
- `$GPGGA,...,0,...` - No fix (quality = 0)
- `$GPGGA,...,1,...` - GPS fix (quality = 1)

## Understanding GPS Time Synchronization

### GPS Time vs UTC vs TAI

- **GPS Time**: Monotonic time scale used by GPS satellites
  - Started synchronized with UTC on 1980-01-06 00:00:00
  - No leap seconds applied (stays ahead of UTC)
  - Current offset: GPS = UTC + 18 seconds (as of 2017)

- **UTC (Coordinated Universal Time)**: Standard civil time
  - Leap seconds added periodically to match Earth rotation
  - GPS modules typically report time in UTC

- **TAI (International Atomic Time)**: IEEE 1588-2019 timescale
  - Monotonic atomic time (no leap seconds)
  - TAI = UTC + 37 seconds (as of 2017)
  - PTP uses TAI for timestamps

### Time Conversion Process

This example performs the following conversions:

1. **Read NMEA**: GPS module reports UTC time
   ```
   $GPRMC,083218.00,V,,,,,,,131125,,,N*78
   Time: 08:32:18.00 UTC
   Date: 2025-11-13
   ```

2. **Parse**: Extract time and date fields
   ```
   Hours: 8, Minutes: 32, Seconds: 18, Centiseconds: 0
   Year: 2025, Month: 11, Day: 13
   ```

3. **Convert to Unix timestamp**: Calculate seconds since 1970-01-01 00:00:00 UTC
   ```
   UTC Unix timestamp: 1731483138 seconds
   ```

4. **Convert UTC to TAI**: Add TAI-UTC offset (+37 seconds)
   ```
   PTP timestamp (TAI): 1731483175 seconds, 0 nanoseconds
   ```

5. **Compare with system clock**: Calculate offset
   ```
   System time: 1731483175.000045300 (TAI)
   GPS time:    1731483175.000000000 (TAI)
   Offset:      -45.3 μs (system ahead)
   ```

### Timing Accuracy

**GPS Timing Accuracy**:
- **1PPS signal**: ±10-50 nanoseconds (not used in this example)
- **NMEA time**: ±100 nanoseconds to ±1 microsecond
- **NMEA resolution**: 10 milliseconds (centisecond precision)

**This Example's Accuracy**:
- **Target**: ±100 microseconds synchronization
- **Limiting Factors**:
  - NMEA centisecond resolution (10ms)
  - Serial communication latency (1-10ms)
  - System clock resolution (typically 1μs)
- **Real-World Performance**: ±50-500 microseconds typical

**For Higher Accuracy**:
- Use GPS 1PPS (pulse-per-second) signal with hardware timestamping
- Implement PTP hardware timestamps (IEEE 1588 boundary clock)
- Use disciplined oscillator (GPSDO) for long-term stability

## Troubleshooting

### GPS Module Not Responding

**Symptom**: No NMEA sentences received (timeout messages)

**Solutions**:
1. **Check Wiring**:
   - Verify GPS TX → Adapter RX connection
   - Ensure GND is connected between GPS and adapter
   - Check voltage compatibility (3.3V vs 5V)

2. **Verify Serial Port**:
   - Windows: Check Device Manager for correct COM port
   - Linux: Run `ls /dev/ttyUSB*` to find device
   - Try different USB port if needed

3. **Test with Serial Terminal**:
   - Use PuTTY (Windows) or screen (Linux) to verify GPS output
   - Baud rate: 9600, 8 data bits, no parity, 1 stop bit

4. **Check GPS Module Power**:
   - Verify GPS module LED is blinking (indicates power)
   - Some modules require separate power supply (not from USB adapter)

### No GPS Fix (Time-Only Mode)

**Symptom**: `Fix Status: TIME_ONLY` or `NO_FIX`

**Solutions**:
1. **Improve Antenna Placement**:
   - Move antenna closer to window or outdoors
   - Clear line-of-sight to sky (avoid metal roofs, dense tree cover)
   - GPS requires view of at least 4 satellites for full fix

2. **Wait for Satellite Acquisition**:
   - Cold start: 30-60 seconds
   - Warm start (powered on recently): 10-30 seconds
   - Check `Satellites:` count in output (need ≥4 for position fix)

3. **Time-Only Mode is Sufficient**:
   - This example only requires GPS time, not position
   - `TIME_ONLY` mode provides valid UTC time for PTP synchronization
   - Full GPS fix improves accuracy slightly but not required

### Large Clock Offset

**Symptom**: `Clock Offset: >10 ms`

**Solutions**:
1. **Synchronize System Clock**:
   - Windows: Settings → Time & Language → Sync now
   - Linux: `sudo ntpdate pool.ntp.org` or enable NTP service

2. **Check Time Zone**:
   - GPS reports UTC time
   - System time must be set to correct timezone
   - Verify system is using correct UTC offset

3. **Serial Latency**:
   - USB-to-TTL adapters add 1-10ms latency
   - This is normal and expected
   - Offset <10ms is acceptable for this example

### Checksum Errors

**Symptom**: Frequent checksum validation failures

**Solutions**:
1. **Check Baud Rate**:
   - GPS modules typically use 9600 baud
   - Some modules support 4800 or 115200 baud
   - Try different baud rates with `--baud` option

2. **Cable Quality**:
   - Use shorter USB cables (<2 meters)
   - Avoid running cables near power supplies or motors
   - Try different USB-to-TTL adapter

3. **Electrical Noise**:
   - Ensure proper grounding between GPS and adapter
   - Add ferrite bead to USB cable if needed

## Technical Implementation

### Architecture

```
┌─────────────────────────────────────────────┐
│ GPS NMEA Synchronization Application       │
│  gps_nmea_sync_example.cpp                  │
├─────────────────────────────────────────────┤
│ NMEA Parser                  Time Converter │
│  nmea_parser.hpp             gps_time_      │
│  nmea_parser.cpp             converter.hpp  │
│                              gps_time_      │
│                              converter.cpp  │
├─────────────────────────────────────────────┤
│ Serial HAL Interface                        │
│  serial_hal_interface.hpp                   │
├─────────────────────────────────────────────┤
│ Platform-Specific Implementations           │
│  serial_hal_windows.cpp (Win32 API)         │
│  serial_hal_linux.cpp (termios)             │
└─────────────────────────────────────────────┘
         │                        │
         │ COM3/COM4              │ /dev/ttyUSB0
         ▼                        ▼
┌─────────────────────────────────────────────┐
│ USB-to-TTL Adapter                          │
│ (FTDI FT232 / CP2102 / CH340)               │
└─────────────────────────────────────────────┘
                    │
                    │ UART (TX/RX)
                    ▼
┌─────────────────────────────────────────────┐
│ GPS Module (u-blox NEO-6M / NEO-7M)         │
│ Outputs: NMEA-0183 sentences at 9600 baud   │
└─────────────────────────────────────────────┘
                    │
                    │ RF
                    ▼
┌─────────────────────────────────────────────┐
│ GPS Antenna                                 │
│ (Active or Passive)                         │
└─────────────────────────────────────────────┘
```

### NMEA Sentence Format

**GPRMC** (Recommended Minimum Specific GPS/Transit Data):
```
$GPRMC,083218.00,V,,,,,,,131125,,,N*78
  │       │      │ │ │ │ │ │  │   │ │└─ Checksum
  │       │      │ │ │ │ │ │  │   │ └─── Mode indicator (N=no fix)
  │       │      │ │ │ │ │ │  │   └───── Magnetic variation (empty)
  │       │      │ │ │ │ │ │  └───────── Date (DDMMYY)
  │       │      │ │ │ │ │ └──────────── Course over ground (empty)
  │       │      │ │ │ │ └────────────── Speed over ground (empty)
  │       │      │ │ │ └──────────────── Longitude direction (empty)
  │       │      │ │ └────────────────── Longitude (empty)
  │       │      │ └──────────────────── Latitude direction (empty)
  │       │      └────────────────────── Latitude (empty)
  │       └───────────────────────────── Status (A=valid, V=warning)
  └───────────────────────────────────── UTC Time (HHMMSS.SS)
```

**GPGGA** (Global Positioning System Fix Data):
```
$GPGGA,083217.00,,,,,0,00,99.99,,,,,,*69
  │       │      │ │ │ │ │  │    │ │││││└─ Checksum
  │       │      │ │ │ │ │  │    │ └┴┴┴┴── DGPS fields (empty)
  │       │      │ │ │ │ │  │    └─────── Altitude units (M=meters)
  │       │      │ │ │ │ │  └──────────── Altitude above sea level (empty)
  │       │      │ │ │ │ └─────────────── HDOP (99.99=invalid)
  │       │      │ │ │ └───────────────── Satellites in use (0)
  │       │      │ │ └─────────────────── GPS quality (0=no fix)
  │       │      │ └───────────────────── Longitude direction (empty)
  │       │      └─────────────────────── Longitude (empty)
  │       └────────────────────────────── UTC Time (HHMMSS.SS)
  └────────────────────────────────────── Sentence ID
```

### State Machine

GPS Parser maintains a state machine for fix status tracking:

```
┌─────────────┐
│   NO_FIX    │◄───┐
└──────┬──────┘    │
       │           │ Lost signal for >10 sentences
       │ V=warning │
       ▼           │
┌─────────────┐    │
│ TIME_ONLY   │────┤
└──────┬──────┘    │
       │           │
       │ A=active  │
       ▼           │
┌─────────────┐    │
│ GPS_FIX     │────┤
└──────┬──────┘    │
       │           │
       │ Quality=2 │
       ▼           │
┌─────────────┐    │
│  DGPS_FIX   │────┤
└──────┬──────┘    │
       │           │
       │ V=warning │
       ▼           │
┌─────────────┐    │
│SIGNAL_LOST  │────┘
└─────────────┘
```

## Files

- **GPS_NMEA_Integration_Spec.md** - Initial specification document
- **GPS_NMEA_Specification_Refinement.md** - Refined specification based on real data
- **serial_hal_interface.hpp** - Platform-independent serial port interface
- **serial_hal_windows.cpp** - Windows (Win32 API) serial port implementation
- **serial_hal_linux.cpp** - Linux (termios) serial port implementation
- **nmea_parser.hpp** - NMEA-0183 sentence parser interface
- **nmea_parser.cpp** - NMEA parser implementation
- **gps_time_converter.hpp** - GPS time to PTP timestamp converter interface
- **gps_time_converter.cpp** - Time converter implementation
- **gps_nmea_sync_example.cpp** - Main application
- **tests/test_nmea_parser.cpp** - NMEA parser unit tests
- **tests/test_gps_time_converter.cpp** - Time converter unit tests
- **tests/test_integration.cpp** - End-to-end integration tests
- **README.md** - This documentation file

## Further Reading

### IEEE Standards
- **IEEE 1588-2019**: Precision Time Protocol (PTP)
  - Section 7.2: Timescales (TAI, UTC, GPS)
  - Section 5.3.3: Timestamp format

### GPS Documentation
- **NMEA-0183 Standard**: GPS/GNSS sentence format specification
- **u-blox GPS Receivers**: https://www.u-blox.com/en/docs
- **GPS Time**: https://www.gps.gov/technical/icwg/

### Related Examples
- **01-basic-ordinary-clock** - Basic PTP Ordinary Clock example
- **02-basic-boundary-clock** - Basic PTP Boundary Clock example
- **03-ptp-synchronization** - PTP network synchronization example

## License

This GPS NMEA synchronization example is part of the IEEE 1588-2019 PTP Library project.

See parent project LICENSE file for details.

## Support

For issues, questions, or contributions:
- **GitHub Issues**: https://github.com/zarfld/IEEE_1588_2019/issues
- **Documentation**: See `docs/` directory in parent project
- **IEEE 1588-2019 Standard**: Available from IEEE Standards Association

---

**Version**: 1.0.0  
**Last Updated**: November 13, 2025  
**Author**: IEEE 1588-2019 PTP Library Contributors
