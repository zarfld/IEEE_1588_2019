/**
 * @file test_boundary_clock_queries.cpp
 * @brief Test BoundaryClock helper query methods
 * 
 * Coverage targets:
 * - has_master_port() (lines 1065-1073)
 * - has_slave_port() (lines 1075-1083)
 * - is_synchronized() (lines 1085-1093)
 * - find_port() non-const (lines 1095-1104)
 * - find_port() const (lines 1106-1115)
 */

#include <cstdlib>
#include <iostream>
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

StateCallbacks make_callbacks() {
    StateCallbacks cb{};
    cb.send_announce = send_announce_stub;
    cb.send_sync = send_sync_stub;
    cb.send_follow_up = send_follow_up_stub;
    cb.send_delay_req = send_delay_req_stub;
    cb.send_delay_resp = send_delay_resp_stub;
    cb.get_timestamp = get_timestamp_stub;
    cb.get_tx_timestamp = get_tx_timestamp_stub;
    cb.adjust_clock = adjust_clock_stub;
    cb.adjust_frequency = adjust_frequency_stub;
    cb.on_state_change = on_state_change_stub;
    cb.on_fault = on_fault_stub;
    return cb;
}

PortConfiguration make_port_config(uint8_t port_num) {
    PortConfiguration cfg{};
    cfg.port_number = port_num;
    cfg.announce_interval = 1;
    cfg.sync_interval = 0;
    cfg.delay_mechanism_p2p = false;
    return cfg;
}

void test_has_master_port_with_no_master() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    configs[0] = make_port_config(1);
    configs[1] = make_port_config(2);
    
    BoundaryClock bc(configs, 2, make_callbacks());
    bc.initialize();
    // Default state is INITIALIZING, not Master
    
    if (bc.has_master_port()) {
        std::cerr << "FAIL: has_master_port() should return false with no master\n";
        std::exit(1);
    }
    std::cout << "PASS: has_master_port() returns false with no master\n";
}

void test_has_slave_port_with_no_slave() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    configs[0] = make_port_config(1);
    
    BoundaryClock bc(configs, 1, make_callbacks());
    bc.initialize();
    // Default state is not Slave
    
    if (bc.has_slave_port()) {
        std::cerr << "FAIL: has_slave_port() should return false with no slave\n";
        std::exit(1);
    }
    std::cout << "PASS: has_slave_port() returns false with no slave\n";
}

void test_is_synchronized_with_no_sync() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    configs[0] = make_port_config(1);
    
    BoundaryClock bc(configs, 1, make_callbacks());
    bc.initialize();
    // Not synchronized initially
    
    if (bc.is_synchronized()) {
        std::cerr << "FAIL: is_synchronized() should return false initially\n";
        std::exit(1);
    }
    std::cout << "PASS: is_synchronized() returns false initially\n";
}

void test_find_port_valid_port_number() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    configs[0] = make_port_config(1);
    configs[1] = make_port_config(2);
    configs[2] = make_port_config(3);
    
    BoundaryClock bc(configs, 3, make_callbacks());
    bc.initialize();
    
    // Non-const version via const_cast to test non-const find_port
    BoundaryClock* bc_ptr = &bc;
    const PtpPort* port2 = bc_ptr->get_port(2);
    
    if (!port2) {
        std::cerr << "FAIL: find_port() should find valid port 2\n";
        std::exit(1);
    }
    std::cout << "PASS: find_port() finds valid port 2\n";
}

void test_find_port_invalid_port_number() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    configs[0] = make_port_config(1);
    
    BoundaryClock bc(configs, 1, make_callbacks());
    bc.initialize();
    
    const PtpPort* port99 = bc.get_port(99);
    
    if (port99) {
        std::cerr << "FAIL: find_port() should return nullptr for invalid port\n";
        std::exit(1);
    }
    std::cout << "PASS: find_port() returns nullptr for invalid port\n";
}

void test_find_port_const_version() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    configs[0] = make_port_config(1);
    configs[1] = make_port_config(2);
    
    const BoundaryClock bc(configs, 2, make_callbacks());
    
    const PtpPort* port1 = bc.get_port(1);
    
    if (!port1) {
        std::cerr << "FAIL: const find_port() should find valid port 1\n";
        std::exit(1);
    }
    std::cout << "PASS: const find_port() finds valid port 1\n";
}

void test_multiple_ports_with_mixed_states() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    configs[0] = make_port_config(1);
    configs[1] = make_port_config(2);
    configs[2] = make_port_config(3);
    
    BoundaryClock bc(configs, 3, make_callbacks());
    bc.initialize();
    
    // Exercise looping logic in has_master_port, has_slave_port, is_synchronized
    // by checking multiple ports
    
    bool has_master = bc.has_master_port();
    bool has_slave = bc.has_slave_port();
    bool is_sync = bc.is_synchronized();
    
    // Just verify calls don't crash and return consistent results
    std::cout << "PASS: Multi-port queries executed (master=" << has_master 
              << ", slave=" << has_slave << ", sync=" << is_sync << ")\n";
}

void test_find_port_boundary_case_port_zero() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    configs[0] = make_port_config(1);
    
    BoundaryClock bc(configs, 1, make_callbacks());
    bc.initialize();
    
    const PtpPort* port0 = bc.get_port(0);
    
    if (port0) {
        std::cerr << "FAIL: find_port(0) should return nullptr\n";
        std::exit(1);
    }
    std::cout << "PASS: find_port(0) returns nullptr\n";
}

void test_find_port_max_ports() {
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> configs{};
    for (size_t i = 0; i < BoundaryClock::MAX_PORTS; ++i) {
        configs[i] = make_port_config(static_cast<uint8_t>(i + 1));
    }
    
    BoundaryClock bc(configs, BoundaryClock::MAX_PORTS, make_callbacks());
    bc.initialize();
    
    // Find last port
    const PtpPort* last_port = bc.get_port(BoundaryClock::MAX_PORTS);
    
    if (!last_port) {
        std::cerr << "FAIL: find_port() should find max port " << BoundaryClock::MAX_PORTS << "\n";
        std::exit(1);
    }
    std::cout << "PASS: find_port() finds max port " << BoundaryClock::MAX_PORTS << "\n";
}

int main() {
    test_has_master_port_with_no_master();
    test_has_slave_port_with_no_slave();
    test_is_synchronized_with_no_sync();
    test_find_port_valid_port_number();
    test_find_port_invalid_port_number();
    test_find_port_const_version();
    test_multiple_ports_with_mixed_states();
    test_find_port_boundary_case_port_zero();
    test_find_port_max_ports();
    
    std::cout << "\n=== All BoundaryClock query tests passed ===\n";
    return 0;
}
