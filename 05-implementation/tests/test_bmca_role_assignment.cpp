/*
Test: TEST-UNIT-BMCA-ROLE-ASSIGNMENT (GREEN increment)
Phase: 05-implementation
Traceability:
    Requirement: REQ-F-002 (BMCA state machine integration)
    Design: DES-C-003 (BMCA Component), DES-C-010 (Time Sync Component)
    CAP: CAP-20251108-BMCA-001
Goal: Validate run_bmca selects local master when local priority superior and increments role metrics.
Expected: LISTENING -> PRE_MASTER and BMCA_LocalWins incremented, BMCA_ForeignWins remains zero.
*/

#include "clocks.hpp"
#include "bmca.hpp"
#include <cstdio>

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;
using namespace IEEE::_1588::PTP::_2019::BMCA;

// Minimal stub callbacks (only those referenced in announce processing)
static Types::PTPError stub_send_announce(const AnnounceMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_sync(const SyncMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_follow_up(const FollowUpMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_req(const DelayReqMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_resp(const DelayRespMessage&) { return Types::PTPError::Success; }
static Types::Timestamp stub_get_ts() { return Types::Timestamp{}; }
static Types::PTPError stub_get_tx_ts(std::uint16_t, Types::Timestamp* t) { *t = Types::Timestamp{}; return Types::PTPError::Success; }
static Types::PTPError stub_adjust_clock(std::int64_t) { return Types::PTPError::Success; }
static Types::PTPError stub_adjust_freq(double) { return Types::PTPError::Success; }
static void stub_on_state_change(PortState old_s, PortState new_s) {
    std::printf("StateChange: %u -> %u\n", static_cast<unsigned>(old_s), static_cast<unsigned>(new_s));
}
static void stub_on_fault(const char* d) { std::fprintf(stderr, "Fault: %s\n", d); }

int main() {
    // Arrange: OrdinaryClock with default configuration (local clock intended to be better)
    StateCallbacks callbacks{ stub_send_announce, stub_send_sync, stub_send_follow_up, stub_send_delay_req, stub_send_delay_resp,
                              stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq, stub_on_state_change, stub_on_fault };
    PortConfiguration cfg{}; // defaults
    OrdinaryClock clock(cfg, callbacks);
    if (!clock.initialize().is_success() || !clock.start().is_success()) {
        std::fprintf(stderr, "Initialization failure\n");
        return 100;
    }
    const auto& port = clock.get_port();
    if (port.get_state() != PortState::Listening) {
        std::fprintf(stderr, "Precondition failure: expected LISTENING state\n");
        return 101;
    }

    // Craft foreign Announce with deliberately worse parameters (higher priority1, clockClass etc.)
    AnnounceMessage foreign{};
    foreign.header.setMessageType(MessageType::Announce);
    foreign.header.setVersion(2);
    foreign.header.messageLength = sizeof(AnnounceMessage);
    foreign.header.domainNumber = cfg.domain_number;
    foreign.header.sequenceId = 1;
    foreign.header.sourcePortIdentity.port_number = 2; // foreign port
    for (auto &b : foreign.header.sourcePortIdentity.clock_identity) { b = 0xEE; }
    foreign.body.grandmasterPriority1 = 250; // worse
    foreign.body.grandmasterClockClass = 250; // worse class
    foreign.body.grandmasterClockAccuracy = 0xFEFF; // exaggerated worse accuracy (simplified field)
    foreign.body.grandmasterClockVariance = 65000; // worse variance
    foreign.body.grandmasterPriority2 = 250; // worse
    foreign.body.stepsRemoved = 5; // more steps
    for (auto &b : foreign.body.grandmasterIdentity) { b = 0xDD; }

    // Act: Process foreign announce (triggers run_bmca internally)
    auto res = clock.process_message(static_cast<std::uint8_t>(MessageType::Announce), &foreign, sizeof(AnnounceMessage), Types::Timestamp{});
    if (!res.is_success()) {
        std::fprintf(stderr, "process_message returned error\n");
        return 102;
    }

    // Assert: Expect LISTENING -> PRE_MASTER (RS_MASTER) when local better than foreign.
    PortState new_state = clock.get_port().get_state();
    if (new_state != PortState::PreMaster) {
        std::fprintf(stderr, "BMCA role assignment failed: expected PreMaster got state=%u\n", static_cast<unsigned>(new_state));
        return 1;
    }
    // Metrics assertions
    auto selections = Common::utils::metrics::get(Common::utils::metrics::CounterId::BMCA_Selections);
    auto localWins = Common::utils::metrics::get(Common::utils::metrics::CounterId::BMCA_LocalWins);
    auto foreignWins = Common::utils::metrics::get(Common::utils::metrics::CounterId::BMCA_ForeignWins);
    if (selections == 0 || localWins == 0) {
        std::fprintf(stderr, "BMCA role assignment metrics missing (Selections=%llu LocalWins=%llu)\n",
            (unsigned long long)selections, (unsigned long long)localWins);
        return 2;
    }
    if (foreignWins != 0) {
        std::fprintf(stderr, "Unexpected foreignWins counter increment for local master scenario (foreignWins=%llu)\n",
            (unsigned long long)foreignWins);
        return 3;
    }
    std::puts("bmca_role_assignment_integration: PASS (local master selected)");
    return 0;
}
