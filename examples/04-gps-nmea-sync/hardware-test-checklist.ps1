# GT-U7 GPS Module - Hardware Test Checklist Script
# Run this script to perform step-by-step hardware validation

Write-Host @"
========================================
GT-U7 GPS Module Hardware Validation
IEEE 1588-2019 PTP Clock Quality Testing
========================================

"@ -ForegroundColor Cyan

# Function to check if COM port exists
function Test-ComPort {
    param([string]$PortName)
    
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    return $ports -contains $PortName
}

# Function to display available COM ports
function Get-AvailablePorts {
    Write-Host "`n=== Step 1: Check Available Serial Ports ===" -ForegroundColor Yellow
    
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    
    if ($ports.Count -eq 0) {
        Write-Host "❌ No COM ports found!" -ForegroundColor Red
        Write-Host "   - Connect GT-U7 GPS module via USB"
        Write-Host "   - Check Device Manager → Ports (COM & LPT)"
        return $null
    }
    
    Write-Host "✓ Found $($ports.Count) COM port(s):" -ForegroundColor Green
    foreach ($port in $ports) {
        Write-Host "   - $port" -ForegroundColor Cyan
    }
    
    return $ports
}

# Function to test NMEA output
function Test-NmeaOutput {
    param([string]$PortName)
    
    Write-Host "`n=== Step 2: Test NMEA Output from GPS ===" -ForegroundColor Yellow
    Write-Host "Port: $PortName, Baud: 9600, Data: 8N1" -ForegroundColor Gray
    
    try {
        $port = New-Object System.IO.Ports.SerialPort $PortName, 9600, None, 8, One
        $port.ReadTimeout = 5000  # 5 second timeout
        $port.Open()
        
        Write-Host "✓ Serial port opened successfully" -ForegroundColor Green
        Write-Host "`nReading NMEA sentences (10 lines)..." -ForegroundColor Cyan
        Write-Host "Expected: `$GPRMC, `$GPGGA sentences at 9600 baud`n" -ForegroundColor Gray
        
        $validSentences = 0
        $gprmcFound = $false
        $gpggaFound = $false
        
        for ($i = 1; $i -le 10; $i++) {
            try {
                $line = $port.ReadLine()
                
                # Check for valid NMEA sentences
                if ($line -match '^\$GP(RMC|GGA)') {
                    $validSentences++
                    if ($line -match '^\$GPRMC') { $gprmcFound = $true }
                    if ($line -match '^\$GPGGA') { $gpggaFound = $true }
                    Write-Host "[$i] $($line.Substring(0, [Math]::Min(60, $line.Length)))..." -ForegroundColor Green
                } else {
                    Write-Host "[$i] $line" -ForegroundColor Yellow
                }
            } catch {
                Write-Host "[$i] Timeout or error reading line" -ForegroundColor Red
            }
        }
        
        $port.Close()
        
        Write-Host "`n✓ NMEA Test Results:" -ForegroundColor Green
        Write-Host "   Valid NMEA sentences: $validSentences/10"
        Write-Host "   GPRMC found: $(if($gprmcFound){'Yes'}else{'No'})"
        Write-Host "   GPGGA found: $(if($gpggaFound){'Yes'}else{'No'})"
        
        if ($validSentences -ge 5 -and $gprmcFound) {
            Write-Host "`n✓✓ NMEA output looks good!" -ForegroundColor Green
            return $true
        } else {
            Write-Host "`n⚠ NMEA output may have issues:" -ForegroundColor Yellow
            Write-Host "   - Check GPS antenna is connected"
            Write-Host "   - Wait 30-60s for GPS fix (cold start)"
            Write-Host "   - Move antenna to window or outdoors"
            return $false
        }
        
    } catch {
        Write-Host "❌ Error opening serial port: $_" -ForegroundColor Red
        Write-Host "   - Check port name is correct"
        Write-Host "   - Close other programs using the port"
        Write-Host "   - Check USB cable connection"
        return $false
    }
}

# Function to check if executable exists
function Test-Executable {
    param([string]$Path)
    return Test-Path $Path
}

# Main test sequence
$repoRoot = "d:\Repos\IEEE_1588_2019"
$buildPath = "$repoRoot\build\examples\04-gps-nmea-sync\Release"

# Step 1: Check for COM ports
$ports = Get-AvailablePorts

if (-not $ports) {
    Write-Host "`n❌ Cannot proceed without COM port. Please connect GPS hardware." -ForegroundColor Red
    exit 1
}

# Ask user which port to use
Write-Host "`nEnter COM port number (e.g., COM3): " -NoNewline -ForegroundColor Cyan
$userPort = Read-Host

if (-not (Test-ComPort $userPort)) {
    Write-Host "❌ Port $userPort not found!" -ForegroundColor Red
    exit 1
}

# Step 2: Test NMEA output
$nmeaOk = Test-NmeaOutput $userPort

if (-not $nmeaOk) {
    Write-Host "`n⚠ NMEA test failed. Continue anyway? (Y/N): " -NoNewline -ForegroundColor Yellow
    $continue = Read-Host
    if ($continue -ne 'Y' -and $continue -ne 'y') {
        exit 1
    }
}

# Step 3: Check if tests are built
Write-Host "`n=== Step 3: Check Test Executables ===" -ForegroundColor Yellow

$tests = @(
    @{Name="test_nmea_parser"; Path="$buildPath\test_nmea_parser.exe"; Required=$false},
    @{Name="test_gps_time_converter"; Path="$buildPath\test_gps_time_converter.exe"; Required=$false},
    @{Name="test_clock_quality"; Path="$buildPath\test_clock_quality.exe"; Required=$false},
    @{Name="test_pps_hardware"; Path="$buildPath\test_pps_hardware.exe"; Required=$true},
    @{Name="gps_ptp_sync_example"; Path="$buildPath\gps_ptp_sync_example.exe"; Required=$false}
)

$missingTests = @()

foreach ($test in $tests) {
    if (Test-Executable $test.Path) {
        Write-Host "✓ $($test.Name).exe found" -ForegroundColor Green
    } else {
        Write-Host "❌ $($test.Name).exe NOT found" -ForegroundColor Red
        if ($test.Required) {
            $missingTests += $test.Name
        }
    }
}

if ($missingTests.Count -gt 0) {
    Write-Host "`n❌ Missing required test executables. Please build first:" -ForegroundColor Red
    Write-Host "   cd $repoRoot" -ForegroundColor Yellow
    Write-Host "   cmake --build build --config Release" -ForegroundColor Yellow
    exit 1
}

# Step 4: Run quick tests (no hardware)
Write-Host "`n=== Step 4: Run Quick Tests (No Hardware) ===" -ForegroundColor Yellow

if (Test-Executable "$buildPath\test_clock_quality.exe") {
    Write-Host "`nRunning test_clock_quality.exe..." -ForegroundColor Cyan
    & "$buildPath\test_clock_quality.exe"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Clock quality test PASSED" -ForegroundColor Green
    } else {
        Write-Host "❌ Clock quality test FAILED" -ForegroundColor Red
    }
}

# Step 5: Ask about PPS hardware connection
Write-Host "`n=== Step 5: PPS Hardware Connection ===" -ForegroundColor Yellow
Write-Host @"
Before running PPS hardware test, ensure:
  1. GT-U7 GPS module has GPS fix (4+ satellites)
  2. PPS jumpers connected:
     - Pin 3 (TIMEPULSE) → Serial DCD (Pin 1)
     - Pin 24 (GND) → Serial GND (Pin 5)
  3. Antenna has clear sky view

Is PPS hardware connected? (Y/N): 
"@ -ForegroundColor Cyan -NoNewline

$ppsConnected = Read-Host

if ($ppsConnected -eq 'Y' -or $ppsConnected -eq 'y') {
    Write-Host "`n=== Step 6: Run PPS Hardware Test ===" -ForegroundColor Yellow
    Write-Host "Port: $userPort" -ForegroundColor Gray
    Write-Host "Expected: PPS detection in 3-5 seconds" -ForegroundColor Gray
    Write-Host "Timeout: 10 seconds`n" -ForegroundColor Gray
    
    # Set environment variable for COM port (if test supports it)
    $env:GPS_COM_PORT = $userPort
    
    Write-Host "Press Enter to start PPS hardware test..." -NoNewline -ForegroundColor Cyan
    Read-Host
    
    & "$buildPath\test_pps_hardware.exe"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n✓✓✓ PPS HARDWARE TEST PASSED! ✓✓✓" -ForegroundColor Green
        Write-Host "PPS signal detected and validated successfully!" -ForegroundColor Green
        Write-Host "Hardware is ready for sub-microsecond timestamping." -ForegroundColor Green
        
        # Offer to run integrated example
        Write-Host "`nRun integrated GPS+PPS+ClockQuality example? (Y/N): " -NoNewline -ForegroundColor Cyan
        $runExample = Read-Host
        
        if ($runExample -eq 'Y' -or $runExample -eq 'y') {
            Write-Host "`n=== Running Integrated Example ===" -ForegroundColor Yellow
            & "$buildPath\gps_ptp_sync_example.exe"
        }
        
    } else {
        Write-Host "`n❌ PPS HARDWARE TEST FAILED" -ForegroundColor Red
        Write-Host "Troubleshooting:" -ForegroundColor Yellow
        Write-Host "  1. Check GPS has fix (run NMEA test again)"
        Write-Host "  2. Verify PPS jumper connections"
        Write-Host "  3. Use oscilloscope to verify 1Hz on Pin 3"
        Write-Host "  4. Try alternative pins (DSR or CTS)"
        Write-Host "See HARDWARE_VALIDATION_GUIDE.md for details" -ForegroundColor Yellow
    }
} else {
    Write-Host "`n⚠ PPS hardware not connected. Skipping hardware tests." -ForegroundColor Yellow
    Write-Host "To test PPS, connect jumpers and re-run this script." -ForegroundColor Yellow
}

# Step 7: Summary
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Hardware Validation Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "COM Port:     $userPort"
Write-Host "NMEA Output:  $(if($nmeaOk){'OK'}else{'Check antenna/fix'})"
Write-Host "PPS Hardware: $(if($ppsConnected -eq 'Y'){'Tested'}else{'Not tested'})"
Write-Host "`nFor detailed guidance, see HARDWARE_VALIDATION_GUIDE.md" -ForegroundColor Gray
Write-Host "For clock quality info, see CLOCK_QUALITY_MANAGEMENT.md" -ForegroundColor Gray

Write-Host "`n✓ Hardware validation checklist complete!" -ForegroundColor Green
