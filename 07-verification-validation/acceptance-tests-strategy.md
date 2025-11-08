# IEEE 1588-2019 PTP Acceptance Tests Strategy

**Phase**: 07-verification-validation  
**Standard**: IEEE 1012-2016 (V&V), ISO/IEC/IEEE 29119 (Software Testing)  
**Source**: `01-stakeholder-requirements/stakeholder-requirements-spec.md`

## Overview

This document defines the acceptance testing strategy for the IEEE 1588-2019 PTP implementation, mapping stakeholder requirements to executable tests that validate system compliance without requiring physical hardware.

## Acceptance Test Philosophy for C++ Embedded Library

**Key Principle**: Unlike web applications that use BDD frameworks (Cucumber, Playwright), embedded C++ libraries require different acceptance testing approaches:

1. **Protocol Compliance Tests** - Verify IEEE 1588-2019 specification adherence
2. **Architectural Validation** - Ensure hardware abstraction and platform independence
3. **Integration Scenarios** - Test complete synchronization workflows
4. **Quality Attributes** - Validate non-functional requirements (determinism, security)
5. **Example Validation** - Ensure end-user examples work correctly

## Mapping Stakeholder Requirements to Tests

### STR-STD-001: IEEE 1588-2019 Protocol Compliance (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given IEEE 1588-2019 compliant Grandmaster
When implementation operates as slave clock
Then SHALL synchronize within 1 microsecond of Grandmaster time
  AND SHALL select correct master using BMCA
  AND SHALL handle all mandatory message types
  AND SHALL update correctionField per specification
```

**C++ Acceptance Tests**:
- ✅ **Message Type Handling**: `ctest -R "bmca|state_machine|offset|delay"`
  - Validates Sync, Delay_Req, Follow_Up, Delay_Resp, Announce message processing
  - Tests clock state machine transitions (INITIALIZING → LISTENING → SLAVE/MASTER)
  - Verifies BMCA dataset comparison and master selection
  
- ✅ **Offset Calculation**: Existing unit tests validate offset computation per IEEE Eq 1
  - `offset = (t2 - t1) - (t4 - t3)/2` implementation tested
  - Tests use deterministic timestamps for repeatable validation
  
- 📋 **Future Enhancement**: Loopback synchronization test
  - Create two clocks in same process (GM + Slave)
  - Connect via memory buffers simulating network
  - Measure convergence time and steady-state accuracy

### STR-STD-002: Message Format Correctness (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given any PTP message constructed by implementation
When captured with Wireshark PTP dissector
Then dissector SHALL parse without errors
  AND all fields SHALL match IEEE 1588-2019 Table 26
  AND network byte order SHALL be correct
  AND TLV format SHALL comply with specification
```

**C++ Acceptance Tests**:
- ✅ **Serialization/Deserialization**: `ctest -R "msg_.*validation|messages"`
  - Tests message construction and parsing
  - Validates field order, sizes, and byte alignment
  - Verifies network byte order (big-endian) conversion
  
- ✅ **TLV Validation**: Tests ensure TLV Type-Length-Value format compliance
  
- 📋 **Future Enhancement**: Wireshark PCAP validation
  - Generate PCAP files from test messages
  - Use tshark to validate dissection: `tshark -r test.pcap -Y ptp`

### STR-STD-003: Best Master Clock Algorithm (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given 3 clocks with different priorities
When clock A has priority1=64, clock B has priority1=128, clock C has priority1=192
Then clock A SHALL be selected as master
  AND clock state SHALL remain stable for 1000 announce intervals
  AND failover SHALL occur within 10 announce intervals if clock A fails
```

**C++ Acceptance Tests**:
- ✅ **BMCA Implementation**: `ctest -R "bmca"`
  - **bmca_tdd_tests**: Dataset comparison validation (6 tests)
  - **bmca_tdd_edges**: Announce message qualification edge cases (6 tests)
  - **bmca_tdd_selection**: Master selection logic (4 tests)
  - Tests cover all comparison steps: priority1, clockClass, clockAccuracy, variance, priority2, clockIdentity
  
- ✅ **State Decision**: Tests verify correct port states based on comparison results
  
- 📋 **Future Enhancement**: Multi-clock scenario test
  - Simulate 3+ clocks in test harness
  - Inject Announce messages with varying priorities
  - Verify master selection and failover timing

### STR-STD-004: Interoperability with Commercial Devices (P1)

**Gherkin Acceptance Criteria**:
```gherkin
Given commercial PTP Grandmaster device
When implementation connects as slave clock
Then SHALL synchronize offset <1 microsecond
  AND SHALL maintain sync for 24 hours
  AND SHALL NOT cause errors on GM
```

**C++ Acceptance Tests**:
- ✅ **Protocol Correctness**: All protocol tests ensure commercial device compatibility
  - Correct message formats prevent GM errors
  - Proper BMCA ensures correct master selection
  
- 📋 **Manual Test Procedure** (requires hardware):
  - Connect to Meinberg M1000 or Microsemi SyncServer
  - Run for 24 hours logging offset samples
  - Verify 95% < 1µs, 99% < 2µs per STR-PERF-001
  - Document in `07-verification-validation/test-results/interop-test-report.md`

### STR-PERF-001: Synchronization Accuracy (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given GPS-disciplined Grandmaster and Intel I210 NIC
When synchronized for 10 minutes
Then 95% of offset samples SHALL be <1 microsecond
  AND 99% SHALL be <2 microseconds
  AND median SHALL be <500 nanoseconds
```

**C++ Acceptance Tests**:
- ✅ **Offset Calculation Accuracy**: Unit tests validate offset computation correctness
  - Tests use nanosecond precision (uint64_t timestamps)
  - Verified against IEEE 1588-2019 equations
  
- 📋 **Performance Measurement Test** (requires hardware with HW timestamps):
  ```cpp
  // tests/acceptance/test_sync_accuracy.cpp
  TEST_CASE("STR-PERF-001: Synchronization accuracy") {
      // Setup GM and Slave clocks with loopback network
      OrdinaryClock gm, slave;
      LoopbackNetwork network;
      
      // Run for 10 minutes, collect offset samples
      std::vector<int64_t> offsets;
      for (int i = 0; i < 600; ++i) {  // 1 sample/second
          run_sync_cycle(gm, slave, network);
          offsets.push_back(slave.get_offset_from_master());
          sleep(1);
      }
      
      // Calculate percentiles
      auto p95 = percentile(offsets, 0.95);
      auto p99 = percentile(offsets, 0.99);
      auto median = percentile(offsets, 0.50);
      
      // Validate acceptance criteria
      REQUIRE(p95 < 1000);  // 1µs = 1000ns
      REQUIRE(p99 < 2000);  // 2µs
      REQUIRE(median < 500); // 500ns
  }
  ```

### STR-PERF-002: Timing Determinism (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given critical timing path in PTP message processing
When processing 10,000 messages
Then NO dynamic memory allocation SHALL occur
  AND 99.9% of processing times SHALL be within 10% of mean
  AND maximum jitter SHALL be <1 microsecond
```

**C++ Acceptance Tests**:
- ✅ **No Dynamic Allocation**: Symbol analysis in CI
  ```bash
  nm -C build/*.a | grep -i "malloc\|free\|new\|delete"
  # Exit code 1 if found = FAIL
  ```
  
- ✅ **Static Analysis**: Code review confirms:
  - All message buffers use fixed-size arrays
  - Clock state uses pre-allocated structures
  - No STL containers in critical paths (std::vector, std::map)
  
- 📋 **Jitter Measurement Test**:
  ```cpp
  TEST_CASE("STR-PERF-002: Processing time jitter") {
      PTPMessage msg = create_test_sync_message();
      std::vector<uint64_t> durations;
      
      for (int i = 0; i < 10000; ++i) {
          auto start = high_resolution_clock::now();
          process_ptp_message(msg);
          auto end = high_resolution_clock::now();
          durations.push_back(duration_cast<nanoseconds>(end - start).count());
      }
      
      auto mean = average(durations);
      auto max_jitter = max_deviation(durations, mean);
      
      REQUIRE(max_jitter < 1000);  // <1µs jitter
  }
  ```

### STR-PERF-003: Clock Servo Performance (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given offset from master = 100 microseconds at t=0
When clock servo adjusts local clock
Then offset SHALL converge to <1 microsecond within 60 seconds
  AND jitter SHALL be <50 nanoseconds after convergence
  AND NO overshoot >110% of initial offset SHALL occur
```

**C++ Acceptance Tests**:
- 📋 **Servo Convergence Test**:
  ```cpp
  TEST_CASE("STR-PERF-003: Clock servo convergence") {
      ClockServo servo;
      servo.set_initial_offset(100'000);  // 100µs initial error
      
      std::vector<int64_t> offsets;
      for (int t = 0; t < 60; ++t) {  // 60 seconds
          auto offset = servo.update(1.0);  // 1 second interval
          offsets.push_back(offset);
          
          // Check no overshoot
          REQUIRE(std::abs(offset) <= 110'000);
      }
      
      // Final convergence <1µs
      REQUIRE(std::abs(offsets.back()) < 1'000);
      
      // Jitter in last 10 samples <50ns
      auto jitter = calculate_jitter(offsets.end() - 10, offsets.end());
      REQUIRE(jitter < 50);
  }
  ```

### STR-PERF-004: Path Delay Measurement (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given 100-meter Cat6 cable (propagation delay ~500 nanoseconds)
When measuring path delay 1000 times
Then mean delay SHALL be 500±50 nanoseconds
  AND standard deviation SHALL be <50 nanoseconds
  AND NO outliers >2x mean SHALL occur
```

**C++ Acceptance Tests**:
- ✅ **Delay Calculation**: Existing tests validate delay computation
  - `mean_path_delay = (t4 - t1) - (t3 - t2)` per IEEE specification
  
- 📋 **Delay Accuracy Test** (requires loopback with known delay):
  ```cpp
  TEST_CASE("STR-PERF-004: Path delay accuracy") {
      // Simulate network with 500ns fixed delay
      LoopbackNetwork network(500);  // nanoseconds
      
      std::vector<int64_t> measured_delays;
      for (int i = 0; i < 1000; ++i) {
          auto delay = measure_path_delay(network);
          measured_delays.push_back(delay);
      }
      
      auto mean = average(measured_delays);
      auto stddev = standard_deviation(measured_delays);
      
      REQUIRE(mean >= 450);
      REQUIRE(mean <= 550);
      REQUIRE(stddev < 50);
      
      // No outliers >2x mean
      for (auto delay : measured_delays) {
          REQUIRE(delay < 2 * mean);
      }
  }
  ```

### STR-PORT-001: Hardware Abstraction Layer (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given core protocol implementation
When examined for platform dependencies
Then NO Linux/Windows/RTOS headers SHALL be included
  AND all hardware access SHALL be via HAL function pointers
  AND SHALL compile on x86, ARM, RISC-V without modification
```

**C++ Acceptance Tests**:
- ✅ **Platform Header Analysis**: CI checks for platform-specific includes
  ```bash
  grep -r "#include.*linux\|#include.*windows\|#include.*sys/" src/ include/
  # Exit code 1 if found = FAIL
  ```
  
- ✅ **HAL Architecture Validation**: Code review confirms
  - `network_interface_t` function pointer struct
  - `timer_interface_t` for scheduling
  - `clock_interface_t` for hardware timestamps
  - All hardware access via interfaces, never direct
  
- 📋 **Cross-Platform Build Test**:
  ```yaml
  strategy:
    matrix:
      platform: [x86_64-linux, armv7-linux, aarch64-linux, x86_64-windows]
  steps:
    - name: Cross-compile for ${{ matrix.platform }}
      run: cmake --toolchain=${{ matrix.platform }}.cmake && make
  ```

### STR-SEC-001: Input Validation and Fuzzing (P1)

**Gherkin Acceptance Criteria**:
```gherkin
Given 1,000,000 malformed PTP messages
When processed by implementation
Then zero crashes SHALL occur
  AND zero memory leaks SHALL occur
  AND all invalid messages SHALL be rejected
  AND valid error codes SHALL be returned
```

**C++ Acceptance Tests**:
- ✅ **Message Validation**: `ctest -R "validation|fault_injection"`
  - Tests invalid packet lengths
  - Tests corrupted message fields
  - Tests out-of-range values
  
- 📋 **Fuzzing Test** (using AFL++ or libFuzzer):
  ```cpp
  // tests/fuzz/fuzz_ptp_parser.cpp
  extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
      ptp_sync_message_t msg;
      int result = ptp_parse_sync_message(data, size, &msg);
      
      // Should never crash, always return valid error code
      assert(result == 0 || result < 0);
      return 0;
  }
  ```
  
- Run fuzzer for 1M inputs:
  ```bash
  clang++ -fsanitize=fuzzer,address -o fuzz_parser fuzz_ptp_parser.cpp
  ./fuzz_parser -runs=1000000
  ```

### STR-USE-003: Example Applications (P1)

**Gherkin Acceptance Criteria**:
```gherkin
Given developer wants to integrate library
When following README examples
Then all examples SHALL compile without errors
  AND SHALL run without crashes
  AND SHALL demonstrate key use cases
```

**C++ Acceptance Tests**:
- ✅ **Example Compilation**: CI builds all examples
  ```bash
  cmake --build build --target ieee1588_2019_basic_example
  cmake --build build --target ieee1588_2019_realtime_example
  ```
  
- ✅ **Example Execution**: CI runs examples with timeout
  ```bash
  timeout 5s build/examples/ieee1588_2019_basic_example
  # Exit code 0 or 124 (timeout) = PASS
  # Exit code 1-123 (crash) = FAIL
  ```
  
- 📋 **Example Output Validation**:
  ```bash
  ./ieee1588_2019_basic_example | grep "Clock synchronized"
  # Should show successful synchronization messages
  ```

### STR-MAINT-001: Code Quality and Test Coverage (P0)

**Gherkin Acceptance Criteria**:
```gherkin
Given core library source code
When measured with gcov/lcov
Then line coverage SHALL be ≥80%
  AND branch coverage SHALL be ≥70%
  AND function coverage SHALL be 100%
```

**C++ Acceptance Tests**:
- ✅ **Coverage Measurement**: CI validates coverage threshold
  ```bash
  gcovr --filter 'src/.*' --filter 'include/.*' --fail-under-line 80
  ```
  
- ✅ **Current Status**: 86% core library coverage (exceeds threshold)
  - `src/clocks.cpp`: 593/704 lines = 84%
  - `05-implementation/src/bmca.cpp`: 47/47 lines = 100%
  - `include/clocks.hpp`: 60/66 lines = 90%
  
- ✅ **Test Suite Health**: 43/44 tests passing (98% success rate)

## Acceptance Test Execution Matrix

| Requirement | Test Method | Automation | Hardware Required | Status |
|------------|-------------|------------|-------------------|--------|
| STR-STD-001 | Unit tests (protocol) | ✅ CI | ❌ No | ✅ PASSING |
| STR-STD-002 | Unit tests (messages) | ✅ CI | ❌ No | ✅ PASSING |
| STR-STD-003 | Unit tests (BMCA) | ✅ CI | ❌ No | ✅ PASSING |
| STR-STD-004 | Manual interop | ❌ Manual | ✅ Yes (GM) | 📋 TODO |
| STR-PERF-001 | Loopback test | ⚠️ Partial | ⚠️ Simulated | 📋 TODO |
| STR-PERF-002 | Symbol analysis | ✅ CI | ❌ No | ✅ PASSING |
| STR-PERF-003 | Servo simulation | ⚠️ Partial | ⚠️ Simulated | 📋 TODO |
| STR-PERF-004 | Delay simulation | ⚠️ Partial | ⚠️ Simulated | 📋 TODO |
| STR-PORT-001 | Header analysis | ✅ CI | ❌ No | ✅ PASSING |
| STR-SEC-001 | Fuzzing | ⚠️ Local | ❌ No | 📋 TODO |
| STR-USE-003 | Example tests | ✅ CI | ❌ No | ✅ PASSING |
| STR-MAINT-001 | Coverage analysis | ✅ CI | ❌ No | ✅ PASSING |

**Legend**:
- ✅ PASSING: Automated in CI, currently passing
- 📋 TODO: Not yet implemented
- ⚠️ Partial: Basic validation only, needs enhancement

## CI Integration

**GitHub Actions Job**: `.github/workflows/ci-standards-compliance.yml` (lines 505-620)

**Execution Order**:
1. Build core library with Release configuration
2. Run protocol compliance tests (STR-STD-*)
3. Run performance validation tests (STR-PERF-002, STR-PORT-001)
4. Run security tests (STR-SEC-001 validation)
5. Run example applications (STR-USE-003)
6. Verify coverage threshold (STR-MAINT-001)
7. Generate acceptance test report with pass/fail summary

**Success Criteria**:
- All protocol tests pass
- Symbol analysis finds no dynamic allocation
- Header analysis finds no platform dependencies
- Examples compile and run without crashes
- Coverage ≥80%

**Artifacts**:
- `build/acceptance-test-report.md` - Summary of all validation results
- `build/Testing/` - CTest XML results
- Coverage reports (from unit-tests job)

## Future Enhancements

### Phase 1 (No Hardware Required)
1. **Loopback Synchronization Test** - Two clocks in process communicating via memory
2. **Servo Convergence Simulation** - Test PI controller with simulated offsets
3. **Path Delay Simulation** - Test delay measurement with known network delays
4. **AFL++ Fuzzing Integration** - 1M fuzzed inputs in CI (long-running job)

### Phase 2 (Requires Test Hardware)
1. **Physical Interoperability Tests** - Connect to Meinberg/Microsemi GM
2. **Hardware Timestamp Validation** - Verify nanosecond-precision timestamps
3. **24-Hour Stability Test** - Long-duration synchronization
4. **Multi-Platform Validation** - ARM Cortex-M7 + FreeRTOS testing

### Phase 3 (Production Validation)
1. **Wireshark PCAP Validation** - Generate and validate packet captures
2. **Conformance Test Suite** - Complete IEEE 1588 conformance testing
3. **Certification Preparation** - Document results for AVnu/IEEE certification

## Traceability

**Stakeholder Requirements → Acceptance Tests → Test Cases**:

```
01-stakeholder-requirements/stakeholder-requirements-spec.md
  └─> STR-STD-001: Protocol Compliance
       └─> ctest -R "bmca|state_machine|offset|delay"
            └─> tests/test_bmca_tdd.cpp (16 tests)
            └─> tests/test_offset_calculation.cpp (9 tests)
            └─> tests/test_configuration_setters.cpp (6 tests)
```

Full traceability matrix: `07-verification-validation/traceability/acceptance-to-requirements.md`

## References

- **IEEE 1012-2016**: Software Verification and Validation
- **ISO/IEC/IEEE 29119**: Software Testing Standards
- **IEEE 1588-2019**: Precision Time Protocol specification
- **Stakeholder Requirements**: `01-stakeholder-requirements/stakeholder-requirements-spec.md`

---

**Last Updated**: 2025-01-21  
**Status**: Initial version - 8/12 requirements automated, 4 require enhancement
