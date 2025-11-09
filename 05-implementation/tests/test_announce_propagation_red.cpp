/*
Module: 05-implementation/tests/test_announce_propagation_red.cpp
Phase: 05-implementation (RED phase - TDD)
Test ID: TEST-INT-Announce-Propagation
Traceability:
    Gap: GAP-PARENT-001 (Dataset dynamic updates)
    Requirements: REQ-F-202 (BMCA state machine), StR-EXTS-009
    Design: DES-C-003 (BMCA Engine), DES-D-033 (Dataset structures)
    IEEE Spec: IEEE 1588-2019 Section 13.5 (Announce message)
               IEEE 1588-2019 Section 9.3 (BMCA and dataset updates)
Notes: RED phase integration tests - designed to FAIL until GREEN phase implementation.
       Tests verify end-to-end flow: Announce → BMCA → Dataset updates → State transitions.
*/

/**
 * @file test_announce_propagation_red.cpp
 * @brief RED phase integration tests for Announce message propagation through BMCA to datasets
 * @details Tests complete flow from receiving Announce messages through BMCA decision
 *          to updating parent/current datasets and triggering appropriate state transitions.
 * 
 * Test Scenarios:
 * - Multiple Announce messages processed sequentially
 * - Dataset updates reflect most recent BMCA winner
 * - State transitions follow dataset changes
 * - Metrics/health telemetry emitted correctly
 * 
 * IEEE 1588-2019 References:
 * - Section 13.5: Announce message format and handling
 * - Section 9.3: BMCA algorithm and dataset updates
 * - Section 8.2.3: Parent Dataset
 * - Section 8.2.2: Current Dataset
 */

#include <cstdio>
#include <cstring>
#include "clocks.hpp"
#include "bmca.hpp"
#include "Common/utils/metrics.hpp"

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

// State change tracker
static int state_change_count = 0;
static PortState last_old_state = PortState::Initializing;
static PortState last_new_state = PortState::Initializing;
static void track_state_change(PortState old_state, PortState new_state) {
    state_change_count++;
    last_old_state = old_state;
    last_new_state = new_state;
}

static void stub_on_fault(const char*) {}

// Helper to create Announce message
static AnnounceMessage make_announce(
    std::uint8_t priority1,
    std::uint8_t clockClass,
    std::uint8_t clockAccuracy,
    std::uint16_t variance,
    std::uint8_t priority2,
    std::uint16_t stepsRemoved,
    std::uint64_t gmIdentity,
    std::uint64_t sourceClockId,
    std::uint16_t sourcePortNum,
    std::uint16_t sequenceId = 1)
{
    AnnounceMessage msg{};
    msg.header.transport_messageType = static_cast<std::uint8_t>(MessageType::Announce) & 0x0F;
    msg.header.reserved_version = 0x02;  // PTPv2 in lower nibble
    msg.header.messageLength = sizeof(AnnounceMessage);
    msg.header.domainNumber = 0;
    msg.header.sequenceId = sequenceId;
    
    for (int i = 0; i < 8; i++) {
        msg.header.sourcePortIdentity.clock_identity[i] = (sourceClockId >> (56 - i*8)) & 0xFF;
    }
    msg.header.sourcePortIdentity.port_number = sourcePortNum;
    
    msg.body.grandmasterPriority1 = priority1;
    msg.body.grandmasterClockClass = clockClass;
    msg.body.grandmasterClockAccuracy = clockAccuracy;
    msg.body.grandmasterClockVariance = variance;
    msg.body.grandmasterPriority2 = priority2;
    msg.body.stepsRemoved = stepsRemoved;
    
    for (int i = 0; i < 8; i++) {
        msg.body.grandmasterIdentity[i] = (gmIdentity >> (56 - i*8)) & 0xFF;
    }
    
    return msg;
}

int main() {
    std::printf("=== TEST-INT-Announce-Propagation (RED Phase) ===\n\n");
    
    int failures = 0;
    
    // Test 1: Sequential Announce processing with dataset updates
    {
        std::printf("--- Test 1: Sequential Announce messages update datasets correctly ---\n");
        
        state_change_count = 0;
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            track_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        OrdinaryClock clock(cfg, callbacks);
        clock.initialize();
        clock.start();
        
        // Announce 1: Foreign master A (priority1=120)
        auto announce1 = make_announce(120, 140, 0x25, 6000, 120, 2, 0xAAAAAAAAAAAAAAAAULL, 0xA000000000000001ULL, 1, 100);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &announce1, sizeof(AnnounceMessage), Types::Timestamp{});
        
        auto parent_ds_1 = clock.get_port().get_parent_data_set();
        std::uint8_t gm1 = parent_ds_1.grandmaster_priority1;
        
        // Announce 2: Foreign master B (priority1=115 - better)
        auto announce2 = make_announce(115, 135, 0x22, 5500, 115, 1, 0xBBBBBBBBBBBBBBBBULL, 0xB000000000000002ULL, 2, 101);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &announce2, sizeof(AnnounceMessage), Types::Timestamp{});
        
        auto parent_ds_2 = clock.get_port().get_parent_data_set();
        std::uint8_t gm2 = parent_ds_2.grandmaster_priority1;
        
        // Announce 3: Foreign master C (priority1=110 - even better)
        auto announce3 = make_announce(110, 130, 0x21, 5000, 110, 1, 0xCCCCCCCCCCCCCCCCULL, 0xC000000000000003ULL, 3, 102);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &announce3, sizeof(AnnounceMessage), Types::Timestamp{});
        
        auto parent_ds_3 = clock.get_port().get_parent_data_set();
        std::uint8_t gm3 = parent_ds_3.grandmaster_priority1;
        
        // Verify datasets reflect most recent BMCA winner (master C with priority1=110)
        bool gm_correct = true;
        for (int i = 0; i < 8; i++) {
            std::uint8_t expected = (0xCCCCCCCCCCCCCCCCULL >> (56 - i*8)) & 0xFF;
            if (parent_ds_3.grandmaster_identity[i] != expected) {
                gm_correct = false;
                break;
            }
        }
        
        if (!gm_correct || gm3 != 110 || parent_ds_3.grandmaster_clock_quality.clock_class != 130) {
            std::printf("[FAIL] Sequential announces did not update dataset correctly:\n");
            std::printf("  After announce 1: priority1=%u (expected <=120)\n", gm1);
            std::printf("  After announce 2: priority1=%u (expected <=115)\n", gm2);
            std::printf("  After announce 3: priority1=%u (expected 110)\n", gm3);
            std::printf("  Final GM identity correct: %s\n", gm_correct ? "yes" : "no");
            std::printf("  Final clockClass: %u (expected 130)\n", parent_ds_3.grandmaster_clock_quality.clock_class);
            failures++;
        } else {
            std::printf("[PASS] Test 1: Sequential announces updated datasets correctly\n");
        }
    }
    
    // Test 2: State transitions follow dataset changes
    {
        std::printf("\n--- Test 2: State transitions reflect dataset changes ---\n");
        
        state_change_count = 0;
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            track_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        OrdinaryClock clock(cfg, callbacks);
        clock.initialize();
        clock.start();
        
        int initial_transitions = state_change_count;
        
        // Send better foreign master - should trigger transition to slave
        auto better = make_announce(100, 128, 0x20, 5000, 100, 1, 0xDDDDDDDDDDDDDDDDULL, 0xD000000000000004ULL, 4);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &better, sizeof(AnnounceMessage), Types::Timestamp{});
        
        int transitions_after_better = state_change_count - initial_transitions;
        PortState state_after_better = clock.get_port().get_state();
        
        // Send worse foreign master - local should win and become master
        auto worse = make_announce(200, 248, 0xFE, 0xFFFF, 200, 5, 0xEEEEEEEEEEEEEEEEULL, 0xE000000000000005ULL, 5);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &worse, sizeof(AnnounceMessage), Types::Timestamp{});
        
        int total_transitions = state_change_count - initial_transitions;
        PortState final_state = clock.get_port().get_state();
        
        // Verify state transitions occurred
        bool slave_state_reached = (state_after_better == PortState::Uncalibrated || 
                                    state_after_better == PortState::Slave);
        bool master_state_reached = (final_state == PortState::PreMaster || 
                                     final_state == PortState::Master);
        
        if (!slave_state_reached || !master_state_reached || total_transitions < 2) {
            std::printf("[FAIL] State transitions did not follow dataset changes:\n");
            std::printf("  Transitions after better master: %d\n", transitions_after_better);
            std::printf("  Total transitions: %d (expected >= 2)\n", total_transitions);
            std::printf("  State after better: %u (expected Uncalibrated/Slave)\n", static_cast<unsigned>(state_after_better));
            std::printf("  Final state: %u (expected PreMaster/Master)\n", static_cast<unsigned>(final_state));
            failures++;
        } else {
            std::printf("[PASS] Test 2: State transitions followed dataset changes\n");
        }
    }
    
    // Test 3: BMCA metrics updated on dataset changes
    {
        std::printf("\n--- Test 3: BMCA metrics reflect dataset update operations ---\n");
        
        // Reset metrics
        Common::utils::metrics::reset();
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            track_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        OrdinaryClock clock(cfg, callbacks);
        clock.initialize();
        clock.start();
        
        std::uint64_t bmca_selections_before = Common::utils::metrics::get(
            Common::utils::metrics::CounterId::BMCA_Selections);
        
        // Process foreign master announce
        auto announce = make_announce(100, 128, 0x20, 5000, 100, 1, 0xFFFFFFFFFFFFFFFFULL, 0xF000000000000006ULL, 6);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &announce, sizeof(AnnounceMessage), Types::Timestamp{});
        
        std::uint64_t bmca_selections_after = Common::utils::metrics::get(
            Common::utils::metrics::CounterId::BMCA_Selections);
        std::uint64_t foreign_wins = Common::utils::metrics::get(
            Common::utils::metrics::CounterId::BMCA_ForeignWins);
        
        std::uint64_t selections_delta = bmca_selections_after - bmca_selections_before;
        
        if (selections_delta < 1 || foreign_wins < 1) {
            std::printf("[FAIL] BMCA metrics not updated:\n");
            std::printf("  BMCA selections delta: %llu (expected >= 1)\n", 
                       static_cast<unsigned long long>(selections_delta));
            std::printf("  Foreign wins: %llu (expected >= 1)\n", 
                       static_cast<unsigned long long>(foreign_wins));
            failures++;
        } else {
            std::printf("[PASS] Test 3: BMCA metrics updated correctly\n");
        }
    }
    
    // Test 4: Dataset consistency across multiple updates
    {
        std::printf("\n--- Test 4: Dataset consistency maintained across updates ---\n");
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            track_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        OrdinaryClock clock(cfg, callbacks);
        clock.initialize();
        clock.start();
        
        // Process master A
        auto masterA = make_announce(105, 130, 0x21, 4800, 105, 1, 0xABCDABCDABCDABCDULL, 0xA111111111111111ULL, 1);
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                             &masterA, sizeof(AnnounceMessage), Types::Timestamp{});
        
        auto parent_ds = clock.get_port().get_parent_data_set();
        auto current_ds = clock.get_port().get_current_data_set();
        
        // Verify all related fields are consistent
        bool parent_port_matches_source = true;
        for (int i = 0; i < 8; i++) {
            std::uint8_t expected = (0xA111111111111111ULL >> (56 - i*8)) & 0xFF;
            if (parent_ds.parent_port_identity.clock_identity[i] != expected) {
                parent_port_matches_source = false;
                break;
            }
        }
        
        bool gm_matches_announce = true;
        for (int i = 0; i < 8; i++) {
            std::uint8_t expected = (0xABCDABCDABCDABCDULL >> (56 - i*8)) & 0xFF;
            if (parent_ds.grandmaster_identity[i] != expected) {
                gm_matches_announce = false;
                break;
            }
        }
        
        // stepsRemoved should be announce value + 1
        bool steps_correct = (current_ds.steps_removed == 2);  // 1 + 1
        
        // All clock quality fields should match
        bool quality_consistent = (
            parent_ds.grandmaster_priority1 == 105 &&
            parent_ds.grandmaster_clock_quality.clock_class == 130 &&
            parent_ds.grandmaster_clock_quality.clock_accuracy == 0x21 &&
            parent_ds.grandmaster_clock_quality.offset_scaled_log_variance == 4800 &&
            parent_ds.grandmaster_priority2 == 105
        );
        
        if (!parent_port_matches_source || !gm_matches_announce || 
            !steps_correct || !quality_consistent) {
            std::printf("[FAIL] Dataset fields not consistent:\n");
            std::printf("  Parent port matches source: %s\n", parent_port_matches_source ? "yes" : "no");
            std::printf("  GM identity matches announce: %s\n", gm_matches_announce ? "yes" : "no");
            std::printf("  Steps removed correct: %s (got %u, expected 2)\n", 
                       steps_correct ? "yes" : "no", current_ds.steps_removed);
            std::printf("  Clock quality consistent: %s\n", quality_consistent ? "yes" : "no");
            failures++;
        } else {
            std::printf("[PASS] Test 4: Dataset consistency maintained\n");
        }
    }
    
    std::printf("\n=== TEST-INT-Announce-Propagation Summary ===\n");
    std::printf("Total integration tests: 4\n");
    std::printf("Failures: %d\n\n", failures);
    
    if (failures == 0) {
        std::printf("GREEN PHASE: All Announce propagation tests passed!\n");
        std::printf("Implementation correctly propagates Announce → BMCA → Datasets.\n");
        return 0;
    } else {
        std::printf("RED PHASE: Tests failing as expected.\n");
        std::printf("Implementation needed: Ensure Announce data flows through BMCA to datasets.\n");
        return failures;
    }
}
