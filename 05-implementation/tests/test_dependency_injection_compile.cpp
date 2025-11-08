// @satisfies STR-PORT-002 - Reference HAL Implementations (evidence of DI friendly interfaces)
// @satisfies STR-PORT-001 - Hardware Abstraction Layer (constructing with mock callbacks)
// Purpose: Ensure we can instantiate clocks with mock callbacks only, proving decoupling.

#include <cstdio>
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static Timestamp ts() { Timestamp t{}; return t; }
static PTPError okA(const AnnounceMessage&) { return PTPError::Success; }
static PTPError okS(const SyncMessage&) { return PTPError::Success; }
static PTPError okF(const FollowUpMessage&) { return PTPError::Success; }
static PTPError okDRq(const DelayReqMessage&) { return PTPError::Success; }
static PTPError okDRp(const DelayRespMessage&) { return PTPError::Success; }
static PTPError okTx(std::uint16_t, Timestamp*) { return PTPError::Success; }
static PTPError okAdj(std::int64_t) { return PTPError::Success; }
static PTPError okFreq(double) { return PTPError::Success; }
static void stateCb(PortState, PortState) {}
static void faultCb(const char*) {}

int main(){
    StateCallbacks cbs{};
    cbs.send_announce = okA; cbs.send_sync = okS; cbs.send_follow_up = okF;
    cbs.send_delay_req = okDRq; cbs.send_delay_resp = okDRp;
    cbs.get_timestamp = ts; cbs.get_tx_timestamp = okTx;
    cbs.adjust_clock = okAdj; cbs.adjust_frequency = okFreq;
    cbs.on_state_change = stateCb; cbs.on_fault = faultCb;

    PortConfiguration cfg{}; cfg.port_number = 1;
    OrdinaryClock oc(cfg, cbs);
    if(!oc.initialize().is_success()) return 1;
    if(!oc.start().is_success()) return 2;

    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> cfgs{}; cfgs[0] = cfg;
    BoundaryClock bc(cfgs, 1, cbs);
    if(!bc.initialize().is_success()) return 3;
    if(!bc.start().is_success()) return 4;

    std::puts("dependency_injection_compile: PASS");
    return 0;
}
