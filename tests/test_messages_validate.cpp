// TEST: TEST-MSG-VALIDATE-001
// REQ Trace: REQ-F-001 (Message Type Support), REQ-NF-P-001 (Accuracy - validation speed)
// Purpose: Validate CommonHeader and Announce/Sync body field basic checks
// IEEE 1588-2019 Sections: 13.3 (Common Header), 13.5 (Announce), 13.6 (Sync)

// @satisfies STR-STD-002 - Message Format Correctness
// @satisfies STR-SEC-001 - Input Validation and Fuzzing
// @test-category: message-validation
// @test-priority: P0

#include <cstdio>
#include <cstdint>
#include "IEEE/1588/PTP/2019/messages.hpp"
#include "IEEE/1588/PTP/2019/types.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;

static int run_messages_validate() {
    // Construct a minimal valid PortIdentity
    PortIdentity pid{};
    pid.clock_identity = {0,1,2,3,4,5,6,7};
    pid.port_number = 1; // valid non-zero

    // Build Announce message
    AnnounceMessage announce{};
    announce.initialize(MessageType::Announce, DEFAULT_DOMAIN, pid);
    announce.body.grandmasterClockClass = 128;
    announce.body.grandmasterClockAccuracy = 0x20; // arbitrary
    announce.body.grandmasterClockVariance = 0; // network order neutral for 0
    announce.body.stepsRemoved = 0; // network order neutral for 0
    auto headerRes = announce.header.validate();
    if (!headerRes.isSuccess()) {
        std::fprintf(stderr, "FAIL: Announce header validation error %u\n", (unsigned)headerRes.getError());
        return 1;
    }
    auto bodyRes = announce.body.validate();
    if (!bodyRes.isSuccess()) {
        std::fprintf(stderr, "FAIL: Announce body validation error %u\n", (unsigned)bodyRes.getError());
        return 2;
    }

    // Build Sync message with valid timestamp
    SyncMessage sync{};
    sync.initialize(MessageType::Sync, DEFAULT_DOMAIN, pid);
    sync.body.originTimestamp.setTotalSeconds(10);
    sync.body.originTimestamp.nanoseconds = 999999999; // boundary valid value
    auto syncRes = sync.body.validate();
    if (!syncRes.isSuccess()) {
        std::fprintf(stderr, "FAIL: Sync body validation error %u\n", (unsigned)syncRes.getError());
        return 3;
    }

    // Negative test: invalid version in header
    CommonHeader badHeader = announce.header;
    badHeader.setVersion(3); // invalid version (spec uses v2)
    auto badRes = badHeader.validate();
    if (badRes.isSuccess()) {
        std::fprintf(stderr, "FAIL: Expected invalid version error\n");
        return 4;
    }

    std::puts("test_messages_validate: PASS");
    return 0;
}

int messages_main() { return run_messages_validate(); }
