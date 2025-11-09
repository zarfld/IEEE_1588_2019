// TEST: TEST-UNIT-BMCA-ForcedTie-Passive-Red
// Trace to: REQ-F-202 (Deterministic BMCA per gPTP constraints)
// Purpose: RED test — when a tie is forced via fault injection, expect PASSIVE recommendation.
// Current implementation only enters PASSIVE on true equality, not forced ties → should FAIL.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "clocks.hpp"
#include "Common/utils/fault_injection.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Timestamp now_ts{};
static PTPError noop_send_announce(const AnnounceMessage&) { return PTPError::Success; }
static PTPError noop_send_sync(const SyncMessage&) { return PTPError::Success; }
static PTPError noop_send_follow_up(const FollowUpMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_req(const DelayReqMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_resp(const DelayRespMessage&) { return PTPError::Success; }
static Timestamp get_timestamp_now() { return now_ts; }
static PTPError get_tx_timestamp(std::uint16_t, Timestamp* ts) { if (ts) *ts = now_ts; return PTPError::Success; }
static PTPError adjust_clock(std::int64_t) { return PTPError::Success; }
static PTPError adjust_frequency(double) { return PTPError::Success; }
static void on_state_change(PortState, PortState) {}
static void on_fault(const char*) {}

static PortIdentity make_foreign_id(std::uint8_t seed) {
    PortIdentity id{};
    for (size_t i = 0; i < id.clock_identity.size(); ++i) {
        id.clock_identity[i] = static_cast<std::uint8_t>(seed + i);
    }
    id.port_number = static_cast<std::uint16_t>(seed);
    return id;
}

int main() {
    // Arrange: minimal port in Listening state
    PortConfiguration cfg{};
    cfg.port_number = 1;
    cfg.domain_number = 0;
    cfg.announce_interval = 0;
    cfg.sync_interval = 0;
    cfg.announce_receipt_timeout = 3;

    StateCallbacks cbs{};
    cbs.send_announce = &noop_send_announce;
    cbs.send_sync = &noop_send_sync;
    cbs.send_follow_up = &noop_send_follow_up;
    cbs.send_delay_req = &noop_send_delay_req;
    cbs.send_delay_resp = &noop_send_delay_resp;
    cbs.get_timestamp = &get_timestamp_now;
    cbs.get_tx_timestamp = &get_tx_timestamp;
    cbs.adjust_clock = &adjust_clock;
    cbs.adjust_frequency = &adjust_frequency;
    cbs.on_state_change = &on_state_change;
    cbs.on_fault = &on_fault;

    PtpPort port(cfg, cbs);
    if (!port.initialize().is_success()) return 1;
    if (!port.start().is_success()) return 2;
    if (port.get_state() != PortState::Listening) return 3;

    // Build a foreign Announce different from local (so true equality is false)
    AnnounceMessage foreign{};
    foreign.initialize(MessageType::Announce, cfg.domain_number, make_foreign_id(0x10));
    foreign.body.grandmasterPriority1 = 127; // better than default 128
    foreign.body.grandmasterClockClass = 128; // arbitrary
    foreign.body.grandmasterClockAccuracy = 0x22; // arbitrary
    foreign.body.grandmasterClockVariance = 0x0100; // arbitrary
    foreign.body.grandmasterPriority2 = 127; // better than default 128
    foreign.body.stepsRemoved = 0; // equal steps baseline
    if (!port.process_announce(foreign).is_success()) return 4;

    // Force a BMCA tie on the next comparison
    Common::utils::fi::force_bmca_tie_next(1);

    // Act: tick to trigger BMCA
    now_ts.setTotalSeconds(0);
    now_ts.nanoseconds = 0;
    if (!port.tick(now_ts).is_success()) return 5;

    // Assert: Expected PASSIVE under forced tie signal
    if (port.get_state() != PortState::Passive) {
        std::fprintf(stderr, "[RED-BMCA] Expected PASSIVE under forced tie, got different state.\n");
        return 100; // RED: should fail with current implementation
    }
    std::puts("[RED-BMCA] PASS (unexpected): implementation already treats forced ties as passive");
    return 0;
}
