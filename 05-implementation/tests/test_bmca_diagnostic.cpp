/*
Diagnostic test to understand BMCA selection with multiple foreign masters
*/

#include "clocks.hpp"
#include "bmca.hpp"
#include "Common/utils/metrics.hpp"
#include <cstdio>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;
using namespace IEEE::_1588::PTP::_2019::BMCA;

// Minimal stubs
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

static AnnounceMessage make_announce(
    std::uint8_t priority1,
    std::uint8_t clockClass,
    std::uint8_t clockAccuracy,
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
    std::printf("=== BMCA Diagnostic Test ===\n\n");
    
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
    
    // Print local clock parameters
    const auto& port = clock.get_port();
    const auto& parent_ds = port.get_parent_data_set();
    const auto& port_ds = port.get_port_data_set();
    
    std::printf("Local Clock Parameters:\n");
    std::printf("  priority1: %u\n", parent_ds.grandmaster_priority1);
    std::printf("  clockClass: %u\n", parent_ds.grandmaster_clock_quality.clock_class);
    std::printf("  clockAccuracy: 0x%02X\n", parent_ds.grandmaster_clock_quality.clock_accuracy);
    std::printf("  variance: %u\n", parent_ds.grandmaster_clock_quality.offset_scaled_log_variance);
    std::printf("  priority2: %u\n", parent_ds.grandmaster_priority2);
    std::printf("  clock_identity: ");
    for (auto b : port_ds.port_identity.clock_identity) {
        std::printf("%02X", b);
    }
    std::printf("\n  grandmaster_identity: ");
    for (auto b : parent_ds.grandmaster_identity) {
        std::printf("%02X", b);
    }
    std::printf("\n\n");
    
    // Create three foreign masters
    auto foreign_a = make_announce(150, 200, 0x30, 8000, 150, 3, 0x0000AAAA00000001ULL, 0, 1);
    auto foreign_b = make_announce(100, 128, 0x20, 5000, 100, 1, 0x0000BBBB00000002ULL, 0, 2);
    auto foreign_c = make_announce(200, 240, 0x50, 12000, 200, 5, 0x0000CCCC00000003ULL, 0, 3);
    
    std::printf("Foreign Master A: priority1=150, class=200\n");
    std::printf("Foreign Master B: priority1=100, class=128 (BEST)\n");
    std::printf("Foreign Master C: priority1=200, class=240\n\n");
    
    // Process announces
    std::printf("Processing Foreign Master A (priority1=150, class=200)...\n");
    clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                         &foreign_a, sizeof(AnnounceMessage), Types::Timestamp{});
    std::printf("Processing Foreign Master B (priority1=100, class=128)...\n");
    clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                         &foreign_b, sizeof(AnnounceMessage), Types::Timestamp{});
    std::printf("Processing Foreign Master C (priority1=200, class=240)...\n");
    clock.process_message(static_cast<std::uint8_t>(MessageType::Announce),
                         &foreign_c, sizeof(AnnounceMessage), Types::Timestamp{});
    std::printf("\n");
    
    PortState final_state = clock.get_port().get_state();
    std::printf("Final State: %u ", static_cast<unsigned>(final_state));
    
    const char* state_name = "UNKNOWN";
    switch (final_state) {
        case PortState::Listening: state_name = "LISTENING"; break;
        case PortState::PreMaster: state_name = "PRE_MASTER"; break;
        case PortState::Master: state_name = "MASTER"; break;
        case PortState::Passive: state_name = "PASSIVE"; break;
        case PortState::Uncalibrated: state_name = "UNCALIBRATED"; break;
        case PortState::Slave: state_name = "SLAVE"; break;
        default: break;
    }
    std::printf("(%s)\n\n", state_name);
    
    if (final_state == PortState::Uncalibrated || final_state == PortState::Slave) {
        std::printf("✓ CORRECT: Foreign master selected (best was priority1=100)\n");
        return 0;
    } else if (final_state == PortState::PreMaster || final_state == PortState::Master) {
        std::printf("✗ INCORRECT: Local clock selected despite worse parameters\n");
        std::printf("  Local priority1=%u, best foreign priority1=100\n", parent_ds.grandmaster_priority1);
        return 1;
    } else {
        std::printf("? UNEXPECTED STATE\n");
        return 2;
    }
}
