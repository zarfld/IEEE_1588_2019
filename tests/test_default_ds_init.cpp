/*
 * Test: TEST-UNIT-DefaultDS-Init
 * Phase: 05-implementation
 * Traceability:
 *     Design: DES-D-004-DefaultDataset
 *     Requirements: REQ-F-1588-002-BMCA, REQ-F-205
 *     Specification: IEEE 1588-2019 Section 8.2.1
 * Purpose: Verify DefaultDataSet initialization and accessor methods
 * 
 * Test validates:
 * 1. DefaultDataSet structure is initialized correctly per IEEE 1588-2019 Table 8
 * 2. All fields have correct default values (twoStepFlag, clockIdentity, numberPorts, 
 *    clockQuality, priority1, priority2, domainNumber, slaveOnly)
 * 3. Accessor method get_default_data_set() returns correct values
 * 4. DefaultDataSet is accessible through PtpPort interface
 * 
 * IEEE 1588-2019 Compliance:
 * - Section 8.2.1: defaultDS data set member specifications
 * - Table 8: defaultDS data set members
 * - Default values per specification (priority1=128, priority2=128, etc.)
 * 
 * @note This test ensures BMCA has access to required clock quality and priority data
 */

#include "clocks.hpp"
#include <cstdint>
#include <iostream>
#include <iomanip>

using namespace IEEE::_1588::PTP::_2019;

// Simple test result counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            std::cout << "✅ PASS: " << message << std::endl; \
        } else { \
            tests_failed++; \
            std::cerr << "❌ FAIL: " << message << std::endl; \
        } \
    } while(0)

/**
 * @brief Test DefaultDataSet initialization in PtpPort constructor
 * @design DES-D-004-DefaultDataset
 * @traces REQ-F-205 (Dataset/MIB-Based Management)
 */
void test_default_ds_initialization() {
    std::cout << "\n=== Test: DefaultDataSet Initialization ===" << std::endl;
    
    // Create PtpPort configuration
    Clocks::PortConfiguration config;
    config.port_number = 1;
    config.domain_number = 42;  // Non-default domain for testing
    config.announce_interval = 1;
    config.sync_interval = 0;
    config.delay_req_interval = 0;
    
    // Create minimal callbacks (not used in this test)
    Clocks::StateCallbacks callbacks{};
    callbacks.send_announce = nullptr;
    callbacks.send_sync = nullptr;
    callbacks.send_follow_up = nullptr;
    callbacks.send_delay_req = nullptr;
    callbacks.send_delay_resp = nullptr;
    callbacks.get_timestamp = nullptr;
    callbacks.get_tx_timestamp = nullptr;
    callbacks.adjust_clock = nullptr;
    callbacks.adjust_frequency = nullptr;
    callbacks.on_state_change = nullptr;
    callbacks.on_fault = nullptr;
    
    // Create PtpPort instance
    Clocks::PtpPort port(config, callbacks);
    
    // Get DefaultDataSet via accessor
    const auto& default_ds = port.get_default_data_set();
    
    // Test 1: Verify twoStepFlag is TRUE (two-step clock by default)
    TEST_ASSERT(default_ds.twoStepFlag == true,
                "twoStepFlag should be TRUE (two-step clock)");
    
    // Test 2: Verify clockIdentity is non-zero (initialized)
    bool clock_identity_non_zero = false;
    for (auto byte : default_ds.clockIdentity) {
        if (byte != 0) {
            clock_identity_non_zero = true;
            break;
        }
    }
    TEST_ASSERT(clock_identity_non_zero,
                "clockIdentity should be non-zero (initialized)");
    
    // Test 3: Verify numberPorts default value (1 port)
    TEST_ASSERT(default_ds.numberPorts == 1,
                "numberPorts should be 1 (single port default)");
    
    // Test 4: Verify clockQuality.clock_class default value (248)
    TEST_ASSERT(default_ds.clockQuality.clock_class == 248,
                "clockQuality.clock_class should be 248 (default application-specific)");
    
    // Test 5: Verify clockQuality.clock_accuracy default value (0xFE)
    TEST_ASSERT(default_ds.clockQuality.clock_accuracy == 0xFE,
                "clockQuality.clock_accuracy should be 0xFE (unknown accuracy)");
    
    // Test 6: Verify clockQuality.offset_scaled_log_variance default value (0xFFFF)
    TEST_ASSERT(default_ds.clockQuality.offset_scaled_log_variance == 0xFFFF,
                "clockQuality.offset_scaled_log_variance should be 0xFFFF (maximum variance)");
    
    // Test 7: Verify priority1 default value (128)
    TEST_ASSERT(default_ds.priority1 == 128,
                "priority1 should be 128 (IEEE 1588-2019 default)");
    
    // Test 8: Verify priority2 default value (128)
    TEST_ASSERT(default_ds.priority2 == 128,
                "priority2 should be 128 (IEEE 1588-2019 default)");
    
    // Test 9: Verify domainNumber matches configuration (42)
    TEST_ASSERT(default_ds.domainNumber == 42,
                "domainNumber should match configuration (42)");
    
    // Test 10: Verify slaveOnly default value (false)
    TEST_ASSERT(default_ds.slaveOnly == false,
                "slaveOnly should be FALSE (can become master)");
    
    // Test 11: Verify clockIdentity matches port_identity.clock_identity
    const auto& port_ds = port.get_port_data_set();
    bool identities_match = true;
    for (size_t i = 0; i < default_ds.clockIdentity.size(); ++i) {
        if (default_ds.clockIdentity[i] != port_ds.port_identity.clock_identity[i]) {
            identities_match = false;
            break;
        }
    }
    TEST_ASSERT(identities_match,
                "DefaultDataSet.clockIdentity should match PortDataSet.port_identity.clock_identity");
    
    std::cout << "\n--- DefaultDataSet Structure Contents ---" << std::endl;
    std::cout << "twoStepFlag: " << (default_ds.twoStepFlag ? "TRUE" : "FALSE") << std::endl;
    std::cout << "clockIdentity: ";
    for (auto byte : default_ds.clockIdentity) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(byte) << ":";
    }
    std::cout << std::dec << std::endl;
    std::cout << "numberPorts: " << default_ds.numberPorts << std::endl;
    std::cout << "clockQuality.clock_class: " << static_cast<int>(default_ds.clockQuality.clock_class) << std::endl;
    std::cout << "clockQuality.clock_accuracy: 0x" << std::hex << static_cast<int>(default_ds.clockQuality.clock_accuracy) << std::dec << std::endl;
    std::cout << "clockQuality.offset_scaled_log_variance: 0x" << std::hex << default_ds.clockQuality.offset_scaled_log_variance << std::dec << std::endl;
    std::cout << "priority1: " << static_cast<int>(default_ds.priority1) << std::endl;
    std::cout << "priority2: " << static_cast<int>(default_ds.priority2) << std::endl;
    std::cout << "domainNumber: " << static_cast<int>(default_ds.domainNumber) << std::endl;
    std::cout << "slaveOnly: " << (default_ds.slaveOnly ? "TRUE" : "FALSE") << std::endl;
}

/**
 * @brief Test DefaultDataSet size constraint for deterministic access
 * @design DES-D-004-DefaultDataset
 */
void test_default_ds_size_constraint() {
    std::cout << "\n=== Test: DefaultDataSet Size Constraint ===" << std::endl;
    
    // Test: Verify DefaultDataSet size is compact (≤64 bytes)
    const size_t default_ds_size = sizeof(Clocks::DefaultDataSet);
    std::cout << "DefaultDataSet size: " << default_ds_size << " bytes" << std::endl;
    
    TEST_ASSERT(default_ds_size <= 64,
                "DefaultDataSet must be ≤64 bytes for deterministic access (static_assert enforced)");
    
    // Print size breakdown
    std::cout << "\n--- Size Breakdown ---" << std::endl;
    std::cout << "bool twoStepFlag: " << sizeof(bool) << " byte" << std::endl;
    std::cout << "ClockIdentity (array<uint8_t,8>): " << sizeof(Types::ClockIdentity) << " bytes" << std::endl;
    std::cout << "uint16_t numberPorts: " << sizeof(std::uint16_t) << " bytes" << std::endl;
    std::cout << "ClockQuality: " << sizeof(Types::ClockQuality) << " bytes" << std::endl;
    std::cout << "uint8_t priority1: " << sizeof(std::uint8_t) << " byte" << std::endl;
    std::cout << "uint8_t priority2: " << sizeof(std::uint8_t) << " byte" << std::endl;
    std::cout << "uint8_t domainNumber: " << sizeof(std::uint8_t) << " byte" << std::endl;
    std::cout << "bool slaveOnly: " << sizeof(bool) << " byte" << std::endl;
    std::cout << "Total theoretical minimum: ~17 bytes (padding may increase)" << std::endl;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << "TEST-UNIT-DefaultDS-Init: DefaultDataSet Initialization" << std::endl;
    std::cout << "IEEE 1588-2019 Section 8.2.1 Compliance Test" << std::endl;
    std::cout << "======================================================" << std::endl;
    
    test_default_ds_initialization();
    test_default_ds_size_constraint();
    
    // Summary
    std::cout << "\n======================================================" << std::endl;
    std::cout << "Test Summary:" << std::endl;
    std::cout << "  PASSED: " << tests_passed << std::endl;
    std::cout << "  FAILED: " << tests_failed << std::endl;
    std::cout << "======================================================" << std::endl;
    
    return (tests_failed == 0) ? 0 : 1;
}
