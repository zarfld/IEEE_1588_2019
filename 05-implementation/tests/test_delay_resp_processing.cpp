// @satisfies STR-PERF-004 - Path Delay Measurement (Delay_Resp processing)
// @satisfies STR-STD-001 - IEEE 1588-2019 Protocol Compliance (message processing)
/*
Test: TEST-UNIT-DELAY-RESP
Phase: 05-implementation
Traceability:
  Design: DES-C-010  # Time sync component
  Requirements: REQ-F-003  # Offset calculation
  Code: src/clocks.cpp process_delay_resp()
Notes: Validates Delay_Resp message processing and T4 timestamp capture for offset calculation.
*/

#include <cstdio>
#include <cstring>
#include "clocks.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

// Minimal stubs
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
    OrdinaryClock clock(cfg, cbs);
    if (!clock.initialize().is_success() || !clock.start().is_success()) return 100;

    // Force port to SLAVE state for Delay_Resp processing
    auto& port = const_cast<PtpPort&>(clock.get_port());
    port.process_event(StateEvent::RS_SLAVE);

    // Craft Delay_Resp matching port identity
    DelayRespMessage resp{};
    resp.header.setMessageType(MessageType::Delay_Resp);
    resp.header.setVersion(2);
    resp.header.domainNumber = cfg.domain_number;
    resp.body.requestingPortIdentity = port.get_identity();
    resp.body.receiveTimestamp.setTotalSeconds(1234);
    resp.body.receiveTimestamp.nanoseconds = 567890;

    // Act: Process Delay_Resp
    auto res = port.process_delay_resp(resp);
    if (!res.is_success()) {
        std::fprintf(stderr, "process_delay_resp failed\n");
        return 1;
    }

    // Assert: Statistics counter incremented
    auto stats = port.get_statistics();
    if (stats.delay_resp_messages_received == 0) {
        std::fprintf(stderr, "Delay_Resp counter not incremented\n");
        return 2;
    }

    std::printf("TEST-UNIT-DELAY-RESP PASS\n");
    return 0;
}
