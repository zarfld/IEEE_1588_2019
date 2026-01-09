@echo off
echo ========================================
echo Intel I226 PTP Timestamp Functionality Test
echo Requires Administrator Privileges
echo ========================================
echo.

cd /d "%~dp0"
set EXE_PATH=C:\Users\dzarf\source\repos\IEEE_1588_2019\build-intel\examples\12-intel-avb-windows\Release\test_timestamp_functionality.exe
set OUTPUT_FILE=timestamp_test_output.txt

if not exist "%EXE_PATH%" (
    echo ERROR: Test executable not found
    echo Path: %EXE_PATH%
    pause
    exit /b 1
)

echo Running test...
echo.

"%EXE_PATH%" > "%OUTPUT_FILE%" 2>&1
set EXIT_CODE=%ERRORLEVEL%

echo.
echo Test completed with exit code: %EXIT_CODE%
echo.

if exist "%OUTPUT_FILE%" (
    echo ======== Test Output ========
    type "%OUTPUT_FILE%"
    echo =============================
) else (
    echo WARNING: Output file not found
)

echo.
pause
