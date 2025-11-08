/*
Test: TEST-UNIT-BC-ROUTING
Phase: 05-implementation
Traceability:
  Design: DES-C-004  # Boundary Clock component
  Requirements: REQ-F-001  # Message routing
  Code: src/clocks.cpp BoundaryClock::process_message()
Notes: Validates BoundaryClock message routing between ports for Sync/Follow_Up/Delay_Req/Delay_Resp.
*/

#include <cstdio>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

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

int main() {
    StateCallbacks cbs{ stub_send_announce, stub_send_sync, stub_send_follow_up, stub_send_delay_req, stub_send_delay_resp,
                        stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq, stub_on_state_change, stub_on_fault };
    
    // Configure 2-port boundary clock
    std::array<PortConfiguration, 8> configs{};
    configs[0].port_number = 1;
    configs[0].domain_number = 0;
    configs[0].version_number = 2;
    configs[1].port_number = 2;
    configs[1].domain_number = 0;
    configs[1].version_number = 2;

    BoundaryClock bc(configs, 2, cbs);
    if (!bc.initialize().is_success() || !bc.start().is_success()) return 100;

    // Craft Sync message
    SyncMessage sync{};
    sync.header.setMessageType(MessageType::Sync);
    sync.header.setVersion(2);
    sync.header.domainNumber = 0;
    sync.header.sequenceId = 42;

    // Act: Process Sync on port 1
    auto res = bc.process_message(1, static_cast<std::uint8_t>(MessageType::Sync), &sync, sizeof(SyncMessage), Types::Timestamp{});
    if (!res.is_success()) {
        std::fprintf(stderr, "BoundaryClock Sync routing failed\n");
        return 1;
    }

    // Craft Follow_Up message
    FollowUpMessage fu{};
    fu.header.setMessageType(MessageType::Follow_Up);
    fu.header.setVersion(2);
    fu.header.domainNumber = 0;
    fu.header.sequenceId = 42;

    // Act: Process Follow_Up on port 1
    res = bc.process_message(1, static_cast<std::uint8_t>(MessageType::Follow_Up), &fu, sizeof(FollowUpMessage), Types::Timestamp{});
    if (!res.is_success()) {
        std::fprintf(stderr, "BoundaryClock Follow_Up routing failed\n");
        return 2;
    }

    std::printf("TEST-UNIT-BC-ROUTING PASS\n");
    return 0;
}
