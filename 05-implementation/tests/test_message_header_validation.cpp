// @satisfies STR-STD-002 - Message Format Correctness (header validation)
/*
Test: TEST-UNIT-MSG-HEADER-VALIDATION
Phase: 05-implementation
Traceability:
  Design: DES-C-001  # Message format design (assumed placeholder)
  Requirements: REQ-F-001  # PTP message types
  Code: include/IEEE/1588/PTP/2019/messages.hpp
Notes: Validates CommonHeader::validate error branches (version, length, reserved bits).
*/

#include <cstdio>
#include <cstdint>
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;

int main() {
    // Base valid header
    CommonHeader h{};
    h.setMessageType(MessageType::Announce);
    h.setVersion(2);
    h.messageLength = detail::host_to_be16(static_cast<uint16_t>(sizeof(CommonHeader))); // minimal valid
    h.domainNumber = 0;
    h.minorVersionPTP = 1;
    h.flagField = 0;
    h.correctionField = CorrectionField{};
    h.messageTypeSpecific = 0;
    for(auto &b: h.sourcePortIdentity.clock_identity) b = 0xAA; // arbitrary
    h.sourcePortIdentity.port_number = 1;
    h.sequenceId = detail::host_to_be16(1);
    h.controlField = 0xFF;
    h.logMessageInterval = 0;

    // Valid case
    if (!h.validate().isSuccess()) {
        std::fprintf(stderr, "Expected valid header to pass\n");
        return 1;
    }

    // Invalid version
    {
        CommonHeader v = h;
        v.setVersion(3);
        auto r = v.validate();
        if (r.isSuccess() || r.getError() != PTPError::INVALID_VERSION) {
            std::fprintf(stderr, "Version validation failed to detect error\n");
            return 2;
        }
    }

    // Invalid length (too small)
    {
        CommonHeader v = h;
        v.messageLength = detail::host_to_be16(static_cast<uint16_t>(sizeof(CommonHeader)-1));
        auto r = v.validate();
        if (r.isSuccess() || r.getError() != PTPError::INVALID_LENGTH) {
            std::fprintf(stderr, "Length validation failed to detect error\n");
            return 3;
        }
    }

    // Invalid reserved bits (upper nibble non-zero)
    {
        CommonHeader v = h;
        // reserved_version stores upper nibble reserved; set with high bits
        v.setVersion(2);
        v.reserved_version = 0xF0 | (v.reserved_version & 0x0F);
        auto r = v.validate();
        if (r.isSuccess() || r.getError() != PTPError::INVALID_RESERVED_FIELD) {
            std::fprintf(stderr, "Reserved field validation failed\n");
            return 4;
        }
    }

    return 0;
}
