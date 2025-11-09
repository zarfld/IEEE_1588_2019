#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

// Intentional failing test (TDD RED) for CAP-20251109-04 (Signaling handling)
// References: GAP-SIGNAL-001, REQ-PTP-SIGNALING (to be added)
// IEEE 1588-2019 Clause 13.12 (Signaling message) - referenced by number only.

namespace {

// Minimal synthetic signaling PTP header bytes (not reproducing spec text; structure inferred)
static std::vector<std::uint8_t> buildMinimalSignalingPacket() {
    std::vector<std::uint8_t> pkt(44, 0); // size placeholder
    // messageType nibble (0xC for Signaling) placed at first byte high nibble
    pkt[0] = (0xC << 4) | 0x02; // version + messageType combination (simplified)
    // sequenceId placeholder
    pkt[30] = 0x00;
    pkt[31] = 0x01;
    return pkt;
}

// Placeholder for future parse API (expected to exist later)
// We declare a prototype to express desired contract; implementation absent => link failure avoided by using EXPECT_TRUE(false).
// Future signature suggestion:
// bool parse_signaling(const uint8_t* data, size_t len, SignalingMessage& out);

TEST(SignalingHandling, ParseAndDispatch_NotImplementedYet) {
    auto pkt = buildMinimalSignalingPacket();
    // Expected: future parser returns true and dispatches to handler registry.
    // Current: Feature not implemented; force RED state.
    ADD_FAILURE() << "Signaling parsing/dispatch not implemented (CAP-20251109-04).";
    // Keep assertion explicit for coverage accounting.
    EXPECT_TRUE(false);
}

} // namespace
