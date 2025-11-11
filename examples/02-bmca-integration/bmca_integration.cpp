/**
 * @file bmca_integration.cpp
 * @brief BMCA Integration Example - Multi-Clock Master Selection
 * 
 * This example demonstrates the Best Master Clock Algorithm (BMCA) per
 * IEEE 1588-2019 Section 9.3 in a scenario with multiple clocks competing
 * to become the PTP master.
 * 
 * Key Demonstrations:
 * - BMCA comparison hierarchy (Priority1 → Class → Accuracy → Variance → Priority2 → Identity)
 * - Master selection with clocks of different qualities
 * - Dynamic master failover scenarios
 * - Tie-breaking when clocks have equal attributes
 * 
 * @copyright Based on IEEE 1588-2019 Section 9.3 concepts
 * @version 1.0.0
 * @date 2025-11-11
 */

#include <IEEE/1588/2019/types/clock_identity.hpp>
#include <IEEE/1588/2019/types/port_identity.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>

using namespace IEEE::_1588::_2019::types;

//============================================================================
// PTP Clock Quality Structure (IEEE 1588-2019 Section 7.6.2)
//============================================================================

struct ClockQuality {
    std::uint8_t clock_class;        // Table 5: Clock class enumeration
    std::uint8_t clock_accuracy;     // Table 6: Clock accuracy enumeration
    std::uint16_t offset_scaled_log_variance;  // Stability metric
    
    ClockQuality(std::uint8_t cls = 248, 
                 std::uint8_t acc = 0xFE, 
                 std::uint16_t var = 0x4E5D)
        : clock_class(cls), clock_accuracy(acc), offset_scaled_log_variance(var) {}
};

//============================================================================
// PTP Clock Structure (Simplified)
//============================================================================

struct PTPClock {
    ClockIdentity identity;
    std::uint8_t priority1;
    std::uint8_t priority2;
    ClockQuality quality;
    std::string name;
    bool active;  // Simulates clock being online/offline
    
    PTPClock(const std::string& n,
             const std::uint8_t* id_bytes,
             std::uint8_t p1,
             std::uint8_t p2,
             const ClockQuality& q)
        : identity(id_bytes),
          priority1(p1),
          priority2(p2),
          quality(q),
          name(n),
          active(true) {}
};

//============================================================================
// BMCA Comparison Result
//============================================================================

enum class BMCADecision {
    ACCEPT,   // Accept as new/better master
    REJECT    // Reject (current master is better or equal)
};

enum class ComparisonStep {
    PRIORITY1,
    CLOCK_CLASS,
    CLOCK_ACCURACY,
    VARIANCE,
    PRIORITY2,
    CLOCK_IDENTITY
};

const char* step_to_string(ComparisonStep step) {
    switch (step) {
        case ComparisonStep::PRIORITY1: return "Priority1";
        case ComparisonStep::CLOCK_CLASS: return "Clock Class";
        case ComparisonStep::CLOCK_ACCURACY: return "Clock Accuracy";
        case ComparisonStep::VARIANCE: return "Offset Variance";
        case ComparisonStep::PRIORITY2: return "Priority2";
        case ComparisonStep::CLOCK_IDENTITY: return "Clock Identity";
        default: return "Unknown";
    }
}

//============================================================================
// BMCA Implementation (IEEE 1588-2019 Section 9.3.2.5)
//============================================================================

struct BMCAResult {
    BMCADecision decision;
    ComparisonStep decisive_step;
    std::string reason;
};

BMCAResult compare_clocks(const PTPClock& candidate, const PTPClock* current_master) {
    BMCAResult result;
    
    // If no current master, accept candidate
    if (!current_master || !current_master->active) {
        result.decision = BMCADecision::ACCEPT;
        result.decisive_step = ComparisonStep::PRIORITY1;
        result.reason = "No active master";
        return result;
    }
    
    // Step 1: Compare Priority1 (lower is better)
    if (candidate.priority1 < current_master->priority1) {
        result.decision = BMCADecision::ACCEPT;
        result.decisive_step = ComparisonStep::PRIORITY1;
        result.reason = "Lower Priority1";
        return result;
    } else if (candidate.priority1 > current_master->priority1) {
        result.decision = BMCADecision::REJECT;
        result.decisive_step = ComparisonStep::PRIORITY1;
        result.reason = "Higher Priority1";
        return result;
    }
    
    // Step 2: Compare Clock Class (lower is better)
    if (candidate.quality.clock_class < current_master->quality.clock_class) {
        result.decision = BMCADecision::ACCEPT;
        result.decisive_step = ComparisonStep::CLOCK_CLASS;
        result.reason = "Better Clock Class";
        return result;
    } else if (candidate.quality.clock_class > current_master->quality.clock_class) {
        result.decision = BMCADecision::REJECT;
        result.decisive_step = ComparisonStep::CLOCK_CLASS;
        result.reason = "Worse Clock Class";
        return result;
    }
    
    // Step 3: Compare Clock Accuracy (lower is better)
    if (candidate.quality.clock_accuracy < current_master->quality.clock_accuracy) {
        result.decision = BMCADecision::ACCEPT;
        result.decisive_step = ComparisonStep::CLOCK_ACCURACY;
        result.reason = "Better Clock Accuracy";
        return result;
    } else if (candidate.quality.clock_accuracy > current_master->quality.clock_accuracy) {
        result.decision = BMCADecision::REJECT;
        result.decisive_step = ComparisonStep::CLOCK_ACCURACY;
        result.reason = "Worse Clock Accuracy";
        return result;
    }
    
    // Step 4: Compare Offset Scaled Log Variance (lower is better)
    if (candidate.quality.offset_scaled_log_variance < current_master->quality.offset_scaled_log_variance) {
        result.decision = BMCADecision::ACCEPT;
        result.decisive_step = ComparisonStep::VARIANCE;
        result.reason = "Better Variance";
        return result;
    } else if (candidate.quality.offset_scaled_log_variance > current_master->quality.offset_scaled_log_variance) {
        result.decision = BMCADecision::REJECT;
        result.decisive_step = ComparisonStep::VARIANCE;
        result.reason = "Worse Variance";
        return result;
    }
    
    // Step 5: Compare Priority2 (lower is better)
    if (candidate.priority2 < current_master->priority2) {
        result.decision = BMCADecision::ACCEPT;
        result.decisive_step = ComparisonStep::PRIORITY2;
        result.reason = "Lower Priority2";
        return result;
    } else if (candidate.priority2 > current_master->priority2) {
        result.decision = BMCADecision::REJECT;
        result.decisive_step = ComparisonStep::PRIORITY2;
        result.reason = "Higher Priority2";
        return result;
    }
    
    // Step 6: Compare Clock Identity (lower is better - final tie-breaker)
    const std::uint8_t* cand_id = candidate.identity.data();
    const std::uint8_t* master_id = current_master->identity.data();
    
    int cmp = std::memcmp(cand_id, master_id, 8);
    if (cmp < 0) {
        result.decision = BMCADecision::ACCEPT;
        result.decisive_step = ComparisonStep::CLOCK_IDENTITY;
        result.reason = "Lower Clock Identity (tie-breaker)";
        return result;
    } else {
        result.decision = BMCADecision::REJECT;
        result.decisive_step = ComparisonStep::CLOCK_IDENTITY;
        result.reason = "Higher or Equal Clock Identity";
        return result;
    }
}

//============================================================================
// Helper Functions
//============================================================================

void print_clock_info(const PTPClock& clock) {
    std::cout << "  Clock Identity: ";
    const std::uint8_t* id = clock.identity.data();
    for (int i = 0; i < 8; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned>(id[i]);
        if (i < 7) std::cout << ":";
    }
    std::cout << std::dec << "\n";
    std::cout << "  Priority1: " << static_cast<int>(clock.priority1) << "\n";
    std::cout << "  Clock Class: " << static_cast<int>(clock.quality.clock_class);
    
    // Decode clock class
    std::uint8_t cls = clock.quality.clock_class;
    if (cls >= 6 && cls <= 7) {
        std::cout << " (Primary Reference - GPS synchronized)\n";
    } else if (cls >= 13 && cls <= 14) {
        std::cout << " (Application Specific - disciplined by PTP)\n";
    } else if (cls == 52) {
        std::cout << " (Degraded Primary)\n";
    } else if (cls == 248) {
        std::cout << " (Default - uncalibrated)\n";
    } else if (cls == 255) {
        std::cout << " (Slave Only)\n";
    } else {
        std::cout << " (Other)\n";
    }
    
    std::cout << "  Clock Accuracy: ";
    if (clock.quality.clock_accuracy == 0x20) {
        std::cout << "Within 25 ns\n";
    } else if (clock.quality.clock_accuracy == 0x21) {
        std::cout << "Within 100 ns\n";
    } else if (clock.quality.clock_accuracy == 0xFE) {
        std::cout << "Unknown\n";
    } else {
        std::cout << "0x" << std::hex << static_cast<int>(clock.quality.clock_accuracy) 
                  << std::dec << "\n";
    }
    
    std::cout << "  Variance: 0x" << std::hex << clock.quality.offset_scaled_log_variance 
              << std::dec << "\n";
    std::cout << "  Priority2: " << static_cast<int>(clock.priority2) << "\n";
}

void print_comparison_details(const PTPClock& candidate, const PTPClock* current_master) {
    if (!current_master) {
        std::cout << "  No current master, accepting first candidate\n";
        return;
    }
    
    std::cout << "  Comparing " << candidate.name << " vs " << current_master->name << ":\n";
    std::cout << "    Step 1 (Priority1): " << static_cast<int>(candidate.priority1) 
              << " vs " << static_cast<int>(current_master->priority1);
    if (candidate.priority1 < current_master->priority1) {
        std::cout << " → Candidate WINS\n";
    } else if (candidate.priority1 > current_master->priority1) {
        std::cout << " → Current master WINS\n";
    } else {
        std::cout << " → Equal, continue...\n";
        
        std::cout << "    Step 2 (Clock Class): " << static_cast<int>(candidate.quality.clock_class)
                  << " vs " << static_cast<int>(current_master->quality.clock_class);
        if (candidate.quality.clock_class < current_master->quality.clock_class) {
            std::cout << " → Candidate WINS\n";
        } else if (candidate.quality.clock_class > current_master->quality.clock_class) {
            std::cout << " → Current master WINS\n";
        } else {
            std::cout << " → Equal, continue to tie-breaking...\n";
        }
    }
}

//============================================================================
// Main Demonstration
//============================================================================

int main() {
    std::cout << "=====================================\n";
    std::cout << "  BMCA Integration Example\n";
    std::cout << "  IEEE 1588-2019 Implementation\n";
    std::cout << "=====================================\n\n";
    
    std::cout << "Setting up multi-clock scenario...\n\n";
    
    // Create Clock A: GPS-disciplined primary reference (best quality)
    std::uint8_t clock_a_id[] = {0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0x00, 0x00, 0x01};
    ClockQuality quality_a(6, 0x20, 0x4E5D);  // Class 6 (GPS), 25ns accuracy
    PTPClock clock_a("Clock A", clock_a_id, 128, 128, quality_a);
    
    std::cout << "Creating Clock A (GPS-disciplined primary reference):\n";
    print_clock_info(clock_a);
    std::cout << "\n";
    
    // Create Clock B: Application-specific with Priority1=64 (admin preference)
    std::uint8_t clock_b_id[] = {0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0x00, 0x00, 0x02};
    ClockQuality quality_b(248, 0xFE, 0x4E5D);  // Class 248 (default), unknown accuracy
    PTPClock clock_b("Clock B", clock_b_id, 64, 128, quality_b);  // Priority1=64 (better than default!)
    
    std::cout << "Creating Clock B (Application-specific ordinary clock):\n";
    print_clock_info(clock_b);
    std::cout << "\n";
    
    // Create Clock C: Default ordinary clock
    std::uint8_t clock_c_id[] = {0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0x00, 0x00, 0x03};
    ClockQuality quality_c(248, 0xFE, 0x4E5D);
    PTPClock clock_c("Clock C", clock_c_id, 128, 128, quality_c);
    
    std::cout << "Creating Clock C (Default ordinary clock):\n";
    print_clock_info(clock_c);
    std::cout << "\n";
    
    // Observer (local clock) starts in LISTENING state
    std::cout << "Creating Observer (Local clock):\n";
    std::uint8_t observer_id[] = {0x00, 0x11, 0x22, 0xFF, 0xFE, 0x33, 0x44, 0x55};
    ClockIdentity observer_identity(observer_id);
    std::cout << "  Clock Identity: ";
    for (int i = 0; i < 8; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned>(observer_id[i]);
        if (i < 7) std::cout << ":";
    }
    std::cout << std::dec << "\n";
    std::cout << "  Current State: LISTENING\n";
    std::cout << "  Listening for Announce messages...\n\n";
    
    PTPClock* current_master = nullptr;
    
    //========================================================================
    // Round 1: All clocks announce - demonstrate Priority1 override
    //========================================================================
    
    std::cout << "--- Round 1: Initial Master Selection ---\n\n";
    
    // Announce from Clock A
    std::cout << "Announce from " << clock_a.name << " received\n";
    print_comparison_details(clock_a, current_master);
    BMCAResult result_a = compare_clocks(clock_a, current_master);
    std::cout << "  BMCA Decision: " << (result_a.decision == BMCADecision::ACCEPT ? "ACCEPT" : "REJECT")
              << " (" << result_a.reason << " at " << step_to_string(result_a.decisive_step) << ")\n\n";
    
    if (result_a.decision == BMCADecision::ACCEPT) {
        current_master = &clock_a;
        std::cout << "  → Master selected: " << current_master->name << "\n\n";
    }
    
    // Announce from Clock B (has Priority1=64, should win!)
    std::cout << "Announce from " << clock_b.name << " received\n";
    print_comparison_details(clock_b, current_master);
    BMCAResult result_b = compare_clocks(clock_b, current_master);
    std::cout << "  BMCA Decision: " << (result_b.decision == BMCADecision::ACCEPT ? "ACCEPT" : "REJECT")
              << " (" << result_b.reason << " at " << step_to_string(result_b.decisive_step) << ")\n\n";
    
    if (result_b.decision == BMCADecision::ACCEPT) {
        std::cout << "  → Master changed from " << current_master->name << " to " << clock_b.name << "\n";
        std::cout << "  → Key Insight: Priority1 (" << static_cast<int>(clock_b.priority1) 
                  << ") overrides better Clock Class!\n\n";
        current_master = &clock_b;
    }
    
    // Announce from Clock C (default, should lose)
    std::cout << "Announce from " << clock_c.name << " received\n";
    print_comparison_details(clock_c, current_master);
    BMCAResult result_c = compare_clocks(clock_c, current_master);
    std::cout << "  BMCA Decision: " << (result_c.decision == BMCADecision::ACCEPT ? "ACCEPT" : "REJECT")
              << " (" << result_c.reason << " at " << step_to_string(result_c.decisive_step) << ")\n\n";
    
    if (result_c.decision == BMCADecision::ACCEPT) {
        current_master = &clock_c;
    }
    
    std::cout << "Current Master after Round 1: " << current_master->name << "\n";
    std::cout << "Reason: Priority1=" << static_cast<int>(current_master->priority1) 
              << " wins before considering Clock Class\n\n";
    
    //========================================================================
    // Round 2: Clock B fails - demonstrate master failover
    //========================================================================
    
    std::cout << "--- Round 2: Master Failover (Clock B fails) ---\n\n";
    
    clock_b.active = false;
    std::cout << clock_b.name << " has failed or disconnected.\n";
    std::cout << "Observer detects timeout (no Announce for >3 intervals).\n";
    std::cout << "State: Searching for new master...\n\n";
    
    // Re-evaluate with Clock A and Clock C
    current_master = nullptr;
    
    std::cout << "Announce from " << clock_a.name << " received\n";
    result_a = compare_clocks(clock_a, current_master);
    std::cout << "  BMCA Decision: " << (result_a.decision == BMCADecision::ACCEPT ? "ACCEPT" : "REJECT")
              << " (" << result_a.reason << ")\n\n";
    if (result_a.decision == BMCADecision::ACCEPT) {
        current_master = &clock_a;
    }
    
    std::cout << "Announce from " << clock_c.name << " received\n";
    print_comparison_details(clock_c, current_master);
    result_c = compare_clocks(clock_c, current_master);
    std::cout << "  BMCA Decision: " << (result_c.decision == BMCADecision::ACCEPT ? "ACCEPT" : "REJECT")
              << " (" << result_c.reason << " at " << step_to_string(result_c.decisive_step) << ")\n\n";
    
    std::cout << "Current Master after Round 2: " << current_master->name << "\n";
    std::cout << "Reason: Best available clock (Class 6 GPS reference)\n\n";
    
    //========================================================================
    // Round 3: Tie-breaking demonstration
    //========================================================================
    
    std::cout << "--- Round 3: Tie-Breaking by Clock Identity ---\n\n";
    
    std::cout << "Creating 3 clocks with IDENTICAL attributes (except identity):\n\n";
    
    std::uint8_t clock_d_id[] = {0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0xDD, 0xDD, 0xDD};
    PTPClock clock_d("Clock D", clock_d_id, 128, 128, ClockQuality(248, 0xFE, 0x4E5D));
    
    std::uint8_t clock_e_id[] = {0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0xEE, 0xEE, 0xEE};
    PTPClock clock_e("Clock E", clock_e_id, 128, 128, ClockQuality(248, 0xFE, 0x4E5D));
    
    std::uint8_t clock_f_id[] = {0xAA, 0xBB, 0xCC, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF};
    PTPClock clock_f("Clock F", clock_f_id, 128, 128, ClockQuality(248, 0xFE, 0x4E5D));
    
    print_clock_info(clock_d);
    std::cout << "\n";
    print_clock_info(clock_e);
    std::cout << "\n";
    print_clock_info(clock_f);
    std::cout << "\n";
    
    // Run BMCA
    current_master = nullptr;
    
    std::cout << "Running BMCA comparison:\n\n";
    
    for (auto& clock : {&clock_d, &clock_e, &clock_f}) {
        std::cout << "Comparing " << clock->name << ":\n";
        BMCAResult result = compare_clocks(*clock, current_master);
        std::cout << "  Decision: " << (result.decision == BMCADecision::ACCEPT ? "ACCEPT" : "REJECT")
                  << " (" << result.reason << " at " << step_to_string(result.decisive_step) << ")\n";
        
        if (result.decision == BMCADecision::ACCEPT) {
            if (current_master) {
                std::cout << "  → Master changed from " << current_master->name << " to " << clock->name << "\n";
            }
            current_master = clock;
        }
        std::cout << "\n";
    }
    
    std::cout << "Winner: " << current_master->name << "\n";
    std::cout << "Reason: Lowest Clock Identity (aa:bb:cc:ff:fe:dd:dd:dd)\n";
    std::cout << "Key Insight: Clock Identity provides deterministic tie-breaking\n\n";
    
    //========================================================================
    // Summary
    //========================================================================
    
    std::cout << "=====================================\n";
    std::cout << "  Example Complete!\n";
    std::cout << "=====================================\n\n";
    
    std::cout << "Summary:\n";
    std::cout << "  ✓ Demonstrated BMCA with multiple clocks\n";
    std::cout << "  ✓ Showed Priority1 overrides Clock Class\n";
    std::cout << "  ✓ Simulated master failover scenario\n";
    std::cout << "  ✓ Demonstrated tie-breaking by Clock Identity\n";
    std::cout << "  ✓ Displayed comparison hierarchy\n\n";
    
    std::cout << "Key Learnings:\n";
    std::cout << "  • Priority1 is checked FIRST (admin control)\n";
    std::cout << "  • Clock Class separates reference quality\n";
    std::cout << "  • Clock Identity provides final tie-breaking\n";
    std::cout << "  • BMCA runs continuously in production\n";
    std::cout << "  • Master changes trigger re-synchronization\n\n";
    
    std::cout << "BMCA Comparison Order (IEEE 1588-2019 Section 9.3.2.5):\n";
    std::cout << "  1. Priority1          (administrator control)\n";
    std::cout << "  2. Clock Class        (quality hierarchy)\n";
    std::cout << "  3. Clock Accuracy     (precision capability)\n";
    std::cout << "  4. Offset Variance    (stability metric)\n";
    std::cout << "  5. Priority2          (admin tie-breaker)\n";
    std::cout << "  6. Clock Identity     (final deterministic tie-breaker)\n\n";
    
    std::cout << "Next Steps:\n";
    std::cout << "  → Study bmca_integration.cpp source code\n";
    std::cout << "  → Read IEEE 1588-2019 Section 9.3 (BMCA specification)\n";
    std::cout << "  → Try Example 3: HAL Implementation Template\n";
    std::cout << "  → Explore library BMCA in include/IEEE/1588/2019/clock/bmca.hpp\n\n";
    
    return 0;
}
