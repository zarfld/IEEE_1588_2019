/**
 * @file test_ordinary_clock_dispatcher.cpp
 * @brief Test OrdinaryClock::process_message() message dispatcher (lines 912-946)
 * 
 * Tests the OrdinaryClock message routing logic that dispatches different
 * PTP message types to the appropriate port handler methods.
 * 
 * Coverage Target: Lines 912-946 in src/clocks.cpp (35 lines)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;

// Test callback tracking
static bool send_callback_invoked = false;
static bool timer_callback_invoked = false;

static void test_send_callback(Types::PortNumber port_number,
                               std::uint8_t message_type,
                               const void* message_data,
                               std::size_t message_size) {
    send_callback_invoked = true;
}

static void test_timer_callback(Types::PortNumber port_number,
                               std::uint32_t timer_id,
                               std::uint32_t interval_ms) {
    timer_callback_invoked = true;
}

static void test_state_change_callback(Types::PortNumber port_number,
                                      PortState old_state,
                                      PortState new_state) {
    // State changes tracked
}

int main() {
    printf("=== Testing OrdinaryClock Message Dispatcher ===\n");
    
    // Setup configuration
    PortConfiguration config{};
    config.port_number = 1;
    config.announce_interval = 1;
    config.sync_interval = 0;
    config.delay_mechanism_p2p = false;  // E2E mode
    config.clock_identity = Types::ClockIdentity{0x00, 0x11, 0x22, 0xFF, 0xFE, 0x33, 0x44, 0x55};
    
    StateCallbacks callbacks{};
    callbacks.send_callback = test_send_callback;
    callbacks.timer_callback = test_timer_callback;
    callbacks.state_change_callback = test_state_change_callback;
    
    // Create OrdinaryClock (not bare PtpPort)
    OrdinaryClock clock(config, callbacks);
    
    auto init_result = clock.initialize();
    if (!init_result.is_success()) {
        printf("FAILED: Clock initialization failed\n");
        return EXIT_FAILURE;
    }
    
    auto start_result = clock.start();
    if (!start_result.is_success()) {
        printf("FAILED: Clock start failed\n");
        return EXIT_FAILURE;
    }
    
    // Create test timestamp
    Types::Timestamp rx_timestamp{1000000000, 500000000};  // 1.5 seconds
    
    // Test 1: Process Announce message through dispatcher
    printf("Test 1: Announce message dispatch...\n");
    AnnounceMessage announce_msg{};
    announce_msg.header.message_type = static_cast<std::uint8_t>(MessageType::Announce);
    announce_msg.header.version_ptp = 2;
    announce_msg.header.message_length = sizeof(AnnounceMessage);
    announce_msg.header.source_port_identity.clock_identity = Types::ClockIdentity{0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0xDD, 0xEE, 0xFF};
    announce_msg.header.source_port_identity.port_number = 1;
    announce_msg.header.sequence_id = 100;
    announce_msg.current_utc_offset = 37;
    announce_msg.grandmaster_priority1 = 128;
    announce_msg.grandmaster_clock_quality.clock_class = 248;
    announce_msg.grandmaster_clock_quality.clock_accuracy = 0x20;
    announce_msg.grandmaster_clock_quality.offset_scaled_log_variance = 0x4E5D;
    announce_msg.grandmaster_priority2 = 128;
    announce_msg.grandmaster_identity = announce_msg.header.source_port_identity.clock_identity;
    announce_msg.steps_removed = 0;
    announce_msg.time_source = 0xA0;  // INTERNAL_OSCILLATOR
    
    auto announce_result = clock.process_message(
        static_cast<std::uint8_t>(MessageType::Announce),
        &announce_msg,
        sizeof(AnnounceMessage),
        rx_timestamp
    );
    
    if (!announce_result.is_success()) {
        printf("FAILED: Announce message dispatch failed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: Announce message dispatched\n");
    
    // Test 2: Process Sync message through dispatcher
    printf("Test 2: Sync message dispatch...\n");
    SyncMessage sync_msg{};
    sync_msg.header.message_type = static_cast<std::uint8_t>(MessageType::Sync);
    sync_msg.header.version_ptp = 2;
    sync_msg.header.message_length = sizeof(SyncMessage);
    sync_msg.header.source_port_identity.clock_identity = announce_msg.header.source_port_identity.clock_identity;
    sync_msg.header.source_port_identity.port_number = 1;
    sync_msg.header.sequence_id = 200;
    sync_msg.origin_timestamp.seconds_high = 0;
    sync_msg.origin_timestamp.seconds_low = 1;
    sync_msg.origin_timestamp.nanoseconds = 100000000;
    
    auto sync_result = clock.process_message(
        static_cast<std::uint8_t>(MessageType::Sync),
        &sync_msg,
        sizeof(SyncMessage),
        rx_timestamp
    );
    
    if (!sync_result.is_success()) {
        printf("FAILED: Sync message dispatch failed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: Sync message dispatched\n");
    
    // Test 3: Process Follow_Up message through dispatcher
    printf("Test 3: Follow_Up message dispatch...\n");
    FollowUpMessage follow_up_msg{};
    follow_up_msg.header.message_type = static_cast<std::uint8_t>(MessageType::Follow_Up);
    follow_up_msg.header.version_ptp = 2;
    follow_up_msg.header.message_length = sizeof(FollowUpMessage);
    follow_up_msg.header.source_port_identity = sync_msg.header.source_port_identity;
    follow_up_msg.header.sequence_id = 200;  // Must match Sync
    follow_up_msg.precise_origin_timestamp.seconds_high = 0;
    follow_up_msg.precise_origin_timestamp.seconds_low = 1;
    follow_up_msg.precise_origin_timestamp.nanoseconds = 100123456;
    
    auto follow_up_result = clock.process_message(
        static_cast<std::uint8_t>(MessageType::Follow_Up),
        &follow_up_msg,
        sizeof(FollowUpMessage),
        rx_timestamp
    );
    
    if (!follow_up_result.is_success()) {
        printf("FAILED: Follow_Up message dispatch failed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: Follow_Up message dispatched\n");
    
    // Test 4: Process Delay_Req through dispatcher (Slave sending)
    printf("Test 4: Delay_Req message dispatch...\n");
    DelayReqMessage delay_req_msg{};
    delay_req_msg.header.message_type = static_cast<std::uint8_t>(MessageType::Delay_Req);
    delay_req_msg.header.version_ptp = 2;
    delay_req_msg.header.message_length = sizeof(DelayReqMessage);
    delay_req_msg.header.source_port_identity = config.clock_identity;  // From us (Slave)
    delay_req_msg.header.source_port_identity.port_number = config.port_number;
    delay_req_msg.header.sequence_id = 300;
    delay_req_msg.origin_timestamp.seconds_high = 0;
    delay_req_msg.origin_timestamp.seconds_low = 1;
    delay_req_msg.origin_timestamp.nanoseconds = 200000000;
    
    auto delay_req_result = clock.process_message(
        static_cast<std::uint8_t>(MessageType::Delay_Req),
        &delay_req_msg,
        sizeof(DelayReqMessage),
        rx_timestamp
    );
    
    if (!delay_req_result.is_success()) {
        printf("FAILED: Delay_Req message dispatch failed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: Delay_Req message dispatched\n");
    
    // Test 5: Process Delay_Resp through dispatcher
    printf("Test 5: Delay_Resp message dispatch...\n");
    DelayRespMessage delay_resp_msg{};
    delay_resp_msg.header.message_type = static_cast<std::uint8_t>(MessageType::Delay_Resp);
    delay_resp_msg.header.version_ptp = 2;
    delay_resp_msg.header.message_length = sizeof(DelayRespMessage);
    delay_resp_msg.header.source_port_identity = announce_msg.header.source_port_identity;
    delay_resp_msg.header.sequence_id = 300;  // Must match Delay_Req
    delay_resp_msg.receive_timestamp.seconds_high = 0;
    delay_resp_msg.receive_timestamp.seconds_low = 1;
    delay_resp_msg.receive_timestamp.nanoseconds = 205000000;
    delay_resp_msg.requesting_port_identity.clock_identity = config.clock_identity;
    delay_resp_msg.requesting_port_identity.port_number = config.port_number;
    
    auto delay_resp_result = clock.process_message(
        static_cast<std::uint8_t>(MessageType::Delay_Resp),
        &delay_resp_msg,
        sizeof(DelayRespMessage),
        rx_timestamp
    );
    
    if (!delay_resp_result.is_success()) {
        printf("FAILED: Delay_Resp message dispatch failed\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: Delay_Resp message dispatched\n");
    
    // Test 6: Invalid message size handling
    printf("Test 6: Invalid message size (too small)...\n");
    auto invalid_size_result = clock.process_message(
        static_cast<std::uint8_t>(MessageType::Announce),
        &announce_msg,
        10,  // Too small for AnnounceMessage
        rx_timestamp
    );
    
    if (invalid_size_result.is_success()) {
        printf("FAILED: Should reject too-small message\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: Invalid size rejected\n");
    
    // Test 7: Unsupported message type
    printf("Test 7: Unsupported message type...\n");
    auto unsupported_result = clock.process_message(
        0xFF,  // Invalid message type
        &announce_msg,
        sizeof(AnnounceMessage),
        rx_timestamp
    );
    
    if (unsupported_result.is_success()) {
        printf("FAILED: Should reject unsupported message type\n");
        return EXIT_FAILURE;
    }
    printf("PASSED: Unsupported message type rejected\n");
    
    printf("\n=== All OrdinaryClock Dispatcher Tests Passed ===\n");
    printf("Coverage: Lines 912-946 in process_message() (~35 lines)\n");
    
    return EXIT_SUCCESS;
}
