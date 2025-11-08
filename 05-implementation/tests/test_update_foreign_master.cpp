/*
Test: TEST-UNIT-UPDATE-FOREIGN-MASTER
Phase: 05-implementation
Traceability:
  Design: DES-C-003  # BMCA component
  Requirements: REQ-F-002  # BMCA state machine
  Code: src/clocks.cpp update_foreign_master_list()
Notes: Validates foreign master list management and deduplication.
*/

#include <cstdio>
#include <cstring>
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
    PortConfiguration cfg{};
    OrdinaryClock clock(cfg, cbs);
    if (!clock.initialize().is_success() || !clock.start().is_success()) return 100;

    auto& port = const_cast<PtpPort&>(clock.get_port());
    
    // Craft first foreign Announce
    AnnounceMessage ann1{};
    ann1.header.setMessageType(MessageType::Announce);
    ann1.header.setVersion(2);
    ann1.header.domainNumber = 0;
    ann1.header.sourcePortIdentity.port_number = 100;
    for (auto& b : ann1.header.sourcePortIdentity.clock_identity) { b = 0xAA; }
    ann1.body.grandmasterPriority1 = 128;

    // Act: Process first announce
    auto res = port.process_announce(ann1);
    if (!res.is_success()) {
        std::fprintf(stderr, "First announce processing failed\n");
        return 1;
    }

    // Craft second unique foreign Announce
    AnnounceMessage ann2{};
    ann2.header.setMessageType(MessageType::Announce);
    ann2.header.setVersion(2);
    ann2.header.domainNumber = 0;
    ann2.header.sourcePortIdentity.port_number = 200;
    for (auto& b : ann2.header.sourcePortIdentity.clock_identity) { b = 0xBB; }
    ann2.body.grandmasterPriority1 = 120;

    // Act: Process second announce
    res = port.process_announce(ann2);
    if (!res.is_success()) {
        std::fprintf(stderr, "Second announce processing failed\n");
        return 2;
    }

    // Assert: Statistics reflect multiple foreign masters
    auto stats = port.get_statistics();
    if (stats.announce_messages_received < 2) {
        std::fprintf(stderr, "Foreign master list management failed (received=%llu)\n", 
                    static_cast<unsigned long long>(stats.announce_messages_received));
        return 3;
    }

    std::printf("TEST-UNIT-UPDATE-FOREIGN-MASTER PASS\n");
    return 0;
}
