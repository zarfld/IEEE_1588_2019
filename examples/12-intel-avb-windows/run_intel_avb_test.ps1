# run_intel_avb_test.ps1
# Script to run Intel AVB PTP example with administrator privileges

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "This script requires administrator privileges." -ForegroundColor Yellow
    Write-Host "Restarting with elevated permissions..." -ForegroundColor Yellow
    Start-Sleep -Seconds 2
    
    # Restart script with admin privileges
    Start-Process powershell -Verb RunAs -ArgumentList "-NoExit", "-File", $PSCommandPath
    exit
}

Write-Host "Running with administrator privileges" -ForegroundColor Green
Write-Host ""

# Navigate to build directory
$buildDir = "c:\Users\dzarf\source\repos\IEEE_1588_2019\build-intel\examples\12-intel-avb-windows\Release"

if (-not (Test-Path $buildDir)) {
    Write-Host "ERROR: Build directory not found: $buildDir" -ForegroundColor Red
    Write-Host "Please build the project first." -ForegroundColor Yellow
    pause
    exit 1
}

Set-Location $buildDir

# Show Intel network adapters
Write-Host "=== Detected Intel Network Adapters ===" -ForegroundColor Cyan
Get-NetAdapter | Where-Object { $_.DriverDescription -like "*Intel*" } | Format-Table Name, DriverDescription, Status -AutoSize
Write-Host ""

# Check if driver is loaded
Write-Host "=== Checking Intel AVB Filter Driver Status ===" -ForegroundColor Cyan
$driverStatus = sc.exe query IntelAvbFilter 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "Intel AVB Filter Driver is loaded" -ForegroundColor Green
    Write-Host $driverStatus
} else {
    Write-Host "WARNING: Intel AVB Filter Driver may not be loaded" -ForegroundColor Yellow
    Write-Host "Error code: $LASTEXITCODE"
}
Write-Host ""

# Run the example
Write-Host "=== Running Intel AVB PTP Example ===" -ForegroundColor Cyan
Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
Write-Host ""

.\intel_avb_ptp_example.exe

Write-Host ""
Write-Host "=== Test Complete ===" -ForegroundColor Green
Write-Host ""
pause
