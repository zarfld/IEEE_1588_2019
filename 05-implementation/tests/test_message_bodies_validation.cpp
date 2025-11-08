/*
Test: TEST-UNIT-MSG-BODIES-VALIDATION
Phase: 05-implementation
Traceability:
  Design: DES-C-001  # Message format design
  Requirements: REQ-F-001  # PTP message types & validation
  Code: include/IEEE/1588/PTP/2019/messages.hpp
Notes: Validates message body error branches (Announce stepsRemoved, PdelayReq reserved, DelayResp/Sync/FollowUp timestamps).
*/

#include <cstdio>
#include <cstdint>
#include "IEEE/1588/PTP/2019/messages.hpp"

using namespace IEEE::_1588::PTP::_2019;

int main() {
    // AnnounceBody: stepsRemoved <= 255 is valid; >255 should fail
    {
        AnnounceBody a{};
        a.stepsRemoved = detail::host_to_be16(200);
        if (!a.validate().isSuccess()) {
            std::fprintf(stderr, "AnnounceBody valid stepsRemoved rejected\n");
            return 1;
        }

        a.stepsRemoved = detail::host_to_be16(300);
        auto r = a.validate();
        if (r.isSuccess() || r.getError() != Types::PTPError::INVALID_STEPS_REMOVED) {
            std::fprintf(stderr, "AnnounceBody invalid stepsRemoved not detected\n");
            return 2;
        }
    }

    // PdelayReqBody: any non-zero reserved byte should fail
    {
        PdelayReqBody p{};
        // default zero reserved passes
        if (!p.validate().isSuccess()) {
            std::fprintf(stderr, "PdelayReqBody zero reserved rejected\n");
            return 3;
        }
        // set one byte non-zero
        p.reserved[5] = 1;
        auto r = p.validate();
        if (r.isSuccess() || r.getError() != Types::PTPError::INVALID_RESERVED_FIELD) {
            std::fprintf(stderr, "PdelayReqBody invalid reserved not detected\n");
            return 4;
        }
    }

    // DelayRespBody: invalid timestamp should be detected
    {
        DelayRespBody d{};
        d.requestingPortIdentity.port_number = 1; // valid
        d.receiveTimestamp.setTotalSeconds(0);
        d.receiveTimestamp.nanoseconds = 1'000'000'000U; // invalid ns
        auto r = d.validate();
        if (r.isSuccess() || r.getError() != Types::PTPError::Invalid_Timestamp) {
            std::fprintf(stderr, "DelayRespBody invalid timestamp not detected\n");
            return 5;
        }
        // Fix timestamp to valid and ensure pass
        d.receiveTimestamp.nanoseconds = 999'999'999U;
        if (!d.validate().isSuccess()) {
            std::fprintf(stderr, "DelayRespBody valid case rejected\n");
            return 6;
        }
    }

    // SyncBody and FollowUpBody: invalid timestamp detection
    {
        SyncBody s{};
        s.originTimestamp.setTotalSeconds(0);
        s.originTimestamp.nanoseconds = 1'000'000'000U;
        if (s.validate().isSuccess()) {
            std::fprintf(stderr, "SyncBody invalid timestamp not detected\n");
            return 7;
        }
        s.originTimestamp.nanoseconds = 0;
        if (!s.validate().isSuccess()) {
            std::fprintf(stderr, "SyncBody valid timestamp rejected\n");
            return 8;
        }

        FollowUpBody f{};
        f.preciseOriginTimestamp.setTotalSeconds(0);
        f.preciseOriginTimestamp.nanoseconds = 1'000'000'000U;
        if (f.validate().isSuccess()) {
            std::fprintf(stderr, "FollowUpBody invalid timestamp not detected\n");
            return 9;
        }
        f.preciseOriginTimestamp.nanoseconds = 123;
        if (!f.validate().isSuccess()) {
            std::fprintf(stderr, "FollowUpBody valid timestamp rejected\n");
            return 10;
        }
    }

    return 0;
}
