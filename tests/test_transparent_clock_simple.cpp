/**
 * @file test_transparent_clock_simple.cpp
 * @brief Simple verification test for TransparentClock correctionField accumulation
 * 
 * Tests basic transparent clock functionality:
 * 1. Residence time calculation from timestamps
 * 2. CorrectionField accumulation
 * 3. Negative timestamp rejection
 */

#include "clocks.hpp"
#include <cstdio>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

int main() {
    printf("=== TransparentClock Simple Verification Test ===\n\n");
    
    // Setup transparent clock
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    TransparentClock tc(TransparentClock::TransparentType::END_TO_END,
                       port_configs, 2, callbacks);
    
    // Test 1: Basic residence time (100ns)
    printf("TEST 1: Basic residence time calculation (100ns)\n");
    
    Types::Timestamp ingress;
    ingress.setTotalSeconds(1000);
    ingress.nanoseconds = 100000000;  // 1000.100000000s
    
    Types::Timestamp egress;
    egress.setTotalSeconds(1000);
    egress.nanoseconds = 100000100;   // 1000.100000100s (100ns later)
    
    // Create a mock message (CommonHeader)
    CommonHeader msg{};
    msg.correctionField = Types::CorrectionField(0);  // Start with zero correction
    
    auto result = tc.forward_message(1, 2,
                                    &msg, sizeof(msg),
                                    ingress, egress);
    
    if (!result.is_success()) {
        printf("  FAIL: forward_message returned error\n");
        return 1;
    }
    
    double correction_ns = msg.correctionField.toNanoseconds();
    printf("  CorrectionField after forwarding: %.2f ns\n", correction_ns);
    
    if (correction_ns < 99.0 || correction_ns > 101.0) {
        printf("  FAIL: Expected ~100ns, got %.2f ns\n", correction_ns);
        return 1;
    }
    
    printf("  PASS: CorrectionField correctly updated\n\n");
    
    // Test 2: Accumulation across multiple hops
    printf("TEST 2: CorrectionField accumulation (initial 50ns + 100ns hop = 150ns)\n");
    
    CommonHeader msg2{};
    msg2.correctionField = Types::CorrectionField::fromNanoseconds(50.0);  // Start with 50ns
    
    result = tc.forward_message(1, 2,
                               &msg2, sizeof(msg2),
                               ingress, egress);
    
    if (!result.is_success()) {
        printf("  FAIL: forward_message returned error\n");
        return 1;
    }
    
    correction_ns = msg2.correctionField.toNanoseconds();
    printf("  CorrectionField after forwarding: %.2f ns\n", correction_ns);
    
    if (correction_ns < 149.0 || correction_ns > 151.0) {
        printf("  FAIL: Expected ~150ns, got %.2f ns\n", correction_ns);
        return 1;
    }
    
    printf("  PASS: CorrectionField correctly accumulated\n\n");
    
    // Test 3: Negative residence time rejection
    printf("TEST 3: Negative residence time rejection\n");
    
    CommonHeader msg3{};
    msg3.correctionField = Types::CorrectionField(0);
    
    // Swap timestamps (egress before ingress - invalid!)
    result = tc.forward_message(1, 2,
                               &msg3, sizeof(msg3),
                               egress, ingress);  // SWAPPED!
    
    if (result.is_success()) {
        printf("  FAIL: forward_message should have rejected negative residence time\n");
        return 1;
    }
    
    printf("  PASS: Negative residence time correctly rejected\n\n");
    
    printf("=== All Tests PASSED ===\n");
    return 0;
}
