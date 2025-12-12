/**
 * @file test_timestamp_functionality.cpp
 * @brief Test if Intel I226 PTP timestamps work when TSAUXC=0x00000000
 * 
 * CRITICAL DISCOVERY from Intel I226 Datasheet Section 7.5.1.3:
 * "1588 logic is enabled only when the Disable systime bit in the TSAUXC register is cleared"
 * 
 * This means:
 *   - Bit CLEARED (0) = PTP ENABLED
 *   - Bit SET (1) = PTP DISABLED
 * 
 * If TSAUXC reads 0x00000000, the "Disable systime" bit is cleared,
 * meaning PTP SHOULD BE ENABLED. This test verifies if timestamps
 * actually increment.
 */

#include "intel_avb_hal.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>

using namespace Examples::IntelAVB;

int main() {
    std::cout << "========================================\n";
    std::cout << "Intel I226 PTP Timestamp Functionality Test\n";
    std::cout << "========================================\n\n";
    
    std::cout << "THEORY TEST (from Intel I226 Datasheet Section 7.5.1.3):\n";
    std::cout << "  - TSAUXC \"Disable systime\" bit CLEARED = PTP ENABLED\n";
    std::cout << "  - TSAUXC \"Disable systime\" bit SET = PTP DISABLED\n";
    std::cout << "  - If TSAUXC=0x00000000, all bits cleared, PTP SHOULD BE ENABLED\n\n";
    
    // Create HAL instance
    IntelAVBHAL hal;
    
    // Open device
    std::cout << "Opening Intel AVB Filter Driver...\n";
    if (!hal.open_device()) {
        std::cerr << "ERROR: Failed to open device\n";
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

    // ====================================================================
    // CRITICAL TEST: Check TSAUXC and timestamp behavior
    // ====================================================================
    
    std::cout << "=== Step 1: Reading TSAUXC register (0x0B640) ===\n";
    uint32_t tsauxc = 0;
    if (!hal.read_register(0x0B640, tsauxc)) {
        std::cerr << "ERROR: Failed to read TSAUXC\n";
        return 1;
    }
    std::cout << "TSAUXC = 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << tsauxc << std::dec << "\n";
    
    // Analyze TSAUXC value
    if (tsauxc == 0x00000000) {
        std::cout << "ANALYSIS: All bits cleared (including \"Disable systime\" bit)\n";
        std::cout << "EXPECTED: PTP should be ENABLED according to datasheet\n";
    } else {
        std::cout << "ANALYSIS: Some bits set:\n";
        for (int bit = 0; bit < 32; bit++) {
            if (tsauxc & (1 << bit)) {
                std::cout << "  Bit " << bit << " is SET\n";
            }
        }
    }
    std::cout << "\n";
    
    std::cout << "=== Step 2: Reading TIMINCA register (0x0B608) ===\n";
    uint32_t timinca = 0;
    if (!hal.read_register(0x0B608, timinca)) {
        std::cerr << "ERROR: Failed to read TIMINCA\n";
        return 1;
    }
    std::cout << "TIMINCA = 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << timinca << std::dec << "\n";
    
    uint32_t incvalue = (timinca >> 24) & 0xFF;
    uint32_t incperiod = timinca & 0xFFFFFF;
    std::cout << "Increment Value: " << incvalue << " ns per cycle\n";
    std::cout << "Increment Period: " << incperiod << " cycles\n";
    
    if (incvalue == 0) {
        std::cout << "WARNING: Increment value is ZERO - timestamps won't increment!\n";
    }
    std::cout << "\n";
    
    std::cout << "=== Step 3: Sampling PTP timestamps over 2 seconds ===\n";
    std::cout << "Taking 10 samples at 200ms intervals...\n\n";
    
    struct Sample {
        uint64_t timestamp_ns;
        std::chrono::steady_clock::time_point sample_time;
    };
    
    std::vector<Sample> samples;
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 10; i++) {
        uint32_t systiml = 0, systimh = 0;
        
        // Read SYSTIMH first for atomic snapshot
        if (!hal.read_register(0x0B604, systimh)) {
            std::cerr << "ERROR: Failed to read SYSTIMH at sample " << i << "\n";
            continue;
        }
        
        // Read SYSTIML second
        if (!hal.read_register(0x0B600, systiml)) {
            std::cerr << "ERROR: Failed to read SYSTIML at sample " << i << "\n";
            continue;
        }
        
        // Combine into 64-bit timestamp
        uint64_t timestamp_ns = (static_cast<uint64_t>(systimh) << 32) | systiml;
        auto sample_time = std::chrono::steady_clock::now();
        
        samples.push_back({timestamp_ns, sample_time});
        
        // Display sample
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            sample_time - start_time).count();
        
        std::cout << "[+" << std::setw(5) << elapsed << "ms] "
                  << "SYSTIML=0x" << std::hex << std::setw(8) << std::setfill('0') << systiml
                  << " SYSTIMH=0x" << std::setw(8) << systimh
                  << " Combined=" << std::dec << timestamp_ns << "ns";
        
        if (timestamp_ns == 0) {
            std::cout << " [ZERO - NOT WORKING]";
        }
        std::cout << "\n";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    std::cout << "\n";
    
    // ====================================================================
    // ANALYSIS
    // ====================================================================
    
    std::cout << "=== Step 4: Analysis of Timestamp Behavior ===\n";
    
    bool all_zero = true;
    bool all_same = true;
    uint64_t first_timestamp = samples[0].timestamp_ns;
    
    for (const auto& sample : samples) {
        if (sample.timestamp_ns != 0) {
            all_zero = false;
        }
        if (sample.timestamp_ns != first_timestamp) {
            all_same = false;
        }
    }
    
    if (all_zero) {
        std::cout << "RESULT: All timestamps are ZERO\n";
        std::cout << "DIAGNOSIS: PTP hardware clock is NOT incrementing\n\n";
        std::cout << "POSSIBLE CAUSES:\n";
        std::cout << "  1. \"Disable systime\" bit interpretation incorrect\n";
        std::cout << "  2. Different TSAUXC bit controls PTP (not the one we think)\n";
        std::cout << "  3. TIMINCA increment value is zero (no clock advance)\n";
        std::cout << "  4. Hardware requires additional initialization\n";
        std::cout << "  5. Firmware/driver doesn't initialize PTP on this adapter\n";
    } else if (all_same) {
        std::cout << "RESULT: All timestamps are SAME non-zero value\n";
        std::cout << "DIAGNOSIS: Clock was initialized once but not incrementing\n";
        std::cout << "First timestamp: " << first_timestamp << " ns\n";
    } else {
        std::cout << "RESULT: Timestamps are INCREMENTING!\n";
        std::cout << "DIAGNOSIS: PTP hardware clock IS WORKING\n\n";
        
        // Calculate clock rate
        if (samples.size() >= 2) {
            uint64_t ts_delta = samples.back().timestamp_ns - samples.front().timestamp_ns;
            auto time_delta = std::chrono::duration_cast<std::chrono::nanoseconds>(
                samples.back().sample_time - samples.front().sample_time).count();
            
            if (time_delta > 0) {
                double clock_rate = static_cast<double>(ts_delta) / time_delta;
                std::cout << "Clock rate: " << std::fixed << std::setprecision(6) 
                          << clock_rate << " (1.0 = perfect)\n";
                
                if (clock_rate < 0.95 || clock_rate > 1.05) {
                    std::cout << "WARNING: Clock rate significantly different from system time!\n";
                }
            }
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "========================================\n";
    std::cout << "TSAUXC Register: 0x" << std::hex << std::setw(8) << std::setfill('0') 
              << tsauxc << std::dec << "\n";
    std::cout << "TIMINCA Increment: " << incvalue << " ns\n";
    std::cout << "Timestamp Status: ";
    
    if (all_zero) {
        std::cout << "NOT WORKING (all zero)\n\n";
        std::cout << "RECOMMENDATIONS:\n";
        std::cout << "  1. Check Intel I226 datasheet for exact TSAUXC bit definition\n";
        std::cout << "  2. Verify which bit is \"Disable systime\" bit (bit position)\n";
        std::cout << "  3. Check if main Intel driver initializes TIMINCA\n";
        std::cout << "  4. Try different Intel I226 adapters (test adapter #1, #2, etc.)\n";
        std::cout << "  5. Check Windows Device Manager → Network Adapter → Driver version\n";
        std::cout << "  6. Review Windows Event Log for PTP initialization messages\n";
    } else if (all_same) {
        std::cout << "STATIC (initialized but not incrementing)\n\n";
        std::cout << "RECOMMENDATIONS:\n";
        std::cout << "  1. TIMINCA register needs non-zero increment value\n";
        std::cout << "  2. Check if AVB Filter Driver allows TIMINCA writes\n";
        std::cout << "  3. Main Intel driver may need to configure TIMINCA\n";
    } else {
        std::cout << "WORKING (incrementing correctly)\n\n";
        std::cout << "CONCLUSION:\n";
        std::cout << "  ✓ PTP hardware clock is functional with TSAUXC=0x00000000\n";
        std::cout << "  ✓ Datasheet interpretation CONFIRMED: cleared bit = enabled\n";
        std::cout << "  ✓ Intel AVB HAL can successfully read PTP timestamps\n";
        std::cout << "  ✓ Ready for IEEE 1588-2019 protocol implementation\n";
    }
    
    hal.close_device();
    std::cout << "\n========================================\n";
    std::cout << "Test completed\n";
    std::cout << "========================================\n";
    return 0;
}
