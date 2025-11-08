/*
Test: TEST-UNIT-TIMEOUTS
Phase: 05-implementation
Traceability:
  Design: DES-C-010  # Time sync component
  Requirements: REQ-F-002  # BMCA state machine
  Code: src/clocks.cpp check_timeouts()
Notes: Validates announce receipt timeout detection and ANNOUNCE_RECEIPT_TIMEOUT event emission.
*/

#include <cstdio>
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
    cfg.announce_interval = 1;  // 2^1 = 2 seconds
    cfg.announce_receipt_timeout = 3;  // 3 intervals = 6 seconds total
    OrdinaryClock clock(cfg, cbs);
    if (!clock.initialize().is_success() || !clock.start().is_success()) return 100;

    // Force port to SLAVE state where announce timeout applies
    auto& port = const_cast<PtpPort&>(clock.get_port());
    port.process_event(StateEvent::RS_SLAVE);

    // Simulate time advance beyond announce timeout (6+ seconds)
    Types::Timestamp start_time{};
    start_time.setTotalSeconds(1000);
    Types::Timestamp timeout_time{};
    timeout_time.setTotalSeconds(1007);  // 7 seconds later (exceeds 6 second timeout)

    // Act: Tick with expired timeout
    auto res = port.tick(timeout_time);
    if (!res.is_success()) {
        std::fprintf(stderr, "tick with timeout failed\n");
        return 1;
    }

    // Assert: Timeout counter incremented
    auto stats = port.get_statistics();
    if (stats.announce_timeouts == 0) {
        std::fprintf(stderr, "Announce timeout counter not incremented\n");
        return 2;
    }

    std::printf("TEST-UNIT-TIMEOUTS PASS\n");
    return 0;
}
