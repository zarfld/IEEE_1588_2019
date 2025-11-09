// TEST: TEST-UNIT-P2P-Delay-Red
// Trace to: REQ-F-204 (Peer-to-Peer delay mechanism for full-duplex links)
// Purpose: RED test — in P2P profile mode, E2E offset/delay updates must NOT be applied.
// Current implementation applies E2E path regardless of mode; this test should FAIL until fixed.

#include <cstdint>
#include <cstdio>
#include "clocks.hpp"

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

static Timestamp make_ns(uint64_t ns_total) {
    Timestamp t{};
    t.setTotalSeconds(ns_total / 1'000'000'000ULL);
    t.nanoseconds = static_cast<std::uint32_t>(ns_total % 1'000'000'000ULL);
    return t;
}

// Provide a small entry to be called via dispatcher in test_state_machine_basic
int p2p_delay_red_main();

int p2p_delay_red_main() {
    // Arrange: P2P mode enabled
    PortConfiguration cfg{};
    cfg.port_number = 1;
    cfg.domain_number = 0;
    cfg.announce_interval = 0; // 1s
    cfg.sync_interval = 0;     // 1s
    cfg.delay_req_interval = 0;
    cfg.announce_receipt_timeout = 3;
    cfg.delay_mechanism_p2p = true; // gPTP-style P2P

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

    // Act: Feed an E2E measurement sequence (Sync/Follow_Up + DelayReq/DelayResp)
    // In P2P mode, E2E updates MUST NOT update mean_path_delay.
    SyncMessage sync{}; sync.header.setMessageType(MessageType::Sync);
    FollowUpMessage fu{}; fu.header.setMessageType(MessageType::Follow_Up);
    fu.body.preciseOriginTimestamp = make_ns(1'000); // T1
    DelayReqMessage dreq{}; dreq.header.setMessageType(MessageType::Delay_Req);
    DelayRespMessage dr{}; dr.header.setMessageType(MessageType::Delay_Resp);
    dr.body.requestingPortIdentity = port.get_identity();

    // T2, T3, T4 such that E2E path would be positive
    if (!port.process_sync(sync, make_ns(2'000)).is_success()) return 3; // T2
    if (!port.process_follow_up(fu).is_success()) return 3;              // T1 inside FU
    if (!port.process_delay_req(dreq, make_ns(3'000)).is_success()) return 3; // T3
    dr.body.receiveTimestamp = make_ns(4'000); // T4
    if (!port.process_delay_resp(dr).is_success()) return 3; // would trigger E2E computation today

    // Assert: In P2P mode, mean_path_delay SHOULD remain zero until Pdelay path exists
    const auto& cds = port.get_current_data_set();
    const double mpd_ns = cds.mean_path_delay.toNanoseconds();
    if (mpd_ns != 0.0) {
        std::fprintf(stderr, "[RED-P2P] mean_path_delay updated via E2E in P2P mode: %.3f ns (expected 0)\n", mpd_ns);
        return 100; // RED: should fail with current implementation
    }
    std::puts("[RED-P2P] PASS (unexpected): implementation already protects against E2E in P2P");
    return 0;
}

// Also allow standalone execution for local debug
int main() { return p2p_delay_red_main(); }
