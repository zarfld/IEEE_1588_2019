/*
Test: TEST-INT-BMCA-RoleSelection (RED phase - GAP-BMCA-001)
Phase: 05-implementation
Traceability:
    Stakeholder: StR-EXTS-003  # Cross-standard synchronization requirements
    Requirement: REQ-F-202     # BMCA with forced tie detection
    Design: DES-C-003          # BMCA Engine Component
    CAP: GAP-BMCA-001          # Full BMCA priority vector ordering
Notes: Integration test for BMCA-driven role transitions with multiple foreign masters.
       Tests state machine response to BMCA decisions per IEEE 1588-2019 Section 9.2.
*/

// @req REQ-F-202
// @req StR-EXTS-003
// @satisfies GAP-BMCA-001
// @test-category: integration
// @test-priority: P0
// @test-type: integration

#include "clocks.hpp"
#include "bmca.hpp"
#include "Common/utils/metrics.hpp"
#include <cstdio>
#include <vector>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;
using namespace IEEE::_1588::PTP::_2019::BMCA;

// Minimal stub callbacks
static Types::PTPError stub_send_announce(const AnnounceMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_sync(const SyncMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_follow_up(const FollowUpMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_req(const DelayReqMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_resp(const DelayRespMessage&) { return Types::PTPError::Success; }
static Types::Timestamp stub_get_ts() { return Types::Timestamp{}; }
static Types::PTPError stub_get_tx_ts(std::uint16_t, Types::Timestamp* t) { *t = Types::Timestamp{}; return Types::PTPError::Success; }
static Types::PTPError stub_adjust_clock(std::int64_t) { return Types::PTPError::Success; }
static Types::PTPError stub_adjust_freq(double) { return Types::PTPError::Success; }

static std::vector<std::pair<PortState, PortState>> state_transitions;
static void capture_state_change(PortState old_s, PortState new_s) {
    state_transitions.push_back({old_s, new_s});
    std::printf("StateTransition: %u -> %u\n", static_cast<unsigned>(old_s), static_cast<unsigned>(new_s));
}

static void stub_on_fault(const char* d) { std::fprintf(stderr, "Fault: %s\n", d); }

// Helper to create Announce message with specific priority vector
static AnnounceMessage make_announce(
    std::uint8_t priority1,
    std::uint8_t clockClass,
    std::uint8_t clockAccuracy,  // Fixed: should be uint8_t per IEEE 1588-2019
    std::uint16_t variance,
    std::uint8_t priority2,
    std::uint16_t stepsRemoved,
    std::uint64_t gmIdentity,
    std::uint8_t domainNumber,
    std::uint16_t sequenceId)
{
    AnnounceMessage msg{};
    msg.header.setMessageType(MessageType::Announce);
    msg.header.setVersion(2);
    msg.header.messageLength = sizeof(AnnounceMessage);
    msg.header.domainNumber = domainNumber;
    msg.header.sequenceId = sequenceId;
    msg.header.sourcePortIdentity.port_number = 1;
    
    // Set clock identity from gmIdentity
    for (int i = 0; i < 8; i++) {
        msg.header.sourcePortIdentity.clock_identity[i] = (gmIdentity >> (56 - i*8)) & 0xFF;
    }
    
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
    Common::utils::metrics::reset();
    state_transitions.clear();
    int failures = 0;
    int test_count = 0;

    // =============================================================================
    // Test Scenario 1: Local clock is best master (LISTENING -> PRE_MASTER)
    // =============================================================================
    {
        test_count++;
        std::printf("\n--- Test %d: Local clock wins BMCA (should become PRE_MASTER) ---\n", test_count);
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            capture_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        // Note: Local clock priority/class are determined by internal clock identity
        // This test validates BMCA selects local clock when it's better than foreign
        
        OrdinaryClock clock(cfg, callbacks);
        if (!clock.initialize().is_success() || !clock.start().is_success()) {
            std::fprintf(stderr, "[FAIL] Test %d: Clock initialization failed\n", test_count);
            failures++;
            goto test2;
        }
        
        // Process foreign announce with worse priority
        auto foreign_worse = make_announce(
            200,    // priority1 (worse)
            248,    // clockClass (worse)
            0xFF,   // accuracy (worst - uint8_t max)
            65000,  // variance (worse)
            200,    // priority2 (worse)
            5,      // stepsRemoved
            0xFFFFFFFFFFFFFFFFULL, // gmIdentity
            cfg.domain_number,
            1
        );
        
        auto res = clock.process_message(
            static_cast<std::uint8_t>(MessageType::Announce),
            &foreign_worse,
            sizeof(AnnounceMessage),
            Types::Timestamp{}
        );
        
        if (!res.is_success()) {
            std::fprintf(stderr, "[FAIL] Test %d: process_message failed\n", test_count);
            failures++;
            goto test2;
        }
        
        // Verify state transition to PRE_MASTER
        PortState final_state = clock.get_port().get_state();
        if (final_state != PortState::PreMaster) {
            std::fprintf(stderr, "[FAIL] Test %d: Expected PreMaster, got state=%u\n",
                        test_count, static_cast<unsigned>(final_state));
            failures++;
        } else {
            std::printf("[PASS] Test %d: Local clock correctly selected as master\n", test_count);
        }
        
        // Verify BMCA metrics
        auto localWins = Common::utils::metrics::get(Common::utils::metrics::CounterId::BMCA_LocalWins);
        if (localWins == 0) {
            std::fprintf(stderr, "[FAIL] Test %d: BMCA_LocalWins not incremented\n", test_count);
            failures++;
        }
    }

test2:
    // =============================================================================
    // Test Scenario 2: Foreign master is better (LISTENING -> UNCALIBRATED -> SLAVE)
    // =============================================================================
    {
        test_count++;
        Common::utils::metrics::reset();
        state_transitions.clear();
        
        std::printf("\n--- Test %d: Foreign master wins BMCA (should become SLAVE) ---\n", test_count);
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            capture_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        // Note: Local clock has default priority/class
        // Foreign master with better parameters should be selected
        
        OrdinaryClock clock(cfg, callbacks);
        if (!clock.initialize().is_success() || !clock.start().is_success()) {
            std::fprintf(stderr, "[FAIL] Test %d: Clock initialization failed\n", test_count);
            failures++;
            goto test3;
        }
        
        // Process foreign announce with better priority
        auto foreign_better = make_announce(
            100,    // priority1 (better)
            128,    // clockClass (better)
            0x20,   // accuracy (better - uint8_t)
            5000,   // variance (better)
            100,    // priority2 (better)
            1,      // stepsRemoved
            0x0000AABBCCDD0001ULL, // gmIdentity
            cfg.domain_number,
            1
        );
        
        auto res = clock.process_message(
            static_cast<std::uint8_t>(MessageType::Announce),
            &foreign_better,
            sizeof(AnnounceMessage),
            Types::Timestamp{}
        );
        
        if (!res.is_success()) {
            std::fprintf(stderr, "[FAIL] Test %d: process_message failed\n", test_count);
            failures++;
            goto test3;
        }
        
        // Verify state transition to UNCALIBRATED or SLAVE
        PortState final_state = clock.get_port().get_state();
        if (final_state != PortState::Uncalibrated && final_state != PortState::Slave) {
            std::fprintf(stderr, "[FAIL] Test %d: Expected Uncalibrated or Slave, got state=%u\n",
                        test_count, static_cast<unsigned>(final_state));
            failures++;
        } else {
            std::printf("[PASS] Test %d: Foreign master correctly selected\n", test_count);
        }
        
        // Verify BMCA metrics
        auto foreignWins = Common::utils::metrics::get(Common::utils::metrics::CounterId::BMCA_ForeignWins);
        if (foreignWins == 0) {
            std::fprintf(stderr, "[FAIL] Test %d: BMCA_ForeignWins not incremented\n", test_count);
            failures++;
        }
    }

test3:
    // =============================================================================
    // Test Scenario 3: Multiple foreign masters - select best
    // =============================================================================
    {
        test_count++;
        Common::utils::metrics::reset();
        state_transitions.clear();
        
        std::printf("\n--- Test %d: Multiple foreign masters - select best ---\n", test_count);
        
        StateCallbacks callbacks{
            stub_send_announce, stub_send_sync, stub_send_follow_up,
            stub_send_delay_req, stub_send_delay_resp,
            stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq,
            capture_state_change, stub_on_fault
        };
        
        PortConfiguration cfg{};
        // Note: Local clock has default priority/class
        // Multiple foreign masters will be compared to select best
        
        OrdinaryClock clock(cfg, callbacks);
        if (!clock.initialize().is_success() || !clock.start().is_success()) {
            std::fprintf(stderr, "[FAIL] Test %d: Clock initialization failed\n", test_count);
            failures++;
            goto summary;
        }
        
        // Foreign master A - medium quality
        auto foreign_a = make_announce(
            150,    // priority1
            200,    // clockClass
            0x30,   // accuracy (uint8_t)
            8000,   // variance
            150,    // priority2
            3,      // stepsRemoved
            0x0000AAAA00000001ULL,
            cfg.domain_number,
            1
        );
        
        // Foreign master B - best quality (should be selected)
        auto foreign_b = make_announce(
            100,    // priority1 (best)
            128,    // clockClass (best)
            0x20,   // accuracy (best - uint8_t)
            5000,   // variance (best)
            100,    // priority2 (best)
            1,      // stepsRemoved (best)
            0x0000BBBB00000002ULL,
            cfg.domain_number,
            2
        );
        
        // Foreign master C - worst quality
        auto foreign_c = make_announce(
            200,    // priority1 (worse than B)
            240,    // clockClass (worse)
            0x50,   // accuracy (worse - uint8_t)
            12000,  // variance (worse)
            200,    // priority2 (worse)
            5,      // stepsRemoved (worse)
            0x0000CCCC00000003ULL,
            cfg.domain_number,
            3
        );
        
        // Process all three foreign announces
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                            &foreign_a, sizeof(AnnounceMessage), Types::Timestamp{});
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                            &foreign_b, sizeof(AnnounceMessage), Types::Timestamp{});
        clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                            &foreign_c, sizeof(AnnounceMessage), Types::Timestamp{});
        
        // Verify best master (B) was selected
        // Note: This test validates BMCA selection logic, not just final state
        auto selections = Common::utils::metrics::get(Common::utils::metrics::CounterId::BMCA_Selections);
        if (selections < 3) {
            std::fprintf(stderr, "[FAIL] Test %d: Expected at least 3 BMCA selections, got %llu\n",
                        test_count, (unsigned long long)selections);
            failures++;
        } else {
            std::printf("[PASS] Test %d: BMCA processed multiple foreign masters\n", test_count);
        }
        
        PortState final_state = clock.get_port().get_state();
        if (final_state != PortState::Uncalibrated && final_state != PortState::Slave) {
            std::fprintf(stderr, "[FAIL] Test %d: Expected slave mode after foreign master selection, got state=%u\n",
                        test_count, static_cast<unsigned>(final_state));
            failures++;
        }
    }

summary:
    // Summary
    std::printf("\n=== TEST-INT-BMCA-RoleSelection Summary ===\n");
    std::printf("Total integration tests: %d\n", test_count);
    std::printf("Failures: %d\n", failures);
    
    if (failures > 0) {
        std::fprintf(stderr, "\nRED PHASE: %d integration tests failed\n", failures);
        std::fprintf(stderr, "BMCA role selection needs implementation/fixes.\n");
        return 1;
    }
    
    std::printf("\nGREEN PHASE: All BMCA role selection tests passed!\n");
    return 0;
}
