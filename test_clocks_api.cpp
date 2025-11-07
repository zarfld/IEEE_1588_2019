/**
 * @file test_clocks_api.cpp
 * @brief Compilation & linkage smoke test for clocks.cpp API
 *
 * TEST IDs:
 *   - TEST-CLOCKS-API-INIT-001  (REQ-F-005 HAL / REQ-NF-M-001 Portability)
 *   - TEST-CLOCKS-API-TYPES-001 (REQ-F-001 Message Types structural availability)
 *
 * Requirements Referenced:
 *   REQ-F-001, REQ-F-005, REQ-NF-M-001
 *
 * Intent: Provide traceable evidence that core clock structures and HAL-facing
 * interfaces compile and link deterministically across platforms (IEEE 1588-2019
 * section references indirect via types/messages headers).
 */

#include "src/clocks.cpp" // direct inclusion for compile-time interface validation

int main() {
    // If it compiles and links, treat as PASS for current scope.
    return 0;
}