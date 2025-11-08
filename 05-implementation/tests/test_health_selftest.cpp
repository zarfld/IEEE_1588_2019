/*
Test: TEST-UNIT-HealthSelfTest
Phase: 05-implementation
Traceability:
  Design: DES-I-007  # Health/self-test interface
  Requirements: REQ-NF-REL-004  # Health/self-test API
  Code: include/Common/utils/health.hpp, include/Common/utils/metrics.hpp
Notes: Verifies self_test aggregates metrics and records last values.
*/

#include <cstdio>
#include <cstdint>
#include "Common/utils/health.hpp"
#include "Common/utils/metrics.hpp"

using namespace Common::utils;

static void dummy_observer(const health::SelfTestReport&) {}

int main() {
    metrics::reset();
    health::set_observer(dummy_observer); // ensure no crash when emitting

    // Simulate some metrics
    metrics::increment(metrics::CounterId::OffsetsComputed, 3);
    metrics::increment(metrics::CounterId::BMCA_Selections, 2);
    health::record_offset_ns(123);
    health::record_bmca_selection(7);

    auto r = health::self_test();
    if (r.offsetsComputed != 3 || r.bmcaSelections != 2) {
        std::fprintf(stderr, "Unexpected counters in health report\n");
        return 1;
    }
    if (r.lastOffsetNanoseconds != 123 || r.lastBMCABestIndex != 7) {
        std::fprintf(stderr, "Unexpected last values in health report\n");
        return 2;
    }
    return 0;
}
