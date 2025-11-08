/*
Test: TEST-UNIT-FOREIGN-MASTER-OVERFLOW
Phase: 05-implementation
Traceability:
  Design: DES-I-034  # Foreign master list management
  Requirements: REQ-NF-REL-001 (Telemetry), REQ-NF-REL-002 (Guards)
Notes: Fills foreign master list to capacity and verifies overflow emits telemetry and returns Resource_Unavailable.
*/

#include <cstdio>
#include <cstdint>
#include <array>
#include <cstring>
#include "IEEE/1588/PTP/2019/types.hpp"
#include "IEEE/1588/PTP/2019/messages.hpp"
#include "clocks.hpp"
#include "Common/utils/metrics.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Clocks;

static void fill_identity(Types::PortIdentity& id, std::uint8_t base) {
    for (size_t i = 0; i < Types::CLOCK_IDENTITY_LENGTH; ++i) {
        id.clock_identity[i] = static_cast<std::uint8_t>(base + i);
    }
    id.port_number = static_cast<std::uint16_t>(base);
}

int main() {
    Common::utils::metrics::reset();

    // Minimal callbacks (all null/optional)
    StateCallbacks cbs{};
    cbs.get_timestamp = []() -> Types::Timestamp { return Types::Timestamp{}; };

    PortConfiguration cfg{};
    cfg.port_number = 1;

    PtpPort port(cfg, cbs);
    (void)port.initialize();

    // Prepare and insert MAX_FOREIGN_MASTERS distinct entries
    const std::size_t max_entries = 16; // matches PtpPort::MAX_FOREIGN_MASTERS
    for (std::size_t i = 0; i < max_entries; ++i) {
        AnnounceMessage msg{};
        msg.initialize(MessageType::Announce, cfg.domain_number, Types::PortIdentity{});
        fill_identity(msg.header.sourcePortIdentity, static_cast<std::uint8_t>(i+1));
        auto r = port.process_announce(msg);
        if (!r.is_success()) {
            std::fprintf(stderr, "Unexpected failure inserting foreign master %zu\n", i);
            return 1;
        }
    }

    auto beforeFailed = Common::utils::metrics::get(Common::utils::metrics::CounterId::ValidationsFailed);

    // One more should overflow
    AnnounceMessage overflow_msg{};
    overflow_msg.initialize(MessageType::Announce, cfg.domain_number, Types::PortIdentity{});
    fill_identity(overflow_msg.header.sourcePortIdentity, 0xEE);
    auto r = port.process_announce(overflow_msg);
    if (r.is_success()) {
        std::fprintf(stderr, "Expected overflow (Resource_Unavailable) but got success\n");
        return 2;
    }

    auto afterFailed = Common::utils::metrics::get(Common::utils::metrics::CounterId::ValidationsFailed);
    if (afterFailed < beforeFailed + 1) {
        std::fprintf(stderr, "Expected ValidationsFailed to increment on foreign master overflow (before=%llu after=%llu)\n",
                     static_cast<unsigned long long>(beforeFailed),
                     static_cast<unsigned long long>(afterFailed));
        return 3;
    }

    return 0;
}
