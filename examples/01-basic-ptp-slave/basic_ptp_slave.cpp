/**
 * @file basic_ptp_slave.cpp
 * @brief Complete Basic PTP Slave Implementation Example
 * 
 * This example demonstrates a working PTP slave clock that:
 * 1. Discovers and selects a master clock (BMCA)
 * 2. Synchronizes time using Sync/Follow_Up messages
 * 3. Measures path delay using Delay_Req/Delay_Resp
 * 4. Adjusts local clock to match master
 * 
 * This is a SIMPLIFIED but COMPLETE demonstration of PTP slave operation
 * per IEEE 1588-2019. Production systems would add:
 * - Continuous operation loop
 * - Robust error handling
 * - State machine transitions
 * - Real network/timestamping hardware
 * 
 * @copyright Based on IEEE 1588-2019 concepts
 * @version 1.0.0
 * @date 2025-11-11
 */

#include "minimal_hal.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cmath>

using namespace Examples::MinimalHAL;

//============================================================================
// PTP Slave State Machine States (Simplified)
// Based on IEEE 1588-2019 Section 9.2
//============================================================================

enum class PTPState {
    INITIALIZING,  // Initial power-on state
    LISTENING,     // Listening for Announce messages
    UNCALIBRATED,  // Master selected, not yet synced
    SLAVE,         // Synchronized to master
    FAULTY         // Error condition
};

const char* state_to_string(PTPState state) {
    switch (state) {
        case PTPState::INITIALIZING: return "INITIALIZING";
        case PTPState::LISTENING: return "LISTENING";
        case PTPState::UNCALIBRATED: return "UNCALIBRATED";
        case PTPState::SLAVE: return "SLAVE";
        case PTPState::FAULTY: return "FAULTY";
        default: return "UNKNOWN";
    }
}

//============================================================================
// PTP Slave Clock Structure
//============================================================================

struct PTPSlaveClock {
    // Clock Identity (8 octets, typically derived from MAC address)
    std::uint8_t clock_identity[8];
    std::uint16_t port_number;
    
    // Current state
    PTPState state;
    
    // Selected master information
    std::uint8_t master_clock_identity[8];
    std::uint16_t master_port_number;
    bool has_master;
    
    // Synchronization data
    std::uint64_t last_sync_timestamp_ns;      // t1: Master's send time
    std::uint64_t last_sync_receive_time_ns;   // t2: Our receive time
    std::uint64_t last_delay_req_send_time_ns; // t3: Our Delay_Req send time
    std::uint64_t last_delay_resp_time_ns;     // t4: Master's receive time
    
    // Calculated offsets and delays
    std::int64_t time_offset_ns;   // Offset from master
    std::int64_t path_delay_ns;    // Network path delay
    bool offset_valid;
    bool delay_valid;
    
    // HAL interface
    MinimalHALSystem* hal;
};

//============================================================================
// BMCA (Best Master Clock Algorithm) - Simplified
// Based on IEEE 1588-2019 Section 9.3
//============================================================================

enum class BMCADecision {
    ACCEPT,   // Accept this master
    REJECT    // Reject this master
};

BMCADecision compare_master(const PTPMessage& announce, const PTPSlaveClock& self) {
    std::cout << "  → Best Master Clock Algorithm (BMCA) comparing...\n";
    
    // Compare Priority1 (lower is better)
    if (announce.priority1 < 128) {  // Our default priority
        std::cout << "     Master Priority1 (" << static_cast<int>(announce.priority1) 
                  << ") < Local (128) → ACCEPT\n";
        return BMCADecision::ACCEPT;
    }
    
    // Compare Clock Class (lower is better)
    // Class 6-7: Primary reference (atomic clock, GPS)
    // Class 13-14: ARB (disciplined by PTP)
    // Class 248: Default (unknown)
    if (announce.clock_class < 248) {
        std::cout << "     Master Class (" << static_cast<int>(announce.clock_class) 
                  << ") < Local (248) → ACCEPT\n";
        return BMCADecision::ACCEPT;
    }
    
    // Compare Clock Accuracy (lower is better)
    // 0x20 = 25ns, 0x21 = 100ns, 0x22 = 250ns, etc.
    if (announce.clock_accuracy < 0xFE) {  // 0xFE = Unknown
        std::cout << "     Master has known accuracy → ACCEPT\n";
        return BMCADecision::ACCEPT;
    }
    
    // Compare Priority2 (lower is better)
    if (announce.priority2 < 128) {  // Our default priority
        std::cout << "     Master Priority2 (" << static_cast<int>(announce.priority2) 
                  << ") < Local (128) → ACCEPT\n";
        return BMCADecision::ACCEPT;
    }
    
    // If all else equal, accept first master we see
    std::cout << "     No better master found, accepting current\n";
    return BMCADecision::ACCEPT;
}

//============================================================================
// PTP Message Processing Functions
//============================================================================

void process_announce_message(PTPSlaveClock& slave, const PTPMessage& msg) {
    std::cout << "\n[Step 1] Receiving Announce message from Master...\n";
    std::cout << "  Master Clock ID: ";
    for (int i = 0; i < 8; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned>(msg.clock_identity[i]);
        if (i < 7) std::cout << ":";
    }
    std::cout << std::dec << "\n";
    std::cout << "  Master Priority1: " << static_cast<int>(msg.priority1) << "\n";
    std::cout << "  Master Priority2: " << static_cast<int>(msg.priority2) << "\n";
    std::cout << "  Master Clock Class: " << static_cast<int>(msg.clock_class) << "\n";
    
    // Apply BMCA
    BMCADecision decision = compare_master(msg, slave);
    
    if (decision == BMCADecision::ACCEPT) {
        std::cout << "  → Best Master Clock Algorithm (BMCA) Result: ACCEPT\n";
        
        // Store master information
        std::memcpy(slave.master_clock_identity, msg.clock_identity, 8);
        slave.master_port_number = msg.port_number;
        slave.has_master = true;
        
        // Transition to UNCALIBRATED state
        if (slave.state == PTPState::LISTENING) {
            std::cout << "  → State Transition: LISTENING → UNCALIBRATED\n";
            slave.state = PTPState::UNCALIBRATED;
        }
    } else {
        std::cout << "  → Best Master Clock Algorithm (BMCA) Result: REJECT\n";
    }
}

void process_sync_message(PTPSlaveClock& slave, const PTPMessage& msg) {
    std::cout << "\n[Step 2] Receiving Sync message...\n";
    
    // t1: Time when master sent Sync (from message)
    std::uint64_t t1_master_send_ns = msg.timestamp_seconds * 1000000000ULL + msg.timestamp_nanoseconds;
    
    // t2: Time when we received Sync (capture timestamp)
    std::uint64_t t2_slave_receive_ns = slave.hal->timestamp().get_time_ns();
    
    std::cout << "  Sync Timestamp (t1): " << msg.timestamp_seconds << "." 
              << std::setw(9) << std::setfill('0') << msg.timestamp_nanoseconds << "\n";
    std::cout << "  Received at (t2): " << (t2_slave_receive_ns / 1000000000ULL) << "." 
              << std::setw(9) << std::setfill('0') << (t2_slave_receive_ns % 1000000000ULL) << "\n";
    
    // Calculate raw offset (not yet corrected for path delay)
    std::int64_t raw_offset = static_cast<std::int64_t>(t2_slave_receive_ns - t1_master_send_ns);
    
    std::cout << "  → Calculated raw offset: " << raw_offset << " ns\n";
    
    // Store for later correction
    slave.last_sync_timestamp_ns = t1_master_send_ns;
    slave.last_sync_receive_time_ns = t2_slave_receive_ns;
    slave.time_offset_ns = raw_offset;
}

void process_follow_up_message(PTPSlaveClock& slave, const PTPMessage& msg) {
    std::cout << "\n[Step 3] Receiving Follow_Up message...\n";
    
    // Follow_Up contains precise timestamp of Sync transmission
    // (Sync message may have contained less precise timestamp)
    std::uint64_t precise_t1_ns = msg.timestamp_seconds * 1000000000ULL + msg.timestamp_nanoseconds;
    
    std::cout << "  Precise Timestamp (t1): " << msg.timestamp_seconds << "." 
              << std::setw(9) << std::setfill('0') << msg.timestamp_nanoseconds << "\n";
    
    // Recalculate offset with precise timestamp
    std::int64_t precise_offset = static_cast<std::int64_t>(
        slave.last_sync_receive_time_ns - precise_t1_ns);
    
    slave.last_sync_timestamp_ns = precise_t1_ns;
    slave.time_offset_ns = precise_offset;
    slave.offset_valid = true;
    
    std::cout << "  → Updated offset calculation with precise timestamp\n";
}

void send_delay_request(PTPSlaveClock& slave) {
    std::cout << "\n[Step 4] Sending Delay_Req to measure path delay...\n";
    
    // t3: Time when we send Delay_Req (capture timestamp)
    std::uint64_t t3_send_ns = slave.hal->timestamp().get_time_ns();
    
    std::cout << "  Sent at (t3): " << (t3_send_ns / 1000000000ULL) << "." 
              << std::setw(9) << std::setfill('0') << (t3_send_ns % 1000000000ULL) << "\n";
    
    // Store for later path delay calculation
    slave.last_delay_req_send_time_ns = t3_send_ns;
    
    // Send packet (simulated)
    std::uint8_t packet[64] = {0};
    packet[0] = static_cast<std::uint8_t>(MessageType::DELAY_REQ);
    slave.hal->network().send_packet(packet, 64);
}

void process_delay_resp_message(PTPSlaveClock& slave, const PTPMessage& msg) {
    std::cout << "\n[Step 5] Receiving Delay_Resp from Master...\n";
    
    // t4: Time when master received our Delay_Req (from message)
    std::uint64_t t4_master_receive_ns = msg.timestamp_seconds * 1000000000ULL + msg.timestamp_nanoseconds;
    
    std::cout << "  Master received Delay_Req at (t4): " << msg.timestamp_seconds << "." 
              << std::setw(9) << std::setfill('0') << msg.timestamp_nanoseconds << "\n";
    
    // Calculate path delay:
    // path_delay = (t4 - t3)
    // This represents the time for Delay_Req to travel from slave to master
    std::int64_t path_delay = static_cast<std::int64_t>(
        t4_master_receive_ns - slave.last_delay_req_send_time_ns);
    
    std::cout << "  → Calculated path delay: " << path_delay << " ns (" 
              << (path_delay / 1000.0) << " μs)\n";
    
    slave.last_delay_resp_time_ns = t4_master_receive_ns;
    slave.path_delay_ns = path_delay;
    slave.delay_valid = true;
}

void calculate_and_apply_correction(PTPSlaveClock& slave) {
    std::cout << "\n[Synchronization Results]\n";
    
    if (!slave.offset_valid || !slave.delay_valid) {
        std::cout << "  ERROR: Incomplete synchronization data\n";
        return;
    }
    
    std::cout << "  Time Offset from Master: " << slave.time_offset_ns << " ns (" 
              << (slave.time_offset_ns / 1000000.0) << " ms)\n";
    std::cout << "  Path Delay: " << slave.path_delay_ns << " ns (" 
              << (slave.path_delay_ns / 1000.0) << " μs)\n";
    
    // Corrected offset accounts for one-way path delay
    // Assumes symmetric path (reasonable for LANs)
    std::int64_t one_way_delay = slave.path_delay_ns / 2;
    std::int64_t corrected_offset = slave.time_offset_ns - one_way_delay;
    
    std::cout << "  Corrected Offset: " << corrected_offset << " ns (" 
              << (corrected_offset / 1000000.0) << " ms)\n";
    std::cout << "  → Adjusting clock by: " << -corrected_offset << " ns\n";
    
    // Apply correction to local clock
    slave.hal->clock().adjust_clock(-corrected_offset);
    
    // Transition to SLAVE state
    if (slave.state == PTPState::UNCALIBRATED) {
        std::cout << "\nClock synchronized successfully!\n";
        std::cout << "Final State: SLAVE\n";
        slave.state = PTPState::SLAVE;
    }
}

//============================================================================
// Main Function - Demonstrating Complete Sync Sequence
//============================================================================

int main() {
    std::cout << "=====================================\n";
    std::cout << "  Basic PTP Slave Example\n";
    std::cout << "  IEEE 1588-2019 Implementation\n";
    std::cout << "=====================================\n\n";
    
    // Initialize HAL
    MinimalHALSystem hal;
    if (hal.initialize() != 0) {
        std::cerr << "ERROR: Failed to initialize HAL\n";
        return 1;
    }
    
    // Create PTP slave clock
    PTPSlaveClock slave{};
    slave.clock_identity[0] = 0x00;
    slave.clock_identity[1] = 0x11;
    slave.clock_identity[2] = 0x22;
    slave.clock_identity[3] = 0xFF;
    slave.clock_identity[4] = 0xFE;
    slave.clock_identity[5] = 0x33;
    slave.clock_identity[6] = 0x44;
    slave.clock_identity[7] = 0x55;
    slave.port_number = 1;
    slave.state = PTPState::INITIALIZING;
    slave.has_master = false;
    slave.offset_valid = false;
    slave.delay_valid = false;
    slave.hal = &hal;
    
    std::cout << "Initializing PTP Slave...\n";
    std::cout << "Clock Identity: ";
    for (int i = 0; i < 8; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned>(slave.clock_identity[i]);
        if (i < 7) std::cout << ":";
    }
    std::cout << std::dec << "\n";
    std::cout << "Port Number: " << slave.port_number << "\n";
    
    // Transition to LISTENING
    slave.state = PTPState::LISTENING;
    std::cout << "Initial Clock State: " << state_to_string(slave.state) << "\n";
    
    std::cout << "\n--- Starting Synchronization Sequence ---\n";
    
    // Simulate receiving Announce message from master
    PTPMessage announce{};
    announce.message_type = MessageType::ANNOUNCE;
    announce.clock_identity[0] = 0xAA;
    announce.clock_identity[1] = 0xBB;
    announce.clock_identity[2] = 0xCC;
    announce.clock_identity[3] = 0xFF;
    announce.clock_identity[4] = 0xFE;
    announce.clock_identity[5] = 0xDD;
    announce.clock_identity[6] = 0xEE;
    announce.clock_identity[7] = 0xFF;
    announce.port_number = 1;
    announce.priority1 = 128;
    announce.priority2 = 128;
    announce.clock_class = 248;
    announce.clock_accuracy = 0x21;  // Within 100ns
    announce.offset_scaled_log_variance = 0x4E5D;
    
    hal.network().simulate_receive(announce);
    process_announce_message(slave, announce);
    
    // Simulate receiving Sync message
    hal.timestamp().set_simulated_time(1699564800501234567ULL);  // Slave receive time (t2)
    
    PTPMessage sync{};
    sync.message_type = MessageType::SYNC;
    std::memcpy(sync.clock_identity, announce.clock_identity, 8);
    sync.port_number = 1;
    sync.timestamp_seconds = 1699564800;
    sync.timestamp_nanoseconds = 500000000;  // Master send time (t1)
    
    hal.network().simulate_receive(sync);
    process_sync_message(slave, sync);
    
    // Simulate receiving Follow_Up message (with precise timestamp)
    PTPMessage follow_up{};
    follow_up.message_type = MessageType::FOLLOW_UP;
    std::memcpy(follow_up.clock_identity, announce.clock_identity, 8);
    follow_up.port_number = 1;
    follow_up.timestamp_seconds = 1699564800;
    follow_up.timestamp_nanoseconds = 500000000;  // Precise t1
    
    hal.network().simulate_receive(follow_up);
    process_follow_up_message(slave, follow_up);
    
    // Send Delay_Req
    hal.timestamp().set_simulated_time(1699564800502000000ULL);  // Slave send time (t3)
    send_delay_request(slave);
    
    // Simulate receiving Delay_Resp
    PTPMessage delay_resp{};
    delay_resp.message_type = MessageType::DELAY_RESP;
    std::memcpy(delay_resp.clock_identity, announce.clock_identity, 8);
    delay_resp.port_number = 1;
    delay_resp.timestamp_seconds = 1699564800;
    delay_resp.timestamp_nanoseconds = 502050000;  // Master receive time (t4)
    
    hal.network().simulate_receive(delay_resp);
    process_delay_resp_message(slave, delay_resp);
    
    // Calculate and apply correction
    calculate_and_apply_correction(slave);
    
    // Summary
    std::cout << "\n=====================================\n";
    std::cout << "  Example Complete!\n";
    std::cout << "=====================================\n\n";
    
    std::cout << "Summary:\n";
    std::cout << "  ✓ Discovered and selected master clock\n";
    std::cout << "  ✓ Calculated time offset (" << (slave.time_offset_ns / 1000000.0) << " ms)\n";
    std::cout << "  ✓ Measured path delay (" << (slave.path_delay_ns / 1000.0) << " μs)\n";
    std::cout << "  ✓ Synchronized local clock to master\n";
    std::cout << "  ✓ Achieved SLAVE state\n\n";
    
    std::cout << "In a real system:\n";
    std::cout << "  • Network HAL would use actual Ethernet/UDP sockets\n";
    std::cout << "  • Timestamps would come from hardware timestamping\n";
    std::cout << "  • Clock adjustment would use system time APIs\n";
    std::cout << "  • Process would repeat continuously for ongoing sync\n\n";
    
    std::cout << "Next Steps:\n";
    std::cout << "  → Study the source code in basic_ptp_slave.cpp\n";
    std::cout << "  → Examine minimal_hal.cpp for HAL patterns\n";
    std::cout << "  → See integration-guide.md for production HAL implementation\n";
    std::cout << "  → Try example 2: BMCA Integration (multi-clock scenario)\n\n";
    
    // Cleanup
    hal.shutdown();
    
    return 0;
}
