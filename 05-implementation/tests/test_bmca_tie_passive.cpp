/*
Test: TEST-UNIT-BMCA-TiePassive
Phase: 05-implementation
Traceability:
  Requirement: REQ-F-002 (BMCA state machine integration)
  Design: DES-C-003 (BMCA Component), DES-C-010 (Time Sync Component)
  CAP: CAP-20251108-02
Goal: When local and foreign priority vectors tie, port should recommend PASSIVE (RS_PASSIVE) from Listening state.
*/

#include "clocks.hpp"
#include "bmca.hpp"
#include "Common/utils/metrics.hpp"
#include <cstdio>

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
static void stub_on_state_change(PortState old_s, PortState new_s) {
    std::printf("StateChange: %u -> %u\n", static_cast<unsigned>(old_s), static_cast<unsigned>(new_s));
}
static void stub_on_fault(const char* d) { std::fprintf(stderr, "Fault: %s\n", d); }

int main() {
    Common::utils::metrics::reset();
    StateCallbacks callbacks{ stub_send_announce, stub_send_sync, stub_send_follow_up, stub_send_delay_req, stub_send_delay_resp,
                                      stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq, stub_on_state_change, stub_on_fault };
    PortConfiguration cfg{};
    OrdinaryClock clock(cfg, callbacks);
    if (!clock.initialize().is_success() || !clock.start().is_success()) {
        std::fprintf(stderr, "Initialization failure\n");
        return 100;
    }
    if (clock.get_port().get_state() != PortState::Listening) {
        std::fprintf(stderr, "Precondition failure: expected LISTENING state\n");
        return 101;
    }

    // Mirror local parent dataset into a foreign Announce to create an exact priority tie
    const auto &pds = clock.get_port().get_parent_data_set();
    AnnounceMessage foreign{};
    foreign.header.setMessageType(MessageType::Announce);
    foreign.header.setVersion(2);
    foreign.header.messageLength = sizeof(AnnounceMessage);
    foreign.header.domainNumber = cfg.domain_number;
    foreign.header.sequenceId = 1;
    foreign.header.sourcePortIdentity = clock.get_port().get_identity(); // copy then tweak port_number to ensure it's foreign
    foreign.header.sourcePortIdentity.port_number = static_cast<std::uint16_t>(foreign.header.sourcePortIdentity.port_number + 1);
    // Body mirrors local grandmaster fields
    foreign.body.grandmasterPriority1 = pds.grandmaster_priority1;
    foreign.body.grandmasterClockClass = pds.grandmaster_clock_quality.clock_class;
    foreign.body.grandmasterClockAccuracy = pds.grandmaster_clock_quality.clock_accuracy;
    foreign.body.grandmasterClockVariance = pds.grandmaster_clock_quality.offset_scaled_log_variance;
    foreign.body.grandmasterPriority2 = pds.grandmaster_priority2;
    foreign.body.grandmasterIdentity = pds.grandmaster_identity;
    foreign.body.stepsRemoved = static_cast<std::uint16_t>(clock.get_port().get_current_data_set().steps_removed); // typically 0

    // Act
    auto res = clock.process_message(static_cast<std::uint8_t>(MessageType::Announce), &foreign, sizeof(AnnounceMessage), Types::Timestamp{});
    if (!res.is_success()) {
        std::fprintf(stderr, "process_message returned error\n");
        return 102;
    }

    // Expect PASSIVE recommendation
    auto state = clock.get_port().get_state();
    if (state != PortState::Passive) {
        std::fprintf(stderr, "Expected PASSIVE state on tie, got %u\n", static_cast<unsigned>(state));
        return 1; // RED until tie handling implemented
    }

    // Verify passive win metric incremented
    auto passiveWins = Common::utils::metrics::get(Common::utils::metrics::CounterId::BMCA_PassiveWins);
    if (passiveWins == 0) {
        std::fprintf(stderr, "Passive wins counter not incremented (passiveWins=%llu)\n", (unsigned long long)passiveWins);
        return 2;
    }

    std::puts("bmca_tie_passive: PASS");
    return 0;
}
