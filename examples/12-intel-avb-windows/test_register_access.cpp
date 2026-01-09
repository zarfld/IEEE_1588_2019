/**
 * @file test_register_access.cpp
 * @brief Test program to verify register read/write capabilities
 * 
 * This diagnostic tool tests:
 * - Register read operations
 * - Register write operations
 * - Write-read-verify sequence
 * 
 * Use this to determine if the Intel AVB Filter Driver supports
 * register write operations or if PTP configuration requires a
 * different approach.
 */

#include "intel_avb_hal.hpp"
#include <iostream>
#include <iomanip>

using namespace Examples::IntelAVB;

int main() {
    std::cout << "========================================\n";
    std::cout << "Intel AVB Register Access Test\n";
    std::cout << "========================================\n\n";

    // Create HAL instance
    IntelAVBHAL hal;
    
    // Open device
    std::cout << "Opening Intel AVB Filter Driver...\n";
    if (!hal.open_device()) {
        std::cerr << "ERROR: Failed to open device\n";
        std::cerr << "Error: " << hal.get_error_string() << "\n\n";
        std::cerr << "Please ensure:\n";
        std::cerr << "1. Intel AVB Filter Driver is installed\n";
        std::cerr << "2. Driver is loaded and running\n";
        std::cerr << "3. Application has administrator privileges\n\n";
        return 1;
    }
    std::cout << "Device opened successfully\n\n";

    // Enumerate adapters
    std::cout << "=== Enumerating Adapters ===\n";
    AdapterInfo adapters[8];
    size_t count = hal.enumerate_adapters(adapters, 8);
    if (count == 0) {
        std::cerr << "ERROR: No Intel adapters found\n";
        return 1;
    }
    std::cout << "Found " << count << " adapter(s)\n\n";

    // Select first PTP-capable adapter
    size_t selected_adapter = 0;
    for (size_t i = 0; i < count; ++i) {
        if (adapters[i].supports_ptp()) {
            selected_adapter = i;
            break;
        }
    }

    std::cout << "=== Using Adapter #" << selected_adapter << " ===\n";
    std::cout << "Description: " << adapters[selected_adapter].description << "\n\n";

    // Open adapter
    if (!hal.open_adapter(adapters[selected_adapter].vendor_id, 
                          adapters[selected_adapter].device_id)) {
        std::cerr << "ERROR: Failed to open adapter\n";
        return 1;
    }
    std::cout << "Adapter opened successfully\n\n";

    // Test 1: Read-only registers (should always work)
    std::cout << "=== Test 1: Read-Only Registers ===\n";
    uint32_t ctrl = 0, status = 0;
    
    if (hal.read_register(0x00000, ctrl)) {
        std::cout << "CTRL (0x00000):   0x" << std::hex << std::setw(8) 
                  << std::setfill('0') << ctrl << std::dec << " [OK]\n";
    } else {
        std::cout << "CTRL read failed (status=" << hal.get_last_status() << ")\n";
    }
    
    if (hal.read_register(0x00008, status)) {
        std::cout << "STATUS (0x00008): 0x" << std::hex << std::setw(8) 
                  << std::setfill('0') << status << std::dec << " [OK]\n";
    } else {
        std::cout << "STATUS read failed (status=" << hal.get_last_status() << ")\n";
    }
    std::cout << "\n";

    // Test 2: PTP registers (read-only test)
    std::cout << "=== Test 2: PTP Register Reads ===\n";
    uint32_t tsauxc = 0, timinca = 0, systiml = 0, systimh = 0;
    
    if (hal.read_register(0x0B640, tsauxc)) {
        std::cout << "TSAUXC (0x0B640):  0x" << std::hex << std::setw(8) 
                  << std::setfill('0') << tsauxc << std::dec << " [OK]\n";
    } else {
        std::cout << "TSAUXC read failed (status=" << hal.get_last_status() << ")\n";
    }
    
    if (hal.read_register(0x0B608, timinca)) {
        std::cout << "TIMINCA (0x0B608): 0x" << std::hex << std::setw(8) 
                  << std::setfill('0') << timinca << std::dec << " [OK]\n";
    } else {
        std::cout << "TIMINCA read failed (status=" << hal.get_last_status() << ")\n";
    }
    
    if (hal.read_register(0x0B600, systiml)) {
        std::cout << "SYSTIML (0x0B600): 0x" << std::hex << std::setw(8) 
                  << std::setfill('0') << systiml << std::dec << " [OK]\n";
    } else {
        std::cout << "SYSTIML read failed (status=" << hal.get_last_status() << ")\n";
    }
    
    if (hal.read_register(0x0B604, systimh)) {
        std::cout << "SYSTIMH (0x0B604): 0x" << std::hex << std::setw(8) 
                  << std::setfill('0') << systimh << std::dec << " [OK]\n";
    } else {
        std::cout << "SYSTIMH read failed (status=" << hal.get_last_status() << ")\n";
    }
    std::cout << "\n";

    // Test 3: Write test on TSAUXC (with verification)
    std::cout << "=== Test 3: Register Write Test ===\n";
    std::cout << "Testing write to TSAUXC register...\n";
    std::cout << "Original value: 0x" << std::hex << tsauxc << std::dec << "\n";
    
    uint32_t test_value = 0x80000000; // Set bit 31
    std::cout << "Attempting write of 0x" << std::hex << test_value << std::dec << "...\n";
    
    if (hal.write_register(0x0B640, test_value)) {
        std::cout << "Write operation completed (status=" << hal.get_last_status() << ")\n";
        
        // Read back to verify
        uint32_t verify = 0;
        if (hal.read_register(0x0B640, verify)) {
            std::cout << "Read-back value: 0x" << std::hex << verify << std::dec << "\n";
            if (verify == test_value) {
                std::cout << "✓ Write-read-verify PASSED\n";
            } else if (verify == tsauxc) {
                std::cout << "✗ Value unchanged - write had no effect\n";
            } else {
                std::cout << "? Value different - unexpected result\n";
            }
        }
    } else {
        std::cout << "✗ Write operation FAILED\n";
        std::cout << "  Status code: " << hal.get_last_status() << "\n";
        std::cout << "  Error: " << hal.get_error_string() << "\n";
        std::cout << "\nPossible causes:\n";
        std::cout << "1. Driver doesn't support IOCTL_AVB_WRITE_REGISTER\n";
        std::cout << "2. Register writes require special permissions\n";
        std::cout << "3. This register is read-only in the driver\n";
        std::cout << "4. PTP configuration requires different IOCTL\n";
    }
    std::cout << "\n";

    // Test 4: Alternative - Check if there's a SET_TIMESTAMP IOCTL
    std::cout << "=== Test 4: IOCTL_AVB_SET_TIMESTAMP Test ===\n";
    PTPTimestamp test_ts;
    test_ts.seconds = 1733400000; // Dec 5, 2024
    test_ts.nanoseconds = 500000000;
    
    std::cout << "Attempting to set timestamp via SET_TIMESTAMP IOCTL...\n";
    if (hal.set_timestamp(test_ts)) {
        std::cout << "✓ SET_TIMESTAMP succeeded\n";
        
        // Read back
        PTPTimestamp verify_ts;
        if (hal.get_timestamp(verify_ts)) {
            std::cout << "Timestamp: " << verify_ts.seconds << "s + " 
                      << verify_ts.nanoseconds << "ns\n";
        }
    } else {
        std::cout << "✗ SET_TIMESTAMP failed (status=" << hal.get_last_status() << ")\n";
    }
    std::cout << "\n";

    std::cout << "=== Summary ===\n";
    std::cout << "Register reads:  " << (ctrl != 0 ? "✓ Working" : "✗ Failed") << "\n";
    std::cout << "PTP reg reads:   " << (tsauxc != 0 || systiml != 0 ? "✓ Working" : "? Unknown") << "\n";
    std::cout << "Register writes: Run test to determine\n";
    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}
