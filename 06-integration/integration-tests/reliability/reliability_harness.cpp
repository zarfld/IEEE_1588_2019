#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <random>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <set>
#include <utility>

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
    (void)rng;  // Suppress unused parameter warning (reserved for future use)
    namespace P = IEEE::_1588::PTP::_2019;
    using namespace P::Clocks;
    // Simulate receipt of Sync (T2) and Follow_Up (T1) and Delay Req/Resp (T3/T4)
    // Provide non-zero monotonically increasing timestamps to yield positive path delay
    P::Types::Timestamp t1{}; t1.nanoseconds = 0; // preciseOriginTimestamp (T1)
    P::Types::Timestamp t2{}; t2.nanoseconds = 1000; // Sync RX timestamp (T2)
    P::Types::Timestamp t3{}; t3.nanoseconds = 2000; // DelayReq TX timestamp (T3)
    P::Types::Timestamp t4{}; t4.nanoseconds = 3000; // DelayResp receive timestamp (T4)
    auto sync_msg = SyncMessage{};
    auto fu_msg = FollowUpMessage{}; fu_msg.body.preciseOriginTimestamp = t1;
    auto dreq_msg = DelayReqMessage{}; auto dresp_msg = DelayRespMessage{}; dresp_msg.body.receiveTimestamp = t4; dresp_msg.body.requestingPortIdentity = port.get_identity();
    // Sequence ensuring calculate_offset triggers inside follow_up
    port.process_sync(sync_msg, t2);
    port.process_delay_req(dreq_msg, t3);
    port.process_delay_resp(dresp_msg);
    port.process_follow_up(fu_msg);
    // After cycle, attempt tick to emit health and maybe transition states
    port.tick(t4);
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

// OP-004: Multi-port BoundaryClock routing (basic simulation)
struct BoundaryRoutingResult {
    bool passed;
    int severity;
    const char* state_port1;
    const char* state_port2;
    std::string message;
};

BoundaryRoutingResult run_boundary_routing(
    IEEE::_1588::PTP::_2019::Clocks::BoundaryClock &bc) {
    namespace P = IEEE::_1588::PTP::_2019;
    using namespace P::Clocks;
    P::Types::Timestamp ts{};
    // Simulate a Sync/Follow_Up/Delay cycle entering port 1 (master side)
    SyncMessage sync{}; FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = P::Types::Timestamp{};
    DelayReqMessage dreq{}; DelayRespMessage dresp{};
    auto r1 = bc.process_message(1, static_cast<uint8_t>(P::MessageType::Sync), &sync, sizeof(sync), ts);
    auto r2 = bc.process_message(1, static_cast<uint8_t>(P::MessageType::Follow_Up), &fu, sizeof(fu), ts);
    auto r3 = bc.process_message(1, static_cast<uint8_t>(P::MessageType::Delay_Req), &dreq, sizeof(dreq), ts);
    auto r4 = bc.process_message(1, static_cast<uint8_t>(P::MessageType::Delay_Resp), &dresp, sizeof(dresp), ts);
    bc.tick(ts);
    const PtpPort* p1 = bc.get_port(1);
    const PtpPort* p2 = bc.get_port(2);
    auto state1 = p1 ? p1->get_state() : P::Types::PortState::Faulty;
    auto state2 = p2 ? p2->get_state() : P::Types::PortState::Faulty;
    bool ok = r1.is_success() && r2.is_success() && r3.is_success() && r4.is_success() &&
              state1 != P::Types::PortState::Faulty && state2 != P::Types::PortState::Faulty;
    int severity = ok ? 1 : 6; // moderate severity on routing failure
    return {ok, severity, state_name(state1), state_name(state2), ok ? "Boundary routing ok" : "Boundary routing failure"};
}

// Weighted operation selection based on RTP example
struct WeightedOp { double weight; enum Kind { Offset, BMCA, Heartbeat, BoundaryRouting } kind; };

// Helper: deterministic state sweep to cover all states and transitions
// Drives the PtpPort through explicit events and a few offset cycles to reach SLAVE.
static TestResult run_state_sweep(
    IEEE::_1588::PTP::_2019::Clocks::PtpPort &port,
    std::set<IEEE::_1588::PTP::_2019::Types::PortState>& states_visited,
    std::unordered_map<IEEE::_1588::PTP::_2019::Types::PortState, std::size_t>& transitions_from,
    std::set<std::pair<IEEE::_1588::PTP::_2019::Types::PortState, IEEE::_1588::PTP::_2019::Types::PortState>>& edges_visited,
    IEEE::_1588::PTP::_2019::Types::PortState& previous_state
) {
    namespace P = IEEE::_1588::PTP::_2019;
    using P::Types::PortState;
    using P::Clocks::StateEvent;

    auto record = [&]() {
        PortState before = previous_state;
        PortState after = port.get_state();
        states_visited.insert(after);
        if (after != before) {
            transitions_from[before]++;
            edges_visited.insert({before, after});
        }
        previous_state = after;
    };

    // Reset to INITIALIZING
    port.initialize();
    previous_state = port.get_state();
    states_visited.insert(previous_state);

    // Initializing -> Listening
    port.process_event(StateEvent::INITIALIZE); record();

    // Return to Initializing then to Faulty, then back to Initializing
    port.initialize(); previous_state = port.get_state(); states_visited.insert(previous_state);
    port.process_event(StateEvent::FAULT_DETECTED); record(); // -> Faulty
    port.process_event(StateEvent::FAULT_CLEARED); record();        // -> Initializing

    // Initializing -> Disabled -> Listening
    port.process_event(StateEvent::DESIGNATED_DISABLED); record(); // -> Disabled
    port.process_event(StateEvent::DESIGNATED_ENABLED);  record();     // -> Listening

    // Listening -> PreMaster -> Master
    port.process_event(StateEvent::RS_MASTER);           record();     // -> PreMaster
    port.process_event(StateEvent::QUALIFICATION_TIMEOUT); record();   // -> Master

    // Master -> Uncalibrated, then to Passive
    port.process_event(StateEvent::RS_SLAVE);            record();        // -> Uncalibrated
    port.process_event(StateEvent::RS_PASSIVE);          record();  // -> Passive

    // Passive -> PreMaster
    port.process_event(StateEvent::RS_MASTER);           record();       // -> PreMaster

    // PreMaster -> Passive, then back to Listening via Uncalibrated path
    port.process_event(StateEvent::RS_PASSIVE);          record();     // -> Passive
    port.process_event(StateEvent::RS_SLAVE);            record();       // -> Uncalibrated

    // Uncalibrated -> Listening via SYNCHRONIZATION_FAULT
    port.process_event(StateEvent::SYNCHRONIZATION_FAULT); record();// -> Listening

    // Listening -> Uncalibrated (explicit path)
    port.process_event(StateEvent::RS_SLAVE);            record();     // -> Uncalibrated

    // Uncalibrated -> Listening via ANNOUNCE_RECEIPT_TIMEOUT
    port.process_event(StateEvent::ANNOUNCE_RECEIPT_TIMEOUT); record(); // -> Listening

    // Listening -> Faulty -> Initializing -> Listening
    port.process_event(StateEvent::FAULT_DETECTED);      record();     // -> Faulty
    port.process_event(StateEvent::FAULT_CLEARED);       record();        // -> Initializing
    port.process_event(StateEvent::INITIALIZE);          record();  // -> Listening

    // Listening -> Disabled (explicit) -> Listening
    port.process_event(StateEvent::DESIGNATED_DISABLED); record();     // -> Disabled
    port.process_event(StateEvent::DESIGNATED_ENABLED);  record();      // -> Listening

    // Direct Listening -> Passive edge (was missing in coverage) and return to Listening
    port.process_event(StateEvent::RS_PASSIVE);          record(); // -> Passive (Listening->Passive)
    port.process_event(StateEvent::RS_SLAVE);            record(); // -> Uncalibrated (Passive->Uncalibrated already covered)
    port.process_event(StateEvent::SYNCHRONIZATION_FAULT); record(); // -> Listening (Uncalibrated->Listening)

    // Listening -> PreMaster -> Master -> Uncalibrated and explicit PreMaster->Uncalibrated edge repeat to ensure coverage
    port.process_event(StateEvent::RS_GRAND_MASTER);     record();     // -> PreMaster
    port.process_event(StateEvent::QUALIFICATION_TIMEOUT); record();   // -> Master
    port.process_event(StateEvent::RS_SLAVE);            record();        // -> Uncalibrated
    // Drive back to PreMaster then explicitly to Uncalibrated to cover PreMaster->Uncalibrated which was missing
    port.process_event(StateEvent::RS_MASTER);           record();     // -> PreMaster
    port.process_event(StateEvent::RS_SLAVE);            record();     // -> Uncalibrated (PreMaster->Uncalibrated)

    // CRITICAL: Reset metrics to allow Uncalibrated -> Slave transition
    // The heuristic requires fails == 0, so clear any validation failures from earlier tests
    Common::utils::metrics::reset();
    
    // From UNCALIBRATED, perform offset cycles to achieve SLAVE (heuristic) with deterministic success.
    // Strategy: ensure have_sync_, have_follow_up_, have_delay_req_, have_delay_resp_ sequence repeated
    // until successful_offsets_in_window_ reaches threshold (>=3) then transition_to_state(Slave) occurs.
    // Use incrementing timestamps to ensure uniqueness across iterations
    // Pattern based on test_offset_calc_red.cpp but with monotonically increasing values
    for (int i=0;i<6;i++) {
        // T1 = master sends Sync at (1+i).000s
        // T2 = slave receives Sync at (1+i).100s (100ms later, includes offset + path delay)
        // T3 = slave sends Delay_Req at (2+i).000s 
        // T4 = master receives Delay_Req at (2+i).050s (50ms later, path delay only)
        // Expected: offset ~= 75ms, path_delay ~= 75ms (definitely positive)
        P::Types::Timestamp t1{};
        t1.setTotalSeconds(1 + i);
        t1.nanoseconds = 0;
        
        P::Types::Timestamp t2{};
        t2.setTotalSeconds(1 + i);
        t2.nanoseconds = 100'000'000;  // +100ms
        
        P::Types::Timestamp t3{};
        t3.setTotalSeconds(2 + i);
        t3.nanoseconds = 0;
        
        P::Types::Timestamp t4{};
        t4.setTotalSeconds(2 + i);
        t4.nanoseconds = 50'000'000;   // +50ms
        
        // CRITICAL: Re-initialize messages in each loop iteration
        // calculate_offset_and_delay() resets all have_* flags after calculation
        P::Clocks::SyncMessage sync{}; 
        sync.initialize(P::Types::MessageType::Sync, 0, port.get_identity());
        
        P::Clocks::FollowUpMessage fu{}; 
        fu.initialize(P::Types::MessageType::Follow_Up, 0, port.get_identity());
        fu.body.preciseOriginTimestamp = t1;
        
        P::Clocks::DelayReqMessage dreq{}; 
        dreq.initialize(P::Types::MessageType::Delay_Req, 0, port.get_identity());
        
        P::Clocks::DelayRespMessage dresp{}; 
        dresp.initialize(P::Types::MessageType::Delay_Resp, 0, port.get_identity());
        dresp.body.receiveTimestamp = t4; 
        dresp.body.requestingPortIdentity = port.get_identity();
        port.process_sync(sync, t2);
        port.process_delay_req(dreq, t3);
        port.process_delay_resp(dresp);
        port.process_follow_up(fu); // triggers offset calculation
        port.tick(t4);
        if (port.get_state() == PortState::Slave) {
            std::fprintf(stderr, "SLAVE REACHED after iteration %d!\n", i);
            break; // heuristic satisfied
        }
    }
    // Record potential UNCALIBRATED -> SLAVE transition (if occurred)
    record();
    if (port.get_state() != PortState::Slave) {
        // Second attempt window: force additional cycles with larger monotonic timestamps
        for (int i=0;i<6;i++) {
            P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(30000 + i*5000);
            P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
            P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
            P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
            P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
            P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
            port.process_sync(sync, t2);
            port.process_delay_req(dreq, t3);
            port.process_delay_resp(dresp);
            port.process_follow_up(fu);
            port.tick(t4);
            if (port.get_state() == PortState::Slave) break;
        }
    record();
    }

    // Test transitions from SLAVE state (complete coverage of Slave exit paths)
    // We should already be in SLAVE from the offset cycles above - verify and test all exits
    if (port.get_state() == PortState::Slave) {
        // Already in Slave - test all 4 exit transitions
        // 1. Slave -> PreMaster (via RS_MASTER)
        port.process_event(StateEvent::RS_MASTER);           record();     // -> PreMaster (Slave->PreMaster)
        
        // Return to Slave via Uncalibrated for next transition
        port.process_event(StateEvent::RS_SLAVE);            record();     // -> Uncalibrated
        for (int i=0;i<6;i++) {
            P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(200000 + i*5000);
            P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
            P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
            P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
            P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
            P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
            port.process_sync(sync, t2);
            port.process_delay_req(dreq, t3);
            port.process_delay_resp(dresp);
            port.process_follow_up(fu);
            port.tick(t4);
            if (port.get_state() == PortState::Slave) break;
        }
        record();
        
        // 2. Slave -> Passive (via RS_PASSIVE)
        if (port.get_state() == PortState::Slave) {
            port.process_event(StateEvent::RS_PASSIVE);          record();     // -> Passive (Slave->Passive)
        }
        
        // Return to Slave for next transition
        port.process_event(StateEvent::RS_SLAVE);            record();     // -> Uncalibrated
        for (int i=0;i<6;i++) {
            P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(250000 + i*5000);
            P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
            P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
            P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
            P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
            P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
            port.process_sync(sync, t2);
            port.process_delay_req(dreq, t3);
            port.process_delay_resp(dresp);
            port.process_follow_up(fu);
            port.tick(t4);
            if (port.get_state() == PortState::Slave) break;
        }
        record();
        
        // 3. Slave -> Uncalibrated (via SYNCHRONIZATION_FAULT)
        if (port.get_state() == PortState::Slave) {
            port.process_event(StateEvent::SYNCHRONIZATION_FAULT); record();   // -> Uncalibrated (Slave->Uncalibrated)
        }
        
        // Return to Slave for final transition
        for (int i=0;i<6;i++) {
            P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(300000 + i*5000);
            P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
            P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
            P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
            P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
            P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
            port.process_sync(sync, t2);
            port.process_delay_req(dreq, t3);
            port.process_delay_resp(dresp);
            port.process_follow_up(fu);
            port.tick(t4);
            if (port.get_state() == PortState::Slave) break;
        }
        record();
        
        // 4. Slave -> Listening (via ANNOUNCE_RECEIPT_TIMEOUT)
        if (port.get_state() == PortState::Slave) {
            port.process_event(StateEvent::ANNOUNCE_RECEIPT_TIMEOUT); record(); // -> Listening (Slave->Listening)
        }
    }

    // Regardless of reaching SLAVE above, also explicitly cover Master->Passive independently
    // Drive to Master, then RS_PASSIVE to Passive
    port.process_event(StateEvent::RS_MASTER);           record();     // -> PreMaster
    port.process_event(StateEvent::QUALIFICATION_TIMEOUT); record();   // -> Master
    port.process_event(StateEvent::RS_PASSIVE);          record();     // -> Passive (Master->Passive)
    // Return to Uncalibrated and Listening to continue
    port.process_event(StateEvent::RS_SLAVE);            record();     // -> Uncalibrated
    port.process_event(StateEvent::SYNCHRONIZATION_FAULT); record();   // -> Listening

    // Attempt again to reach SLAVE robustly and then create Slave->Passive edge explicitly
    port.process_event(StateEvent::RS_SLAVE);            record();     // -> Uncalibrated
    for (int i=0;i<10;i++) {
        P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(100000 + i*4000);
        P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
        P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
        P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
        P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
        P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
        port.process_sync(sync, t2);
        port.process_delay_req(dreq, t3);
        port.process_delay_resp(dresp);
        port.process_follow_up(fu);
        port.tick(t4);
        if (port.get_state() == PortState::Slave) break;
    }
    record();
    if (port.get_state() == PortState::Slave) {
        // Cover Slave->Listening
        port.process_event(StateEvent::ANNOUNCE_RECEIPT_TIMEOUT); record(); // -> Listening
        // Drive back to Slave deterministically
        port.process_event(StateEvent::RS_MASTER);           record();     // -> PreMaster
        port.process_event(StateEvent::QUALIFICATION_TIMEOUT); record();   // -> Master
        port.process_event(StateEvent::RS_SLAVE);            record();     // -> Uncalibrated
        for (int i=0;i<6;i++) {
            P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(140000 + i*4000);
            P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
            P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
            P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
            P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
            P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
            port.process_sync(sync, t2);
            port.process_delay_req(dreq, t3);
            port.process_delay_resp(dresp);
            port.process_follow_up(fu);
            port.tick(t4);
            if (port.get_state() == PortState::Slave) break;
        }
        record();
        if (port.get_state() == PortState::Slave) {
            // Cover Slave->Uncalibrated
            port.process_event(StateEvent::SYNCHRONIZATION_FAULT); record(); // -> Uncalibrated
            // Regain Slave again and cover Slave->Passive
            for (int i=0;i<6;i++) {
                P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(170000 + i*4000);
                P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
                P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
                P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
                P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
                P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
                port.process_sync(sync, t2);
                port.process_delay_req(dreq, t3);
                port.process_delay_resp(dresp);
                port.process_follow_up(fu);
                port.tick(t4);
                if (port.get_state() == PortState::Slave) break;
            }
            record();
            if (port.get_state() == PortState::Slave) {
                port.process_event(StateEvent::RS_PASSIVE);      record();     // -> Passive (Slave->Passive)
                // Return to Uncalibrated to keep subsequent logic stable
                port.process_event(StateEvent::RS_SLAVE);        record();     // -> Uncalibrated
            }
        }
    }

    // From SLAVE (if reached), cover edges to Listening, PreMaster, Uncalibrated, Passive
    PortState s = port.get_state();
    if (s == PortState::Slave) {
        // Direct edges from Slave
    port.process_event(StateEvent::ANNOUNCE_RECEIPT_TIMEOUT); record();       // -> Listening
        // Return to Slave again via sequence Listening->PreMaster->Master->Uncalibrated->Slave
    port.process_event(StateEvent::RS_MASTER);       record();      // -> PreMaster
    port.process_event(StateEvent::QUALIFICATION_TIMEOUT); record(); // -> Master
    port.process_event(StateEvent::RS_SLAVE);        record();         // -> Uncalibrated
        // Additional offset cycles to regain SLAVE (first window)
        for (int i=0;i<3;i++) {
            P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(50000 + i*4000);
            P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
            P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
            P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
            P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
            P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
            port.process_sync(sync, t2);
            port.process_delay_req(dreq, t3);
            port.process_delay_resp(dresp);
            port.process_follow_up(fu);
            port.tick(t4);
        }
    record(); // may become Slave inside follow_up
        if (port.get_state() != PortState::Slave) {
            // Second window to ensure Slave regained
            for (int i=0;i<4;i++) {
                P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(65000 + i*5000);
                P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
                P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
                P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
                P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
                P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
                port.process_sync(sync, t2);
                port.process_delay_req(dreq, t3);
                port.process_delay_resp(dresp);
                port.process_follow_up(fu);
                port.tick(t4);
                if (port.get_state() == PortState::Slave) break;
            }
            record();
        }
        // If Slave again, cover Slave->PreMaster and Slave->Uncalibrated edges deterministically
        if (port.get_state() == PortState::Slave) {
            // Slave -> PreMaster
            port.process_event(StateEvent::RS_MASTER);   record();          // -> PreMaster
            // Drive back to Slave (PreMaster->Master->Uncalibrated plus offset cycles)
            port.process_event(StateEvent::QUALIFICATION_TIMEOUT); record(); // -> Master
            port.process_event(StateEvent::RS_SLAVE);    record();         // -> Uncalibrated
            for (int i=0;i<3;i++) {
                P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(80000 + i*4000);
                P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
                P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
                P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
                P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
                P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4; dresp.body.requestingPortIdentity = port.get_identity();
                port.process_sync(sync, t2);
                port.process_delay_req(dreq, t3);
                port.process_delay_resp(dresp);
                port.process_follow_up(fu);
                port.tick(t4);
            }
            record();
        }
        if (port.get_state() == PortState::Slave) {
            // Slave -> Uncalibrated
            port.process_event(StateEvent::SYNCHRONIZATION_FAULT); record(); // -> Uncalibrated
            // Regain Slave again to proceed to Passive path
            for (int i=0;i<3;i++) {
                P::Types::Timestamp t1{}; t1.nanoseconds = static_cast<std::uint32_t>(120000 + i*4000);
                P::Types::Timestamp t2{}; t2.nanoseconds = t1.nanoseconds + 1000;
                P::Types::Timestamp t3{}; t3.nanoseconds = t2.nanoseconds + 1000;
                P::Types::Timestamp t4{}; t4.nanoseconds = t3.nanoseconds + 1000;
                P::Clocks::SyncMessage sync{}; P::Clocks::FollowUpMessage fu{}; fu.body.preciseOriginTimestamp = t1;
                P::Clocks::DelayReqMessage dreq{}; P::Clocks::DelayRespMessage dresp{}; dresp.body.receiveTimestamp = t4;
                port.process_sync(sync, t2);
                port.process_delay_req(dreq, t3);
                port.process_delay_resp(dresp);
                port.process_follow_up(fu);
                port.tick(t4);
            }
            record();
        }
        if (port.get_state() == PortState::Slave) {
            // Slave -> Passive via RS_PASSIVE
            port.process_event(StateEvent::RS_PASSIVE);  record();          // -> Passive
            // Passive -> PreMaster
            port.process_event(StateEvent::RS_MASTER);   record();        // -> PreMaster
            // PreMaster -> Master
            port.process_event(StateEvent::QUALIFICATION_TIMEOUT); record(); // -> Master
            // Master -> Uncalibrated
            port.process_event(StateEvent::RS_SLAVE);    record();         // -> Uncalibrated
            // Uncalibrated -> Passive
            port.process_event(StateEvent::RS_PASSIVE);  record();   // -> Passive
            // Passive -> Uncalibrated
            port.process_event(StateEvent::RS_SLAVE);    record();        // -> Uncalibrated
            // Uncalibrated -> Listening
            port.process_event(StateEvent::SYNCHRONIZATION_FAULT); record(); // -> Listening
            // Cover remaining missing edges explicitly (Master->Passive, Uncalibrated->PreMaster, Slave->Passive handled earlier).
            // Ensure Uncalibrated->PreMaster edge
            port.process_event(StateEvent::RS_MASTER);   record();      // -> PreMaster (from Uncalibrated)
            // Master->Passive edge via Master path
            port.process_event(StateEvent::QUALIFICATION_TIMEOUT); record(); // -> Master (already PreMaster->Master)
            port.process_event(StateEvent::RS_PASSIVE);  record();         // -> Passive (Master->Passive)
            // Passive -> PreMaster for completeness then back to Uncalibrated
            port.process_event(StateEvent::RS_MASTER);   record();        // -> PreMaster
            port.process_event(StateEvent::RS_SLAVE);    record();      // -> Uncalibrated (PreMaster->Uncalibrated already covered but repeat is harmless)
            // Final Uncalibrated -> PreMaster edge (redundant safety)
            port.process_event(StateEvent::RS_MASTER);   record();   // -> PreMaster
            port.process_event(StateEvent::RS_SLAVE);    record();      // -> Uncalibrated
        }
    }

    // Final check
    auto final_state = port.get_state();
    return {true, 1, "OP-005", state_name(final_state), "Deterministic state sweep completed"};
}

int main(int argc, char** argv) {
    namespace P = IEEE::_1588::PTP::_2019;
    using namespace P::Clocks;
    std::size_t iterations = 200;
    std::string csv_path = "srg_failures.csv";
    int injectCriticalPct = 0; // optional synthetic severity-10 injection percent (0-100)
    if (argc > 1) { iterations = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10)); }
    if (argc > 2) { csv_path = argv[2]; }
    if (argc > 3) { injectCriticalPct = std::max(0, std::min(100, std::atoi(argv[3]))); }

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

    // BoundaryClock configuration for OP-004 (2 ports: 1 master side, 2 slave side simulation)
    std::array<PortConfiguration, BoundaryClock::MAX_PORTS> bc_cfgs{};
    bc_cfgs[0].port_number = 1; bc_cfgs[0].version_number = 2; bc_cfgs[0].domain_number = 0;
    bc_cfgs[1].port_number = 2; bc_cfgs[1].version_number = 2; bc_cfgs[1].domain_number = 0;
    BoundaryClock boundary_clock(bc_cfgs, 2, callbacks);
    boundary_clock.initialize();
    boundary_clock.start();

    const std::vector<WeightedOp> ops = {
        {0.50, {WeightedOp::Offset}},
        {0.25, {WeightedOp::BMCA}},
        {0.15, {WeightedOp::Heartbeat}},
        {0.10, {WeightedOp::BoundaryRouting}}, // OP-004
    };

    auto start = std::chrono::steady_clock::now();
    std::size_t executed = 0; std::size_t passed = 0;

    // Coverage tracking (Phase 06 exit criteria: states + transitions)
    std::set<P::Types::PortState> states_visited;
    std::unordered_map<P::Types::PortState, std::size_t> transitions_from;
    std::set<std::pair<P::Types::PortState, P::Types::PortState>> edges_visited; // unique transitions
    P::Types::PortState previous_state = port.get_state();
    states_visited.insert(previous_state);

    // Execute OP-005: one-time deterministic state sweep before weighted loop
    {
        auto sweep = run_state_sweep(port, states_visited, transitions_from, edges_visited, previous_state);
        (void)sweep; // currently always passes; failures here would be logic bugs
    }

    // Operation usage counters (excluding OP-005 sweep)
    std::size_t op_count_offset = 0, op_count_bmca = 0, op_count_heartbeat = 0, op_count_boundary = 0;

    while (executed < iterations) {
        double r = dist(rng);
        WeightedOp::Kind kind;
    if (r < 0.50) { kind = WeightedOp::Offset; op_count_offset++; }
    else if (r < 0.75) { kind = WeightedOp::BMCA; op_count_bmca++; }
    else if (r < 0.90) { kind = WeightedOp::Heartbeat; op_count_heartbeat++; }
    else { kind = WeightedOp::BoundaryRouting; op_count_boundary++; }

        TestResult tr{true,1,"OP-000","Initializing",""};
        switch (kind) {
        case WeightedOp::Offset: tr = run_offset_cycle(port, rng); break;
        case WeightedOp::BMCA: tr = run_bmca_cycle(port); break;
        case WeightedOp::Heartbeat: tr = run_health_heartbeat(port); break;
        case WeightedOp::BoundaryRouting: {
            auto br = run_boundary_routing(boundary_clock);
            if (!br.passed) {
                auto now = std::chrono::steady_clock::now();
                double secs = std::chrono::duration_cast<std::chrono::duration<double>>(now - start).count();
                failures.push_back(FailureRecord{failures.size()+1, secs, br.severity, "OP-004", br.state_port1, false});
            }
            // Update coverage with boundary clock port states
            if (const PtpPort* p1 = boundary_clock.get_port(1)) {
                auto s1 = p1->get_state(); states_visited.insert(s1); transitions_from[s1]+=0; /* ensure key presence */ }
            if (const PtpPort* p2 = boundary_clock.get_port(2)) {
                auto s2 = p2->get_state(); states_visited.insert(s2); transitions_from[s2]+=0; }
            // No direct port state transition tracking between boundary clock ports for simplicity
            // Continue loop (do not treat as failure in main counters beyond failure record already added)
            if (!br.passed) { /* do not increment passed */ } else { passed++; }
            executed++;
            continue; // Skip common bookkeeping below (already handled)
        }
        }

        // Optional synthetic critical failure injection (per Phase 06 gate validation)
        if (injectCriticalPct > 0) {
            std::uniform_int_distribution<int> pcent(0, 99);
            if (pcent(rng) < injectCriticalPct) {
                auto now = std::chrono::steady_clock::now();
                double secs = std::chrono::duration_cast<std::chrono::duration<double>>(now - start).count();
                // Use current port state for state context
                auto s = port.get_state();
                failures.push_back(FailureRecord{failures.size()+1, secs, 10, "OP-999", state_name(s), false});
            }
        }

        // Update coverage metrics after operation (state may have changed via callbacks/state machine)
        P::Types::PortState current_state = port.get_state();
        if (current_state != previous_state) {
            transitions_from[previous_state]++;
            edges_visited.insert({previous_state, current_state});
            previous_state = current_state;
        }
        states_visited.insert(current_state);
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
    auto end = std::chrono::steady_clock::now();
    double durationSec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();

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

    // Write state/transition coverage CSV sibling to failures CSV
    // Derive coverage path by replacing filename portion if user passed custom path
    std::string coverage_path;
    {
        auto pos = csv_path.find_last_of("/\\");
        if (pos == std::string::npos) coverage_path = "state_transition_coverage.csv";
        else coverage_path = csv_path.substr(0, pos + 1) + "state_transition_coverage.csv";
    }
    const std::size_t TOTAL_STATES = 9; // PortState enum known count
    double stateCoveragePct = (static_cast<double>(states_visited.size()) / TOTAL_STATES) * 100.0;
    // Expected transitions per clocks.cpp state machine (+ heuristic Uncalibrated->Slave)
    const std::vector<std::pair<P::Types::PortState, P::Types::PortState>> expected_edges = {
        // Initializing
        {P::Types::PortState::Initializing, P::Types::PortState::Listening},
        {P::Types::PortState::Initializing, P::Types::PortState::Faulty},
        {P::Types::PortState::Initializing, P::Types::PortState::Disabled},
        // Faulty
        {P::Types::PortState::Faulty, P::Types::PortState::Initializing},
        // Disabled
        {P::Types::PortState::Disabled, P::Types::PortState::Listening},
        // Listening
        {P::Types::PortState::Listening, P::Types::PortState::PreMaster},
        {P::Types::PortState::Listening, P::Types::PortState::Uncalibrated},
        {P::Types::PortState::Listening, P::Types::PortState::Passive},
        {P::Types::PortState::Listening, P::Types::PortState::Faulty},
        {P::Types::PortState::Listening, P::Types::PortState::Disabled},
        // PreMaster
        {P::Types::PortState::PreMaster, P::Types::PortState::Master},
        {P::Types::PortState::PreMaster, P::Types::PortState::Uncalibrated},
        {P::Types::PortState::PreMaster, P::Types::PortState::Passive},
        // Master
        {P::Types::PortState::Master, P::Types::PortState::Uncalibrated},
        {P::Types::PortState::Master, P::Types::PortState::Passive},
        // Passive
        {P::Types::PortState::Passive, P::Types::PortState::PreMaster},
        {P::Types::PortState::Passive, P::Types::PortState::Uncalibrated},
        // Uncalibrated
        {P::Types::PortState::Uncalibrated, P::Types::PortState::PreMaster},
        {P::Types::PortState::Uncalibrated, P::Types::PortState::Passive},
        {P::Types::PortState::Uncalibrated, P::Types::PortState::Listening},
        {P::Types::PortState::Uncalibrated, P::Types::PortState::Slave}, // heuristic path
        // Slave
        {P::Types::PortState::Slave, P::Types::PortState::PreMaster},
        {P::Types::PortState::Slave, P::Types::PortState::Passive},
        {P::Types::PortState::Slave, P::Types::PortState::Uncalibrated},
        {P::Types::PortState::Slave, P::Types::PortState::Listening}
    };
    const std::size_t TOTAL_EDGES = expected_edges.size();
    // Compute edge coverage count
    std::size_t edges_hit = 0;
    for (const auto& e : expected_edges) {
        if (edges_visited.count(e)) edges_hit++;
    }
    double edgeCoveragePct = (static_cast<double>(edges_hit) / TOTAL_EDGES) * 100.0;
    if (edges_hit < TOTAL_EDGES) {
        // Print missing edges to stderr for diagnosis
        for (const auto& e : expected_edges) {
            if (!edges_visited.count(e)) {
                std::cerr << "MissingEdge: " << state_name(e.first) << "->" << state_name(e.second) << "\n";
            }
        }
    }
    // No synthetic edge injection: coverage must be achieved through real transitions
    std::ofstream cov(coverage_path.c_str(), std::ios::out | std::ios::trunc);
    if (cov.is_open()) {
        cov << "State,Visited,TransitionsFrom\n";
        // Emit all states for deterministic column completeness
        const P::Types::PortState allStates[TOTAL_STATES] = {
            P::Types::PortState::Initializing,
            P::Types::PortState::Listening,
            P::Types::PortState::PreMaster,
            P::Types::PortState::Master,
            P::Types::PortState::Passive,
            P::Types::PortState::Uncalibrated,
            P::Types::PortState::Slave,
            P::Types::PortState::Faulty,
            P::Types::PortState::Disabled
        };
        for (auto s : allStates) {
            cov << state_name(s) << ','
                << (states_visited.count(s) ? 1 : 0) << ','
                << (transitions_from.count(s) ? transitions_from[s] : 0) << '\n';
        }
        cov << "Summary,StatesCoveragePct," << stateCoveragePct << '\n';
        cov << "Summary,TotalTransitions," << [&]{ std::size_t total=0; for (auto &p: transitions_from) total+=p.second; return total; }() << '\n';
        cov << "Summary,EdgesVisited," << edges_hit << '\n';
        cov << "Summary,EdgesExpected," << TOTAL_EDGES << '\n';
        cov << "Summary,EdgesCoveragePct," << edgeCoveragePct << '\n';
    }
    std::cout << "Coverage CSV: " << coverage_path << "\n";

    // Append run history for trend checks (Phase 07)
    // Determine history path alongside failures CSV
    std::string history_path;
    {
        auto pos = csv_path.find_last_of("/\\");
        if (pos == std::string::npos) history_path = "reliability_history.csv";
        else history_path = csv_path.substr(0, pos + 1) + "reliability_history.csv";
    }
    bool writeHeader = false;
    {
        std::ifstream check(history_path.c_str(), std::ios::in | std::ios::binary);
        if (!check.good()) writeHeader = true; // file doesn't exist
    }
    std::ofstream hist(history_path.c_str(), std::ios::out | std::ios::app);
    if (hist.is_open()) {
        if (writeHeader) {
            hist << "RunTimestamp,Iterations,Passed,Failures,PassRate,MTBF,CriticalFailures,DurationSec\n";
        }
        // ISO-8601-ish timestamp (seconds precision) using system_clock
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
#if defined(_WIN32)
        struct tm tmBuf; localtime_s(&tmBuf, &t);
        char ts[32]; std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tmBuf);
#else
        char ts[32]; std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
#endif
        std::size_t criticalCount = 0; for (auto &f : failures) { if (f.severity == 10) criticalCount++; }
        hist << ts << ',' << iterations << ',' << passed << ',' << failures.size() << ','
             << passRate << ',' << mtbf << ',' << criticalCount << ',' << durationSec << '\n';
    }
    std::cout << "History CSV: " << history_path << "\n";

    // Operation usage conformance (±10% absolute tolerance), only if iterations >= 100
    std::vector<std::string> gate_reasons;
    if (iterations >= 100) {
        const double n = static_cast<double>(executed);
        const auto chk = [&](const char* name, double observed, double expected){
            double pct = (observed / n) * 100.0; double tol = 10.0;
            if (std::fabs(pct - expected) > tol) {
                gate_reasons.emplace_back(std::string("Usage weight ") + name + "=" + std::to_string(pct) + "% not within ±10% of " + std::to_string(expected) + "%");
            }
        };
        chk("OP-002", static_cast<double>(op_count_offset), 50.0);
        chk("OP-001", static_cast<double>(op_count_bmca), 25.0);
        chk("OP-003", static_cast<double>(op_count_heartbeat), 15.0);
        chk("OP-004", static_cast<double>(op_count_boundary), 10.0);
    }

    // Quality gate: pass rate >=95%, no severity=10 failures, and 100% coverage (states & edges)
    bool criticalPresent = false;
    for (auto &f : failures) { if (f.severity == 10) { criticalPresent = true; break; } }
    if (stateCoveragePct < 100.0) {
        gate_reasons.emplace_back("State coverage < 100%");
    }
    if (edgeCoveragePct < 100.0) {
        gate_reasons.emplace_back("Transition (edge) coverage < 100%");
    }
    if (passRate < 95.0) {
        gate_reasons.emplace_back("Pass rate < 95%");
    }
    if (criticalPresent) {
        gate_reasons.emplace_back("Critical failures present");
    }
    if (!gate_reasons.empty()) {
        std::cerr << "Reliability quality gate FAILED\n";
        for (auto &r : gate_reasons) { std::cerr << " - " << r << "\n"; }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
