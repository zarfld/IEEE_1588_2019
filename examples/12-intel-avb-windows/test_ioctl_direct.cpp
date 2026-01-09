/**
 * @file test_ioctl_direct.cpp
 * @brief Direct IOCTL test to validate which IOCTLs actually work
 * 
 * This test bypasses the HAL's conditional compilation to test
 * the actual IOCTL behavior directly, matching the reference test
 * from ptp_clock_control_test.c
 */

#include <windows.h>
#include "avb_ioctl.h"
#include <iostream>
#include <iomanip>
#include <cstring>

void print_timestamp(const char* label, uint64_t ns) {
    uint32_t seconds = static_cast<uint32_t>(ns / 1000000000ULL);
    uint32_t nanoseconds = static_cast<uint32_t>(ns % 1000000000ULL);
    std::cout << label << seconds << "s + " << nanoseconds << "ns\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Direct IOCTL Test (Matches Reference Test)\n";
    std::cout << "========================================\n\n";

    // Open device
    std::cout << "Opening IntelAvbFilter device...\n";
    HANDLE hDevice = CreateFileW(
        L"\\\\.\\IntelAvbFilter",
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "ERROR: Failed to open device (GLE=" << GetLastError() << ")\n";
        return 1;
    }
    std::cout << "✓ Device opened successfully\n\n";

    // Initialize device (required for write access)
    std::cout << "Calling IOCTL_AVB_INIT_DEVICE...\n";
    DWORD bytesReturned = 0;
    BOOL initOk = DeviceIoControl(
        hDevice,
        IOCTL_AVB_INIT_DEVICE,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );
    
    if (!initOk) {
        std::cerr << "ERROR: IOCTL_AVB_INIT_DEVICE failed (GLE=" << GetLastError() << ")\n";
        CloseHandle(hDevice);
        return 1;
    }
    std::cout << "✓ Device initialized\n\n";

    // Enumerate adapters
    std::cout << "Enumerating adapters...\n";
    AVB_ENUM_REQUEST enumReq = {};
    enumReq.index = 0;
    
    BOOL enumOk = DeviceIoControl(
        hDevice,
        IOCTL_AVB_ENUM_ADAPTERS,
        &enumReq, sizeof(enumReq),
        &enumReq, sizeof(enumReq),
        &bytesReturned,
        nullptr
    );
    
    if (!enumOk) {
        std::cerr << "ERROR: IOCTL_AVB_ENUM_ADAPTERS failed (GLE=" << GetLastError() << ")\n";
        CloseHandle(hDevice);
        return 1;
    }
    std::cout << "✓ Found " << enumReq.count << " adapter(s)\n";
    std::cout << "  Using adapter 0: 0x" << std::hex << enumReq.vendor_id 
              << ":0x" << enumReq.device_id << std::dec << "\n\n";

    // Open adapter
    std::cout << "Opening adapter...\n";
    AVB_OPEN_REQUEST openReq = {};
    openReq.vendor_id = enumReq.vendor_id;
    openReq.device_id = enumReq.device_id;
    
    BOOL openOk = DeviceIoControl(
        hDevice,
        IOCTL_AVB_OPEN_ADAPTER,
        &openReq, sizeof(openReq),
        &openReq, sizeof(openReq),
        &bytesReturned,
        nullptr
    );
    
    if (!openOk || openReq.status != 0) {
        std::cerr << "ERROR: IOCTL_AVB_OPEN_ADAPTER failed (GLE=" << GetLastError() 
                  << ", status=" << openReq.status << ")\n";
        CloseHandle(hDevice);
        return 1;
    }
    std::cout << "✓ Adapter opened\n\n";

    // Test 1: IOCTL_AVB_GET_TIMESTAMP (should work)
    std::cout << "=== Test 1: IOCTL_AVB_GET_TIMESTAMP ===\n";
    AVB_TIMESTAMP_REQUEST getReq = {};
    getReq.clock_id = 0;
    
    BOOL getOk = DeviceIoControl(
        hDevice,
        IOCTL_AVB_GET_TIMESTAMP,
        &getReq, sizeof(getReq),
        &getReq, sizeof(getReq),
        &bytesReturned,
        nullptr
    );
    
    if (getOk && getReq.status == 0) {
        std::cout << "✓ IOCTL_AVB_GET_TIMESTAMP SUCCEEDED\n";
        print_timestamp("  Current timestamp: ", getReq.timestamp);
    } else {
        std::cout << "✗ IOCTL_AVB_GET_TIMESTAMP FAILED\n";
        std::cout << "  GLE=" << GetLastError() << ", status=" << getReq.status << "\n";
    }
    std::cout << "\n";

    // Test 2: IOCTL_AVB_SET_TIMESTAMP (reference test shows this FAILS)
    std::cout << "=== Test 2: IOCTL_AVB_SET_TIMESTAMP ===\n";
    std::cout << "This is the test that FAILED in reference (GLE=21)\n\n";
    
    AVB_TIMESTAMP_REQUEST setReq = {};
    setReq.timestamp = 1733400000000000000ULL; // Dec 5, 2024, 0:0:0 UTC
    setReq.clock_id = 0;
    
    std::cout << "Attempting to set timestamp via IOCTL...\n";
    print_timestamp("  Target: ", setReq.timestamp);
    
    BOOL setOk = DeviceIoControl(
        hDevice,
        IOCTL_AVB_SET_TIMESTAMP,
        &setReq, sizeof(setReq),
        &setReq, sizeof(setReq),
        &bytesReturned,
        nullptr
    );
    
    DWORD setError = GetLastError();
    
    if (setOk && setReq.status == 0) {
        std::cout << "✓ IOCTL_AVB_SET_TIMESTAMP SUCCEEDED (unexpected!)\n";
        std::cout << "  This contradicts reference test which showed GLE=21\n";
        
        // Verify by reading back
        AVB_TIMESTAMP_REQUEST verifyReq = {};
        verifyReq.clock_id = 0;
        BOOL verifyOk = DeviceIoControl(
            hDevice,
            IOCTL_AVB_GET_TIMESTAMP,
            &verifyReq, sizeof(verifyReq),
            &verifyReq, sizeof(verifyReq),
            &bytesReturned,
            nullptr
        );
        
        if (verifyOk && verifyReq.status == 0) {
            print_timestamp("  Readback: ", verifyReq.timestamp);
            if (verifyReq.timestamp == setReq.timestamp) {
                std::cout << "  ✓ Timestamp set successfully!\n";
            } else {
                std::cout << "  ? Timestamp different from target\n";
            }
        }
    } else {
        std::cout << "✗ IOCTL_AVB_SET_TIMESTAMP FAILED (as expected from reference)\n";
        std::cout << "  GLE=" << setError;
        if (setError == ERROR_NOT_READY) {
            std::cout << " (ERROR_NOT_READY) - matches reference test!\n";
        } else {
            std::cout << " (unexpected error code)\n";
        }
        std::cout << "  status=" << setReq.status << "\n";
    }
    std::cout << "\n";

    // Test 3: Direct register write via IOCTL_AVB_WRITE_REGISTER
    std::cout << "=== Test 3: Direct Register Write (SYSTIML) ===\n";
    std::cout << "Reference test shows this WORKS (2/2 passed)\n\n";
    
    AVB_REGISTER_REQUEST writeReq = {};
    writeReq.offset = 0x0B600; // SYSTIML
    writeReq.value = 500000000; // 0.5 seconds
    
    std::cout << "Writing SYSTIML=0x" << std::hex << writeReq.value << std::dec << "...\n";
    
    BOOL writeOk = DeviceIoControl(
        hDevice,
        IOCTL_AVB_WRITE_REGISTER,
        &writeReq, sizeof(writeReq),
        &writeReq, sizeof(writeReq),
        &bytesReturned,
        nullptr
    );
    
    if (writeOk && writeReq.status == 0) {
        std::cout << "✓ Register write SUCCEEDED\n";
        
        // Read back
        AVB_REGISTER_REQUEST readReq = {};
        readReq.offset = 0x0B600;
        BOOL readOk = DeviceIoControl(
            hDevice,
            IOCTL_AVB_READ_REGISTER,
            &readReq, sizeof(readReq),
            &readReq, sizeof(readReq),
            &bytesReturned,
            nullptr
        );
        
        if (readOk && readReq.status == 0) {
            std::cout << "  Readback: 0x" << std::hex << readReq.value << std::dec << "\n";
            if (readReq.value == writeReq.value) {
                std::cout << "  ✓ Write-read-verify PASSED\n";
            } else {
                std::cout << "  ? Value different (clock may have incremented)\n";
            }
        }
    } else {
        std::cout << "✗ Register write FAILED\n";
        std::cout << "  GLE=" << GetLastError() << ", status=" << writeReq.status << "\n";
    }
    std::cout << "\n";

    // Test 4: IOCTL_AVB_ADJUST_FREQUENCY (untested in reference)
    std::cout << "=== Test 4: IOCTL_AVB_ADJUST_FREQUENCY ===\n";
    std::cout << "This IOCTL was NOT tested in reference\n\n";
    
    AVB_FREQUENCY_REQUEST freqReq = {};
    freqReq.increment_ns = 24;
    freqReq.increment_frac = 0; // No adjustment
    
    std::cout << "Attempting frequency adjustment...\n";
    
    BOOL freqOk = DeviceIoControl(
        hDevice,
        IOCTL_AVB_ADJUST_FREQUENCY,
        &freqReq, sizeof(freqReq),
        &freqReq, sizeof(freqReq),
        &bytesReturned,
        nullptr
    );
    
    if (freqOk && freqReq.status == 0) {
        std::cout << "✓ IOCTL_AVB_ADJUST_FREQUENCY SUCCEEDED\n";
        std::cout << "  This IOCTL appears to work!\n";
    } else {
        std::cout << "✗ IOCTL_AVB_ADJUST_FREQUENCY FAILED\n";
        std::cout << "  GLE=" << GetLastError() << ", status=" << freqReq.status << "\n";
    }
    std::cout << "\n";

    std::cout << "========================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "========================================\n";
    std::cout << "IOCTL_AVB_GET_TIMESTAMP:     " << (getOk && getReq.status == 0 ? "✓ WORKS" : "✗ FAILS") << "\n";
    std::cout << "IOCTL_AVB_SET_TIMESTAMP:     " << (setOk && setReq.status == 0 ? "✓ WORKS" : "✗ FAILS (expected)") << "\n";
    std::cout << "Direct register write:        " << (writeOk && writeReq.status == 0 ? "✓ WORKS" : "✗ FAILS") << "\n";
    std::cout << "IOCTL_AVB_ADJUST_FREQUENCY:  " << (freqOk && freqReq.status == 0 ? "✓ WORKS" : "✗ FAILS") << "\n";
    std::cout << "========================================\n";
    std::cout << "\nKEY FINDING:\n";
    if (setOk && setReq.status == 0) {
        std::cout << "✓ IOCTL_AVB_SET_TIMESTAMP WORKS (contradicts reference test!)\n";
        std::cout << "  This means our refactored HAL could use the IOCTL approach.\n";
    } else {
        std::cout << "✗ IOCTL_AVB_SET_TIMESTAMP FAILS (matches reference test)\n";
        std::cout << "  Direct register writes are the only working approach.\n";
    }
    std::cout << "========================================\n";

    CloseHandle(hDevice);
    return 0;
}
