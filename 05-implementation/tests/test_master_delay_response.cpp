// @satisfies STR-PERF-004 - Path Delay Measurement (Master Delay_Resp handling)
// @satisfies STR-STD-001 - IEEE 1588-2019 Protocol Compliance (message handling)
/**
 * @file test_master_delay_response.cpp
 * @brief Test Master-side process_delay_req() sending Delay_Resp (lines 472-491)
 * 
 * Tests the Master port's response to Delay_Req messages, including:
 * - Delay_Resp message construction
 * - Timestamping of received Delay_Req
 * - Callback invocation for sending response
 * 
 * Coverage Target: Lines 472-491 in src/clocks.cpp (20 lines)
 */

#include <cstdio>
#include <cstdlib>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace Clocks;

// Track callback invocations
static bool delay_resp_sent = false;
static DelayRespMessage captured_delay_resp{};

// Test callback implementations
static Types::PTPError send_announce_stub(const AnnounceMessage&) { 
    return Types::PTPError::Success; 
}

static Types::PTPError send_sync_stub(const SyncMessage&) { 
    return Types::PTPError::Success; 
}

static Types::PTPError send_follow_up_stub(const FollowUpMessage&) { 
    return Types::PTPError::Success; 
}

static Types::PTPError send_delay_req_stub(const DelayReqMessage&) { 
    return Types::PTPError::Success; 
}

static Types::PTPError send_delay_resp_stub(const DelayRespMessage& msg) { 
    delay_resp_sent = true;
    captured_delay_resp = msg;
    return Types::PTPError::Success; 
}

static Types::Timestamp get_timestamp_stub() { 
    return Types::Timestamp{}; 
}

static Types::PTPError get_tx_timestamp_stub(std::uint16_t, Types::Timestamp*) { 
    return Types::PTPError::Success; 
}

static Types::PTPError adjust_clock_stub(std::int64_t) { 
    return Types::PTPError::Success; 
}

static Types::PTPError adjust_frequency_stub(double) { 
    return Types::PTPError::Success; 
}

static void on_state_change_stub(PortState, PortState) { }

static void on_fault_stub(const char*) { }

int main() {
    printf("=== Testing Master Delay Response ===\n");
    
    // Setup configuration
    PortConfiguration config{};
    config.port_number = 1;
    config.domain_number = 0;
    config.announce_interval = 1;
    config.sync_interval = 0;
    config.delay_mechanism_p2p = false;  // E2E mode
    
    StateCallbacks callbacks{};
    callbacks.send_announce = send_announce_stub;
    callbacks.send_sync = send_sync_stub;
    callbacks.send_follow_up = send_follow_up_stub;
    callbacks.send_delay_req = send_delay_req_stub;
    callbacks.send_delay_resp = send_delay_resp_stub;  // KEY: Capture Delay_Resp
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
    
    auto start_result = port.start();
    if (!start_result.is_success()) {
        printf("FAILED: Port start failed\n");
        return EXIT_FAILURE;
    }
    
    // Transition port to Master state
    // First send better Announce to become Master
    port.process_event(StateEvent::RS_MASTER);
    
    // Verify we're in Master state (or PreMaster transitioning to Master)
    auto port_state = port.get_state();
    if (port_state != PortState::Master && port_state != PortState::PreMaster) {
        printf("FAILED: Port not in Master/PreMaster state (state=%d)\n", static_cast<int>(port_state));
        return EXIT_FAILURE;
    }
    
    // If PreMaster, trigger qualification timeout to become Master
    if (port_state == PortState::PreMaster) {
        port.process_event(StateEvent::QUALIFICATION_TIMEOUT);
        port_state = port.get_state();
        if (port_state != PortState::Master) {
            printf("FAILED: Port not in Master state after qualification (state=%d)\n", static_cast<int>(port_state));
            return EXIT_FAILURE;
        }
    }
    
    printf("Test 1: Master receives Delay_Req and sends Delay_Resp...\n");
    
    // Reset callback tracking
    delay_resp_sent = false;
    
    // Create Delay_Req from Slave
    DelayReqMessage delay_req{};
    delay_req.header.setMessageType(MessageType::Delay_Req);
    delay_req.header.setVersion(2);
    delay_req.header.domainNumber = 0;
    delay_req.header.sequenceId = 42;
    
    // Set source port identity (from Slave)
    Types::ClockIdentity slave_clock_id{0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0xDD, 0xEE, 0xFF};
    delay_req.header.sourcePortIdentity.clock_identity = slave_clock_id;
    delay_req.header.sourcePortIdentity.port_number = 1;
    
    // Set origin timestamp (T3 from Slave's perspective)
    Types::Timestamp t3{};
    t3.setTotalSeconds(1000);
    t3.nanoseconds = 300000000;  // 1000.300 seconds
    delay_req.body.originTimestamp = t3;
    
    // Master receives Delay_Req at T4
    Types::Timestamp t4{};
    t4.setTotalSeconds(1000);
    t4.nanoseconds = 305000000;  // 1000.305 seconds (5ms path delay)
    
    // Process Delay_Req in Master state - should send Delay_Resp
    auto result = port.process_delay_req(delay_req, t4);
    
    if (!result.is_success()) {
        printf("FAILED: process_delay_req() failed\n");
        return EXIT_FAILURE;
    }
    
    // Verify Delay_Resp was sent
    if (!delay_resp_sent) {
        printf("FAILED: Delay_Resp callback was not invoked\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: Delay_Resp send callback invoked\n");
    
    // Test 2: Verify Delay_Resp message contents
    printf("Test 2: Verify Delay_Resp message structure...\n");
    
    // Check message type
    if (captured_delay_resp.header.getMessageType() != MessageType::Delay_Resp) {
        printf("FAILED: Wrong message type in Delay_Resp\n");
        return EXIT_FAILURE;
    }
    
    // Check sequence ID matches Delay_Req
    if (captured_delay_resp.header.sequenceId != 42) {
        printf("FAILED: Sequence ID mismatch (expected 42, got %d)\n", 
               captured_delay_resp.header.sequenceId);
        return EXIT_FAILURE;
    }
    
    // Check receive timestamp is T4
    auto resp_timestamp = captured_delay_resp.body.receiveTimestamp;
    if (resp_timestamp.getTotalSeconds() != 1000 || 
        resp_timestamp.nanoseconds != 305000000) {
        printf("FAILED: Receive timestamp mismatch\n");
        return EXIT_FAILURE;
    }
    
    // Check requesting port identity matches Delay_Req source
    auto& req_port_id = captured_delay_resp.body.requestingPortIdentity;
    if (req_port_id.clock_identity != slave_clock_id || 
        req_port_id.port_number != 1) {
        printf("FAILED: Requesting port identity mismatch\n");
        return EXIT_FAILURE;
    }
    
    printf("PASSED: Delay_Resp message correctly formed\n");
    
    // Test 3: Verify domain number
    printf("Test 3: Verify domain number preservation...\n");
    if (captured_delay_resp.header.domainNumber != 0) {
        printf("FAILED: Domain number not preserved (expected 0, got %d)\n",
               captured_delay_resp.header.domainNumber);
        return EXIT_FAILURE;
    }
    printf("PASSED: Domain number preserved\n");
    
    printf("\n=== All Master Delay Response Tests Passed ===\n");
    printf("Coverage: Lines 472-491 in process_delay_req() Master path (20 lines)\n");
    printf("Key coverage:\n");
    printf("  - Delay_Resp message construction\n");
    printf("  - Header field population (messageType, sequenceId, sourcePortIdentity)\n");
    printf("  - Body field population (receiveTimestamp, requestingPortIdentity)\n");
    printf("  - send_delay_resp callback invocation\n");
    printf("  - Statistics increment (delay_resp_messages_sent)\n");
    
    return EXIT_SUCCESS;
}
