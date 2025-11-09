# Phase 05 Implementation TDD Tests - BMCA
# Traceability: REQ-F-002 (Best Master Clock Algorithm)
# Design References: DES-C-031, DES-I-032, DES-D-033

cmake_minimum_required(VERSION 3.16)

# Shared sources used by BMCA tests
set(BMCA_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
)

# Test 1: Basic BMCA selection
add_executable(bmca_tdd_tests
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_basic.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_tdd_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_tdd_tests PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_red_green_refactor_basic COMMAND bmca_tdd_tests)

# Test 2: Edge comparisons
add_executable(bmca_tdd_edges
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_edges.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_tdd_edges PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_tdd_edges PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_edge_comparisons COMMAND bmca_tdd_edges)

# Test 3: Selection list behaviors
add_executable(bmca_tdd_selection
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_selection_list.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_tdd_selection PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_tdd_selection PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_selection_list COMMAND bmca_tdd_selection)

# Test 4: Message header validation branches
add_executable(msg_header_validation
    ${CMAKE_CURRENT_LIST_DIR}/test_message_header_validation.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(msg_header_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(msg_header_validation PRIVATE IEEE1588_2019_interface)
add_test(NAME ptp_message_header_validation COMMAND msg_header_validation)

# Test 5: Message bodies validation (error branches)
add_executable(msg_bodies_validation
    ${CMAKE_CURRENT_LIST_DIR}/test_message_bodies_validation.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(msg_bodies_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(msg_bodies_validation PRIVATE IEEE1588_2019_interface)
add_test(NAME ptp_message_bodies_validation COMMAND msg_bodies_validation)

# Test: Signaling message handling (TDD RED for CAP-20251109-04)
add_executable(signaling_message
    ${CMAKE_CURRENT_LIST_DIR}/test_signaling_message.cpp
)
target_include_directories(signaling_message PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(signaling_message PRIVATE IEEE1588_2019_interface)
add_test(NAME ptp_signaling_message_handling COMMAND signaling_message)

# Test 6: Offset calculation robustness
add_executable(offset_calculation_tests
    ${CMAKE_CURRENT_LIST_DIR}/test_offset_calculation.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(offset_calculation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(offset_calculation_tests PRIVATE IEEE1588_2019_interface)
add_test(NAME offset_calculation_robustness COMMAND offset_calculation_tests)

# Test 7: Fault injection - offset jitter
add_executable(fault_injection_offset
    ${CMAKE_CURRENT_LIST_DIR}/test_fault_injection_offset.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(fault_injection_offset PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(fault_injection_offset PRIVATE IEEE1588_2019_interface)
add_test(NAME fault_injection_offset_jitter COMMAND fault_injection_offset)

# Test 8: Fault injection - BMCA tie behavior
add_executable(fault_injection_bmca
    ${CMAKE_CURRENT_LIST_DIR}/test_fault_injection_bmca.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(fault_injection_bmca PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(fault_injection_bmca PRIVATE IEEE1588_2019_interface)
add_test(NAME fault_injection_bmca_tie COMMAND fault_injection_bmca)

# Test 9: Health/self-test aggregation
add_executable(health_selftest
    ${CMAKE_CURRENT_LIST_DIR}/test_health_selftest.cpp
)
target_include_directories(health_selftest PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(health_selftest PRIVATE IEEE1588_2019_interface)
add_test(NAME health_selftest COMMAND health_selftest)

# Test 10: Timestamp ordering assertions
add_executable(timestamp_ordering_assertions
    ${CMAKE_CURRENT_LIST_DIR}/test_timestamp_ordering_assertions.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(timestamp_ordering_assertions PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(timestamp_ordering_assertions PRIVATE IEEE1588_2019_interface)
add_test(NAME timestamp_ordering_assertions COMMAND timestamp_ordering_assertions)

# Test 11: Foreign master overflow guard
add_executable(foreign_master_overflow
    ${CMAKE_CURRENT_LIST_DIR}/test_foreign_master_overflow.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(foreign_master_overflow PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(foreign_master_overflow PRIVATE IEEE1588_2019_interface)
add_test(NAME foreign_master_overflow_guard COMMAND foreign_master_overflow)

# Test 12: Health heartbeat emission
add_executable(health_heartbeat
    ${CMAKE_CURRENT_LIST_DIR}/test_health_heartbeat.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(health_heartbeat PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(health_heartbeat PRIVATE IEEE1588_2019_interface)
add_test(NAME health_heartbeat_emission COMMAND health_heartbeat)

# Test 13: Sync heuristic tightening
add_executable(sync_heuristic_tightening
    ${CMAKE_CURRENT_LIST_DIR}/test_sync_heuristic_tightening.cpp
    ${CMAKE_SOURCE_DIR}/src/clocks.cpp
    ${CMAKE_SOURCE_DIR}/05-implementation/src/bmca.cpp
)
target_include_directories(sync_heuristic_tightening PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(sync_heuristic_tightening PRIVATE IEEE1588_2019_interface)
add_test(NAME sync_heuristic_tightening COMMAND sync_heuristic_tightening)

# Test 14: BMCA role assignment integration (RED)
add_executable(bmca_role_assignment
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_role_assignment.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_role_assignment PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_role_assignment PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_role_assignment_integration COMMAND bmca_role_assignment)

# Test 15: BMCA tie passive scenario (RED until passive logic implemented)
add_executable(bmca_tie_passive
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_tie_passive.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_tie_passive PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_tie_passive PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_tie_passive COMMAND bmca_tie_passive)

# Test 26: Forced tie passive scenario (REQ-F-202) - originally RED, now expected GREEN
add_executable(bmca_forced_tie_passive
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_forced_tie_passive_red.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_forced_tie_passive PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_forced_tie_passive PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_forced_tie_passive COMMAND bmca_forced_tie_passive)

# Test 27: Dataset accessors coherence (RED for REQ-F-205)
add_executable(datasets_accessors_red
    ${CMAKE_CURRENT_LIST_DIR}/test_datasets_accessors_red.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(datasets_accessors_red PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(datasets_accessors_red PRIVATE IEEE1588_2019_interface)
add_test(NAME datasets_accessors_red COMMAND datasets_accessors_red)

# Test 28: GAP-BMCA-001 - Priority vector ordering (RED for full BMCA compliance)
add_executable(bmca_priority_order_red
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_priority_order_red.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_priority_order_red PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_priority_order_red PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_priority_order_red COMMAND bmca_priority_order_red)

# Test 29: GAP-BMCA-001 - Role selection integration (RED for multi-master scenarios)
add_executable(bmca_role_selection_red
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_role_selection_red.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_role_selection_red PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_role_selection_red PRIVATE IEEE1588_2019_interface)
add_test(NAME bmca_role_selection_red COMMAND bmca_role_selection_red)

# Test 30: BMCA diagnostic helper (debug multi-master selection)
add_executable(bmca_diagnostic
    ${CMAKE_CURRENT_LIST_DIR}/test_bmca_diagnostic.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(bmca_diagnostic PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(bmca_diagnostic PRIVATE IEEE1588_2019_interface)

# Test 16: Delay_Resp processing
add_executable(delay_resp_processing
    ${CMAKE_CURRENT_LIST_DIR}/test_delay_resp_processing.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(delay_resp_processing PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(delay_resp_processing PRIVATE IEEE1588_2019_interface)
add_test(NAME delay_resp_processing COMMAND delay_resp_processing)

# Test 17: Timeout detection
add_executable(timeout_detection
    ${CMAKE_CURRENT_LIST_DIR}/test_timeout_detection.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(timeout_detection PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(timeout_detection PRIVATE IEEE1588_2019_interface)
add_test(NAME timeout_detection COMMAND timeout_detection)

# Test 18: Boundary clock routing
add_executable(boundary_clock_routing
    ${CMAKE_CURRENT_LIST_DIR}/test_boundary_clock_routing.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(boundary_clock_routing PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(boundary_clock_routing PRIVATE IEEE1588_2019_interface)
add_test(NAME boundary_clock_routing COMMAND boundary_clock_routing)

# Test 19: State-specific actions
add_executable(state_actions
    ${CMAKE_CURRENT_LIST_DIR}/test_state_actions.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(state_actions PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(state_actions PRIVATE IEEE1588_2019_interface)
add_test(NAME state_actions COMMAND state_actions)

# Test 20: Foreign master list management
add_executable(update_foreign_master
    ${CMAKE_CURRENT_LIST_DIR}/test_update_foreign_master.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(update_foreign_master PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(update_foreign_master PRIVATE IEEE1588_2019_interface)
add_test(NAME update_foreign_master COMMAND update_foreign_master)

# Test 21: Calculate offset and delay (T1/T2/T3/T4 algorithm)
add_executable(calculate_offset_and_delay
    ${CMAKE_CURRENT_LIST_DIR}/test_calculate_offset_and_delay.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(calculate_offset_and_delay PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(calculate_offset_and_delay PRIVATE IEEE1588_2019_interface)
add_test(NAME calculate_offset_and_delay COMMAND calculate_offset_and_delay)

# Test 22: OrdinaryClock message dispatcher (lines 912-946, 35-line coverage)
# BLOCKED: Commented out due to 60+ compilation errors with PTPMessage template structure
# Requires refactoring of message API or alternative testing approach
# add_executable(ordinary_clock_dispatcher
#     ${CMAKE_CURRENT_LIST_DIR}/test_ordinary_clock_dispatcher.cpp
#     ${BMCA_IMPL_SOURCES}
# )
# target_include_directories(ordinary_clock_dispatcher PRIVATE
#     ${CMAKE_SOURCE_DIR}/include
#     ${CMAKE_SOURCE_DIR}/05-implementation/src
# )
# target_link_libraries(ordinary_clock_dispatcher PRIVATE IEEE1588_2019_interface)
# add_test(NAME ordinary_clock_dispatcher COMMAND ordinary_clock_dispatcher)

# Test 23: Configuration setters (lines 855-874, 20-line coverage)
add_executable(configuration_setters
    ${CMAKE_CURRENT_LIST_DIR}/test_configuration_setters.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(configuration_setters PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(configuration_setters PRIVATE IEEE1588_2019_interface)
add_test(NAME configuration_setters COMMAND configuration_setters)

# Test 24: Master delay response (lines 472-491, 20-line coverage)
add_executable(master_delay_response
    ${CMAKE_CURRENT_LIST_DIR}/test_master_delay_response.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(master_delay_response PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(master_delay_response PRIVATE IEEE1588_2019_interface)
add_test(NAME master_delay_response COMMAND master_delay_response)

# Test 25: BoundaryClock query helpers (lines 1065-1186, ~120-line coverage)
add_executable(boundary_clock_queries
    ${CMAKE_CURRENT_LIST_DIR}/test_boundary_clock_queries.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(boundary_clock_queries PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(boundary_clock_queries PRIVATE IEEE1588_2019_interface)
add_test(NAME boundary_clock_queries COMMAND boundary_clock_queries)

message(STATUS "Phase 05 tests configured: bmca_tdd_tests, bmca_tdd_edges, bmca_tdd_selection, msg_header_validation, msg_bodies_validation, offset_calculation_tests, delay_resp_processing, timeout_detection, boundary_clock_routing, state_actions, update_foreign_master, calculate_offset_and_delay, configuration_setters, master_delay_response, boundary_clock_queries")

# Security: No buffer overruns (STR-SEC-002)
add_executable(no_buffer_overruns
    ${CMAKE_CURRENT_LIST_DIR}/test_no_buffer_overruns.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(no_buffer_overruns PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(no_buffer_overruns PRIVATE IEEE1588_2019_interface)
add_test(NAME no_buffer_overruns COMMAND no_buffer_overruns)

# Performance: Resource efficiency evidence (STR-PERF-005)
add_executable(resource_efficiency
    ${CMAKE_CURRENT_LIST_DIR}/test_resource_efficiency.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(resource_efficiency PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(resource_efficiency PRIVATE IEEE1588_2019_interface)
add_test(NAME resource_efficiency COMMAND resource_efficiency)

# Portability: Reference HALs / DI compile evidence (STR-PORT-002)
add_executable(dependency_injection_compile
    ${CMAKE_CURRENT_LIST_DIR}/test_dependency_injection_compile.cpp
)
target_include_directories(dependency_injection_compile PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(dependency_injection_compile PRIVATE IEEE1588_2019)
add_test(NAME dependency_injection_compile COMMAND dependency_injection_compile)

# Usability: API docs presence (STR-USE-001)
add_executable(api_docs_presence
    ${CMAKE_CURRENT_LIST_DIR}/test_api_docs_presence.cpp
)
target_include_directories(api_docs_presence PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(api_docs_presence PRIVATE IEEE1588_2019_interface)
target_compile_definitions(api_docs_presence PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME api_docs_presence COMMAND api_docs_presence)

# Usability: Getting started/tutorial presence (STR-USE-002) and Maintainability: CI & contribution (STR-MAINT-002, STR-MAINT-004)
add_executable(getting_started_and_ci_presence
    ${CMAKE_CURRENT_LIST_DIR}/test_getting_started_and_ci_presence.cpp
)
target_include_directories(getting_started_and_ci_presence PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(getting_started_and_ci_presence PRIVATE IEEE1588_2019_interface)
target_compile_definitions(getting_started_and_ci_presence PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME getting_started_and_ci_presence COMMAND getting_started_and_ci_presence)

# Performance: Servo convergence deterministic simulation (STR-PERF-003)
add_executable(servo_convergence_sim
    ${CMAKE_CURRENT_LIST_DIR}/test_servo_convergence_sim.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(servo_convergence_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(servo_convergence_sim PRIVATE IEEE1588_2019_interface)
add_test(NAME servo_convergence_sim COMMAND servo_convergence_sim)

# Standards: Interoperability evidence placeholder (STR-STD-004)
add_executable(interoperability_evidence
    ${CMAKE_CURRENT_LIST_DIR}/test_interoperability_evidence.cpp
)
target_include_directories(interoperability_evidence PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(interoperability_evidence PRIVATE IEEE1588_2019_interface)
target_compile_definitions(interoperability_evidence PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME interoperability_evidence COMMAND interoperability_evidence)

# Acceptance-style: Multi-instance BMCA + Sync/Delay evidence (STR-STD-003, STR-PERF-003)
add_executable(multi_instance_bmca_sync
    ${CMAKE_CURRENT_LIST_DIR}/test_multi_instance_bmca_sync.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(multi_instance_bmca_sync PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(multi_instance_bmca_sync PRIVATE IEEE1588_2019_interface)
add_test(NAME multi_instance_bmca_sync COMMAND multi_instance_bmca_sync)

 # Cross-platform build system presence evidence (@satisfies STR-PORT-004)
 add_executable(build_system_presence test_build_system_presence.cpp)
 add_test(NAME build_system_presence COMMAND build_system_presence)

# Timing determinism evidence (@satisfies STR-PERF-002)
add_executable(timing_determinism
    ${CMAKE_CURRENT_LIST_DIR}/test_timing_determinism.cpp
    ${BMCA_IMPL_SOURCES}
)
target_include_directories(timing_determinism PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/05-implementation/src
)
target_link_libraries(timing_determinism PRIVATE IEEE1588_2019_interface)
add_test(NAME timing_determinism COMMAND timing_determinism)

# ADR presence evidence (@satisfies STR-MAINT-003)
add_executable(adrs_presence test_adrs_presence.cpp)
target_compile_definitions(adrs_presence PRIVATE REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME adrs_presence COMMAND adrs_presence)

# OS assumptions scan (@satisfies STR-PORT-003)
add_executable(os_assumptions test_os_assumptions.cpp)
add_test(NAME os_assumptions COMMAND os_assumptions)

# Example applications presence (@satisfies STR-USE-003)
add_executable(example_app_presence test_example_app_presence.cpp)
target_compile_definitions(example_app_presence PRIVATE REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME example_app_presence COMMAND example_app_presence)

# Porting guide presence (@satisfies STR-USE-004)
add_executable(porting_guide_presence ${CMAKE_CURRENT_LIST_DIR}/test_porting_guide_presence.cpp)
target_compile_definitions(porting_guide_presence PRIVATE REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME porting_guide_presence COMMAND porting_guide_presence)

# -------------------------------------------------------------
# Acceptance Test Skeletons (Profile-Agnostic) - StR mapping
# -------------------------------------------------------------
# AT-001: P2P enforcement & E2E rejection (StR-001, StR-009)
add_executable(at_p2p_enforcement ${CMAKE_CURRENT_LIST_DIR}/test_acceptance_p2p_enforcement.cpp)
target_include_directories(at_p2p_enforcement PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(at_p2p_enforcement PRIVATE IEEE1588_2019_interface)
add_test(NAME at_p2p_enforcement COMMAND at_p2p_enforcement)

# AT-002: Path Trace continuity (StR-004)
add_executable(at_path_trace_continuity ${CMAKE_CURRENT_LIST_DIR}/test_acceptance_path_trace_continuity.cpp)
target_include_directories(at_path_trace_continuity PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(at_path_trace_continuity PRIVATE IEEE1588_2019_interface)
add_test(NAME at_path_trace_continuity COMMAND at_path_trace_continuity)

# AT-003: Convergence performance (<1s to <1µs) (StR-019, StR-010)
add_executable(at_convergence_performance ${CMAKE_CURRENT_LIST_DIR}/test_acceptance_convergence_performance.cpp)
target_include_directories(at_convergence_performance PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(at_convergence_performance PRIVATE IEEE1588_2019_interface)
add_test(NAME at_convergence_performance COMMAND at_convergence_performance)

# AT-004: Multi-domain + CMLDS scaffolding (StR-011, StR-012, StR-016, StR-017)
add_executable(at_multi_domain_cmlds ${CMAKE_CURRENT_LIST_DIR}/test_acceptance_multi_domain_cmlds.cpp)
target_include_directories(at_multi_domain_cmlds PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(at_multi_domain_cmlds PRIVATE IEEE1588_2019_interface)
add_test(NAME at_multi_domain_cmlds COMMAND at_multi_domain_cmlds)

# AT-005: Timestamp accuracy placeholder (≤8ns) (StR-018)
add_executable(at_timestamp_accuracy ${CMAKE_CURRENT_LIST_DIR}/test_acceptance_timestamp_accuracy.cpp)
target_include_directories(at_timestamp_accuracy PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(at_timestamp_accuracy PRIVATE IEEE1588_2019_interface)
add_test(NAME at_timestamp_accuracy COMMAND at_timestamp_accuracy)
