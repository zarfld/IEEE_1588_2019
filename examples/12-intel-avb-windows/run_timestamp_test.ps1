# Run timestamp functionality test and capture output
Write-Host "Running Intel I226 PTP Timestamp Functionality Test..." -ForegroundColor Cyan
Write-Host "This test validates if PTP timestamps work when TSAUXC=0x00000000" -ForegroundColor Cyan
Write-Host "Based on Intel I226 Datasheet Section 7.5.1.3" -ForegroundColor Cyan
Write-Host ""

$exePath = "C:\Users\dzarf\source\repos\IEEE_1588_2019\build-intel\examples\12-intel-avb-windows\Release\test_timestamp_functionality.exe"
$outputFile = "C:\Users\dzarf\source\repos\IEEE_1588_2019\examples\12-intel-avb-windows\timestamp_test_output.txt"

if (-not (Test-Path $exePath)) {
    Write-Host "ERROR: Test executable not found at $exePath" -ForegroundColor Red
    exit 1
}

Write-Host "Executable: $exePath" -ForegroundColor Yellow
Write-Host "Output file: $outputFile" -ForegroundColor Yellow
Write-Host ""

# Run the test directly (requires PowerShell running as admin)
& $exePath | Tee-Object -FilePath $outputFile

$exitCode = $LASTEXITCODE
Write-Host ""
Write-Host "Test completed with exit code: $exitCode" -ForegroundColor $(if ($exitCode -eq 0) { "Green" } else { "Red" })
