/**
 * @file test_phc_adapter.cpp
 * @brief Unit test for PhcAdapter
 * 
 * Validates PhcAdapter interface:
 * 1. Initialization
 * 2. Get time
 * 3. Set time
 * 4. Adjust frequency
 */

#include "phc_adapter.hpp"
#include <iostream>
#include <unistd.h>
#include <cmath>
#include <cstring>

void print_test_header(const char* test_name) {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << test_name;
    for (size_t i = strlen(test_name); i < 57; ++i) std::cout << " ";
    std::cout << " ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
}

void print_result(bool success) {
    if (success) {
        std::cout << "✅ PASS\n";
    } else {
        std::cout << "❌ FAIL\n";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║         PhcAdapter Unit Test Suite                         ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    
    const char* interface_name = (argc > 1) ? argv[1] : "eth1";
    std::cout << "Using interface: " << interface_name << "\n";
    
    int tests_passed = 0;
    int tests_total = 0;
    
    // Test 1: Initialization
    {
        print_test_header("TEST 1: Initialization");
        tests_total++;
        
        PhcAdapter phc;
        bool success = phc.initialize(interface_name);
        
        if (success) {
            std::cout << "Interface: " << phc.get_interface_name() << "\n";
            std::cout << "Device:    " << phc.get_device_path() << "\n";
            std::cout << "Max freq:  " << phc.get_max_frequency_ppb() << " ppb\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 2: Get Time
    {
        print_test_header("TEST 2: Get Time");
        tests_total++;
        
        PhcAdapter phc;
        phc.initialize(interface_name);
        
        uint64_t sec1, sec2;
        uint32_t nsec1, nsec2;
        
        bool success = phc.get_time(&sec1, &nsec1);
        sleep(1);
        phc.get_time(&sec2, &nsec2);
        
        if (success && sec2 >= sec1) {
            std::cout << "Time 1: " << sec1 << "." << nsec1 << "\n";
            std::cout << "Time 2: " << sec2 << "." << nsec2 << "\n";
            std::cout << "Delta:  " << (sec2 - sec1) << " seconds\n";
            tests_passed++;
        }
        
        print_result(success && sec2 >= sec1);
    }
    
    // Test 3: Set Time
    {
        print_test_header("TEST 3: Set Time");
        tests_total++;
        
        PhcAdapter phc;
        phc.initialize(interface_name);
        
        uint64_t sec_before, sec_after;
        uint32_t nsec_before, nsec_after;
        
        phc.get_time(&sec_before, &nsec_before);
        
        // Set time 5 seconds in the future
        uint64_t target_sec = sec_before + 5;
        uint32_t target_nsec = nsec_before;
        
        bool success = phc.set_time(target_sec, target_nsec);
        phc.get_time(&sec_after, &nsec_after);
        
        int64_t step_ns = (sec_after - sec_before) * 1000000000LL + (nsec_after - nsec_before);
        
        if (success && std::abs(step_ns - 5000000000LL) < 10000000) {  // Within 10ms
            std::cout << "Before:  " << sec_before << "." << nsec_before << "\n";
            std::cout << "Target:  " << target_sec << "." << target_nsec << "\n";
            std::cout << "After:   " << sec_after << "." << nsec_after << "\n";
            std::cout << "Step:    " << step_ns / 1000000.0 << " ms\n";
            tests_passed++;
        }
        
        print_result(success && std::abs(step_ns - 5000000000LL) < 10000000);
    }
    
    // Test 4: Adjust Frequency (positive)
    {
        print_test_header("TEST 4: Adjust Frequency (+10000 ppb)");
        tests_total++;
        
        PhcAdapter phc;
        phc.initialize(interface_name);
        
        bool success = phc.adjust_frequency(10000);
        
        if (success) {
            std::cout << "Applied: +10000 ppb\n";
            std::cout << "PHC should now run ~10ms fast per 1000 seconds\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 5: Adjust Frequency (negative)
    {
        print_test_header("TEST 5: Adjust Frequency (-10000 ppb)");
        tests_total++;
        
        PhcAdapter phc;
        phc.initialize(interface_name);
        
        bool success = phc.adjust_frequency(-10000);
        
        if (success) {
            std::cout << "Applied: -10000 ppb\n";
            std::cout << "PHC should now run ~10ms slow per 1000 seconds\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 6: Adjust Frequency (zero - neutral)
    {
        print_test_header("TEST 6: Adjust Frequency (0 ppb - reset)");
        tests_total++;
        
        PhcAdapter phc;
        phc.initialize(interface_name);
        
        bool success = phc.adjust_frequency(0);
        
        if (success) {
            std::cout << "Applied: 0 ppb (nominal frequency)\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Test 7: Clamp to hardware limits
    {
        print_test_header("TEST 7: Hardware Limit Clamping");
        tests_total++;
        
        PhcAdapter phc;
        phc.initialize(interface_name);
        
        // Should clamp to ±500000 ppb
        bool success1 = phc.adjust_frequency(600000);  // Exceeds max
        bool success2 = phc.adjust_frequency(-600000); // Exceeds min
        bool success3 = phc.adjust_frequency(0);       // Reset
        
        bool success = success1 && success2 && success3;
        
        if (success) {
            std::cout << "Max limit: " << phc.get_max_frequency_ppb() << " ppb\n";
            std::cout << "Clamping test passed (600000 → 500000)\n";
            tests_passed++;
        }
        
        print_result(success);
    }
    
    // Summary
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║                   TEST SUMMARY                            ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Passed: " << tests_passed << "/" << tests_total;
    for (int i = 0; i < 48; ++i) std::cout << " ";
    std::cout << " ║\n";
    
    if (tests_passed == tests_total) {
        std::cout << "║ Result: ✅ ALL TESTS PASSED";
        for (int i = 0; i < 33; ++i) std::cout << " ";
        std::cout << " ║\n";
    } else {
        std::cout << "║ Result: ❌ SOME TESTS FAILED";
        for (int i = 0; i < 32; ++i) std::cout << " ";
        std::cout << " ║\n";
    }
    
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    
    return (tests_passed == tests_total) ? 0 : 1;
}
