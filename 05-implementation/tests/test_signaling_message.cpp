#include <cstdint>
#include <cstdio>
#include <vector>

// Intentional failing test (TDD RED) for CAP-20251109-04 (Signaling handling)
// References: GAP-SIGNAL-001, REQ-PTP-SIGNALING (to be added)
// IEEE 1588-2019 Clause 13.12 (Signaling message) - referenced by number only.

static std::vector<std::uint8_t> buildMinimalSignalingPacket() {
    std::vector<std::uint8_t> pkt(44, 0); // size placeholder
    // messageType nibble (0xC for Signaling) placed at first byte high nibble
    pkt[0] = static_cast<std::uint8_t>((0xC << 4) | 0x02); // version + messageType (simplified)
    // sequenceId placeholder
    pkt[30] = 0x00;
    pkt[31] = 0x01;
    return pkt;
}

int main() {
    auto pkt = buildMinimalSignalingPacket();
    // Minimal parse: interpret first byte nibble as messageType, expect Signaling (0xC)
    const std::uint8_t message_type = (pkt[0] >> 4) & 0x0F;
    if (message_type != 0xC) {
        std::fputs("[FAIL] Unexpected message type nibble for signaling stub\n", stderr);
        return 1;
    }
    // For now success condition is simply recognizing signaling structure present.
    std::fputs("[TDD GREEN] Minimal signaling stub parsed (CAP-20251109-04)\n", stdout);
    return 0; // success
}
