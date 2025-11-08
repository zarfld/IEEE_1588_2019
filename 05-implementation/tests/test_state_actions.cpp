/*
Test: TEST-UNIT-STATE-ACTIONS
Phase: 05-implementation
Traceability:
  Design: DES-C-010  # Time sync component
  Requirements: REQ-F-001  # Message transmission per state
  Code: src/clocks.cpp execute_state_actions()
Notes: Validates state-specific actions (Master sends Sync/Announce, Slave sends Delay_Req).
*/

#include <cstdio>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static int announce_sent = 0;
static int sync_sent = 0;
static int delay_req_sent = 0;

static Types::PTPError stub_send_announce(const AnnounceMessage&) { announce_sent++; return Types::PTPError::Success; }
static Types::PTPError stub_send_sync(const SyncMessage&) { sync_sent++; return Types::PTPError::Success; }
static Types::PTPError stub_send_follow_up(const FollowUpMessage&) { return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_req(const DelayReqMessage&) { delay_req_sent++; return Types::PTPError::Success; }
static Types::PTPError stub_send_delay_resp(const DelayRespMessage&) { return Types::PTPError::Success; }
static Types::Timestamp stub_get_ts() { 
    Types::Timestamp t{};
    t.setTotalSeconds(1000);
    return t;
}
static Types::PTPError stub_get_tx_ts(std::uint16_t, Types::Timestamp* t) { *t = stub_get_ts(); return Types::PTPError::Success; }
static Types::PTPError stub_adjust_clock(std::int64_t) { return Types::PTPError::Success; }
static Types::PTPError stub_adjust_freq(double) { return Types::PTPError::Success; }
static void stub_on_state_change(PortState, PortState) {}
static void stub_on_fault(const char*) {}

int main() {
    StateCallbacks cbs{ stub_send_announce, stub_send_sync, stub_send_follow_up, stub_send_delay_req, stub_send_delay_resp,
                        stub_get_ts, stub_get_tx_ts, stub_adjust_clock, stub_adjust_freq, stub_on_state_change, stub_on_fault };
    PortConfiguration cfg{};
    cfg.announce_interval = 0;  // 1 second
    cfg.sync_interval = 0;      // 1 second
    cfg.delay_req_interval = 0;  // 1 second
    
    OrdinaryClock clock(cfg, cbs);
    if (!clock.initialize().is_success() || !clock.start().is_success()) return 100;

    // Test Master state actions (sends Announce/Sync)
    auto& port = const_cast<PtpPort&>(clock.get_port());
    port.process_event(StateEvent::RS_MASTER);
    
    Types::Timestamp t{};
    t.setTotalSeconds(1002);  // Advance time to trigger interval
    port.tick(t);
    
    if (announce_sent == 0 || sync_sent == 0) {
        std::fprintf(stderr, "Master state actions not executed (announce=%d sync=%d)\n", announce_sent, sync_sent);
        return 1;
    }

    // Test Slave state actions (sends Delay_Req)
    announce_sent = sync_sent = delay_req_sent = 0;
    port.process_event(StateEvent::RS_SLAVE);
    
    t.setTotalSeconds(1004);
    port.tick(t);
    
    if (delay_req_sent == 0) {
        std::fprintf(stderr, "Slave state actions not executed (delay_req=%d)\n", delay_req_sent);
        return 2;
    }

    std::printf("TEST-UNIT-STATE-ACTIONS PASS\n");
    return 0;
}
