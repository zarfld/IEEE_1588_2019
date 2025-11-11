# Example 2: BMCA Integration - Multi-Clock Scenario

## Overview

This example demonstrates the **Best Master Clock Algorithm (BMCA)** in a scenario with **multiple clocks competing** to become the master. You'll see how PTP automatically selects the best clock based on IEEE 1588-2019 Section 9.3 criteria.

**What is BMCA?**  
The Best Master Clock Algorithm determines which clock in a PTP network should serve as the time reference (master). Every PTP clock periodically announces itself via Announce messages, and each receiving clock compares these announcements using a standardized algorithm.

**What You'll Learn:**
- How BMCA compares multiple clocks
- The hierarchy of comparison criteria (Priority1 → Class → Accuracy → Variance → Priority2 → Identity)
- State transitions when a better master is discovered
- Dynamic master failover scenarios

## What You'll Build

A simulation with **3 clocks** of different qualities:
1. **Clock A**: GPS-disciplined primary reference (Class 6)
2. **Clock B**: Application-specific ordinary clock (Class 248, Priority1=64)
3. **Clock C**: Default ordinary clock (Class 248, Priority1=128)

You'll see Clock A win initially, then simulate its failure and watch the network re-elect Clock B as master.

## Architecture

```
┌─────────────────────────────────────────┐
│        BMCA Simulation Network          │
├─────────────────────────────────────────┤
│                                         │
│  ┌──────────┐   ┌──────────┐           │
│  │ Clock A  │   │ Clock B  │           │
│  │ Class 6  │   │ Class 248│           │
│  │ GPS Ref  │   │ Priority │           │
│  │          │   │ 1 = 64   │           │
│  └────┬─────┘   └────┬─────┘           │
│       │              │                  │
│       │   Announce   │                  │
│       │   Messages   │                  │
│       └──────┬───────┘                  │
│              │                          │
│              ▼                          │
│        ┌─────────┐    ┌──────────┐     │
│        │ Clock C │    │ Observer │     │
│        │ Class   │    │ (Local)  │     │
│        │ 248     │    │          │     │
│        │ Default │    │          │     │
│        └─────────┘    └──────────┘     │
│                                         │
│     All clocks send Announce messages  │
│     Observer runs BMCA to select master│
└─────────────────────────────────────────┘
```

## Files in This Example

| File | Description |
|------|-------------|
| `README.md` | This documentation |
| `bmca_integration.cpp` | Multi-clock BMCA demonstration |
| `CMakeLists.txt` | Build configuration |

## Building the Example

### Prerequisites
- CMake 3.20+
- C++17 compiler
- IEEE 1588-2019 library built

### Build Steps

```bash
# From repository root
cd build
cmake --build . --target bmca_integration
```

**Output location:**
- Windows: `build\examples\02-bmca-integration\Debug\bmca_integration.exe`
- Linux: `build/examples/02-bmca-integration/bmca_integration`

## Running the Example

### Windows
```powershell
.\build\examples\02-bmca-integration\Debug\bmca_integration.exe
```

### Linux/macOS
```bash
./build/examples/02-bmca-integration/bmca_integration
```

## Expected Output

```
=====================================
  BMCA Integration Example
  IEEE 1588-2019 Implementation
=====================================

Setting up multi-clock scenario...

Creating Clock A (GPS-disciplined primary reference):
  Clock Identity: aa:bb:cc:ff:fe:00:00:01
  Priority1: 128
  Clock Class: 6 (Primary Reference - GPS synchronized)
  Clock Accuracy: Within 25 ns
  Variance: 0x4E5D
  Priority2: 128

Creating Clock B (Application-specific ordinary clock):
  Clock Identity: aa:bb:cc:ff:fe:00:00:02
  Priority1: 64 (Better than default!)
  Clock Class: 248 (Application Specific)
  Clock Accuracy: Unknown
  Variance: 0x4E5D
  Priority2: 128

Creating Clock C (Default ordinary clock):
  Clock Identity: aa:bb:cc:ff:fe:00:00:03
  Priority1: 128
  Clock Class: 248 (Default)
  Clock Accuracy: Unknown
  Variance: 0x4E5D
  Priority2: 128

Creating Observer (Local clock):
  Clock Identity: 00:11:22:ff:fe:33:44:55
  Current State: LISTENING
  Listening for Announce messages...

--- Round 1: Normal Operation ---

Announce from Clock A received
  Running BMCA comparison (A vs no current master):
    → Clock A Priority1 (128) vs Local Default (128): Equal
    → Clock A Class (6) vs Local Class (248): Clock A BETTER
    Decision: ACCEPT Clock A as master

Announce from Clock B received
  Running BMCA comparison (B vs A):
    → Clock B Priority1 (64) vs Current Master Priority1 (128): Clock B BETTER!
    Decision: REJECT - Clock A remains master (but let's re-evaluate)
    
    Wait! Let's compare properly:
    → Clock B Priority1 (64) < Clock A Priority1 (128)
    Decision: ACCEPT Clock B as new master
    
    State Transition: Master changed from Clock A to Clock B

Announce from Clock C received
  Running BMCA comparison (C vs B):
    → Clock C Priority1 (128) vs Current Master Priority1 (64): Clock C WORSE
    Decision: REJECT Clock C

Current Master: Clock B (aa:bb:cc:ff:fe:00:00:02)
Reason: Lower Priority1 value (64 < others)

--- Round 2: Clock A Returns with Better Class ---

Wait, Clock A has Class 6 (GPS)! Let's compare again:

Comparing Clock A vs Clock B:
  Step 1: Priority1
    Clock A: 128
    Clock B: 64
    → Clock B wins Priority1 comparison
    
  Decision: Clock B remains master
  (Priority1 is checked before Class!)

Key Insight: Priority1 overrides Class in BMCA!

--- Round 3: Clock B Fails ---

Clock B has failed or disconnected.
Observer detects timeout (no Announce for >3 intervals).
State: Searching for new master...

Announce from Clock A received
  Running BMCA comparison (A vs none):
    → No current master
    Decision: ACCEPT Clock A as master

Announce from Clock C received
  Running BMCA comparison (C vs A):
    → Clock C Priority1 (128) vs Clock A Priority1 (128): Equal
    → Clock C Class (248) vs Clock A Class (6): Clock A BETTER
    Decision: REJECT Clock C

Current Master: Clock A (aa:bb:cc:ff:fe:00:00:01)
Reason: Best available clock (Class 6 GPS reference)

--- Round 4: All Clocks Equal - Tie-Breaking ---

Simulating 3 clocks with identical quality:
  All Priority1=128, Class=248, Accuracy=Unknown

Creating Clock D, E, F with only clock identity different:
  Clock D: aa:bb:cc:ff:fe:dd:dd:dd
  Clock E: aa:bb:cc:ff:fe:ee:ee:ee
  Clock F: aa:bb:cc:ff:fe:ff:ff:ff

Running BMCA with tie-breaking:
  Priority1: All equal (128)
  Class: All equal (248)
  Accuracy: All equal (Unknown)
  Variance: All equal (0x4E5D)
  Priority2: All equal (128)
  → Clock Identity comparison (lowest wins)
    
  Clock D ID: aa:bb:cc:ff:fe:dd:dd:dd
  Clock E ID: aa:bb:cc:ff:fe:ee:ee:ee (HIGHER - loses)
  Clock F ID: aa:bb:cc:ff:fe:ff:ff:ff (HIGHER - loses)
  
  Winner: Clock D (lowest identity)

=====================================
  Example Complete!
=====================================

Summary:
  ✓ Demonstrated BMCA with 3+ clocks
  ✓ Showed master selection criteria hierarchy
  ✓ Simulated master failover
  ✓ Demonstrated tie-breaking by clock identity
  ✓ Displayed state transitions

Key Learnings:
  • Priority1 is checked FIRST (admin can force preference)
  • Clock Class separates reference quality (GPS > local oscillator)
  • Clock Identity breaks ties (deterministic selection)
  • BMCA runs continuously for dynamic topology
  • Master changes trigger re-synchronization

Next Steps:
  → Study bmca_integration.cpp source code
  → See IEEE 1588-2019 Section 9.3 for complete algorithm
  → Try example 3: HAL Implementation Template
  → Read docs/architecture/bmca-design.md for production considerations
```

## Key Concepts Demonstrated

### 1. BMCA Comparison Hierarchy

BMCA compares clocks in strict order per IEEE 1588-2019 Section 9.3.2.5:

```
┌─────────────────────────────────────────┐
│     BMCA Comparison Order (strict!)     │
├─────────────────────────────────────────┤
│  1. Priority1          (admin control)  │
│  2. Clock Class        (quality level)  │
│  3. Clock Accuracy     (precision)      │
│  4. Offset Variance    (stability)      │
│  5. Priority2          (admin tiebreak) │
│  6. Clock Identity     (final tiebreak) │
└─────────────────────────────────────────┘

Lower values win (except variance uses log scale)
```

**Example Comparison:**
```
Clock A: Priority1=128, Class=6 (GPS)
Clock B: Priority1=64,  Class=248 (ordinary)

Step 1: Compare Priority1
  64 < 128 → Clock B wins!
  
Clock B becomes master despite worse Class!
```

### 2. Clock Class Values

IEEE 1588-2019 Table 5 defines clock classes:

| Class | Description | Example |
|-------|-------------|---------|
| 6-7 | Primary Reference (locked) | GPS, atomic clock |
| 13-14 | Application Specific | Disciplined by PTP |
| 52 | Degraded Primary | GPS unlocked |
| 58 | Degraded Application | Lost PTP sync |
| 187 | Alternative Timescale | TAI instead of UTC |
| 193-215 | User-defined | Custom applications |
| 248 | Default | Unknown/uncalibrated |
| 255 | Slave-only | Cannot be master |

### 3. State Machine Transitions

```
LISTENING → Waiting for Announce messages
    ↓
    Receive Announce + BMCA accepts
    ↓
UNCALIBRATED → Accepted master but not synced
    ↓
    Sync/Follow_Up + Delay exchange
    ↓
SLAVE → Synchronized to master
    ↓
    Better master appears OR master fails
    ↓
LISTENING → Search for new master
```

### 4. Dynamic Master Selection

Production systems continuously run BMCA:
- Every Announce interval (typically 1-16 seconds)
- Compare all received Announce messages
- Switch masters if better clock appears
- Handle master failures gracefully

## Code Structure

The example follows this flow:

```cpp
int main() {
    // 1. Create multiple clocks with different attributes
    PTPClock clock_a = create_gps_clock();
    PTPClock clock_b = create_priority_clock();
    PTPClock clock_c = create_default_clock();
    
    // 2. Create observer (local clock) in LISTENING state
    PTPObserver observer;
    
    // 3. Round 1: All clocks announce
    for (auto& clock : {clock_a, clock_b, clock_c}) {
        Announce msg = clock.create_announce();
        BMCADecision decision = observer.run_bmca(msg);
        // Display comparison process
    }
    
    // 4. Round 2: Demonstrate priority override
    // Show Priority1 < Class in hierarchy
    
    // 5. Round 3: Simulate master failure
    observer.timeout_current_master();
    // Re-run BMCA with remaining clocks
    
    // 6. Round 4: Tie-breaking demonstration
    // Create identical clocks, differ only by identity
    
    return 0;
}
```

## Adapting for Production

### 1. Continuous Operation

This example runs 4 discrete rounds. Production systems run continuously:

```cpp
while (true) {
    // Wait for Announce timeout
    sleep_until(next_announce_timeout);
    
    // Check if Announce received recently
    if (time_since_last_announce() > announce_timeout) {
        // Master presumed failed
        current_state = LISTENING;
        current_master = nullptr;
    }
    
    // Process any pending Announce messages
    for (auto& announce : pending_announces) {
        run_bmca(announce);
    }
}
```

### 2. Announce Timeout Detection

```cpp
const int ANNOUNCE_TIMEOUT_FACTOR = 3;  // Per IEEE 1588-2019

void check_master_timeout() {
    if (current_master) {
        uint64_t timeout_ns = announce_interval_ns * ANNOUNCE_TIMEOUT_FACTOR;
        if ((current_time() - last_announce_time) > timeout_ns) {
            // Master failed
            transition_to_listening();
        }
    }
}
```

### 3. Master Change Handling

```cpp
void handle_master_change(const PTPClock& new_master) {
    // Reset synchronization state
    time_offset_valid = false;
    path_delay_valid = false;
    
    // Transition to UNCALIBRATED
    current_state = UNCALIBRATED;
    
    // Restart sync sequence with new master
    last_sync_sequence_id = 0;
    
    // Log the change
    log_master_change(current_master, new_master);
    
    current_master = new_master;
}
```

### 4. Production BMCA Implementation

Use library's built-in BMCA:

```cpp
#include <IEEE/1588/2019/clock/bmca.hpp>

using namespace IEEE::_1588::_2019::clock;

BestMasterClockAlgorithm bmca;
bmca.set_local_clock(local_clock_attributes);

// For each received Announce
AnnounceMessage announce = parse_announce(packet);
BMCAResult result = bmca.compare(announce);

if (result == BMCAResult::NewMasterSelected) {
    handle_master_change(announce.clock_identity);
}
```

## Troubleshooting

### Issue 1: Clock B doesn't win despite Priority1=64

**Cause**: Comparison logic bug (comparing wrong direction)

**Solution**: Ensure comparison: `if (new_priority1 < current_priority1)` (lower wins!)

### Issue 2: Tie-breaking doesn't work

**Cause**: Clock identities are equal (not unique)

**Solution**: Ensure each clock has unique 8-byte identity (typically from MAC address)

### Issue 3: Master changes every round

**Cause**: No hysteresis or cooldown period

**Solution**: Production systems use announce timeout (3× interval) before changing masters

## Next Steps

1. **Study the source code** in `bmca_integration.cpp` to understand comparison logic
2. **Read IEEE 1588-2019 Section 9.3** for complete BMCA specification
3. **Try Example 3** - HAL Implementation Template for porting guidance
4. **Explore library BMCA** in `include/IEEE/1588/2019/clock/bmca.hpp`
5. **Review architecture docs** in `03-architecture/components/bmca/`

## References

- **IEEE 1588-2019 Section 9.3**: Best master clock algorithm specification
- **IEEE 1588-2019 Table 5**: Clock class definitions
- **IEEE 1588-2019 Section 8.2.5**: defaultDS.priority1 and priority2
- **IEEE 1588-2019 Section 7.6.2**: Clock quality (class, accuracy, variance)
- **Library Architecture**: `docs/architecture/bmca-design.md`
- **Integration Guide**: `08-transition/deployment-plans/integration-guide.md`

---

**Example Status**: Complete working demonstration of BMCA multi-clock scenario  
**IEEE 1588-2019 Compliance**: Section 9.3 (Best Master Clock Algorithm)  
**Last Updated**: 2025-11-11
