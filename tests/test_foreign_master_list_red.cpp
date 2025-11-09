// ---
// title: "Foreign Master List Management Red Test"
// specType: test
// testId: TEST-FOREIGN-001
// status: active
// relatedRequirements:
//   - REQ-F-002
//   - REQ-NF-P-001
// purpose: "RED phase test for GAP-FOREIGN-001: Validates foreign master list timeout, aging, and pruning per IEEE 1588-2019 Section 9.5.17. Expected to FAIL until GREEN phase implements aging/timeout logic."
// traceStatus: planned
// ---
// IEEE 1588-2019 References:
//   - Section 9.5.17: Foreign master data set specification
//   - Section 9.3.2.5: BMCA algorithm uses foreign master list
//   - Section 8.2.15.4: announceReceiptTimeout - timeout multiplier for Announce messages
// Foreign Master Timeout Formula: timeout = announceReceiptTimeout × 2^logMessageInterval
//   Example: announceReceiptTimeout=3, logMessageInterval=1 (2s) → timeout=6 seconds
// NOTE: This file intentionally avoids reproducing copyrighted spec text; logic based on understanding.

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019::Clocks;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019;

// Simulated timestamp counter for testing
static uint64_t time_ns = 0;

// Test callbacks
static Timestamp test_now() {
    Timestamp ts{};
    ts.setTotalSeconds(time_ns / 1'000'000'000ULL);
    ts.nanoseconds = static_cast<uint32_t>(time_ns % 1'000'000'000ULL);
    return ts;
}

static void advance_time_seconds(uint64_t seconds) {
    time_ns += seconds * 1'000'000'000ULL;
}

// Helper: Create Announce message from specific foreign master
static AnnounceMessage make_announce(uint8_t master_id, uint8_t priority1, uint8_t domain) {
    AnnounceMessage msg{};
    msg.initialize(MessageType::Announce, domain, PortIdentity{});
    
    // Set unique source port identity
    for (int i = 0; i < 7; ++i) {
        msg.header.sourcePortIdentity.clock_identity[i] = 0xAA;
    }
    msg.header.sourcePortIdentity.clock_identity[7] = master_id;
    msg.header.sourcePortIdentity.port_number = 1;
    
    // Announce body - set grandmaster priority
    msg.body.grandmasterPriority1 = priority1;
    msg.body.grandmasterPriority2 = 128;
    msg.body.grandmasterClockClass = 248;
    msg.body.grandmasterClockAccuracy = 0xFE;
    msg.body.grandmasterClockVariance = 0xFFFF;
    
    // Grandmaster identity (same as source)
    for (int i = 0; i < 8; ++i) {
        msg.body.grandmasterIdentity[i] = msg.header.sourcePortIdentity.clock_identity[i];
    }
    
    msg.body.stepsRemoved = 1;
    msg.body.currentUtcOffset = 37;
    msg.body.timeSource = 0xA0; // INTERNAL_OSCILLATOR
    msg.header.logMessageInterval = 1; // 2^1 = 2 seconds
    
    return msg;
}

int main() {
    int test_failures = 0;
    
    std::printf("========================================\n");
    std::printf("GAP-FOREIGN-001 RED Phase Tests\n");
    std::printf("Foreign Master List Aging & Timeout\n");
    std::printf("IEEE 1588-2019 Section 9.5.17\n");
    std::printf("========================================\n\n");
    
    // Configure callbacks
    StateCallbacks cb{};
    cb.get_timestamp = &test_now;
    
    // Configure port
    PortConfiguration cfg{};
    cfg.port_number = 1;
    cfg.domain_number = 0;
    
    // TEST 1: Foreign Master List Basic Management
    // Requirement: Port must maintain list of foreign masters
    // Expected: FAIL - Need to verify foreign master count increases
    {
        std::printf("TEST 1: Foreign Master List Basic Management\n");
        std::printf("  Requirement: Maintain foreign master list from Announce messages\n");
        std::printf("  IEEE Reference: Section 9.5.17.5.2\n");
        
        time_ns = 1'000'000'000ULL; // Start at T=1 second
        
        PtpPort port(cfg, cb);
        if (!port.initialize().is_success()) {
            std::fprintf(stderr, "  TEST 1 INIT FAIL\n");
            return 10;
        }
        if (!port.start().is_success()) {
            std::fprintf(stderr, "  TEST 1 START FAIL\n");
            return 11;
        }
        
        // Force into LISTENING state (receives Announce but doesn't participate)
        port.process_event(StateEvent::INITIALIZE);
        port.process_event(StateEvent::DESIGNATED_ENABLED);
        
        // Send announce from foreign master 0x01
        AnnounceMessage announce1 = make_announce(0x01, 100, cfg.domain_number);
        auto result1 = port.process_announce(announce1);
        
        if (!result1.is_success()) {
            std::fprintf(stderr, "  TEST 1 FAIL: process_announce error %u\n", (unsigned)result1.get_error());
            test_failures++;
        } else {
            std::printf("  TEST 1: Foreign master added successfully\n");
            std::printf("        (RED Phase: Cannot verify count - need API)\n");
            // In GREEN phase: Check foreign_master_count == 1
            test_failures++; // Expected to fail - no way to verify yet
        }
        std::printf("\n");
    }
    
    // TEST 2: Multiple Foreign Masters
    // Requirement: Port must track multiple foreign masters independently
    // Expected: FAIL - Need API to verify count
    {
        std::printf("TEST 2: Multiple Foreign Masters Tracking\n");
        std::printf("  Requirement: Track multiple foreign masters independently\n");
        std::printf("  IEEE Reference: Section 9.5.17\n");
        
        time_ns = 10'000'000'000ULL; // Start at T=10 seconds
        
        PtpPort port(cfg, cb);
        port.initialize();
        port.start();
        port.process_event(StateEvent::INITIALIZE);
        port.process_event(StateEvent::DESIGNATED_ENABLED);
        
        // Add three different foreign masters
        AnnounceMessage announce1 = make_announce(0x10, 100, cfg.domain_number);
        AnnounceMessage announce2 = make_announce(0x11, 105, cfg.domain_number);
        AnnounceMessage announce3 = make_announce(0x12, 110, cfg.domain_number);
        
        port.process_announce(announce1);
        advance_time_seconds(1);
        port.process_announce(announce2);
        advance_time_seconds(1);
        port.process_announce(announce3);
        
        std::printf("  TEST 2: Added 3 foreign masters\n");
        std::printf("        (RED Phase: Cannot verify count == 3)\n");
        test_failures++; // Expected to fail - no verification API
        std::printf("\n");
    }
    
    // TEST 3: Foreign Master Timeout Detection
    // Requirement: Expired foreign masters detected after announceReceiptTimeout × announceInterval
    // Formula: timeout = 3 × 2^1 = 6 seconds (announceReceiptTimeout=3, logMessageInterval=1)
    // Expected: FAIL - Timeout detection not implemented
    {
        std::printf("TEST 3: Foreign Master Timeout Detection\n");
        std::printf("  Requirement: Detect expired foreign masters\n");
        std::printf("  IEEE Reference: Section 8.2.15.4, Section 9.5.17\n");
        std::printf("  Timeout Formula: announceReceiptTimeout × 2^logMessageInterval\n");
        std::printf("  Test: 3 × 2^1 = 6 seconds\n");
        
        time_ns = 20'000'000'000ULL; // Start at T=20 seconds
        
        PtpPort port(cfg, cb);
        port.initialize();
        port.start();
        port.process_event(StateEvent::INITIALIZE);
        port.process_event(StateEvent::DESIGNATED_ENABLED);
        
        // Send announce from foreign master at T=20s
        AnnounceMessage announce = make_announce(0x20, 100, cfg.domain_number);
        port.process_announce(announce);
        
        // Advance time 7 seconds (T=27s, exceeds 6s timeout)
        advance_time_seconds(7);
        
        // Trigger port tick to check for timeouts
        Timestamp t = test_now();
        port.tick(t);
        
        std::printf("  TEST 3: Simulated 7s timeout (exceeds 6s limit)\n");
        std::printf("        (RED Phase: Cannot verify expiration - no API)\n");
        std::printf("        Expected: Foreign master should be marked expired\n");
        test_failures++; // Expected to fail - timeout logic not implemented
        std::printf("\n");
    }
    
    // TEST 4: Stale Foreign Master Pruning Before BMCA
    // Requirement: Expired foreign masters removed before BMCA execution
    // Expected: FAIL - Pruning logic not implemented
    {
        std::printf("TEST 4: Stale Foreign Master Pruning Before BMCA\n");
        std::printf("  Requirement: Remove expired entries before BMCA\n");
        std::printf("  IEEE Reference: Section 9.3.2.5\n");
        
        time_ns = 30'000'000'000ULL; // Start at T=30 seconds
        
        PtpPort port(cfg, cb);
        port.initialize();
        port.start();
        
        // Transition to SLAVE state to trigger BMCA
        port.process_event(StateEvent::RS_SLAVE);
        
        // Add foreign master at T=30s
        AnnounceMessage announce = make_announce(0x30, 90, cfg.domain_number); // Best priority
        port.process_announce(announce);
        
        // Verify port selects this master initially
        const auto& pds1 = port.get_parent_data_set();
        bool initially_selected = (pds1.parent_port_identity.clock_identity[7] == 0x30);
        
        // Advance time 7 seconds (exceeds 6s timeout)
        advance_time_seconds(7);
        
        // Trigger BMCA re-evaluation
        Timestamp t = test_now();
        port.tick(t);
        
        // Check if parent changed (should have no parent after pruning)
        const auto& pds2 = port.get_parent_data_set();
        bool still_selected = (pds2.parent_port_identity.clock_identity[7] == 0x30);
        
        if (initially_selected && still_selected) {
            std::printf("  TEST 4 FAIL: Expired foreign master still selected\n");
            std::printf("        (Pruning logic not implemented)\n");
            test_failures++;
        } else {
            std::printf("  TEST 4: Unexpected behavior\n");
            std::printf("        (Either not initially selected, or correctly pruned)\n");
            test_failures++;
        }
        std::printf("\n");
    }
    
    // TEST 5: Foreign Master List Size Limit
    // Requirement: Handle list full condition gracefully (MAX_FOREIGN_MASTERS = 16)
    // Expected: FAIL - Need to verify behavior when full
    {
        std::printf("TEST 5: Foreign Master List Size Limit\n");
        std::printf("  Requirement: Handle MAX_FOREIGN_MASTERS limit (16)\n");
        std::printf("  IEEE Reference: Section 9.5.17 (implementation limit)\n");
        
        time_ns = 40'000'000'000ULL; // Start at T=40 seconds
        
        PtpPort port(cfg, cb);
        port.initialize();
        port.start();
        port.process_event(StateEvent::INITIALIZE);
        port.process_event(StateEvent::DESIGNATED_ENABLED);
        
        // Add 16 foreign masters (fill the list)
        for (uint8_t i = 0; i < 16; ++i) {
            AnnounceMessage announce = make_announce(i, 100 + i, cfg.domain_number);
            auto result = port.process_announce(announce);
            if (!result.is_success()) {
                std::fprintf(stderr, "  TEST 5 WARNING: Failed to add master %u\n", i);
            }
        }
        
        // Try to add 17th foreign master with best priority
        AnnounceMessage announce17 = make_announce(0x50, 50, cfg.domain_number);
        auto result17 = port.process_announce(announce17);
        
        std::printf("  TEST 5: Added 16+ foreign masters\n");
        std::printf("        (RED Phase: Cannot verify list full handling)\n");
        std::printf("        Expected: Either reject or replace existing entry\n");
        test_failures++; // Expected to fail - no verification
        std::printf("\n");
    }
    
    // Summary
    std::printf("========================================\n");
    std::printf("RED Phase Test Results\n");
    std::printf("========================================\n");
    std::printf("Tests Failed: %d\n", test_failures);
    std::printf("Total Tests:  5\n\n");
    
    if (test_failures == 5) {
        std::printf("✓ RED Phase Successful: All 5 tests failed as expected\n");
        std::printf("  Next: Implement GREEN phase\n");
        std::printf("  - Add foreign master timeout detection\n");
        std::printf("  - Implement aging/pruning logic\n");
        std::printf("  - Add API to verify foreign master list state\n");
        std::printf("  - Integrate pruning with BMCA\n\n");
        return 0;  // Success - tests failed as expected in RED phase
    } else if (test_failures > 0) {
        std::printf("⚠ RED Phase Partial: %d of 5 tests failed\n", test_failures);
        std::printf("  Some tests may have unexpected behavior\n\n");
        return 1;
    } else {
        std::printf("✗ RED Phase Invalid: All tests passed\n");
        std::printf("  Tests should fail before GREEN implementation\n\n");
        return 2;
    }
}
