# Phase 06 Integration Tests

Purpose: Verify multi-component interactions of the IEEE 1588-2019 library in a hardware-agnostic way.

- boundary_clock_integration: smoke test for BoundaryClock with 2 ports
  - Verifies initialize → start → stop across multiple ports deterministically
  - Traceability: TEST-INTEG-BC-STARTSTOP-001 → DES-C-010 (time sync coordination), Section 9.2 state lifecycle

How to run (via CTest): enabled when configured with `-DBUILD_TESTING=ON`. The CI pipeline runs these automatically on Ubuntu and Windows.
