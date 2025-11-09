// @satisfies STR-PERF-005 - Resource Efficiency (static footprint evidence)
// Purpose: Basic size checks and absence of dynamic allocation in critical path (heuristic)
// NOTE: Precise CPU/RAM profiling requires target hardware; this test provides compile-time footprint assertions.

#include <cstdio>
#include <new>
#include <cstdlib>
#include <type_traits>
#include "clocks.hpp"

using namespace IEEE::_1588::PTP::_2019;
using namespace IEEE::_1588::PTP::_2019::Types;
using namespace IEEE::_1588::PTP::_2019::Clocks;

// Helper to detect dynamic allocation (heuristic: override global new/delete locally)
static bool new_called = false;
void* operator new(std::size_t sz) {
    new_called = true;
    return std::malloc(sz);
}
void operator delete(void* p) noexcept { std::free(p); }
// Provide sized delete to match C++17 deallocation signatures (silences -Wsized-deallocation)
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    // Compile-time size constraints (informational) - not hard failing if exceeded but we record.
    if(sizeof(PortDataSet) > 128) return 1;
    if(sizeof(CurrentDataSet) > 64) return 2;
    if(sizeof(ParentDataSet) > 128) return 3;

    StateCallbacks cbs{}; // empty callbacks fine for size test
    PortConfiguration cfg{};
    OrdinaryClock oc(cfg, cbs);
    if(!oc.initialize().is_success()) return 4;

    Timestamp t{}; t.setTotalSeconds(0); t.nanoseconds = 0;
    // Perform a tick; should not invoke dynamic allocation
    if(!oc.tick(t).is_success()) return 5;
    if(new_called) return 6; // dynamic allocation occurred unexpectedly

    std::puts("resource_efficiency: PASS");
    return 0;
}
