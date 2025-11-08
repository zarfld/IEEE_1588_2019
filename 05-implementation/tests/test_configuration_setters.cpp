/**
 * @file test_configuration_setters.cpp
 * @brief Test PtpPort configuration setters (lines 855-874)
 * 
 * Tests the PtpPort::set_announce_interval() and set_sync_interval()
 * configuration methods with valid and invalid values.
 * 
 * Coverage Target: Lines 855-874 in src/clocks.cpp (20 lines)
 */

// @satisfies STR-STD-001 - IEEE 1588-2019 Protocol Compliance (message intervals)
// @test-category: protocol-compliance
// @test-priority: P0

#include <cstdio>
#include <cstdlib>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace Clocks;

// Test callback stubs - minimal implementation
static Types::PTPError send_announce_stub(const AnnounceMessage&) { return Types::PTPError::Success; }
static Types::PTPError send_sync_stub(const SyncMessage&) { return Types::PTPError::Success; }
static Types::PTPError send_follow_up_stub(const FollowUpMessage&) { return Types::PTPError::Success; }
static Types::PTPError send_delay_req_stub(const DelayReqMessage&) { return Types::PTPError::Success; }
static Types::PTPError send_delay_resp_stub(const DelayRespMessage&) { return Types::PTPError::Success; }
static Types::Timestamp get_timestamp_stub() { return Types::Timestamp{}; }
static Types::PTPError get_tx_timestamp_stub(std::uint16_t, Types::Timestamp*) { return Types::PTPError::Success; }
static Types::PTPError adjust_clock_stub(std::int64_t) { return Types::PTPError::Success; }
static Types::PTPError adjust_frequency_stub(double) { return Types::PTPError::Success; }
static void on_state_change_stub(PortState, PortState) { }
static void on_fault_stub(const char*) { }

int main() {
    printf("=== Testing PtpPort Configuration Setters ===\n");
    
    // Setup configuration
    PortConfiguration config{};
    config.port_number = 1;
    config.announce_interval = 1;
    config.sync_interval = 0;
    config.delay_mechanism_p2p = false;
    
    StateCallbacks callbacks{};
    callbacks.send_announce = send_announce_stub;
    callbacks.send_sync = send_sync_stub;
    callbacks.send_follow_up = send_follow_up_stub;
    callbacks.send_delay_req = send_delay_req_stub;
    callbacks.send_delay_resp = send_delay_resp_stub;
    callbacks.get_timestamp = get_timestamp_stub;
    callbacks.get_tx_timestamp = get_tx_timestamp_stub;
    callbacks.adjust_clock = adjust_clock_stub;
    callbacks.adjust_frequency = adjust_frequency_stub;
    callbacks.on_state_change = on_state_change_stub;
    callbacks.on_fault = on_fault_stub;
    
    // Create port
    PtpPort port(config, callbacks);
    
    auto init_result = port.initialize();
    if (!init_result.is_success()) {
        printf("FAILED: Port initialization failed\n");
        return EXIT_FAILURE;
    }
    
    // Test 1: set_announce_interval() with valid value (lines 855-864)
    printf("Test 1: set_announce_interval() with valid value (log=2)...\n");
    auto result1 = port.set_announce_interval(2);  // Valid: 0-4
    if (!result1.is_success()) {
        printf("FAILED: set_announce_interval(2) should succeed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: set_announce_interval(2) succeeded\n");
    
    // Test 2: set_announce_interval() with maximum valid value
    printf("Test 2: set_announce_interval() with max valid value (log=4)...\n");
    auto result2 = port.set_announce_interval(4);  // Max valid: 4 (16 seconds)
    if (!result2.is_success()) {
        printf("FAILED: set_announce_interval(4) should succeed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: set_announce_interval(4) succeeded\n");
    
    // Test 3: set_announce_interval() with invalid value (error path)
    printf("Test 3: set_announce_interval() with invalid value (log=5)...\n");
    auto result3 = port.set_announce_interval(5);  // Invalid: > 4
    if (result3.is_success()) {
        printf("FAILED: set_announce_interval(5) should fail (> max)\n");
        return EXIT_FAILURE;
    }
    // Error checked via is_success() - no need to check error code
    printf("PASSED: set_announce_interval(5) rejected as invalid\n");
    
    // Test 4: set_sync_interval() with valid value (lines 866-874)
    printf("Test 4: set_sync_interval() with valid value (log=1)...\n");
    auto result4 = port.set_sync_interval(1);  // Valid: 0-4
    if (!result4.is_success()) {
        printf("FAILED: set_sync_interval(1) should succeed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: set_sync_interval(1) succeeded\n");
    
    // Test 5: set_sync_interval() with minimum valid value
    printf("Test 5: set_sync_interval() with min valid value (log=0)...\n");
    auto result5 = port.set_sync_interval(0);  // Min valid: 0 (1 second)
    if (!result5.is_success()) {
        printf("FAILED: set_sync_interval(0) should succeed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: set_sync_interval(0) succeeded\n");
    
    // Test 6: set_sync_interval() with invalid value (error path)
    printf("Test 6: set_sync_interval() with invalid value (log=10)...\n");
    auto result6 = port.set_sync_interval(10);  // Invalid: > 4
    if (result6.is_success()) {
        printf("FAILED: set_sync_interval(10) should fail (> max)\n");
        return EXIT_FAILURE;
    }
    // Error checked via is_success() - no need to check error code
    printf("PASSED: set_sync_interval(10) rejected as invalid\n");
    
    printf("\n=== All Configuration Setter Tests Passed ===\n");
    printf("Coverage: Lines 855-874 in set_announce_interval() and set_sync_interval() (20 lines)\n");
    
    return EXIT_SUCCESS;
}
