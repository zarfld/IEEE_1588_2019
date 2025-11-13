# Hardware Testing Quick Start

## Your System

**COM Port Detected**: COM3

## Quick Test Steps

### Step 1: Check COM Port (✓ DONE)
```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```
Result: COM3 found ✓

### Step 2: Test NMEA Output (Run this now)

```powershell
$port = New-Object System.IO.Ports.SerialPort COM3,9600,None,8,One
$port.Open()
1..10 | % { $port.ReadLine() }
$port.Close()
```

**Expected output**: NMEA sentences like:
```
$GPRMC,123456.00,A,5230.123,N,01320.456,E,0.0,0.0,131125,,,A*XX
$GPGGA,123456.00,5230.123,N,01320.456,E,1,08,1.2,100.0,M,45.0,M,,*XX
```

**If "V" in field 2**: GPS has no fix yet → Wait 30-60s for cold start, move antenna to window

### Step 3: Run PPS Hardware Test

```powershell
cd d:\Repos\IEEE_1588_2019
.\build\examples\04-gps-nmea-sync\Release\test_pps_hardware.exe
```

**Prerequisites**:
- GPS must have fix (4+ satellites)
- **(Optional)** For PPS detection: Connect jumper wires:
  - GT-U7 Pin 3 (TIMEPULSE) → Serial DCD (Pin 1)
  - GT-U7 Pin 24 (GND) → Serial GND (Pin 5)

**Expected duration**: 8-12 seconds  
**Expected result**: PPS detected in 3-5s (if PPS connected)

### Step 4: Run Quality Management Test

```powershell
.\build\examples\04-gps-nmea-sync\Release\test_clock_quality.exe
```

**Duration**: <1 second  
**Purpose**: Verify clock quality attributes (no hardware needed)

### Step 5: Run Integrated Example

```powershell
.\build\examples\04-gps-nmea-sync\Release\gps_ptp_sync_example.exe
```

**Duration**: ~15 seconds  
**Purpose**: See GPS → PPS → Clock Quality workflow

## GT-U7 Hardware Setup

### NMEA-Only Mode (Simplest)
```
GT-U7 USB ──────→ PC USB Port (COM3)
```
- Accuracy: ±10ms
- No jumpers needed

### NMEA + PPS Mode (Best Accuracy)
```
GT-U7 USB         ──────→ PC USB Port (COM3)
GT-U7 Pin 3       ──────→ Serial DCD (Pin 1)
GT-U7 Pin 24 (GND)──────→ Serial GND (Pin 5)
```
- Accuracy: **±100ns** (100× better!)
- Requires 2 jumper wires

## Troubleshooting

**No GPS Fix**:
1. Move antenna to window or outdoors
2. Wait 30-60s for cold start
3. Check antenna LED is blinking

**No PPS Signal**:
1. Ensure GPS has fix first
2. Check jumper wiring
3. Try alternative pins (DSR or CTS)

**Build Errors**:
```powershell
cd d:\Repos\IEEE_1588_2019
cmake --build build --config Release
```

## Documentation

- **HARDWARE_VALIDATION_GUIDE.md** - Complete hardware testing guide
- **CLOCK_QUALITY_MANAGEMENT.md** - Clock quality documentation
- **PPS_TESTING_STRATEGY.md** - PPS detection strategy
- **README.md** - Project overview with wiring diagrams

---

**Status**: COM3 detected ✓ | Ready for testing!
