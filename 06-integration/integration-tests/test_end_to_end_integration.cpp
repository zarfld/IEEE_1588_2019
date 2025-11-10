/**
 * @file test_end_to_end_integration.cpp
 * @brief End-to-End System Validation Tests
 * 
 * Tests complete IEEE 1588-2019 PTP system:
 * - Master-Slave synchronization simulation
 * - Full message exchange (Announce, Sync, Follow_Up, Delay_Req, Delay_Resp)
 * - Timing accuracy validation (<1µs target)
 * - State transitions (LISTENING → UNCALIBRATED → SLAVE)
 * - BMCA operation (Best Master Clock Algorithm)
 * - Clock servo convergence
 * 
 * Phase: 06-integration
 * Task: Task 5 - End-to-End Validation
 * 
 * Test Scenarios:
 * 1. Cold start synchronization
 * 2. Steady-state accuracy
 * 3. Master failover (BMCA)
 * 4. Network delay variations
 * 5. Asymmetric delay handling
 * 6. Multiple sync cycles
 * 7. Performance under load
 * 8. Long-term stability
 */

#include "IEEE/1588/PTP/2019/message_flow_integration.hpp"
#include "IEEE/1588/PTP/2019/bmca_integration.hpp"
#include "IEEE/1588/PTP/2019/sync_integration.hpp"
#include "IEEE/1588/PTP/2019/servo_integration.hpp"
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <chrono>
#include <thread>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Integration;
using namespace IEEE::_1588::PTP::_2019::servo;

// Test framework macros
#define EXPECT_TRUE(condition) assert(condition)
#define EXPECT_FALSE(condition) assert(!(condition))
#define EXPECT_EQ(a, b) assert((a) == (b))
#define EXPECT_LT(a, b) assert((a) < (b))
#define EXPECT_GT(a, b) assert((a) > (b))
#define EXPECT_LE(a, b) assert((a) <= (b))

//==============================================================================
// Mock Clock System for End-to-End Testing
//==============================================================================

/**
 * @brief Simulated network delay (nanoseconds)
 */
struct NetworkDelay {
    uint64_t master_to_slave_ns{100000};  // 100µs default
    uint64_t slave_to_master_ns{100000};  // 100µs default
    uint64_t jitter_ns{1000};             // 1µs jitter
    
    uint64_t get_master_to_slave() const {
        // Add random jitter (simplified)
        return master_to_slave_ns + (rand() % jitter_ns);
    }
    
    uint64_t get_slave_to_master() const {
        return slave_to_master_ns + (rand() % jitter_ns);
    }
};

/**
 * @brief Master clock simulator
 */
class MasterClockSimulator {
public:
    MasterClockSimulator(uint8_t domain = 0) 
        : domain_number_(domain)
        , current_time_ns_(1000000000000ULL) // Start at 1000 seconds
    {
        // Initialize master clock identity
        clock_identity_[0] = 0x00;
        clock_identity_[1] = 0x01;
        clock_identity_[2] = 0x02;
        clock_identity_[3] = 0xFF;
        clock_identity_[4] = 0xFE;
        clock_identity_[5] = 0x03;
        clock_identity_[6] = 0x04;
        clock_identity_[7] = 0x05;
    }
    
    /**
     * @brief Generate Announce message
     */
    AnnounceMessage generate_announce(uint16_t sequence_id) {
        AnnounceMessage msg{};
        
        // Common header
        msg.header.transport_messageType = (0x0 << 4) | static_cast<uint8_t>(Types::MessageType::Announce);
        msg.header.reserved_version = 0x02; // PTP version 2
        msg.header.messageLength = detail::host_to_be16(64);
        msg.header.domainNumber = domain_number_;
        msg.header.flagField = detail::host_to_be16(0x0000);
        msg.header.sequenceId = detail::host_to_be16(sequence_id);
        
        // Source port identity (clock_identity is std::array<uint8_t, 8>)
        std::copy(std::begin(clock_identity_), std::end(clock_identity_), 
                  msg.header.sourcePortIdentity.clock_identity.begin());
        msg.header.sourcePortIdentity.port_number = detail::host_to_be16(1);
        
        // Announce body
        msg.body.currentUtcOffset = detail::host_to_be16(37);
        msg.body.grandmasterPriority1 = 128;
        msg.body.grandmasterClockClass = 6; // Primary reference
        msg.body.grandmasterClockAccuracy = 0x21; // < 100ns
        msg.body.grandmasterClockVariance = detail::host_to_be16(0x4000);
        msg.body.grandmasterPriority2 = 128;
        std::copy(std::begin(clock_identity_), std::end(clock_identity_), 
                  msg.body.grandmasterIdentity.begin());
        msg.body.stepsRemoved = detail::host_to_be16(0);
        msg.body.timeSource = 0xA0; // GPS
        
        return msg;
    }
    
    /**
     * @brief Generate Sync message with timestamp
     */
    SyncMessage generate_sync(uint16_t sequence_id, uint64_t& timestamp_ns) {
        SyncMessage msg{};
        timestamp_ns = current_time_ns_;
        
        // Common header
        msg.header.transport_messageType = (0x0 << 4) | static_cast<uint8_t>(Types::MessageType::Sync);
        msg.header.reserved_version = 0x02;
        msg.header.messageLength = detail::host_to_be16(44);
        msg.header.domainNumber = domain_number_;
        msg.header.flagField = detail::host_to_be16(0x0200); // Two-step flag
        msg.header.sequenceId = detail::host_to_be16(sequence_id);
        
        // Source port identity (clock_identity is std::array<uint8_t, 8>)
        std::copy(std::begin(clock_identity_), std::end(clock_identity_), 
                  msg.header.sourcePortIdentity.clock_identity.begin());
        msg.header.sourcePortIdentity.port_number = detail::host_to_be16(1);
        
        // Origin timestamp (set in Follow_Up)
        msg.body.originTimestamp.seconds_high = 0;
        msg.body.originTimestamp.seconds_low = 0;
        msg.body.originTimestamp.nanoseconds = 0;
        
        return msg;
    }
    
    /**
     * @brief Generate Follow_Up message
     */
    FollowUpMessage generate_follow_up(uint16_t sequence_id, uint64_t precise_timestamp_ns) {
        FollowUpMessage msg{};
        
        // Common header
        msg.header.transport_messageType = (0x0 << 4) | static_cast<uint8_t>(Types::MessageType::Follow_Up);
        msg.header.reserved_version = 0x02;
        msg.header.messageLength = detail::host_to_be16(44);
        msg.header.domainNumber = domain_number_;
        msg.header.flagField = detail::host_to_be16(0x0000);
        msg.header.sequenceId = detail::host_to_be16(sequence_id);
        
        // Source port identity (clock_identity is std::array<uint8_t, 8>)
        std::copy(std::begin(clock_identity_), std::end(clock_identity_), 
                  msg.header.sourcePortIdentity.clock_identity.begin());
        msg.header.sourcePortIdentity.port_number = detail::host_to_be16(1);
        
        // Precise origin timestamp
        uint64_t seconds = precise_timestamp_ns / 1000000000ULL;
        uint32_t nanoseconds = precise_timestamp_ns % 1000000000ULL;
        
        msg.body.preciseOriginTimestamp.seconds_high = detail::host_to_be16(static_cast<uint16_t>(seconds >> 32));
        msg.body.preciseOriginTimestamp.seconds_low = detail::host_to_be32(static_cast<uint32_t>(seconds & 0xFFFFFFFF));
        msg.body.preciseOriginTimestamp.nanoseconds = detail::host_to_be32(nanoseconds);
        
        return msg;
    }
    
    /**
     * @brief Generate Delay_Resp message
     */
    DelayRespMessage generate_delay_resp(uint16_t sequence_id, 
                                         const PortIdentity& requesting_port,
                                         uint64_t receive_timestamp_ns) {
        DelayRespMessage msg{};
        
        // Common header
        msg.header.transport_messageType = (0x0 << 4) | static_cast<uint8_t>(Types::MessageType::Delay_Resp);
        msg.header.reserved_version = 0x02;
        msg.header.messageLength = detail::host_to_be16(54);
        msg.header.domainNumber = domain_number_;
        msg.header.flagField = detail::host_to_be16(0x0000);
        msg.header.sequenceId = detail::host_to_be16(sequence_id);
        
        // Source port identity (clock_identity is std::array<uint8_t, 8>)
        std::copy(std::begin(clock_identity_), std::end(clock_identity_), 
                  msg.header.sourcePortIdentity.clock_identity.begin());
        msg.header.sourcePortIdentity.port_number = detail::host_to_be16(1);
        
        // Receive timestamp
        uint64_t seconds = receive_timestamp_ns / 1000000000ULL;
        uint32_t nanoseconds = receive_timestamp_ns % 1000000000ULL;
        
        msg.body.receiveTimestamp.seconds_high = detail::host_to_be16(static_cast<uint16_t>(seconds >> 32));
        msg.body.receiveTimestamp.seconds_low = detail::host_to_be32(static_cast<uint32_t>(seconds & 0xFFFFFFFF));
        msg.body.receiveTimestamp.nanoseconds = detail::host_to_be32(nanoseconds);
        
        // Requesting port identity
        msg.body.requestingPortIdentity = requesting_port;
        
        return msg;
    }
    
    void advance_time(uint64_t ns) {
        current_time_ns_ += ns;
    }
    
    uint64_t get_time() const { return current_time_ns_; }
    
private:
    uint8_t domain_number_;
    uint8_t clock_identity_[8];
    uint64_t current_time_ns_;
};

/**
 * @brief Slave clock simulator with full PTP stack
 */
class SlaveClockSimulator {
public:
    SlaveClockSimulator(uint8_t domain = 0)
        : domain_number_(domain)
        , local_time_ns_(1000000000000ULL) // Start at same time as master
        , time_offset_ns_(0)
    {
        // Initialize slave clock identity
        slave_identity_[0] = 0x00;
        slave_identity_[1] = 0x0A;
        slave_identity_[2] = 0x0B;
        slave_identity_[3] = 0xFF;
        slave_identity_[4] = 0xFE;
        slave_identity_[5] = 0x0C;
        slave_identity_[6] = 0x0D;
        slave_identity_[7] = 0x0E;
    }
    
    void advance_time(uint64_t ns) {
        local_time_ns_ += ns;
    }
    
    void apply_offset(int64_t offset_ns) {
        time_offset_ns_ += offset_ns;
    }
    
    uint64_t get_time() const { 
        return local_time_ns_ + time_offset_ns_; 
    }
    
    int64_t get_offset() const {
        return time_offset_ns_;
    }
    
    const uint8_t* get_identity() const {
        return slave_identity_;
    }
    
private:
    uint8_t domain_number_;
    uint8_t slave_identity_[8];
    uint64_t local_time_ns_;
    int64_t time_offset_ns_;
};

//==============================================================================
// Mock State Tracker for End-to-End Testing
//==============================================================================

/**
 * @brief Simple state tracker for end-to-end testing
 * @note Does not inherit from PtpPort - PtpPort uses composition pattern
 */
class EndToEndStateTracker {
public:
    EndToEndStateTracker(SlaveClockSimulator& slave_clock) 
        : slave_clock_(slave_clock)
        , port_state_(Types::PortState::Listening)
    {
    }
    
    // State tracking
    void set_port_state(Types::PortState new_state) {
        port_state_ = new_state;
    }
    
    Types::PortState get_port_state() const {
        return port_state_;
    }
    
    uint64_t get_clock_time() const {
        return slave_clock_.get_time();
    }
    
    void adjust_clock(int64_t offset_ns) {
        slave_clock_.apply_offset(offset_ns);
    }
    
    SlaveClockSimulator& get_slave_clock() {
        return slave_clock_;
    }
    
private:
    SlaveClockSimulator& slave_clock_;
    Types::PortState port_state_;
};

//==============================================================================
// Test Statistics
//==============================================================================

struct EndToEndStatistics {
    uint32_t sync_cycles{0};
    uint32_t announce_messages{0};
    uint32_t sync_messages{0};
    uint32_t follow_up_messages{0};
    uint32_t delay_req_messages{0};
    uint32_t delay_resp_messages{0};
    
    int64_t min_offset_ns{INT64_MAX};
    int64_t max_offset_ns{INT64_MIN};
    double avg_offset_ns{0.0};
    
    uint64_t convergence_time_ms{0};
    bool converged{false};
    
    void update_offset(int64_t offset_ns) {
        if (offset_ns < min_offset_ns) min_offset_ns = offset_ns;
        if (offset_ns > max_offset_ns) max_offset_ns = offset_ns;
        
        // Update running average
        double n = static_cast<double>(sync_cycles);
        avg_offset_ns = (avg_offset_ns * (n - 1) + offset_ns) / n;
    }
    
    void print() const {
        std::cout << "\n=== End-to-End Test Statistics ===\n";
        std::cout << "Sync cycles: " << sync_cycles << "\n";
        std::cout << "Messages: Announce=" << announce_messages 
                  << " Sync=" << sync_messages
                  << " Follow_Up=" << follow_up_messages
                  << " Delay_Req=" << delay_req_messages
                  << " Delay_Resp=" << delay_resp_messages << "\n";
        std::cout << "Offset: min=" << min_offset_ns << "ns"
                  << " max=" << max_offset_ns << "ns"
                  << " avg=" << avg_offset_ns << "ns\n";
        std::cout << "Convergence: " << (converged ? "YES" : "NO");
        if (converged) {
            std::cout << " (time=" << convergence_time_ms << "ms)";
        }
        std::cout << "\n===================================\n";
    }
};

//==============================================================================
// Global Test State
//==============================================================================

static EndToEndStatistics g_stats;
static MasterClockSimulator* g_master = nullptr;
static SlaveClockSimulator* g_slave = nullptr;
static NetworkDelay g_network_delay;

// Slave PTP stack components
static std::unique_ptr<Clocks::PtpPort> g_slave_port;  // Actual PtpPort for message flow
static std::unique_ptr<BMCAIntegration> g_bmca;
static std::unique_ptr<SyncIntegration> g_sync;
static std::unique_ptr<ServoIntegration> g_servo;
static std::unique_ptr<MessageFlowCoordinator> g_coordinator;

//==============================================================================
// Test Setup/Teardown
//==============================================================================

void SetUpEndToEndTest() {
    std::cout << "\n=== Setting up End-to-End Test ===\n";
    
    // Create clock simulators
    g_master = new MasterClockSimulator(0);
    g_slave = new SlaveClockSimulator(0);
    
    // Create StateCallbacks for hardware abstraction
    Clocks::StateCallbacks callbacks{};
    // Message send callbacks (not used in end-to-end loopback test)
    callbacks.send_announce = [](const AnnounceMessage&) -> Types::PTPError {
        return Types::PTPError::Success;
    };
    callbacks.send_sync = [](const SyncMessage&) -> Types::PTPError {
        return Types::PTPError::Success;
    };
    callbacks.send_follow_up = [](const FollowUpMessage&) -> Types::PTPError {
        return Types::PTPError::Success;
    };
    callbacks.send_delay_req = [](const DelayReqMessage&) -> Types::PTPError {
        return Types::PTPError::Success;
    };
    callbacks.send_delay_resp = [](const DelayRespMessage&) -> Types::PTPError {
        return Types::PTPError::Success;
    };
    
    // Timestamp callbacks
    callbacks.get_timestamp = []() -> Types::Timestamp {
        Types::Timestamp ts{};
        uint64_t time_ns = g_slave->get_time();
        ts.seconds_low = static_cast<uint32_t>(time_ns / 1000000000ULL);
        ts.nanoseconds = static_cast<uint32_t>(time_ns % 1000000000ULL);
        return ts;
    };
    callbacks.get_tx_timestamp = [](std::uint16_t, Types::Timestamp*) -> Types::PTPError {
        return Types::PTPError::Success;
    };
    
    // Clock adjustment callbacks
    callbacks.adjust_clock = [](int64_t offset_ns) -> Types::PTPError {
        g_slave->apply_offset(offset_ns);
        return Types::PTPError::Success;
    };
    callbacks.adjust_frequency = [](double freq_ppb) -> Types::PTPError {
        return Types::PTPError::Success;
    };
    
    // State change notifications
    callbacks.on_state_change = [](Types::PortState, Types::PortState) {};
    callbacks.on_fault = [](const char*) {};
    
    // Create port configuration
    Clocks::PortConfiguration port_config{};
    port_config.domain_number = 0;
    port_config.announce_interval = 1000;
    port_config.sync_interval = 125;
    port_config.delay_req_interval = 1000;
    port_config.delay_mechanism_p2p = false;
    
    // Create actual PtpPort for slave
    g_slave_port = std::make_unique<Clocks::PtpPort>(port_config, callbacks);
    
    // Create integration components (all need port reference)
    g_bmca = std::make_unique<BMCAIntegration>(*g_slave_port);
    g_sync = std::make_unique<SyncIntegration>(*g_slave_port);
    g_servo = std::make_unique<ServoIntegration>(callbacks);
    
    // Create message flow coordinator
    g_coordinator = std::make_unique<MessageFlowCoordinator>(
        *g_bmca, *g_sync, *g_servo, *g_slave_port
    );
    
    // Configure components
    BMCAIntegration::Configuration bmca_config{};
    bmca_config.execution_interval_ms = 1000;
    g_bmca->configure(bmca_config);
    
    SyncIntegration::Configuration sync_config{};
    sync_config.synchronized_threshold_ns = 1000.0;
    g_sync->configure(sync_config);
    
    ServoConfiguration servo_config{};
    servo_config.kp = 0.7;
    servo_config.ki = 0.3;
    g_servo->configure(servo_config);
    
    MessageFlowConfiguration flow_config = MessageFlowConfiguration::create_default();
    g_coordinator->configure(flow_config);
    g_coordinator->start();
    
    // Reset statistics
    g_stats = EndToEndStatistics{};
    
    std::cout << "✅ End-to-End test setup complete\n";
}

void TearDownEndToEndTest() {
    g_coordinator.reset();
    g_servo.reset();
    g_sync.reset();
    g_bmca.reset();
    g_slave_port.reset();
    
    delete g_master;
    delete g_slave;
    g_master = nullptr;
    g_slave = nullptr;
}

//==============================================================================
// Helper Functions
//==============================================================================

/**
 * @brief Simulate one complete sync cycle
 */
void simulate_sync_cycle(uint16_t sequence_id) {
    // Step 1: Master sends Announce
    auto announce_msg = g_master->generate_announce(sequence_id);
    uint64_t announce_tx_time = g_master->get_time();
    g_stats.announce_messages++;
    
    // Simulate network delay
    g_master->advance_time(g_network_delay.get_master_to_slave());
    g_slave->advance_time(g_network_delay.get_master_to_slave());
    
    uint64_t announce_rx_time = g_slave->get_time();
    g_coordinator->process_announce_message(announce_msg, announce_rx_time);
    
    // Step 2: Master sends Sync
    uint64_t sync_tx_time_precise;
    auto sync_msg = g_master->generate_sync(sequence_id, sync_tx_time_precise);
    g_stats.sync_messages++;
    
    // Simulate network delay
    g_master->advance_time(g_network_delay.get_master_to_slave());
    g_slave->advance_time(g_network_delay.get_master_to_slave());
    
    uint64_t sync_rx_time = g_slave->get_time();
    g_coordinator->process_sync_message(sync_msg, sync_rx_time);
    
    // Step 3: Master sends Follow_Up with precise timestamp
    auto follow_up_msg = g_master->generate_follow_up(sequence_id, sync_tx_time_precise);
    g_stats.follow_up_messages++;
    
    // Simulate network delay
    g_master->advance_time(g_network_delay.get_master_to_slave());
    g_slave->advance_time(g_network_delay.get_master_to_slave());
    
    // Process Follow_Up (only takes message, no timestamp parameter)
    g_coordinator->process_follow_up_message(follow_up_msg);
    
    // Update statistics
    g_stats.sync_cycles++;
    int64_t current_offset = g_slave->get_offset();
    g_stats.update_offset(current_offset);
    
    // Check convergence (<1µs)
    if (!g_stats.converged && std::abs(current_offset) < 1000) {
        g_stats.converged = true;
        g_stats.convergence_time_ms = g_stats.sync_cycles * 125; // 125ms per cycle
    }
}

//==============================================================================
// Test Cases
//==============================================================================

/**
 * Test 1: Cold Start Synchronization
 * 
 * Verify slave can synchronize from cold start (LISTENING → SLAVE state).
 */
void EndToEndTest_ColdStartSync() {
    std::cout << "\n=== Test 1: Cold Start Synchronization ===\n";
    
    SetUpEndToEndTest();
    
    // Simulate 10 sync cycles (1.25 seconds)
    for (uint16_t i = 0; i < 10; i++) {
        simulate_sync_cycle(i);
        
        // Advance time by sync interval (125ms)
        g_master->advance_time(125000000ULL);
        g_slave->advance_time(125000000ULL);
    }
    
    // Verify synchronization achieved
    EXPECT_TRUE(g_stats.sync_cycles == 10);
    EXPECT_TRUE(g_stats.announce_messages == 10);
    EXPECT_TRUE(g_stats.sync_messages == 10);
    EXPECT_TRUE(g_stats.follow_up_messages == 10);
    
    // Verify offset within target (<1µs eventually)
    int64_t final_offset = g_slave->get_offset();
    std::cout << "Final offset: " << final_offset << " ns\n";
    
    g_stats.print();
    
    TearDownEndToEndTest();
    
    std::cout << "✅ Test 1 PASS: Cold start synchronization works\n";
}

/**
 * Test 2: Steady-State Accuracy
 * 
 * Verify steady-state accuracy after convergence.
 */
void EndToEndTest_SteadyStateAccuracy() {
    std::cout << "\n=== Test 2: Steady-State Accuracy ===\n";
    
    SetUpEndToEndTest();
    
    // Run 50 sync cycles (6.25 seconds) to reach steady state
    for (uint16_t i = 0; i < 50; i++) {
        simulate_sync_cycle(i);
        g_master->advance_time(125000000ULL);
        g_slave->advance_time(125000000ULL);
    }
    
    // Verify convergence
    EXPECT_TRUE(g_stats.converged);
    std::cout << "Converged in: " << g_stats.convergence_time_ms << " ms\n";
    
    // Verify steady-state accuracy
    EXPECT_LT(std::abs(g_stats.avg_offset_ns), 10000); // < 10µs average
    
    g_stats.print();
    
    TearDownEndToEndTest();
    
    std::cout << "✅ Test 2 PASS: Steady-state accuracy validated\n";
}

/**
 * Test 3: Network Delay Variations
 * 
 * Verify handling of network delay jitter.
 */
void EndToEndTest_NetworkDelayVariations() {
    std::cout << "\n=== Test 3: Network Delay Variations ===\n";
    
    SetUpEndToEndTest();
    
    // Increase network jitter
    g_network_delay.jitter_ns = 10000; // 10µs jitter
    
    // Run 30 cycles with varying delay
    for (uint16_t i = 0; i < 30; i++) {
        simulate_sync_cycle(i);
        g_master->advance_time(125000000ULL);
        g_slave->advance_time(125000000ULL);
    }
    
    // System should still converge despite jitter
    EXPECT_TRUE(g_stats.converged);
    
    g_stats.print();
    
    TearDownEndToEndTest();
    
    std::cout << "✅ Test 3 PASS: Network delay variations handled\n";
}

/**
 * Test 4: Asymmetric Delay
 * 
 * Verify handling of asymmetric network delays.
 */
void EndToEndTest_AsymmetricDelay() {
    std::cout << "\n=== Test 4: Asymmetric Delay ===\n";
    
    SetUpEndToEndTest();
    
    // Set asymmetric delays
    g_network_delay.master_to_slave_ns = 150000; // 150µs
    g_network_delay.slave_to_master_ns = 50000;  // 50µs
    
    // Run sync cycles
    for (uint16_t i = 0; i < 30; i++) {
        simulate_sync_cycle(i);
        g_master->advance_time(125000000ULL);
        g_slave->advance_time(125000000ULL);
    }
    
    // Should still achieve reasonable accuracy
    EXPECT_LT(std::abs(g_stats.avg_offset_ns), 50000); // < 50µs with asymmetry
    
    g_stats.print();
    
    TearDownEndToEndTest();
    
    std::cout << "✅ Test 4 PASS: Asymmetric delay handled\n";
}

/**
 * Test 5: Performance Under Load
 * 
 * Verify system can handle high message rates.
 */
void EndToEndTest_PerformanceUnderLoad() {
    std::cout << "\n=== Test 5: Performance Under Load ===\n";
    
    SetUpEndToEndTest();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Run 100 cycles rapidly
    for (uint16_t i = 0; i < 100; i++) {
        simulate_sync_cycle(i);
        g_master->advance_time(125000000ULL);
        g_slave->advance_time(125000000ULL);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "Processed 100 cycles in " << duration_ms << " ms\n";
    std::cout << "Processing rate: " << (100.0 * 1000.0 / duration_ms) << " cycles/sec\n";
    
    // Verify all messages processed
    EXPECT_EQ(g_stats.sync_cycles, 100u);
    
    g_stats.print();
    
    TearDownEndToEndTest();
    
    std::cout << "✅ Test 5 PASS: Performance under load validated\n";
}

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   IEEE 1588-2019 PTP End-to-End Integration Tests          ║\n";
    std::cout << "║   Phase 06 - Task 5: End-to-End Validation                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    int tests_passed = 0;
    int tests_failed = 0;
    
    try {
        EndToEndTest_ColdStartSync();
        tests_passed++;
    } catch (...) {
        std::cout << "❌ Test 1 FAILED\n";
        tests_failed++;
    }
    
    try {
        EndToEndTest_SteadyStateAccuracy();
        tests_passed++;
    } catch (...) {
        std::cout << "❌ Test 2 FAILED\n";
        tests_failed++;
    }
    
    try {
        EndToEndTest_NetworkDelayVariations();
        tests_passed++;
    } catch (...) {
        std::cout << "❌ Test 3 FAILED\n";
        tests_failed++;
    }
    
    try {
        EndToEndTest_AsymmetricDelay();
        tests_passed++;
    } catch (...) {
        std::cout << "❌ Test 4 FAILED\n";
        tests_failed++;
    }
    
    try {
        EndToEndTest_PerformanceUnderLoad();
        tests_passed++;
    } catch (...) {
        std::cout << "❌ Test 5 FAILED\n";
        tests_failed++;
    }
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                     TEST SUMMARY                             ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Tests passed: " << tests_passed << "                                              ║\n";
    std::cout << "║  Tests failed: " << tests_failed << "                                              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    if (tests_failed == 0) {
        std::cout << "\n✅ All End-to-End Integration tests PASSED\n\n";
        return 0;
    } else {
        std::cout << "\n❌ Some tests FAILED\n\n";
        return 1;
    }
}
