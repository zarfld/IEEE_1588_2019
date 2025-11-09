// Quick verification test to confirm foreign master pruning is working
// This test accesses internal state to verify GREEN implementation

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019::Clocks;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019;

// Simulated timestamp counter
static uint64_t time_ns = 0;

static Timestamp test_now() {
    Timestamp ts{};
    ts.setTotalSeconds(time_ns / 1'000'000'000ULL);
    ts.nanoseconds = static_cast<uint32_t>(time_ns % 1'000'000'000ULL);
    return ts;
}

static void advance_time_seconds(uint64_t seconds) {
    time_ns += seconds * 1'000'000'000ULL;
}

static AnnounceMessage make_announce(uint8_t master_id, uint8_t priority1, uint8_t domain) {
    AnnounceMessage msg{};
    msg.initialize(MessageType::Announce, domain, PortIdentity{});
    
    for (int i = 0; i < 7; ++i) {
        msg.header.sourcePortIdentity.clock_identity[i] = 0xAA;
    }
    msg.header.sourcePortIdentity.clock_identity[7] = master_id;
    msg.header.sourcePortIdentity.port_number = 1;
    
    msg.body.grandmasterPriority1 = priority1;
    msg.body.grandmasterPriority2 = 128;
    msg.body.grandmasterClockClass = 248;
    msg.body.grandmasterClockAccuracy = 0xFE;
    msg.body.grandmasterClockVariance = 0xFFFF;
    
    for (int i = 0; i < 8; ++i) {
        msg.body.grandmasterIdentity[i] = msg.header.sourcePortIdentity.clock_identity[i];
    }
    
    msg.body.stepsRemoved = 1;
    msg.body.currentUtcOffset = 37;
    msg.body.timeSource = 0xA0;
    msg.header.logMessageInterval = 1; // 2^1 = 2 seconds
    
    return msg;
}

int main() {
    std::printf("=== Foreign Master Pruning Verification ===\n\n");
    
    StateCallbacks cb{};
    cb.get_timestamp = &test_now;
    
    PortConfiguration cfg{};
    cfg.port_number = 1;
    cfg.domain_number = 0;
    cfg.announce_receipt_timeout = 3; // 3 × 2^1 = 6 second timeout
    
    time_ns = 10'000'000'000ULL; // T=10s
    
    PtpPort port(cfg, cb);
    port.initialize();
    port.start();
    
    // Transition to SLAVE to enable BMCA
    port.process_event(StateEvent::RS_SLAVE);
    
    // Add foreign master at T=10s
    std::printf("T=10s: Adding foreign master 0x30 (priority1=90)\n");
    AnnounceMessage announce = make_announce(0x30, 90, cfg.domain_number);
    auto result = port.process_announce(announce);
    std::printf("        process_announce result: %s\n", result.is_success() ? "SUCCESS" : "FAILED");
    std::printf("        Foreign master count: %d\n", port.get_statistics().announce_messages_received);
    
    // Check parent selection
    const auto& pds1 = port.get_parent_data_set();
    bool selected_initially = (pds1.parent_port_identity.clock_identity[7] == 0x30);
    std::printf("        Port state: %d\n", static_cast<int>(port.get_port_data_set().port_state));
    std::printf("        Parent port identity: ");
    for (int i = 0; i < 8; ++i) {
        std::printf("%02X", pds1.parent_port_identity.clock_identity[i]);
    }
    std::printf("\n");
    std::printf("        GM identity: ");
    for (int i = 0; i < 8; ++i) {
        std::printf("%02X", pds1.grandmaster_identity[i]);
    }
    std::printf("\n");
    std::printf("        Parent selected: %s\n", selected_initially ? "YES" : "NO");
    
    // Advance time 5 seconds (T=15s, within 6s timeout)
    advance_time_seconds(5);
    std::printf("\nT=15s: Advancing 5s (within 6s timeout)\n");
    Timestamp t = test_now();
    port.tick(t);
    
    const auto& pds2 = port.get_parent_data_set();
    bool still_selected_at_5s = (pds2.parent_port_identity.clock_identity[7] == 0x30);
    std::printf("        Parent still selected: %s\n", still_selected_at_5s ? "YES" : "NO");
    
    // Advance time 2 more seconds (T=17s, 7s elapsed > 6s timeout)
    advance_time_seconds(2);
    std::printf("\nT=17s: Advancing 2s more (total 7s > 6s timeout)\n");
    t = test_now();
    port.tick(t);
    
    const auto& pds3 = port.get_parent_data_set();
    bool still_selected_at_7s = (pds3.parent_port_identity.clock_identity[7] == 0x30);
    std::printf("        Parent still selected: %s\n", still_selected_at_7s ? "YES" : "NO");
    std::printf("        Parent clock ID last byte: 0x%02X\n", 
                pds3.parent_port_identity.clock_identity[7]);
    
    // Verify pruning worked
    std::printf("\n=== Results ===\n");
    if (selected_initially && still_selected_at_5s && !still_selected_at_7s) {
        std::printf("✓ PASS: Foreign master pruning working correctly!\n");
        std::printf("  - Initially selected at T=10s\n");
        std::printf("  - Still valid at T=15s (within timeout)\n");
        std::printf("  - Pruned at T=17s (exceeded timeout)\n");
        return 0;
    } else if (selected_initially && still_selected_at_5s && still_selected_at_7s) {
        std::printf("✗ FAIL: Foreign master NOT pruned after timeout\n");
        std::printf("  - Initially selected: YES\n");
        std::printf("  - Still valid at 5s: YES\n");
        std::printf("  - Still selected at 7s: YES (should be NO)\n");
        std::printf("  → Pruning logic not working\n");
        return 1;
    } else {
        std::printf("⚠ UNEXPECTED: Foreign master selection behavior unclear\n");
        std::printf("  - Initially selected: %s\n", selected_initially ? "YES" : "NO");
        std::printf("  - Still valid at 5s: %s\n", still_selected_at_5s ? "YES" : "NO");
        std::printf("  - Still selected at 7s: %s\n", still_selected_at_7s ? "YES" : "NO");
        return 2;
    }
}
