// @satisfies STR-SEC-002 - No Buffer Overruns (bounds checks for message handling)
// @satisfies STR-SEC-001 - Input Validation (invalid sizes rejected)
// Purpose: Negative tests feeding undersized/oversized buffers into OrdinaryClock/BoundaryClock
// Strategy: Call process_message with sizes smaller than required struct sizes and ensure INVALID_MESSAGE_SIZE
// NOTE: This is evidence of defensive API behavior, not full fuzzing (future: integrate fuzz harness)

#include <cstdio>
#include <cstdint>
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Timestamp now() { Timestamp t{}; return t; }
static PTPError noop_send(const AnnounceMessage&) { return PTPError::Success; }
static PTPError noop_sync(const SyncMessage&) { return PTPError::Success; }
static PTPError noop_follow(const FollowUpMessage&) { return PTPError::Success; }
static PTPError noop_dreq(const DelayReqMessage&) { return PTPError::Success; }
static PTPError noop_dresp(const DelayRespMessage&) { return PTPError::Success; }
static PTPError noop_tx(std::uint16_t, Timestamp*) { return PTPError::Success; }
static PTPError noop_adj(std::int64_t) { return PTPError::Success; }
static PTPError noop_freq(double) { return PTPError::Success; }
static void on_state(PortState, PortState) {}
static void on_fault(const char*) {}

int main() {
    StateCallbacks cbs{};
    cbs.send_announce = noop_send;
    cbs.send_sync = noop_sync;
    cbs.send_follow_up = noop_follow;
    cbs.send_delay_req = noop_dreq;
    cbs.send_delay_resp = noop_dresp;
    cbs.get_timestamp = now;
    cbs.get_tx_timestamp = noop_tx;
    cbs.adjust_clock = noop_adj;
    cbs.adjust_frequency = noop_freq;
    cbs.on_state_change = on_state;
    cbs.on_fault = on_fault;

    PortConfiguration cfg{};
    OrdinaryClock oc(cfg, cbs);
    if(!oc.initialize().is_success()) return 1;
    if(!oc.start().is_success()) return 2;

    // Craft a buffer smaller than AnnounceMessage
    unsigned char tiny[8]{}; // definitely smaller than any message
    auto r1 = oc.process_message(static_cast<std::uint8_t>(MessageType::Announce), tiny, sizeof(tiny), now());
    if(r1.get_error() != PTPError::INVALID_MESSAGE_SIZE) return 10;

    // Oversized but invalid type (simulate mismatch) using Delay_Resp with too small size
    unsigned char small_sync[sizeof(SyncMessage) - 5]{};
    auto r2 = oc.process_message(static_cast<std::uint8_t>(MessageType::Sync), small_sync, sizeof(small_sync), now());
    if(r2.get_error() != PTPError::INVALID_MESSAGE_SIZE) return 11;

    // Boundary clock path: invalid port number triggers INVALID_PORT
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> cfgs{};
    cfgs[0] = cfg; // only one active port
    BoundaryClock bc(cfgs, 1, cbs);
    if(!bc.initialize().is_success()) return 3;
    if(!bc.start().is_success()) return 4;

    auto r3 = bc.process_message(99 /* invalid port */, static_cast<std::uint8_t>(MessageType::Sync), tiny, sizeof(tiny), now());
    if(r3.get_error() != PTPError::INVALID_PORT) return 12;

    std::puts("no_buffer_overruns: PASS");
    return 0;
}
