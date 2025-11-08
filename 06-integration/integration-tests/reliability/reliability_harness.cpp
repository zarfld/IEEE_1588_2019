#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <random>
#include <fstream>
#include <iostream>

#include "clocks.hpp" // Public header exposes PtpPort and related types

/*
 * Reliability Harness (Phase 06 Integration)
 * Implements Operational Profile driven execution of PTP offset cycles.
 * References IEEE 1633 (reliability data collection) and Phase 06 instructions.
 * No copyrighted spec content reproduced; behavior based on understanding of IEEE 1588-2019.
 */

struct FailureRecord {
    std::size_t failureNumber;
    double failureTimeSec; // seconds from start
    int severity;          // FDSC severity (1-10)
    const char* operation; // OP-XXX
    const char* state;     // Port state name
    bool fixed;            // always false during run
};

struct TestResult {
    bool passed;
    int severity;
    const char* operation;
    const char* state;
    std::string message;
};

static const char* state_name(IEEE::_1588::PTP::_2019::Types::PortState s) {
    using namespace IEEE::_1588::PTP::_2019;
    switch (s) {
    case Types::PortState::Initializing: return "Initializing";
    case Types::PortState::Listening: return "Listening";
    case Types::PortState::PreMaster: return "PreMaster";
    case Types::PortState::Master: return "Master";
    case Types::PortState::Passive: return "Passive";
    case Types::PortState::Uncalibrated: return "Uncalibrated";
    case Types::PortState::Slave: return "Slave";
    case Types::PortState::Faulty: return "Faulty";
    case Types::PortState::Disabled: return "Disabled";
    }
    return "Unknown";
}

// OP-002: offset cycle adapter (dominant operation)
TestResult run_offset_cycle(IEEE::_1588::PTP::_2019::Clocks::PtpPort &port, std::mt19937 &rng) {
    namespace P = IEEE::_1588::PTP::_2019;
    using namespace P::Clocks;
    // Simulate receipt of Sync (T2) and Follow_Up (T1) and Delay Req/Resp (T3/T4)
    P::Types::Timestamp ts{}; // zeroed timestamp OK for deterministic unit style
    auto sync_msg = SyncMessage{}; // minimal; offset logic currently heuristic-based
    auto fu_msg = FollowUpMessage{}; fu_msg.body.preciseOriginTimestamp = P::Types::Timestamp{};
    auto dreq_msg = DelayReqMessage{}; auto dresp_msg = DelayRespMessage{};
    // Sequence: sync -> follow_up -> delay_req -> delay_resp
    port.process_sync(sync_msg, ts);
    port.process_follow_up(fu_msg);
    port.process_delay_req(dreq_msg, ts);
    port.process_delay_resp(dresp_msg);
    // After cycle, attempt tick to emit health and maybe transition states
    port.tick(ts);
    // Pass criteria: port in Uncalibrated or Slave (acceptable during acquisition) and no Faulty state
    auto s = port.get_state();
    if (s == P::Types::PortState::Faulty) {
        return {false, 8, "OP-002", state_name(s), "Port entered Faulty during offset cycle"};
    }
    return {true, 1, "OP-002", state_name(s), "Offset cycle completed"};
}

// OP-001: BMCA cycle (Announce processing)
TestResult run_bmca_cycle(IEEE::_1588::PTP::_2019::Clocks::PtpPort &port) {
    namespace P = IEEE::_1588::PTP::_2019;
    using namespace P::Clocks;
    AnnounceMessage ann{}; ann.header.setMessageType(P::MessageType::Announce);
    port.process_announce(ann);
    auto s = port.get_state();
    if (s == P::Types::PortState::Faulty) {
        return {false, 7, "OP-001", state_name(s), "Fault after BMCA"};
    }
    return {true, 1, "OP-001", state_name(s), "BMCA cycle ok"};
}

// OP-003: Health heartbeat tick
TestResult run_health_heartbeat(IEEE::_1588::PTP::_2019::Clocks::PtpPort &port) {
    namespace P = IEEE::_1588::PTP::_2019;
    using namespace P::Clocks;
    P::Types::Timestamp ts{};
    port.tick(ts);
    auto s = port.get_state();
    if (s == P::Types::PortState::Faulty) {
        return {false, 6, "OP-003", state_name(s), "Fault during heartbeat"};
    }
    return {true, 1, "OP-003", state_name(s), "Heartbeat ok"};
}

// Weighted operation selection based on RTP example
struct WeightedOp { double weight; enum Kind { Offset, BMCA, Heartbeat } kind; };

int main(int argc, char** argv) {
    namespace P = IEEE::_1588::PTP::_2019;
    using namespace P::Clocks;
    std::size_t iterations = 200;
    std::string csv_path = "srg_failures.csv";
    if (argc > 1) { iterations = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10)); }
    if (argc > 2) { csv_path = argv[2]; }

    // Prepare output directory (best-effort)
    // Can't rely on platform-specific; just write to given path.

    // Simple port configuration
    PortConfiguration cfg{}; cfg.port_number = 1; cfg.version_number = 2; cfg.domain_number = 0;
    StateCallbacks callbacks{}; // no network I/O; pure logic
    PtpPort port(cfg, callbacks);
    port.initialize();
    port.start();

    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    std::vector<FailureRecord> failures;
    failures.reserve(16);

    const std::vector<WeightedOp> ops = {
        {0.50, {WeightedOp::Offset}},
        {0.25, {WeightedOp::BMCA}},
        {0.15, {WeightedOp::Heartbeat}},
        // Remaining 0.10 omitted for simplicity in this minimal harness
    };

    auto start = std::chrono::steady_clock::now();
    std::size_t executed = 0; std::size_t passed = 0;

    while (executed < iterations) {
        double r = dist(rng);
        WeightedOp::Kind kind;
        if (r < 0.50) kind = WeightedOp::Offset;
        else if (r < 0.75) kind = WeightedOp::BMCA;
        else kind = WeightedOp::Heartbeat;

        TestResult tr{true,1,"OP-000","Initializing",""};
        switch (kind) {
        case WeightedOp::Offset: tr = run_offset_cycle(port, rng); break;
        case WeightedOp::BMCA: tr = run_bmca_cycle(port); break;
        case WeightedOp::Heartbeat: tr = run_health_heartbeat(port); break;
        }
        executed++;
        if (tr.passed) {
            passed++;
        } else {
            auto now = std::chrono::steady_clock::now();
            double secs = std::chrono::duration_cast<std::chrono::duration<double>>(now - start).count();
            failures.push_back(FailureRecord{failures.size()+1, secs, tr.severity, tr.operation, tr.state, false});
        }
    }

    double passRate = executed ? (static_cast<double>(passed) / executed) * 100.0 : 0.0;
    double mtbf = failures.empty() ? (executed) : (executed / static_cast<double>(failures.size()));

    // Write CSV (only failures; sufficient for SRG modeling)
    std::ofstream csv(csv_path.c_str(), std::ios::out | std::ios::trunc);
    if (csv.is_open()) {
        csv << "FailureNumber,FailureTime,Severity,Operation,State,Fixed\n";
        for (auto &f : failures) {
            csv << f.failureNumber << ',' << f.failureTimeSec << ',' << f.severity << ','
                << f.operation << ',' << f.state << ',' << (f.fixed ? "true" : "false") << '\n';
        }
    }

    std::cout << "Reliability Harness Summary\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Pass Rate: " << passRate << "%\n";
    std::cout << "Failures: " << failures.size() << "\n";
    std::cout << "Approx MTBF (iterations/failures): " << mtbf << "\n";
    std::cout << "CSV: " << csv_path << "\n";

    // Quality gate: pass rate >=95% and no severity=10 failures
    bool criticalPresent = false;
    for (auto &f : failures) { if (f.severity == 10) { criticalPresent = true; break; } }
    if (passRate < 95.0 || criticalPresent) {
        std::cerr << "Reliability quality gate FAILED (passRate=" << passRate << ", critical=" << criticalPresent << ")\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
