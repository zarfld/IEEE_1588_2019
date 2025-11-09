// TEST: TEST-UNIT-DATASETS-ACCESSORS-RED
// Trace to: REQ-F-205 (Dataset/MIB management coherence)
// Purpose: RED test ensuring dataset accessors expose coherent snapshots after Announce and Sync sequences.
// Current implementation lacks explicit accessor validation tests; expected to FAIL initially if invariants break.

#include <cstdio>
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static PTPError noop_send_announce(const AnnounceMessage&) { return PTPError::Success; }
static PTPError noop_send_sync(const SyncMessage&) { return PTPError::Success; }
static PTPError noop_send_follow_up(const FollowUpMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_req(const DelayReqMessage&) { return PTPError::Success; }
static PTPError noop_send_delay_resp(const DelayRespMessage&) { return PTPError::Success; }
static Timestamp fake_now;
static Timestamp get_timestamp_now() { return fake_now; }
static PTPError get_tx_timestamp(std::uint16_t, Timestamp* ts) { if (ts) *ts = fake_now; return PTPError::Success; }
static PTPError adjust_clock(std::int64_t) { return PTPError::Success; }
static PTPError adjust_frequency(double) { return PTPError::Success; }
static void on_state_change(PortState, PortState) {}
static void on_fault(const char*) {}

int main() {
    // Arrange: Ordinary clock with deterministic config
    PortConfiguration cfg{}; cfg.port_number = 1; cfg.domain_number = 0; cfg.announce_interval = 0; cfg.sync_interval = 0;
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

    OrdinaryClock clock(cfg, cbs);
    if (!clock.initialize().is_success()) return 1;
    if (!clock.start().is_success()) return 2;

    auto& port = const_cast<PtpPort&>(clock.get_port());

    // Snapshot initial datasets
    const auto& portDS_initial = port.get_port_data_set();
    const auto& currentDS_initial = port.get_current_data_set();
    const auto& parentDS_initial = port.get_parent_data_set();

    // Basic invariants expected (documented for REQ-F-205 acceptance criteria refinement):
    bool invariant_ok = true;
    if (portDS_initial.port_state != PortState::Listening) invariant_ok = false;
    if (currentDS_initial.steps_removed != 0) invariant_ok = false;
    if (parentDS_initial.grandmaster_priority1 != 128) invariant_ok = false;
    if (!invariant_ok) {
        std::fprintf(stderr, "[DATASETS-RED] FAIL: Initial invariants violated before stimuli.\n");
        return 50; // Hard failure if starting state unexpected
    }

    // Stimuli: simulate one Announce then a Sync/Follow_Up + DelayReq/DelayResp sequence to mutate datasets
    AnnounceMessage announce{}; announce.initialize(MessageType::Announce, cfg.domain_number, port.get_identity());
    announce.body.grandmasterPriority1 = 127; // improved local priority triggers potential MASTER path
    auto announceResult = port.process_announce(announce);
    if (!announceResult.is_success()) {
        std::fprintf(stderr, "[DATASETS-RED] FAIL: Announce processing failed.\n");
        return 51;
    }

    // Simulate sync cycle timestamps
    fake_now.setTotalSeconds(0); fake_now.nanoseconds = 1000; // T2
    SyncMessage syncMsg{}; syncMsg.initialize(MessageType::Sync, cfg.domain_number, port.get_identity());
    port.process_sync(syncMsg, fake_now);
    FollowUpMessage fu{}; fu.initialize(MessageType::Follow_Up, cfg.domain_number, port.get_identity());
    fu.body.preciseOriginTimestamp.setTotalSeconds(0); fu.body.preciseOriginTimestamp.nanoseconds = 0; // T1
    port.process_follow_up(fu);
    // Delay request/response for E2E (P2P disabled)
    fake_now.nanoseconds = 2000; // T3
    DelayReqMessage dreq{}; dreq.initialize(MessageType::Delay_Req, cfg.domain_number, port.get_identity());
    port.process_delay_req(dreq, fake_now);
    fake_now.nanoseconds = 3000; // T4
    DelayRespMessage dr{}; dr.initialize(MessageType::Delay_Resp, cfg.domain_number, port.get_identity());
    dr.body.receiveTimestamp.nanoseconds = fake_now.nanoseconds;
    dr.body.requestingPortIdentity = port.get_identity();
    port.process_delay_resp(dr);

    // Post-stimulus datasets
    const auto& currentDS_post = port.get_current_data_set();
    const auto& parentDS_post = port.get_parent_data_set();

    // RED expectation: mean_path_delay should have been updated positive; if zero we mark FAIL.
    if (currentDS_post.mean_path_delay.toNanoseconds() <= 0.0) {
        std::fprintf(stderr, "[DATASETS-RED] FAIL: mean_path_delay not updated (>0 expected).\n");
        return 100; // RED failure until logic ensures dataset coherence
    }

    // Additional RED check: parent grandmaster identity should remain set (not all zeros)
    bool gm_zero = true; for (auto b : parentDS_post.grandmaster_identity) { if (b != 0) { gm_zero = false; break; } }
    if (gm_zero) {
        std::fprintf(stderr, "[DATASETS-RED] FAIL: grandmaster_identity unchanged after Announce sequence.\n");
        return 101;
    }

    // If both updated, treat as unexpected early pass (log for diagnostic)
    std::printf("[DATASETS-RED] PASS (unexpected): dataset coherence already achieved.\n");
    return 0;
}
