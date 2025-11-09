/**
 * @file test_transparent_clock_red.cpp
 * @brief RED Phase Acceptance Tests for IEEE 1588-2019 Transparent Clock
 * 
 * @details Validates transparent clock correctionField accumulation per
 *          IEEE 1588-2019 Section 11.5 - Transparent clock operation.
 *          
 * Test Coverage:
 * - Residence time calculation (egress - ingress timestamps)
 * - CorrectionField arithmetic (accumulation across hops)
 * - E2E Transparent Clock behavior (Section 6.5.4)
 * - P2P Transparent Clock behavior (Section 6.5.5)
 * - Multi-hop correction accumulation
 * - Negative residence time rejection
 * - CorrectionField overflow handling
 * - Message forwarding with timestamp capture
 * 
 * IEEE 1588-2019 References:
 * - Section 6.5.4: End-to-End Transparent Clock
 * - Section 6.5.5: Peer-to-Peer Transparent Clock
 * - Section 11.5: Transparent clock operation
 * - Section 7.3.3.5: CorrectionField format (scaled nanoseconds, 2^-16 units)
 * 
 * @test TEST-UNIT-TransparentClock-ResidenceTime
 * @test TEST-UNIT-TransparentClock-CorrectionAccumulate
 * @test TEST-INT-TransparentClock-MultiHop
 * 
 * @author IEEE 1588-2019 Implementation Team
 * @date 2025-11-09
 * @version 1.0.0
 */

#include "clocks.hpp"
#include <cstdio>
#include <cstdint>
#include <cmath>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, test_name, message) \
    do { \
        tests_run++; \
        if (!(condition)) { \
            printf("[RED-TC] FAIL: %s - %s\n", test_name, message); \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(test_name) \
    do { \
        printf("[RED-TC] PASS (unexpected): %s - implementation already works!\n", test_name); \
        tests_passed++; \
        return true; \
    } while(0)

// Helper: Create timestamp from nanoseconds
Types::Timestamp make_timestamp(uint64_t seconds, uint32_t nanoseconds) {
    Types::Timestamp ts{};
    ts.seconds_high = static_cast<uint16_t>((seconds >> 32) & 0xFFFF);
    ts.seconds_low = static_cast<uint32_t>(seconds & 0xFFFFFFFF);
    ts.nanoseconds = nanoseconds;
    return ts;
}

// Helper: Extract seconds from timestamp
uint64_t get_seconds(const Types::Timestamp& ts) {
    return (static_cast<uint64_t>(ts.seconds_high) << 32) | ts.seconds_low;
}

// Helper: Calculate time difference in nanoseconds
double timestamp_diff_ns(const Types::Timestamp& later, const Types::Timestamp& earlier) {
    uint64_t later_sec = get_seconds(later);
    uint64_t earlier_sec = get_seconds(earlier);
    
    int64_t sec_diff = static_cast<int64_t>(later_sec - earlier_sec);
    int64_t ns_diff = static_cast<int64_t>(later.nanoseconds) - static_cast<int64_t>(earlier.nanoseconds);
    
    return static_cast<double>(sec_diff) * 1e9 + static_cast<double>(ns_diff);
}

// Helper: Check if two corrections are equal (within 1ns tolerance)
bool corrections_equal(int64_t actual, int64_t expected, double tolerance_ns = 1.0) {
    // CorrectionField is in scaled nanoseconds (2^-16 ns units)
    // Convert to nanoseconds for comparison
    double actual_ns = static_cast<double>(actual) / 65536.0;
    double expected_ns = static_cast<double>(expected) / 65536.0;
    return std::abs(actual_ns - expected_ns) < tolerance_ns;
}

// Helper: Create Sync message with correctionField
SyncMessage create_sync_message(int64_t correction_field) {
    SyncMessage msg{};
    msg.header.messageType = static_cast<uint8_t>(MessageType::Sync);
    msg.header.versionPTP = 2;
    msg.header.messageLength = 44;
    msg.header.domainNumber = 0;
    msg.header.correctionField = correction_field;
    msg.header.flags = 0;
    msg.header.sequenceId = 1;
    msg.header.controlField = 0;
    msg.header.logMessageInterval = 0;
    
    // Source port identity
    for (int i = 0; i < 8; ++i) {
        msg.header.sourcePortIdentity.clockIdentity[i] = static_cast<uint8_t>(i);
    }
    msg.header.sourcePortIdentity.portNumber = 1;
    
    // Origin timestamp
    msg.body.originTimestamp = make_timestamp(1000, 100000000);
    
    return msg;
}

/**
 * @brief TEST 1: Basic residence time calculation
 * 
 * IEEE 1588-2019 Section 11.5.2.1:
 * "The residence time is the time difference between the egress timestamp
 *  and the ingress timestamp."
 * 
 * Test Case:
 * - Ingress timestamp: 1000.100000000 seconds
 * - Egress timestamp:  1000.100000050 seconds (50ns later)
 * - Expected residence time: 50ns
 * 
 * Acceptance Criteria:
 * - Residence time calculated as (egress - ingress)
 * - Result is 50ns ± 1ns tolerance
 */
bool test_basic_residence_time() {
    printf("\n[RED-TC] TEST 1: Basic residence time calculation\n");
    
    // Setup transparent clock (E2E type)
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    TransparentClock tc(TransparentClock::TransparentType::END_TO_END,
                       port_configs, 2, callbacks);
    
    // Setup timestamps
    Types::Timestamp ingress_ts = make_timestamp(1000, 100000000);  // 1000.100000000s
    Types::Timestamp egress_ts = make_timestamp(1000, 100000050);   // 1000.100000050s
    
    // Create Sync message with zero correction
    SyncMessage sync_msg = create_sync_message(0);
    
    // Forward message through transparent clock
    auto result = tc.forward_message(1, 2, 
                                    &sync_msg, sizeof(sync_msg),
                                    ingress_ts, egress_ts);
    
    TEST_ASSERT(result.is_success(), "TEST-TC-001",
               "forward_message should succeed");
    
    // Expected: correctionField += 50ns in scaled units (50 * 65536)
    int64_t expected_correction = 50LL * 65536LL;
    
    TEST_ASSERT(corrections_equal(sync_msg.header.correctionField, expected_correction),
               "TEST-TC-001",
               "correctionField should be 50ns after residence time accumulation");
    
    TEST_PASS("TEST-TC-001: Basic residence time calculation");
}

/**
 * @brief TEST 2: CorrectionField accumulation across multiple hops
 * 
 * IEEE 1588-2019 Section 11.5.2.2:
 * "The correctionField is cumulative and is increased by the residence time
 *  at each transparent clock."
 * 
 * Test Case:
 * - Initial correctionField: +20ns (from previous hop)
 * - Hop 1 residence time: 30ns
 * - Hop 2 residence time: 40ns
 * - Expected final correction: 20 + 30 + 40 = 90ns
 * 
 * Acceptance Criteria:
 * - CorrectionField accumulates additively
 * - Result is 90ns ± 1ns tolerance
 * - No overflow or underflow
 */
bool test_correction_field_accumulation() {
    printf("\n[RED-TC] TEST 2: CorrectionField accumulation across hops\n");
    
    // Setup transparent clocks
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    TransparentClock tc1(TransparentClock::TransparentType::END_TO_END,
                        port_configs, 2, callbacks);
    TransparentClock tc2(TransparentClock::TransparentType::END_TO_END,
                        port_configs, 2, callbacks);
    
    // Create Sync message with initial +20ns correction
    int64_t initial_correction = 20LL * 65536LL;  // 20ns in scaled units
    SyncMessage sync_msg = create_sync_message(initial_correction);
    
    // Hop 1: +30ns residence time
    Types::Timestamp ingress1 = make_timestamp(1000, 100000000);
    Types::Timestamp egress1 = make_timestamp(1000, 100000030);
    
    auto result1 = tc1.forward_message(1, 2,
                                      &sync_msg, sizeof(sync_msg),
                                      ingress1, egress1);
    TEST_ASSERT(result1.is_success(), "TEST-TC-002", "Hop 1 forward should succeed");
    
    // Hop 2: +40ns residence time
    Types::Timestamp ingress2 = make_timestamp(1000, 200000000);
    Types::Timestamp egress2 = make_timestamp(1000, 200000040);
    
    auto result2 = tc2.forward_message(1, 2,
                                      &sync_msg, sizeof(sync_msg),
                                      ingress2, egress2);
    TEST_ASSERT(result2.is_success(), "TEST-TC-002", "Hop 2 forward should succeed");
    
    // Expected: 20 + 30 + 40 = 90ns
    int64_t expected_correction = 90LL * 65536LL;
    
    TEST_ASSERT(corrections_equal(sync_msg.header.correctionField, expected_correction),
               "TEST-TC-002",
               "correctionField should accumulate to 90ns after two hops");
    
    TEST_PASS("TEST-TC-002: CorrectionField accumulation");
}

/**
 * @brief TEST 3: Negative residence time rejection
 * 
 * IEEE 1588-2019 Section 11.5.2.1:
 * "The residence time must be non-negative."
 * 
 * Test Case:
 * - Ingress timestamp: 1000.200000000 seconds
 * - Egress timestamp:  1000.100000000 seconds (100ms earlier - invalid!)
 * - Expected: Error/rejection
 * 
 * Acceptance Criteria:
 * - forward_message returns error for negative residence time
 * - CorrectionField remains unchanged
 */
bool test_negative_residence_time() {
    printf("\n[RED-TC] TEST 3: Negative residence time rejection\n");
    
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    TransparentClock tc(TransparentClock::TransparentType::END_TO_END,
                       port_configs, 2, callbacks);
    
    // Setup invalid timestamps (egress before ingress)
    Types::Timestamp ingress_ts = make_timestamp(1000, 200000000);  // Later
    Types::Timestamp egress_ts = make_timestamp(1000, 100000000);   // Earlier (INVALID!)
    
    SyncMessage sync_msg = create_sync_message(0);
    int64_t original_correction = sync_msg.header.correctionField;
    
    // Attempt to forward with invalid timestamps
    auto result = tc.forward_message(1, 2,
                                    &sync_msg, sizeof(sync_msg),
                                    ingress_ts, egress_ts);
    
    TEST_ASSERT(!result.is_success(), "TEST-TC-003",
               "forward_message should fail for negative residence time");
    
    TEST_ASSERT(sync_msg.header.correctionField == original_correction,
               "TEST-TC-003",
               "correctionField should remain unchanged on error");
    
    TEST_PASS("TEST-TC-003: Negative residence time rejection");
}

/**
 * @brief TEST 4: Large residence time (seconds component)
 * 
 * IEEE 1588-2019 Section 7.3.3.5:
 * "The correctionField is expressed in nanoseconds multiplied by 2^16."
 * 
 * Test Case:
 * - Ingress timestamp: 1000.000000000 seconds
 * - Egress timestamp:  1002.500000000 seconds (2.5 second residence time)
 * - Expected correction: +2,500,000,000ns (2.5 seconds)
 * 
 * Acceptance Criteria:
 * - Handles residence times with seconds component
 * - CorrectionField correctly represents large values
 * - No overflow in scaled nanosecond representation
 */
bool test_large_residence_time() {
    printf("\n[RED-TC] TEST 4: Large residence time (seconds component)\n");
    
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    TransparentClock tc(TransparentClock::TransparentType::END_TO_END,
                       port_configs, 2, callbacks);
    
    // Setup timestamps with 2.5 second difference
    Types::Timestamp ingress_ts = make_timestamp(1000, 0);          // 1000.000000000s
    Types::Timestamp egress_ts = make_timestamp(1002, 500000000);   // 1002.500000000s
    
    SyncMessage sync_msg = create_sync_message(0);
    
    auto result = tc.forward_message(1, 2,
                                    &sync_msg, sizeof(sync_msg),
                                    ingress_ts, egress_ts);
    
    TEST_ASSERT(result.is_success(), "TEST-TC-004",
               "forward_message should succeed for large residence time");
    
    // Expected: 2.5 seconds = 2,500,000,000ns in scaled units
    int64_t expected_correction = 2500000000LL * 65536LL;
    
    TEST_ASSERT(corrections_equal(sync_msg.header.correctionField, expected_correction),
               "TEST-TC-004",
               "correctionField should represent 2.5 second residence time");
    
    TEST_PASS("TEST-TC-004: Large residence time");
}

/**
 * @brief TEST 5: E2E Transparent Clock specific behavior
 * 
 * IEEE 1588-2019 Section 6.5.4:
 * "An end-to-end transparent clock forwards Sync, Follow_Up, Delay_Req,
 *  and Delay_Resp messages and updates their correctionField."
 * 
 * Test Case:
 * - E2E transparent clock configuration
 * - Verify residence time added to correctionField
 * - Verify operation for event messages
 * 
 * Acceptance Criteria:
 * - E2E TC adds residence time to Sync messages
 * - CorrectionField properly updated
 */
bool test_e2e_transparent_clock() {
    printf("\n[RED-TC] TEST 5: E2E Transparent Clock behavior\n");
    
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    // Create E2E Transparent Clock
    TransparentClock tc(TransparentClock::TransparentType::END_TO_END,
                       port_configs, 2, callbacks);
    
    TEST_ASSERT(tc.get_transparent_type() == TransparentClock::TransparentType::END_TO_END,
               "TEST-TC-005",
               "Transparent clock should be E2E type");
    
    TEST_ASSERT(tc.get_clock_type() == ClockType::E2E_Transparent,
               "TEST-TC-005",
               "Clock type should be E2E_Transparent");
    
    // Setup timestamps: 100ns residence time
    Types::Timestamp ingress_ts = make_timestamp(1000, 100000000);
    Types::Timestamp egress_ts = make_timestamp(1000, 100000100);
    
    SyncMessage sync_msg = create_sync_message(0);
    
    auto result = tc.forward_message(1, 2,
                                    &sync_msg, sizeof(sync_msg),
                                    ingress_ts, egress_ts);
    
    TEST_ASSERT(result.is_success(), "TEST-TC-005",
               "E2E TC should successfully forward message");
    
    int64_t expected_correction = 100LL * 65536LL;  // 100ns
    
    TEST_ASSERT(corrections_equal(sync_msg.header.correctionField, expected_correction),
               "TEST-TC-005",
               "E2E TC should add 100ns residence time to correctionField");
    
    TEST_PASS("TEST-TC-005: E2E Transparent Clock");
}

/**
 * @brief TEST 6: P2P Transparent Clock specific behavior
 * 
 * IEEE 1588-2019 Section 6.5.5:
 * "A peer-to-peer transparent clock forwards Sync, Follow_Up messages
 *  and updates their correctionField. It also measures peer delay."
 * 
 * Test Case:
 * - P2P transparent clock configuration
 * - Verify residence time added to correctionField
 * - Verify P2P TC type correctly identified
 * 
 * Acceptance Criteria:
 * - P2P TC adds residence time to Sync messages
 * - CorrectionField properly updated
 * - Type correctly identified as P2P
 */
bool test_p2p_transparent_clock() {
    printf("\n[RED-TC] TEST 6: P2P Transparent Clock behavior\n");
    
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    // Create P2P Transparent Clock
    TransparentClock tc(TransparentClock::TransparentType::PEER_TO_PEER,
                       port_configs, 2, callbacks);
    
    TEST_ASSERT(tc.get_transparent_type() == TransparentClock::TransparentType::PEER_TO_PEER,
               "TEST-TC-006",
               "Transparent clock should be P2P type");
    
    TEST_ASSERT(tc.get_clock_type() == ClockType::P2P_Transparent,
               "TEST-TC-006",
               "Clock type should be P2P_Transparent");
    
    // Setup timestamps: 75ns residence time
    Types::Timestamp ingress_ts = make_timestamp(1000, 100000000);
    Types::Timestamp egress_ts = make_timestamp(1000, 100000075);
    
    SyncMessage sync_msg = create_sync_message(0);
    
    auto result = tc.forward_message(1, 2,
                                    &sync_msg, sizeof(sync_msg),
                                    ingress_ts, egress_ts);
    
    TEST_ASSERT(result.is_success(), "TEST-TC-006",
               "P2P TC should successfully forward message");
    
    int64_t expected_correction = 75LL * 65536LL;  // 75ns
    
    TEST_ASSERT(corrections_equal(sync_msg.header.correctionField, expected_correction),
               "TEST-TC-006",
               "P2P TC should add 75ns residence time to correctionField");
    
    TEST_PASS("TEST-TC-006: P2P Transparent Clock");
}

/**
 * @brief TEST 7: Multi-hop transparent clock chain
 * 
 * IEEE 1588-2019 Section 11.5.2.2:
 * "In a chain of transparent clocks, each TC adds its residence time
 *  to the cumulative correctionField."
 * 
 * Test Case:
 * - Chain of 4 transparent clocks
 * - Each with different residence times: 10ns, 20ns, 30ns, 40ns
 * - Initial correction: 5ns
 * - Expected final: 5 + 10 + 20 + 30 + 40 = 105ns
 * 
 * Acceptance Criteria:
 * - CorrectionField accumulates across all hops
 * - No arithmetic errors in multi-hop scenario
 * - Final correction matches expected sum
 */
bool test_multi_hop_transparent_chain() {
    printf("\n[RED-TC] TEST 7: Multi-hop transparent clock chain\n");
    
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    // Create 4 transparent clocks
    TransparentClock tc1(TransparentClock::TransparentType::END_TO_END,
                        port_configs, 2, callbacks);
    TransparentClock tc2(TransparentClock::TransparentType::END_TO_END,
                        port_configs, 2, callbacks);
    TransparentClock tc3(TransparentClock::TransparentType::END_TO_END,
                        port_configs, 2, callbacks);
    TransparentClock tc4(TransparentClock::TransparentType::END_TO_END,
                        port_configs, 2, callbacks);
    
    // Start with +5ns correction
    SyncMessage sync_msg = create_sync_message(5LL * 65536LL);
    
    // Hop 1: +10ns
    auto result1 = tc1.forward_message(1, 2, &sync_msg, sizeof(sync_msg),
                                      make_timestamp(1000, 100000000),
                                      make_timestamp(1000, 100000010));
    TEST_ASSERT(result1.is_success(), "TEST-TC-007", "Hop 1 should succeed");
    
    // Hop 2: +20ns
    auto result2 = tc2.forward_message(1, 2, &sync_msg, sizeof(sync_msg),
                                      make_timestamp(1001, 0),
                                      make_timestamp(1001, 20));
    TEST_ASSERT(result2.is_success(), "TEST-TC-007", "Hop 2 should succeed");
    
    // Hop 3: +30ns
    auto result3 = tc3.forward_message(1, 2, &sync_msg, sizeof(sync_msg),
                                      make_timestamp(1002, 0),
                                      make_timestamp(1002, 30));
    TEST_ASSERT(result3.is_success(), "TEST-TC-007", "Hop 3 should succeed");
    
    // Hop 4: +40ns
    auto result4 = tc4.forward_message(1, 2, &sync_msg, sizeof(sync_msg),
                                      make_timestamp(1003, 0),
                                      make_timestamp(1003, 40));
    TEST_ASSERT(result4.is_success(), "TEST-TC-007", "Hop 4 should succeed");
    
    // Expected: 5 + 10 + 20 + 30 + 40 = 105ns
    int64_t expected_correction = 105LL * 65536LL;
    
    TEST_ASSERT(corrections_equal(sync_msg.header.correctionField, expected_correction),
               "TEST-TC-007",
               "correctionField should accumulate to 105ns after 4 hops");
    
    TEST_PASS("TEST-TC-007: Multi-hop transparent chain");
}

/**
 * @brief TEST 8: Zero residence time handling
 * 
 * Edge case: Message egress timestamp equals ingress timestamp.
 * 
 * Test Case:
 * - Ingress timestamp: 1000.100000000s
 * - Egress timestamp:  1000.100000000s (same - zero residence)
 * - Expected correction: no change (0ns added)
 * 
 * Acceptance Criteria:
 * - Zero residence time is valid
 * - CorrectionField remains unchanged
 * - No error returned
 */
bool test_zero_residence_time() {
    printf("\n[RED-TC] TEST 8: Zero residence time handling\n");
    
    std::array<PortConfiguration, 16> port_configs{};
    port_configs[0].port_number = 1;
    port_configs[1].port_number = 2;
    
    StateCallbacks callbacks{};
    
    TransparentClock tc(TransparentClock::TransparentType::END_TO_END,
                       port_configs, 2, callbacks);
    
    // Same timestamp for ingress and egress
    Types::Timestamp timestamp = make_timestamp(1000, 100000000);
    
    int64_t initial_correction = 50LL * 65536LL;  // Start with 50ns
    SyncMessage sync_msg = create_sync_message(initial_correction);
    
    auto result = tc.forward_message(1, 2,
                                    &sync_msg, sizeof(sync_msg),
                                    timestamp, timestamp);
    
    TEST_ASSERT(result.is_success(), "TEST-TC-008",
               "forward_message should succeed for zero residence time");
    
    // Expected: 50ns (unchanged)
    TEST_ASSERT(corrections_equal(sync_msg.header.correctionField, initial_correction),
               "TEST-TC-008",
               "correctionField should remain 50ns with zero residence time");
    
    TEST_PASS("TEST-TC-008: Zero residence time");
}

// Main test runner
int transparent_clock_red_main() {
    printf("========================================\n");
    printf("IEEE 1588-2019 Transparent Clock Tests\n");
    printf("RED Phase - Acceptance Test Suite\n");
    printf("========================================\n");
    
    // Run all tests
    test_basic_residence_time();
    test_correction_field_accumulation();
    test_negative_residence_time();
    test_large_residence_time();
    test_e2e_transparent_clock();
    test_p2p_transparent_clock();
    test_multi_hop_transparent_chain();
    test_zero_residence_time();
    
    // Print summary
    printf("\n========================================\n");
    printf("Test Summary:\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("========================================\n");
    
    // RED phase expectation: all tests should fail initially
    if (tests_failed == tests_run && tests_passed == 0) {
        printf("\n[RED] ✓ All tests failing as expected (proper RED phase)\n");
        return 0;  // Success for RED phase
    } else if (tests_passed == tests_run && tests_failed == 0) {
        printf("\n[GREEN] ✓ All tests passing (implementation complete!)\n");
        return 0;  // Success - implementation already works
    } else {
        printf("\n[MIXED] ⚠ Some tests passing, some failing\n");
        return 1;  // Mixed state
    }
}

// Standalone execution support
#ifndef INTEGRATED_TEST_SUITE
int main() {
    return transparent_clock_red_main();
}
#endif
