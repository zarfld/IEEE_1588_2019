// TEST: TEST-INTEG-BC-STARTSTOP-001
// Purpose: BoundaryClock multi-port start/stop coordination smoke test
// Traceability: Phase 06 Integration; verifies deterministic multi-port lifecycle
// Standards Context: Uses IEEE 1588-2019 state transitions (see Section 9.2 overview)

#include <cstdint>
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
static Timestamp get_timestamp_now() { return Timestamp{}; }
static PTPError get_tx_timestamp(std::uint16_t, Timestamp* ts) { if (ts) *ts = Timestamp{}; return PTPError::Success; }
static PTPError adjust_clock(std::int64_t) { return PTPError::Success; }
static PTPError adjust_frequency(double) { return PTPError::Success; }
static void on_state_change(PortState, PortState) {}
static void on_fault(const char*) {}

int main() {
    // Arrange: 2-port BoundaryClock configuration
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> cfgs{};
    cfgs[0].port_number = 1; cfgs[0].domain_number = 0; cfgs[0].announce_interval = 0; cfgs[0].sync_interval = 0;
    cfgs[1].port_number = 2; cfgs[1].domain_number = 0; cfgs[1].announce_interval = 0; cfgs[1].sync_interval = 0;

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

    BoundaryClock bc(cfgs, 2, cbs);

    // Act: initialize and start
    if (!bc.initialize().is_success()) return 1;
    if (bc.get_port_count() != 2) return 2;
    const PtpPort* p1 = bc.get_port(1);
    const PtpPort* p2 = bc.get_port(2);
    if (!p1 || !p2) return 3;
    if (p1->get_state() != PortState::Initializing) return 4;
    if (p2->get_state() != PortState::Initializing) return 5;

    if (!bc.start().is_success()) return 6;
    p1 = bc.get_port(1);
    p2 = bc.get_port(2);
    if (p1->get_state() != PortState::Listening) return 7;
    if (p2->get_state() != PortState::Listening) return 8;

    // Act: stop and verify both ports disable
    if (!bc.stop().is_success()) return 9;
    p1 = bc.get_port(1);
    p2 = bc.get_port(2);
    if (p1->get_state() != PortState::Disabled) return 10;
    if (p2->get_state() != PortState::Disabled) return 11;

    std::puts("boundary_clock_integration: PASS");
    return 0;
}
