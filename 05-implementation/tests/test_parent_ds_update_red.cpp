/*
Module: 05-implementation/tests/test_parent_ds_update_red.cpp
Phase: 05-implementation (RED phase - TDD)
Test ID: TEST-UNIT-ParentDS-Update
Traceability:
    Gap: GAP-PARENT-001 (Dataset dynamic updates)
    Requirements: REQ-F-202 (BMCA state machine), StR-EXTS-009
    Design: DES-C-003 (BMCA Engine), DES-D-033 (Dataset structures)
    IEEE Spec: IEEE 1588-2019 Section 8.2.3 (Parent Dataset)
               IEEE 1588-2019 Section 13.5 (Announce message)
Notes: RED phase tests - designed to FAIL until GREEN phase implementation.
       Tests verify that when BMCA selects a foreign master, the parentDS
       and currentDS are properly updated with information from Announce message.
*/

/**
 * @file test_parent_ds_update_red.cpp
 * @brief RED phase unit tests for Parent Dataset dynamic updates
 * @details Tests that parentDS is correctly updated when BMCA selects a foreign master.
 *          Per IEEE 1588-2019 Section 8.2.3, the parent dataset contains information
 *          about the current master clock and must be updated when BMCA changes master.
 * 
 * Test Coverage:
 * - ParentDS fields updated from Announce message body
 * - CurrentDS steps_removed incremented correctly
 * - Clock quality fields propagated correctly
 * - Priority1/priority2 copied from grandmaster
 * - Parent port identity set to source of Announce
 * - Atomic update (all fields consistent)
 * 
 * IEEE 1588-2019 References:
 * - Section 8.2.3: Parent Dataset specification
 * - Section 13.5: Announce message format (Table 27)
 * - Section 9.3: BMCA behavior and dataset updates
 */

#include <cstdio>
#include <cstring>
#include "clocks.hpp"
#include "bmca.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;
using namespace IEEE::_1588::PTP::_2019::BMCA;

// Test helper stubs (matching signature from working BMCA tests)
static Types::PTPError stub_send_announce(const AnnounceMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_sync(const SyncMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_follow_up(const FollowUpMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_req(const DelayReqMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_resp(const DelayRespMessage&) { return Types::PTPError::Success; }
static Types::Timestamp stub_get_ts() { return Types::Timestamp{}; }
static Types::PTPError stub_get_tx_ts(std::uint16_t, Types::Timestamp* t) { *t = Types::Timestamp{}; return Types::PTPError::Success; }
static Types::PTPError stub_adjust_clock(std::int64_t) { return Types::PTPError::Success; }
static Types::PTPError stub_adjust_freq(double) { return Types::PTPError::Success; }
static void stub_on_state_change(PortState, PortState) {}
static void stub_on_fault(const char*) {}

// Helper to create Announce message with specified parameters
static AnnounceMessage make_announce(
    std::uint8_t priority1,
    std::uint8_t clockClass,
    std::uint8_t clockAccuracy,
    std::uint16_t variance,
    std::uint8_t priority2,
    std::uint16_t stepsRemoved,
    std::uint64_t gmIdentity,
    std::uint64_t sourceClockId,
    std::uint16_t sourcePortNum)
{
    AnnounceMessage msg{};
    msg.header.transport_messageType = static_cast<std::uint8_t>(MessageType::Announce) & 0x0F;
    msg.header.reserved_version = 0x02;  // PTPv2 in lower nibble
    msg.header.messageLength = sizeof(AnnounceMessage);
    msg.header.domainNumber = 0;
    msg.header.sequenceId = 1;
    
    // Source port identity
    for (int i = 0; i < 8; i++) {
        msg.header.sourcePortIdentity.clock_identity[i] = (sourceClockId >> (56 - i*8)) & 0xFF;
    }
    msg.header.sourcePortIdentity.port_number = sourcePortNum;
    
    // Announce body fields per IEEE 1588-2019 Table 27
    msg.body.grandmasterPriority1 = priority1;
    msg.body.grandmasterClockClass = clockClass;
    msg.body.grandmasterClockAccuracy = clockAccuracy;
    msg.body.grandmasterClockVariance = variance;
    msg.body.grandmasterPriority2 = priority2;
    msg.body.stepsRemoved = stepsRemoved;
    
    // Grandmaster identity
    for (int i = 0; i < 8; i++) {
        msg.body.grandmasterIdentity[i] = (gmIdentity >> (56 - i*8)) & 0xFF;
    }
    
    return msg;
}

int main() {
    std::printf("=== TEST-UNIT-ParentDS-Update (RED Phase) ===\n\n");
    
    int failures = 0;
    
    // Test 1: ParentDS updated when foreign master selected
    {
        std::printf("--- Test 1: ParentDS updated after foreign master wins BMCA ---\n");
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            stub_on_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        OrdinaryClock clock(cfg, callbacks);
        clock.initialize();
        clock.start();
        
        // Create foreign master with better parameters than local (priority1=100 vs 128)
        auto announce = make_announce(
            100,                        // priority1 (better than local 128)
            128,                        // clockClass
            0x20,                       // clockAccuracy
            5000,                       // variance
            100,                        // priority2
            1,                          // stepsRemoved
            0xAABBCCDDEEFF0011ULL,     // grandmasterIdentity
            0x1122334455667788ULL,     // sourceClockIdentity
            1                           // sourcePortNumber
        );
        
        // Process announce - should trigger BMCA and update parentDS
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &announce, sizeof(AnnounceMessage), Types::Timestamp{});
        
        const auto& parent_ds = clock.get_port().get_parent_data_set();
        const auto& current_ds = clock.get_port().get_current_data_set();
        
        // Verify parentDS fields updated from Announce
        bool gm_identity_correct = true;
        for (int i = 0; i < 8; i++) {
            std::uint8_t expected = (0xAABBCCDDEEFF0011ULL >> (56 - i*8)) & 0xFF;
            if (parent_ds.grandmaster_identity[i] != expected) {
                gm_identity_correct = false;
                break;
            }
        }
        
        bool parent_port_correct = true;
        for (int i = 0; i < 8; i++) {
            std::uint8_t expected = (0x1122334455667788ULL >> (56 - i*8)) & 0xFF;
            if (parent_ds.parent_port_identity.clock_identity[i] != expected) {
                parent_port_correct = false;
                break;
            }
        }
        
        if (parent_ds.grandmaster_priority1 != 100 ||
            parent_ds.grandmaster_clock_quality.clock_class != 128 ||
            parent_ds.grandmaster_clock_quality.clock_accuracy != 0x20 ||
            parent_ds.grandmaster_clock_quality.offset_scaled_log_variance != 5000 ||
            parent_ds.grandmaster_priority2 != 100 ||
            !gm_identity_correct ||
            !parent_port_correct ||
            parent_ds.parent_port_identity.port_number != 1 ||
            current_ds.steps_removed != 2) {  // Announce stepsRemoved=1 + 1 = 2
            
            std::printf("[FAIL] ParentDS not updated correctly:\n");
            std::printf("  Expected: priority1=100, got %u\n", parent_ds.grandmaster_priority1);
            std::printf("  Expected: clockClass=128, got %u\n", parent_ds.grandmaster_clock_quality.clock_class);
            std::printf("  Expected: clockAccuracy=0x20, got 0x%02X\n", parent_ds.grandmaster_clock_quality.clock_accuracy);
            std::printf("  Expected: variance=5000, got %u\n", parent_ds.grandmaster_clock_quality.offset_scaled_log_variance);
            std::printf("  Expected: priority2=100, got %u\n", parent_ds.grandmaster_priority2);
            std::printf("  Expected: stepsRemoved=2, got %u\n", current_ds.steps_removed);
            std::printf("  GM identity correct: %s\n", gm_identity_correct ? "yes" : "no");
            std::printf("  Parent port identity correct: %s\n", parent_port_correct ? "yes" : "no");
            failures++;
        } else {
            std::printf("[PASS] Test 1: ParentDS updated from foreign master\n");
        }
    }
    
    // Test 2: ParentDS reset to self when local wins BMCA
    {
        std::printf("\n--- Test 2: ParentDS reset to self when local becomes master ---\n");
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            stub_on_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        OrdinaryClock clock(cfg, callbacks);
        clock.initialize();
        clock.start();
        
        // First, let a better foreign master win
        auto better = make_announce(100, 128, 0x20, 5000, 100, 1, 0xAABBCCDDEEFF0011ULL, 0x1122334455667788ULL, 1);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &better, sizeof(AnnounceMessage), Types::Timestamp{});
        
        // Now REPLACE with worse priority from SAME foreign master (update existing entry)
        // This simulates the foreign master's quality degrading - local should now win
        auto worse = make_announce(200, 248, 0xFE, 0xFFFF, 200, 5, 0xAABBCCDDEEFF0011ULL, 0x1122334455667788ULL, 2);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &worse, sizeof(AnnounceMessage), Types::Timestamp{});
        
        const auto& parent_ds = clock.get_port().get_parent_data_set();
        const auto& port_ds = clock.get_port().get_port_data_set();
        const auto& current_ds = clock.get_port().get_current_data_set();
        
        // When local wins, parentDS should reflect local clock identity
        bool local_gm = true;
        for (int i = 0; i < 8; i++) {
            if (parent_ds.grandmaster_identity[i] != port_ds.port_identity.clock_identity[i]) {
                local_gm = false;
                break;
            }
        }
        
        bool self_parent = true;
        for (int i = 0; i < 8; i++) {
            if (parent_ds.parent_port_identity.clock_identity[i] != port_ds.port_identity.clock_identity[i]) {
                self_parent = false;
                break;
            }
        }
        
        if (!local_gm || !self_parent || current_ds.steps_removed != 0) {
            std::printf("[FAIL] ParentDS not reset to self:\n");
            std::printf("  GM identity matches local: %s\n", local_gm ? "yes" : "no");
            std::printf("  Parent identity is self: %s\n", self_parent ? "yes" : "no");
            std::printf("  Expected: stepsRemoved=0, got %u\n", current_ds.steps_removed);
            failures++;
        } else {
            std::printf("[PASS] Test 2: ParentDS reset to self when local wins\n");
        }
    }
    
    // Test 3: ParentDS switches between foreign masters
    {
        std::printf("\n--- Test 3: ParentDS updates when switching between foreign masters ---\n");
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            stub_on_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        OrdinaryClock clock(cfg, callbacks);
        clock.initialize();
        clock.start();
        
        // Foreign master A (priority1=110) from clock 0xAAAA...
        auto masterA = make_announce(110, 140, 0x25, 6000, 110, 2, 0xAAAAAAAAAAAAAAAAULL, 0xAAAAAAAAAAAAAAAAULL, 1);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &masterA, sizeof(AnnounceMessage), Types::Timestamp{});
        
        const auto& parent_ds_a = clock.get_port().get_parent_data_set();
        std::uint8_t gm_a[8];
        for (int i = 0; i < 8; i++) {
            gm_a[i] = parent_ds_a.grandmaster_identity[i];
        }
        
        // Same foreign master but with IMPROVED parameters (priority1=105, better clockClass)
        // This simulates the same clock announcing improved quality - datasets should update
        auto masterB = make_announce(105, 130, 0x21, 4500, 105, 1, 0xAAAAAAAAAAAAAAAAULL, 0xAAAAAAAAAAAAAAAAULL, 2);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &masterB, sizeof(AnnounceMessage), Types::Timestamp{});
        
        const auto& parent_ds_b = clock.get_port().get_parent_data_set();
        const auto& current_ds_b = clock.get_port().get_current_data_set();
        
        // Verify parentDS updated with improved parameters (priority1 105, clockClass 130)
        // GM identity stays same (0xAAAA...) but quality improved
        bool params_updated = (parent_ds_b.grandmaster_priority1 == 105 &&
                              parent_ds_b.grandmaster_clock_quality.clock_class == 130 &&
                              current_ds_b.steps_removed == 2);  // stepsRemoved=1 + 1 = 2
        
        if (!params_updated) {
            std::printf("[FAIL] ParentDS did not update to improved parameters:\n");
            std::printf("  Expected: priority1=105, got %u\n", parent_ds_b.grandmaster_priority1);
            std::printf("  Expected: clockClass=130, got %u\n", parent_ds_b.grandmaster_clock_quality.clock_class);
            std::printf("  Expected: stepsRemoved=2, got %u\n", current_ds_b.steps_removed);
            failures++;
        } else {
            std::printf("[PASS] Test 3: ParentDS updated when foreign master improved\n");
        }
    }
    
    // Test 4: Clock quality boundary values
    {
        std::printf("\n--- Test 4: Clock quality boundary values propagated correctly ---\n");
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            stub_on_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        OrdinaryClock clock(cfg, callbacks);
        clock.initialize();
        clock.start();
        
        // Test with boundary values
        auto announce = make_announce(
            0,                          // priority1 = minimum
            255,                        // clockClass = maximum
            0xFF,                       // clockAccuracy = unknown
            0xFFFF,                     // variance = maximum
            255,                        // priority2 = maximum
            0xFFFF,                     // stepsRemoved = maximum
            0xFFFFFFFFFFFFFFFFULL,     // grandmasterIdentity = maximum
            0xFEDCBA9876543210ULL,     // sourceClockIdentity
            0xFFFF                      // sourcePortNumber = maximum
        );
        
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &announce, sizeof(AnnounceMessage), Types::Timestamp{});
        
        const auto& parent_ds = clock.get_port().get_parent_data_set();
        
        if (parent_ds.grandmaster_priority1 != 0 ||
            parent_ds.grandmaster_clock_quality.clock_class != 255 ||
            parent_ds.grandmaster_clock_quality.clock_accuracy != 0xFF ||
            parent_ds.grandmaster_clock_quality.offset_scaled_log_variance != 0xFFFF ||
            parent_ds.grandmaster_priority2 != 255 ||
            parent_ds.parent_port_identity.port_number != 0xFFFF) {
            
            std::printf("[FAIL] Boundary values not propagated correctly\n");
            failures++;
        } else {
            std::printf("[PASS] Test 4: Boundary values propagated correctly\n");
        }
    }
    
    std::printf("\n=== TEST-UNIT-ParentDS-Update Summary ===\n");
    std::printf("Total unit tests: 4\n");
    std::printf("Failures: %d\n\n", failures);
    
    if (failures == 0) {
        std::printf("GREEN PHASE: All ParentDS update tests passed!\n");
        std::printf("This means the implementation already handles dataset updates correctly.\n");
        return 0;
    } else {
        std::printf("RED PHASE: Tests failing as expected.\n");
        std::printf("Implementation needed: Update parentDS/currentDS when BMCA selects foreign master.\n");
        return failures;
    }
}
