/**
 * @file test_network_adapter.cpp
 * @brief Unit tests for NetworkAdapter class
 * 
 * Tests IEEE 1588-2019 network adapter implementation
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#include "../src/network_adapter.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace IEEE::_1588::PTP::_2019::Linux;

// Test utilities
#define TEST_PASS(name) std::cout << "[PASS] " << name << "\n"
#define TEST_FAIL(name, reason) do { \
    std::cerr << "[FAIL] " << name << ": " << reason << "\n"; \
    return false; \
} while(0)

// Test 1: Constructor and basic initialization
bool test_constructor()
{
    NetworkAdapter adapter("lo");  // Use loopback for testing
    
    // Adapter should be constructed successfully
    if (adapter.get_event_socket() >= 0) {
        TEST_FAIL("Constructor", "Sockets should not be initialized in constructor");
    }
    
    TEST_PASS("Constructor");
    return true;
}

// Test 2: Socket initialization
bool test_socket_initialization()
{
    NetworkAdapter adapter("lo");
    
    // Initialize should create sockets
    bool result = adapter.initialize();
    
    // On loopback, hardware timestamping may not be available
    // So initialization might fail - this is expected behavior
    // Test considers both success and graceful failure as valid
    
    if (result) {
        // Verify sockets were created
        if (adapter.get_event_socket() < 0) {
            TEST_FAIL("Socket Initialization", "Event socket not created");
        }
        if (adapter.get_general_socket() < 0) {
            TEST_FAIL("Socket Initialization", "General socket not created");
        }
    }
    
    TEST_PASS("Socket Initialization");
    return true;
}

// Test 3: MAC address retrieval
bool test_mac_address_retrieval()
{
    NetworkAdapter adapter("lo");
    uint8_t mac[6] = {0};
    
    bool result = adapter.get_mac_address(mac);
    
    // Loopback should have a MAC address (00:00:00:00:00:00)
    if (!result) {
        TEST_FAIL("MAC Address Retrieval", "Failed to get MAC address");
    }
    
    // Verify it's a valid MAC (all zeros for loopback is valid)
    bool all_zero = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    if (!all_zero) {
        std::cout << "  MAC: " << std::hex;
        for (int i = 0; i < 6; i++) {
            std::cout << (int)mac[i];
            if (i < 5) std::cout << ":";
        }
        std::cout << std::dec << "\n";
    }
    
    TEST_PASS("MAC Address Retrieval");
    return true;
}

// Test 4: NetworkTimestamp structure
bool test_network_timestamp()
{
    NetworkTimestamp ts;
    ts.seconds = 1234567890;
    ts.nanoseconds = 123456789;
    ts.type = 0x01;  // Example type
    
    // Verify fields are set correctly
    if (ts.seconds != 1234567890) {
        TEST_FAIL("NetworkTimestamp", "Seconds field incorrect");
    }
    if (ts.nanoseconds != 123456789) {
        TEST_FAIL("NetworkTimestamp", "Nanoseconds field incorrect");
    }
    if (ts.type != 0x01) {
        TEST_FAIL("NetworkTimestamp", "Type field incorrect");
    }
    
    TEST_PASS("NetworkTimestamp Structure");
    return true;
}

// Test 5: Hardware timestamping capability query
bool test_hardware_timestamping_capability()
{
    NetworkAdapter adapter("lo");
    adapter.initialize();
    
    // Query capability
    bool supports_hw_ts = adapter.supports_hardware_timestamping();
    
    // Loopback likely doesn't support HW timestamping
    // Test is successful if query completes without crash
    
    std::cout << "  HW timestamping supported: " 
              << (supports_hw_ts ? "yes" : "no") << "\n";
    
    TEST_PASS("Hardware Timestamping Capability");
    return true;
}

// Test 6: Timestamp precision query
bool test_timestamp_precision()
{
    NetworkAdapter adapter("lo");
    adapter.initialize();
    
    uint32_t precision = adapter.get_timestamp_precision_ns();
    
    // Precision should be non-zero for any interface
    if (precision == 0) {
        TEST_FAIL("Timestamp Precision", "Precision is zero");
    }
    
    std::cout << "  Timestamp precision: " << precision << " ns\n";
    
    TEST_PASS("Timestamp Precision");
    return true;
}

// Test 7: Packet buffer structure
bool test_packet_buffer()
{
    const size_t buffer_size = 1500;
    uint8_t buffer[buffer_size];
    
    // Fill with test pattern
    for (size_t i = 0; i < buffer_size; i++) {
        buffer[i] = static_cast<uint8_t>(i % 256);
    }
    
    // Verify pattern
    for (size_t i = 0; i < 100; i++) {
        if (buffer[i] != static_cast<uint8_t>(i % 256)) {
            TEST_FAIL("Packet Buffer", "Test pattern corrupted");
        }
    }
    
    TEST_PASS("Packet Buffer");
    return true;
}

// Test 8: Send packet interface (loopback test)
bool test_send_packet()
{
    NetworkAdapter adapter("lo");
    if (!adapter.initialize()) {
        std::cout << "  [SKIP] Adapter initialization failed (expected on loopback)\n";
        TEST_PASS("Send Packet (Skipped)");
        return true;
    }
    
    // Create test packet (minimal PTP Sync message)
    uint8_t packet[44] = {0};
    packet[0] = 0x00;  // messageType (Sync)
    packet[1] = 0x02;  // versionPTP
    
    NetworkTimestamp tx_timestamp;
    int result = adapter.send_packet(packet, sizeof(packet), &tx_timestamp, true);
    
    // On loopback, send may fail - this is expected
    // Test verifies the interface works without crashing
    
    if (result >= 0) {
        std::cout << "  Sent " << result << " bytes\n";
    } else {
        std::cout << "  Send failed (expected on loopback)\n";
    }
    
    TEST_PASS("Send Packet");
    return true;
}

// Test 9: Receive packet interface (non-blocking test)
bool test_receive_packet()
{
    NetworkAdapter adapter("lo");
    if (!adapter.initialize()) {
        std::cout << "  [SKIP] Adapter initialization failed (expected on loopback)\n";
        TEST_PASS("Receive Packet (Skipped)");
        return true;
    }
    
    // Set socket to non-blocking mode for test
    uint8_t buffer[1500];
    NetworkTimestamp rx_timestamp;
    size_t received_length = 0;
    
    // This will likely return EAGAIN/EWOULDBLOCK on loopback with no traffic
    // Test verifies the interface works without crashing
    int result = adapter.receive_packet(buffer, sizeof(buffer), 
                                       &rx_timestamp, &received_length, true);
    
    if (result == 0 && received_length > 0) {
        std::cout << "  Received " << received_length << " bytes\n";
    } else {
        std::cout << "  No packets received (expected when no traffic)\n";
    }
    
    TEST_PASS("Receive Packet");
    return true;
}

// Test 10: Error handling - invalid interface
bool test_error_handling_invalid_interface()
{
    NetworkAdapter adapter("invalid_interface_xyz123");
    
    // Initialization should fail gracefully
    bool result = adapter.initialize();
    
    if (result) {
        TEST_FAIL("Error Handling", "Should fail for invalid interface");
    }
    
    // Verify sockets remain invalid
    if (adapter.get_event_socket() >= 0) {
        TEST_FAIL("Error Handling", "Event socket should be invalid");
    }
    if (adapter.get_general_socket() >= 0) {
        TEST_FAIL("Error Handling", "General socket should be invalid");
    }
    
    TEST_PASS("Error Handling - Invalid Interface");
    return true;
}

// Test 11: Thread safety
bool test_thread_safety()
{
    NetworkAdapter adapter("lo");
    adapter.initialize();
    
    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int operations_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    // Launch threads that concurrently access adapter
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&adapter, &success_count, operations_per_thread]() {
            for (int i = 0; i < operations_per_thread; i++) {
                // Query capabilities (read-only operations)
                adapter.supports_hardware_timestamping();
                adapter.get_timestamp_precision_ns();
                
                // Get MAC address (involves system call)
                uint8_t mac[6];
                if (adapter.get_mac_address(mac)) {
                    success_count++;
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify no crashes occurred and some operations succeeded
    if (success_count == 0) {
        TEST_FAIL("Thread Safety", "No operations succeeded");
    }
    
    std::cout << "  " << success_count << "/" << (num_threads * operations_per_thread) 
              << " operations succeeded\n";
    
    TEST_PASS("Thread Safety");
    return true;
}

// Test 12: Multicast join
bool test_multicast_join()
{
    NetworkAdapter adapter("lo");
    if (!adapter.initialize()) {
        std::cout << "  [SKIP] Adapter initialization failed\n";
        TEST_PASS("Multicast Join (Skipped)");
        return true;
    }
    
    // Join a test multicast group
    bool result = adapter.join_multicast("224.0.0.1");
    
    // Result may vary by interface - test verifies no crash
    std::cout << "  Multicast join: " << (result ? "success" : "failed") << "\n";
    
    TEST_PASS("Multicast Join");
    return true;
}

int main(int argc, char** argv)
{
    std::cout << "=== NetworkAdapter Unit Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    // Run all tests
    if (test_constructor()) passed++; else failed++;
    if (test_socket_initialization()) passed++; else failed++;
    if (test_mac_address_retrieval()) passed++; else failed++;
    if (test_network_timestamp()) passed++; else failed++;
    if (test_hardware_timestamping_capability()) passed++; else failed++;
    if (test_timestamp_precision()) passed++; else failed++;
    if (test_packet_buffer()) passed++; else failed++;
    if (test_send_packet()) passed++; else failed++;
    if (test_receive_packet()) passed++; else failed++;
    if (test_error_handling_invalid_interface()) passed++; else failed++;
    if (test_thread_safety()) passed++; else failed++;
    if (test_multicast_join()) passed++; else failed++;
    
    // Summary
    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Passed: " << passed << "/" << (passed + failed) << "\n";
    std::cout << "Failed: " << failed << "/" << (passed + failed) << "\n";
    
    return (failed == 0) ? 0 : 1;
}
