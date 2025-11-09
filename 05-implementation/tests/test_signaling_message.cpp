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
    (void)pkt;
    std::fputs("[TDD RED] Signaling parsing/dispatch not implemented (CAP-20251109-04)\n", stderr);
    return 1; // non-zero => FAIL in ctest
}
