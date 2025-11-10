/**
 * @file test_message_flow_integration.cpp
 * @brief Integration tests for Message Flow Coordinator
 * 
 * Tests complete message processing pipeline:
 * - Announce → BMCA → State transitions
 * - Sync → Offset → Servo adjustments
 * - Error handling and recovery
 * - Health monitoring
 * 
 * Phase: 06-integration
 * Task: Task 4 - Message Flow Integration
 * 
 * Test Coverage:
 * 1. Coordinator lifecycle (start/stop/reset)
 * 2. Announce message processing
 * 3. Sync message processing
 * 4. Follow_Up message processing
 * 5. Delay_Resp message processing
 * 6. Domain filtering
 * 7. Message validation
 * 8. Health status monitoring
 * 9. Statistics tracking
 * 10. Component integration
 */

#include "IEEE/1588/PTP/2019/message_flow_integration.hpp"
#include "IEEE/1588/PTP/2019/bmca_integration.hpp"
#include "IEEE/1588/PTP/2019/sync_integration.hpp"
#include "IEEE/1588/PTP/2019/servo_integration.hpp"
#include "IEEE/1588/PTP/2019/clocks.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"
#include <iostream>
#include <cassert>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Integration;
using namespace IEEE::_1588::PTP::_2019::servo;

// Simple test framework macros (no GoogleTest dependency)
#define EXPECT_TRUE(condition) assert(condition)
#define EXPECT_FALSE(condition) assert(!(condition))
#define EXPECT_EQ(a, b) assert((a) == (b))
#define TEST_F(fixture, name) void fixture##_##name()

//==============================================================================
// Mock Infrastructure for Dependencies
//==============================================================================

// Mock clock state for StateCallbacks
struct MockClockState {
    int64_t phase_offset_ns = 0;
    double frequency_offset_ppb = 0.0;
};

static MockClockState g_mock_clock;

static Types::PTPError mock_adjust_clock(int64_t offset_ns) {
    g_mock_clock.phase_offset_ns += offset_ns;
    return Types::PTPError::Success;
}

static Types::PTPError mock_adjust_frequency(double offset_ppb) {
    g_mock_clock.frequency_offset_ppb = offset_ppb;
    return Types::PTPError::Success;
}

static Clocks::StateCallbacks create_mock_callbacks() {
    Clocks::StateCallbacks callbacks;
    callbacks.adjust_clock = mock_adjust_clock;
    callbacks.adjust_frequency = mock_adjust_frequency;
    return callbacks;
}

static void reset_mock_clock() {
    g_mock_clock.phase_offset_ns = 0;
    g_mock_clock.frequency_offset_ppb = 0.0;
}

// Mock PtpPort for testing
class MockPtpPort : public Clocks::PtpPort {
public:
    MockPtpPort() : PtpPort(Clocks::PtpClock{}, 1, create_mock_callbacks()) {
        // Initialize with default state
    }
    
    // Expose state for testing
    void set_port_state(Types::PortState state) {
        // Would set internal state in real implementation
    }
};

//==============================================================================
// Test Fixture - Global test environment
//==============================================================================

static std::unique_ptr<MockPtpPort> g_mock_port;
static std::unique_ptr<BMCACoordinator> g_bmca;
static std::unique_ptr<SyncCoordinator> g_sync;
static std::unique_ptr<ServoIntegration> g_servo;
static std::unique_ptr<MessageFlowCoordinator> g_coordinator;

void SetUpTest() {
    reset_mock_clock();
    
    // Create mock port
    g_mock_port = std::make_unique<MockPtpPort>();
    
    // Create coordinators
    g_bmca = std::make_unique<BMCACoordinator>(*g_mock_port);
    g_sync = std::make_unique<SyncCoordinator>(*g_mock_port);
    
    auto callbacks = create_mock_callbacks();
    g_servo = std::make_unique<ServoIntegration>(callbacks);
    
    // Create message flow coordinator
    g_coordinator = std::make_unique<MessageFlowCoordinator>(
        *g_bmca, *g_sync, *g_servo, *g_mock_port
    );
    
    // Configure components
    BMCAConfiguration bmca_config = BMCAConfiguration::create_default();
    g_bmca->configure(bmca_config);
    
    SyncConfiguration sync_config = SyncConfiguration::create_default();
    g_sync->configure(sync_config);
    
    ServoConfiguration servo_config;
    servo_config.kp = 0.7;
    servo_config.ki = 0.3;
    g_servo->configure(servo_config);
}

void TearDownTest() {
    g_coordinator.reset();
    g_servo.reset();
    g_sync.reset();
    g_bmca.reset();
    g_mock_port.reset();
}

class MessageFlowIntegrationTest {
public:
    
    // Helper to create test Announce message
    AnnounceMessage create_announce_message(
        std::uint8_t domain = 0,
        std::uint16_t sequence = 0
    ) {
        AnnounceMessage msg;
        msg.header = {};
        msg.header.setMessageType(Types::MessageType::ANNOUNCE);
        msg.header.setVersion(2);
        msg.header.domainNumber = domain;
        msg.header.sequenceId = sequence;
        msg.header.messageLength = Types::detail::host_to_be16(
            static_cast<uint16_t>(sizeof(AnnounceMessage))
        );
        
        msg.body = {};
        msg.body.currentUtcOffset = Types::detail::host_to_be16(37);
        msg.body.grandmasterPriority1 = 128;
        msg.body.grandmasterClockQuality.clockClass = 248;
        msg.body.grandmasterClockQuality.clockAccuracy = 0xFE;
        msg.body.grandmasterClockQuality.offsetScaledLogVariance = Types::detail::host_to_be16(0xFFFF);
        msg.body.grandmasterPriority2 = 128;
        msg.body.stepsRemoved = Types::detail::host_to_be16(0);
        msg.body.timeSource = 0xA0;
        
        return msg;
    }
    
    // Helper to create test Sync message
    SyncMessage create_sync_message(
        std::uint8_t domain = 0,
        std::uint16_t sequence = 0
    ) {
        SyncMessage msg;
        msg.header = {};
        msg.header.setMessageType(Types::MessageType::SYNC);
        msg.header.setVersion(2);
        msg.header.domainNumber = domain;
        msg.header.sequenceId = sequence;
        msg.header.messageLength = Types::detail::host_to_be16(
            static_cast<uint16_t>(sizeof(SyncMessage))
        );
        
        msg.body = {};
        msg.body.originTimestamp.secondsField = Types::detail::host_to_be48(1000);
        msg.body.originTimestamp.nanosecondsField = Types::detail::host_to_be32(500000000);
        
        return msg;
    }
    
    // Helper to create test Follow_Up message
    FollowUpMessage create_follow_up_message(
        std::uint8_t domain = 0,
        std::uint16_t sequence = 0
    ) {
        FollowUpMessage msg;
        msg.header = {};
        msg.header.setMessageType(Types::MessageType::FOLLOW_UP);
        msg.header.setVersion(2);
        msg.header.domainNumber = domain;
        msg.header.sequenceId = sequence;
        msg.header.messageLength = Types::detail::host_to_be16(
            static_cast<uint16_t>(sizeof(FollowUpMessage))
        );
        
        msg.body = {};
        msg.body.preciseOriginTimestamp.secondsField = Types::detail::host_to_be48(1000);
        msg.body.preciseOriginTimestamp.nanosecondsField = Types::detail::host_to_be32(500000000);
        
        return msg;
    }
    
    // Helper to create test Delay_Resp message
    DelayRespMessage create_delay_resp_message(
        std::uint8_t domain = 0,
        std::uint16_t sequence = 0
    ) {
        DelayRespMessage msg;
        msg.header = {};
        msg.header.setMessageType(Types::MessageType::DELAY_RESP);
        msg.header.setVersion(2);
        msg.header.domainNumber = domain;
        msg.header.sequenceId = sequence;
        msg.header.messageLength = Types::detail::host_to_be16(
            static_cast<uint16_t>(sizeof(DelayRespMessage))
        );
        
        msg.body = {};
        msg.body.receiveTimestamp.secondsField = Types::detail::host_to_be48(1000);
        msg.body.receiveTimestamp.nanosecondsField = Types::detail::host_to_be32(500100000);
        
        return msg;
    }
};

//==============================================================================
// Test 1: Message Flow Coordinator Lifecycle
//==============================================================================

void MessageFlowIntegrationTest_CoordinatorLifecycle() {
    // Initially not running
    EXPECT_FALSE(g_coordinator->is_running());
    
    // Configure
    MessageFlowConfiguration config = MessageFlowConfiguration::create_default();
    config.expected_domain = 0;
    config.strict_domain_checking = true;
    
    auto config_result = coordinator_->configure(config);
    EXPECT_EQ(config_result, Types::PTPError::Success);
    
    // Start
    auto start_result = coordinator_->start();
    EXPECT_EQ(start_result, Types::PTPError::Success);
    EXPECT_TRUE(coordinator_->is_running());
    
    // Start components
    bmca_->start();
    sync_->start();
    servo_->start();
    
    // Stop
    coordinator_->stop();
    EXPECT_FALSE(coordinator_->is_running());
    
    std::cout << "✅ Test 1 PASS: Coordinator lifecycle works correctly\n";
}

//==============================================================================
// Test 2: Announce Message Processing
//==============================================================================

TEST_F(MessageFlowIntegrationTest, AnnounceMessageProcessing) {
    // Start coordinator
    MessageFlowConfiguration config = MessageFlowConfiguration::create_default();
    config.enable_bmca_on_announce = true;
    coordinator_->configure(config);
    coordinator_->start();
    bmca_->start();
    
    // Process Announce message
    auto announce_msg = create_announce_message(0, 1);
    std::uint64_t reception_time = 1000000000ULL;  // 1 second
    
    auto result = coordinator_->process_announce_message(announce_msg, reception_time);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    // Check statistics
    auto stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.announce_received, 1u);
    EXPECT_EQ(stats.announce_processed, 1u);
    EXPECT_EQ(stats.bmca_triggered, 1u);
    EXPECT_EQ(stats.announce_errors, 0u);
    
    // Process second Announce
    announce_msg = create_announce_message(0, 2);
    reception_time = 2000000000ULL;  // 2 seconds
    
    result = coordinator_->process_announce_message(announce_msg, reception_time);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.announce_received, 2u);
    EXPECT_EQ(stats.announce_processed, 2u);
    
    std::cout << "✅ Test 2 PASS: Announce message processing works\n";
}

//==============================================================================
// Test 3: Sync Message Processing
//==============================================================================

TEST_F(MessageFlowIntegrationTest, SyncMessageProcessing) {
    // Start coordinator
    MessageFlowConfiguration config = MessageFlowConfiguration::create_default();
    config.enable_servo_on_sync = true;
    coordinator_->configure(config);
    coordinator_->start();
    sync_->start();
    servo_->start();
    
    // Process Sync message
    auto sync_msg = create_sync_message(0, 1);
    std::uint64_t reception_time = 1000500000000ULL;  // 1000.5 seconds
    
    auto result = coordinator_->process_sync_message(sync_msg, reception_time);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    // Check statistics
    auto stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.sync_received, 1u);
    EXPECT_EQ(stats.sync_processed, 1u);
    EXPECT_EQ(stats.servo_adjustments, 1u);
    EXPECT_EQ(stats.sync_errors, 0u);
    
    // Process second Sync
    sync_msg = create_sync_message(0, 2);
    reception_time = 1001500000000ULL;  // 1001.5 seconds
    
    result = coordinator_->process_sync_message(sync_msg, reception_time);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.sync_received, 2u);
    EXPECT_EQ(stats.sync_processed, 2u);
    
    std::cout << "✅ Test 3 PASS: Sync message processing works\n";
}

//==============================================================================
// Test 4: Follow_Up Message Processing
//==============================================================================

TEST_F(MessageFlowIntegrationTest, FollowUpMessageProcessing) {
    // Start coordinator
    coordinator_->configure(MessageFlowConfiguration::create_default());
    coordinator_->start();
    
    // Process Follow_Up message
    auto follow_up_msg = create_follow_up_message(0, 1);
    
    auto result = coordinator_->process_follow_up_message(follow_up_msg);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    // Check statistics
    auto stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.follow_up_received, 1u);
    
    std::cout << "✅ Test 4 PASS: Follow_Up message processing works\n";
}

//==============================================================================
// Test 5: Delay_Resp Message Processing
//==============================================================================

TEST_F(MessageFlowIntegrationTest, DelayRespMessageProcessing) {
    // Start coordinator
    coordinator_->configure(MessageFlowConfiguration::create_default());
    coordinator_->start();
    
    // Process Delay_Resp message
    auto delay_resp_msg = create_delay_resp_message(0, 1);
    
    auto result = coordinator_->process_delay_resp_message(delay_resp_msg);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    // Check statistics
    auto stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.delay_resp_received, 1u);
    
    std::cout << "✅ Test 5 PASS: Delay_Resp message processing works\n";
}

//==============================================================================
// Test 6: Domain Filtering
//==============================================================================

TEST_F(MessageFlowIntegrationTest, DomainFiltering) {
    // Start coordinator with strict domain checking
    MessageFlowConfiguration config = MessageFlowConfiguration::create_default();
    config.expected_domain = 0;
    config.strict_domain_checking = true;
    coordinator_->configure(config);
    coordinator_->start();
    
    // Process message with correct domain
    auto announce_msg = create_announce_message(0, 1);
    std::uint64_t reception_time = 1000000000ULL;
    
    auto result = coordinator_->process_announce_message(announce_msg, reception_time);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    // Process message with wrong domain
    announce_msg = create_announce_message(1, 2);  // Domain 1 instead of 0
    
    result = coordinator_->process_announce_message(announce_msg, reception_time);
    EXPECT_EQ(result, Types::PTPError::WRONG_DOMAIN);
    
    // Check statistics
    auto stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.announce_received, 2u);
    EXPECT_EQ(stats.announce_processed, 1u);
    EXPECT_EQ(stats.domain_mismatches, 1u);
    
    std::cout << "✅ Test 6 PASS: Domain filtering works correctly\n";
}

//==============================================================================
// Test 7: Message Validation
//==============================================================================

TEST_F(MessageFlowIntegrationTest, MessageValidation) {
    // Start coordinator
    coordinator_->configure(MessageFlowConfiguration::create_default());
    coordinator_->start();
    
    // Create message with invalid version
    auto announce_msg = create_announce_message(0, 1);
    announce_msg.header.versionPTP = 1;  // Version 1 instead of 2
    
    auto result = coordinator_->process_announce_message(announce_msg, 1000000000ULL);
    EXPECT_EQ(result, Types::PTPError::INVALID_VERSION);
    
    // Check statistics
    auto stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.invalid_messages, 1u);
    EXPECT_EQ(stats.announce_errors, 1u);
    
    std::cout << "✅ Test 7 PASS: Message validation works correctly\n";
}

//==============================================================================
// Test 8: Health Status Monitoring
//==============================================================================

TEST_F(MessageFlowIntegrationTest, HealthStatusMonitoring) {
    // Start all components
    MessageFlowConfiguration config = MessageFlowConfiguration::create_default();
    coordinator_->configure(config);
    coordinator_->start();
    bmca_->start();
    sync_->start();
    servo_->start();
    
    // Initially, no message flows active
    auto health = coordinator_->get_health_status();
    EXPECT_FALSE(health.announce_flow_active);
    EXPECT_FALSE(health.sync_flow_active);
    
    // Process some messages
    auto announce_msg = create_announce_message(0, 1);
    coordinator_->process_announce_message(announce_msg, 1000000000ULL);
    
    auto sync_msg = create_sync_message(0, 1);
    coordinator_->process_sync_message(sync_msg, 1000500000000ULL);
    
    // Now message flows should be active
    health = coordinator_->get_health_status();
    EXPECT_TRUE(health.announce_flow_active);
    EXPECT_TRUE(health.sync_flow_active);
    EXPECT_TRUE(health.bmca_healthy);
    EXPECT_TRUE(health.sync_healthy);
    EXPECT_TRUE(health.servo_healthy);
    
    std::cout << "✅ Test 8 PASS: Health status monitoring works\n";
}

//==============================================================================
// Test 9: Statistics Tracking
//==============================================================================

TEST_F(MessageFlowIntegrationTest, StatisticsTracking) {
    // Start coordinator
    coordinator_->configure(MessageFlowConfiguration::create_default());
    coordinator_->start();
    
    // Process multiple messages
    for (int i = 1; i <= 5; i++) {
        auto announce_msg = create_announce_message(0, static_cast<std::uint16_t>(i));
        std::uint64_t time = 1000000000ULL * i;
        coordinator_->process_announce_message(announce_msg, time);
    }
    
    for (int i = 1; i <= 3; i++) {
        auto sync_msg = create_sync_message(0, static_cast<std::uint16_t>(i));
        std::uint64_t time = 1000500000000ULL * i;
        coordinator_->process_sync_message(sync_msg, time);
    }
    
    // Check statistics
    auto stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.announce_received, 5u);
    EXPECT_EQ(stats.announce_processed, 5u);
    EXPECT_EQ(stats.sync_received, 3u);
    EXPECT_EQ(stats.sync_processed, 3u);
    
    // Reset statistics
    coordinator_->reset();
    stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.announce_received, 0u);
    EXPECT_EQ(stats.sync_received, 0u);
    
    std::cout << "✅ Test 9 PASS: Statistics tracking works correctly\n";
}

//==============================================================================
// Test 10: Component Integration
//==============================================================================

TEST_F(MessageFlowIntegrationTest, ComponentIntegration) {
    // Start all components
    MessageFlowConfiguration config = MessageFlowConfiguration::create_default();
    config.enable_bmca_on_announce = true;
    config.enable_servo_on_sync = true;
    coordinator_->configure(config);
    coordinator_->start();
    
    bmca_->start();
    sync_->start();
    servo_->start();
    
    // Process Announce → BMCA
    auto announce_msg = create_announce_message(0, 1);
    auto result = coordinator_->process_announce_message(announce_msg, 1000000000ULL);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    auto stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.bmca_triggered, 1u);
    
    // Process Sync → Servo
    auto sync_msg = create_sync_message(0, 1);
    result = coordinator_->process_sync_message(sync_msg, 1000500000000ULL);
    EXPECT_EQ(result, Types::PTPError::Success);
    
    stats = coordinator_->get_statistics();
    EXPECT_EQ(stats.servo_adjustments, 1u);
    
    // Verify all components operational
    auto health = coordinator_->get_health_status();
    EXPECT_TRUE(health.bmca_operational);
    EXPECT_TRUE(health.servo_operational);
    EXPECT_TRUE(health.bmca_healthy);
    EXPECT_TRUE(health.sync_healthy);
    EXPECT_TRUE(health.servo_healthy);
    
    std::cout << "✅ Test 10 PASS: Component integration works correctly\n";
}

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    int passed = 0;
    int failed = 0;
    
    // Run each test
    try {
        MessageFlowIntegrationTest test;
        
        test.SetUp();
        MessageFlowIntegrationTest_CoordinatorLifecycle();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_AnnounceMessageProcessing();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_SyncMessageProcessing();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_FollowUpMessageProcessing();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_DelayRespMessageProcessing();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_DomainFiltering();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_MessageValidation();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_HealthStatusMonitoring();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_StatisticsTracking();
        test.TearDown();
        passed++;
        
        test.SetUp();
        MessageFlowIntegrationTest_ComponentIntegration();
        test.TearDown();
        passed++;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Test exception: " << e.what() << "\n";
        failed++;
    } catch (...) {
        std::cout << "❌ Unknown test exception\n";
        failed++;
    }
    
    std::cout << "\n";
    std::cout << "Tests passed: " << passed << "\n";
    std::cout << "Tests failed: " << failed << "\n";
    
    if (failed == 0) {
        std::cout << "\n✅ All Message Flow Integration tests PASSED\n";
        return 0;
    } else {
        std::cout << "\n❌ Some Message Flow Integration tests FAILED\n";
        return 1;
    }
}
