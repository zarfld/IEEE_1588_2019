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

#### Recommended Module: GT-U7 GPS Module
- **Chipset**: Original 7th generation GPS chip (compatible with u-blox NEO-6M/NEO-7M)
- **Operating Voltage**: 3.6V-5V (direct USB power supported)
- **Baud Rate**: 9600 bps default (configurable)
- **USB Interface**: Direct USB connection via built-in USB-to-Serial (no external adapter needed)
- **PPS Output**: Pin 3 (TIMEPULSE) - 1Hz pulse-per-second signal for high-precision timing
- **NMEA Output**: Fully compatible with NEO-6M format (GPRMC, GPGGA, etc.)
- **Antenna**: IPEX antenna interface with active standard antenna included
- **Features**:
  - Integrated EEPROM for parameter storage
  - Backup battery for data retention
  - LED signal indicator
  - High sensitivity tracking (works in urban/forest environments)

**Alternative Compatible Modules**:
- u-blox NEO-6M (NMEA only, no built-in USB)
- u-blox NEO-7M (NMEA only, no built-in USB)
- Any GPS module with NMEA-0183 output at 9600 baud

### Serial Interface

#### GT-U7 Module (Recommended)
- **Connection**: Direct USB connection (built-in USB-to-Serial converter)
- **Windows**: Appears as COM port (check Device Manager)
- **Linux**: Appears as /dev/ttyUSB0 or /dev/ttyACM0
- **No external adapter needed** - module has integrated USB interface

#### Other GPS Modules (NEO-6M/NEO-7M)
- **Windows**: USB-to-TTL serial adapter (FTDI FT232, CP2102, CH340, etc.)
- **Linux**: USB-to-TTL adapter or direct UART connection

### Wiring Diagrams

#### Option 1: GT-U7 Module (Direct USB - Recommended)

```
GT-U7 GPS Module (GoodTech GT-U7)
┌───────────────────────────────────┐
│ ┌─────────────┐                   │
│ │   GT-U7     │  Pin 3: TIMEPULSE │◄─── 1Hz PPS signal (hardware timestamping)
│ │   GPS Chip  │  (1Hz PPS output) │
│ └─────────────┘                   │
│                                   │
│  IPEX Antenna ─┐                  │
│  Connector     │                  │
│                                   │
│  USB Port ─────┼──────────────────┤
│  (Micro-USB)   │                  │
└────────────────┼──────────────────┘
                 │
                 │ USB Cable (Micro-USB to USB-A)
                 │
           ┌─────▼──────────────────┐
           │ Computer               │
           │ Windows: COM3/COM4     │
           │ Linux: /dev/ttyUSB0    │
           └────────────────────────┘
```

**Pin Configuration (GT-U7 Module)**:
```
Pin 1:  NC          (Not Connected)
Pin 2:  SS_N        (Chip Select, not used)
Pin 3:  TIMEPULSE   ◄── PPS output (1Hz, 3.3V, 100ms pulse width)
Pin 4:  EXTINT0     (External Interrupt, not used)
Pin 5:  USB_DM      (USB Data -, internal connection)
Pin 6:  USB_DP      (USB Data +, internal connection)
Pin 7:  VDD_USB     (USB Power, internal connection)
Pin 8:  NC          (Not Connected)
Pin 9:  VCC_RF      (RF Power)
Pin 10: GND         (Ground)
Pin 11: RF_IN       (Antenna Input)
Pin 12: GND         (Ground)
Pin 13: GND         (Ground)
Pin 14: MOSI        (SPI, not used)
Pin 15: MISO        (SPI, not used)
Pin 16: CFG_GPS0    (Configuration)
Pin 17: NC          (Not Connected)
Pin 18: SDA2        (I2C Data)
Pin 19: SCL2        (I2C Clock)
Pin 20: TxD1        (UART TX - connected to USB converter internally)
Pin 21: RxD1        (UART RX - connected to USB converter internally)
Pin 22: V_BCKP      (Backup Battery)
Pin 23: VCC         (Power: 3.6V-5V)
Pin 24: GND         (Ground)
```

**PPS Hardware Timestamping Setup**:
For sub-microsecond timing accuracy, connect Pin 3 (TIMEPULSE) to:
- **Windows**: Serial port DCD (Pin 1), CTS (Pin 8), or DSR (Pin 6) for hardware interrupt
- **Linux**: GPIO pin with interrupt capability, or serial port control lines
- See "PPS Hardware Validation" section below for wiring details

#### Option 2: NEO-6M/NEO-7M Module (External USB Adapter)

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
- **GT-U7 Module**: Simply connect USB cable, no wiring needed for NMEA data
- **NEO-6M/NEO-7M**: Connect GPS module **TX** to adapter **RX** (data flows from GPS to computer)
- **PPS Signal**: GT-U7 Pin 3 (TIMEPULSE) provides 1Hz PPS for hardware timestamping (optional, for sub-μs accuracy)
- **Power**: GT-U7 powered directly from USB (3.6V-5V), no external power supply needed
- **Antenna**: Place with clear view of sky for best results (outdoors or near window)

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

## PPS Hardware Validation (Advanced)

### Overview

For **sub-microsecond timing accuracy**, the GT-U7 GPS module provides a **PPS (Pulse-Per-Second)** signal on Pin 3 (TIMEPULSE). This 1Hz signal marks the precise start of each UTC second with **±10-50 nanosecond accuracy**.

**Timing Comparison**:
- **NMEA only**: ±10 milliseconds (limited by serial communication and centisecond resolution)
- **NMEA + PPS**: ±1 microsecond (PPS provides precise second boundary, NMEA provides absolute time)

### Hardware Requirements

**GT-U7 Module**:
- Pin 3 (TIMEPULSE) - 1Hz PPS output signal
- 3.3V logic level, 100ms pulse width (100ms high, 900ms low)
- Triggered at UTC second boundary (rising edge = start of second)

**Serial Port Connection**:
For hardware timestamping, connect PPS signal to serial port control lines:
- **DCD (Pin 1)** - Data Carrier Detect (recommended)
- **CTS (Pin 8)** - Clear To Send (alternative)
- **DSR (Pin 6)** - Data Set Ready (alternative)

### Wiring Diagram (PPS Hardware Timestamping)

```
GT-U7 GPS Module                      DB9 Serial Port (or USB-to-Serial Adapter)
┌────────────────────┐                ┌──────────────────────────────────┐
│                    │                │                                  │
│  Pin 3: TIMEPULSE  ├────────────────┤  Pin 1: DCD (Data Carrier Detect)│
│  (1Hz PPS output)  │                │  (Interrupt on rising edge)      │
│                    │                │                                  │
│  Pin 24: GND       ├────────────────┤  Pin 5: GND (Signal Ground)      │
│                    │                │                                  │
│  Pin 20: TxD1      ├────────────────┤  Pin 2: RXD (Receive Data)       │
│  (NMEA sentences)  │  (via USB)     │  (NMEA data for absolute time)   │
│                    │                │                                  │
│  Pin 23: VCC       ├────────────────┤  USB Power (5V)                  │
│  (3.6V-5V)         │                │                                  │
└────────────────────┘                └──────────────────────────────────┘

Alternative Wiring (Without USB-to-Serial Adapter):
┌────────────────────┐                ┌──────────────────────────────────┐
│ GT-U7 Module       │                │ PC Serial Port COM3              │
│                    │                │                                  │
│  Pin 3: TIMEPULSE  ├────────────────┤  DCD (Pin 1) via jumper wire     │
│  Pin 24: GND       ├────────────────┤  GND (Pin 5) via jumper wire     │
│                    │                │                                  │
│  USB (Micro-USB)   ├────────────────┤  USB Port (for NMEA data + power)│
└────────────────────┘                └──────────────────────────────────┘
```

**Important**: 
- GT-U7 PPS output is **3.3V logic** - ensure serial port control lines support 3.3V inputs
- Most modern USB-to-serial adapters support 3.3V/5V mixed logic
- Legacy RS-232 ports may require level shifter (3.3V → ±12V)

### Running Hardware Validation Tests

**Step 1: Connect Hardware**
```bash
# Connect GT-U7 module:
# 1. USB cable to PC (provides NMEA data + power)
# 2. Jumper wire: Pin 3 (TIMEPULSE) → Serial port DCD (Pin 1)
# 3. Jumper wire: Pin 24 (GND) → Serial port GND (Pin 5)
```

**Step 2: Identify COM Port**
```bash
# Windows: Check Device Manager → Ports (COM & LPT)
# Example: "USB-SERIAL CH340 (COM3)"

# Linux: List USB serial devices
ls /dev/ttyUSB* /dev/ttyACM*
```

**Step 3: Run Hardware Tests**
```bash
# Fast API tests (no hardware, ~0.03s)
ctest -C Release -L fast

# Hardware PPS validation tests (requires GPS, 2-30s)
ctest -C Release -L hardware

# Or run directly:
# Windows:
.\build\examples\04-gps-nmea-sync\Release\test_pps_hardware.exe

# Linux:
./build/examples/04-gps-nmea-sync/test_pps_hardware
```

### Expected Test Output (With GPS Hardware)

```
============================================================================
GPS PPS Detector - Hardware Validation Tests
============================================================================

⚠️  HARDWARE REQUIRED: These tests need real GPS hardware!

Requirements:
  • u-blox NEO-G7 GPS module or GT-U7 compatible
  • Serial connection on COM3
  • PPS signal connected to DCD (Pin 1), CTS (Pin 8), or DSR (Pin 6)
  • GPS must have satellite lock (PPS LED blinking @ 1Hz)

Expected test duration: 10-20 seconds (waiting for real PPS pulses)
============================================================================

=== Test 1: Serial Port Availability ===
Attempting to open COM3...
✓ Opened COM3 successfully
PASS: Serial port available and configured

=== Test 2: Real PPS Signal Detection ===
🔄  This test requires 2-5 seconds to detect 3 edges @ 1Hz
✓ Opened COM3 successfully

Starting PPS autodetection (10s timeout)...
Monitoring pins: DCD (Pin 1), CTS (Pin 8), DSR (Pin 6)
[PPS] PPS autodetection started, monitoring DCD/CTS/DSR

Waiting for PPS detection...
...[PPS] Rising edge detected on DCD
...[PPS] Rising edge detected on DCD (interval: 1.0023s)
...[PPS] Rising edge detected on DCD (interval: 0.9987s)
[PPS] PPS locked! Frequency: 1.00 Hz

✓ PPS detected in 3.2 seconds

PPS Statistics:
  Total edges:      3
  Valid intervals:  2
  Avg interval:     1.0005 s
  Frequency:        0.9995 Hz
  Detection time:   3200 ms
  
PASS: Real PPS signal validated

=== Test 3: PPS Timestamp Acquisition ===
✓ Opened COM3 successfully
Starting PPS detection...
[PPS] PPS autodetection started, monitoring DCD/CTS/DSR
[PPS] PPS locked! Frequency: 1.00 Hz

Acquiring 3 consecutive PPS timestamps...
  Timestamp 1: 1731483138.000000000 TAI
  Timestamp 2: 1731483139.000000000 TAI (interval: 1.000000 s)
  Timestamp 3: 1731483140.000000000 TAI (interval: 1.000000 s)

Interval validation:
  Interval 1→2: 1.000000 s ✓ (within 0.8-1.2s tolerance)
  Interval 2→3: 1.000000 s ✓ (within 0.8-1.2s tolerance)
  
PASS: PPS timestamps acquired with correct timing

=== Test 4: Detection Timeout Behavior ===
Note: This test verifies timeout logic, not PPS detection
Starting detection with 2s timeout (expected to fail)...
Detection timed out after 2000 ms (expected)
PASS: Timeout behavior validated

============================================================================
Test Summary
============================================================================
Tests Passed:  4
Tests Failed:  0
Tests Skipped: 0
Total Tests:   4

✅ ALL TESTS PASSED
```

### Troubleshooting PPS Hardware Tests

**"PPS not detected" (Test 2 fails after 10s)**:

1. **Check PPS wiring**:
   - Verify Pin 3 (TIMEPULSE) connected to serial port DCD
   - Ensure GND connected between GPS and serial port
   - Check for loose connections or broken wires

2. **Verify GPS has satellite lock**:
   - Check GPS LED is blinking at 1Hz (PPS indicator)
   - If LED solid or off, GPS has no satellite lock
   - Move antenna outdoors or near window for better signal

3. **Test PPS signal with multimeter/oscilloscope**:
   - Measure voltage on Pin 3: Should toggle 0V → 3.3V → 0V every second
   - Pulse width: 100ms high, 900ms low (10% duty cycle)
   - If no signal, GPS module may be defective

4. **Check serial port control line support**:
   - Some USB-to-serial adapters don't support DCD/CTS/DSR hardware interrupts
   - Try different adapter (FTDI FT232 highly recommended)
   - Verify adapter driver supports control line monitoring

**"Test duration 0.03s" (suspiciously fast)**:
- This means test skipped due to missing hardware
- Check COM port is correct (Windows: Device Manager, Linux: `ls /dev/ttyUSB*`)
- Ensure USB cable connected and GPS powered on

**"Timestamp intervals >1.2s or <0.8s" (Test 3 fails)**:
- GPS clock drift or instability (unlikely with satellite lock)
- System clock issues (check `ntpdate` on Linux, time sync on Windows)
- Hardware interference (move GPS away from power supplies, motors)

### PPS Timing Accuracy Analysis

**Without PPS** (NMEA only):
```
GPS NMEA: "083218.00" (centisecond resolution = ±10ms)
Serial latency: 1-10ms variable
System timestamp: When NMEA sentence received
Total accuracy: ±10-20 milliseconds
```

**With PPS** (Hardware timestamping):
```
GPS PPS: Rising edge at UTC second boundary (±10-50ns GPS clock accuracy)
Hardware interrupt: <1μs response time
System timestamp: When DCD interrupt triggered
Total accuracy: ±1 microsecond
```

**Combined NMEA + PPS**:
```
PPS provides:   Precise second boundary (sub-μs)
NMEA provides:  Absolute time (which second, which day)
Result:         Nanosecond-accurate PTP timestamp
```

### Integration with PTP Clock (Future)

Once PPS detection is integrated with GPS time converter:

```cpp
// GPS Time Converter with PPS enhancement
GPSTimeConverter converter;
PPSDetector pps_detector(serial_handle);
converter.set_pps_detector(&pps_detector);

// Parse NMEA for absolute time
GPSTimeData gps_time = parser.parse_gprmc("$GPRMC,083218.00,...");

// Get PPS timestamp (hardware interrupt)
PPSTimestamp pps_edge;
pps_detector.get_pps_timestamp(2000, pps_edge);

// Combine: PPS edge + NMEA absolute time = nanosecond-accurate PTP time
PTPTimestamp ptp_time;
converter.convert_with_pps(gps_time, pps_edge, ptp_time);

// Result: Sub-microsecond synchronization accuracy! 🎯
```

## Technical Implementation

### Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│ GPS NMEA + PPS Synchronization Application                          │
│  gps_nmea_sync_example.cpp                                          │
├─────────────────────────────────────────────────────────────────────┤
│ NMEA Parser         Time Converter         PPS Detector             │
│  nmea_parser.hpp    gps_time_              pps_detector.hpp         │
│  nmea_parser.cpp    converter.hpp          pps_detector.cpp         │
│                     gps_time_                                        │
│                     converter.cpp                                   │
├─────────────────────────────────────────────────────────────────────┤
│ Serial HAL Interface                                                │
│  serial_hal_interface.hpp                                           │
├─────────────────────────────────────────────────────────────────────┤
│ Platform-Specific Implementations                                   │
│  serial_hal_windows.cpp (Win32 API + WaitCommEvent for PPS)        │
│  serial_hal_linux.cpp (termios + ioctl for PPS)                    │
└─────────────────────────────────────────────────────────────────────┘
         │                        │                         │
         │ NMEA (COM3)            │ /dev/ttyUSB0            │ PPS (DCD)
         ▼                        ▼                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│ GT-U7 GPS Module (Direct USB Connection)                            │
│  Pin 20 (TxD1):  NMEA-0183 sentences at 9600 baud                   │
│  Pin 3 (TIMEPULSE): 1Hz PPS signal (sub-microsecond accuracy)       │
│  Built-in USB-to-Serial converter (CH340 or similar)                │
└─────────────────────────────────────────────────────────────────────┘
                    │
                    │ RF (1.5GHz GPS L1 band)
                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│ GPS Antenna (Active IPEX)                                           │
│ Receives signals from GPS satellites                                │
└─────────────────────────────────────────────────────────────────────┘
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
