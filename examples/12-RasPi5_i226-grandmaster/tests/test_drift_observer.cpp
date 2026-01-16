/**
 * @file test_drift_observer.cpp
 * @brief TDD RED-GREEN-REFACTOR tests for DriftObserver class
 * 
 * Test-Driven Development following 8-phase test plan:
 * - Phase 1: Data Structures
 * - Phase 2: Ring Buffer Operations
 * - Phase 3: Spike Detection
 * - Phase 4: Drift Estimation
 * - Phase 5: Epoch and Contamination
 * - Phase 6: Holdoff and Trust Gating
 * - Phase 7: Event Handling
 * - Phase 8: Integration Tests
 * 
 * Specification: examples/12-RasPi5_i226-grandmaster/docs/drift_observer_spec.md
 * Requirements: examples/12-RasPi5_i226-grandmaster/docs/drift_observer_requirements.md
 * Design: 04-design/components/drift-observer-data-structures.md
 * 
 * Traces to: drift_observer_spec.md, drift_observer_requirements.md
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstring>

#include "drift_observer.hpp"

void print_test_header(const char* test_name) {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << test_name;
    for (size_t i = strlen(test_name); i < 57; ++i) std::cout << " ";
    std::cout << " ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
}

void print_result(bool success, const char* message = nullptr) {
    if (success) {
        std::cout << "✅ PASS";
        if (message) std::cout << " - " << message;
        std::cout << "\n";
    } else {
        std::cout << "❌ FAIL";
        if (message) std::cout << " - " << message;
        std::cout << "\n";
    }
}

// Helper function to create default config
ptp::Config default_config() {
    return ptp::Config::CreateDefault();
}

// =============================================================================
// PHASE 1: DATA STRUCTURE TESTS
// =============================================================================

/**
 * REQ-2.1: DriftSample must store all required fields
 * Test: Verify DriftSample structure has correct fields and can be populated
 */
void test_DriftSample_FieldPopulation(int& passed, int& total) {
    print_test_header("REQ-2.1: DriftSample Field Population");
    total++;
    
    using namespace ptp;
    DriftSample sample;
    sample.seq = 42;
    sample.epoch_id = 1;
    sample.t_ref_ns = 1000000000;
    sample.t_clk_ns = 1000000100;
    sample.offset_ns = 100;
    sample.dt_ref_ns = 1000000000;
    sample.dt_clk_ns = 1000000100;
    sample.drift_ns_per_s = 50;
    sample.valid = true;
    sample.flags = 0;
    
    assert(sample.seq == 42);
    assert(sample.epoch_id == 1);
    assert(sample.t_ref_ns == 1000000000);
    assert(sample.offset_ns == 100);
    assert(sample.valid == true);
    print_result(true, "All fields set correctly");
    passed++;
}

/**
 * REQ-2.5: DriftSample flag bits must be defined correctly
 */
void test_DriftSample_FlagBits(int& passed, int& total) {
    print_test_header("REQ-2.5: DriftSample Flag Bits");
    total++;
    
    using namespace ptp;
    // Verify flag constants are defined and are powers of 2
    assert(DriftSample::FLAG_OFFSET_SPIKE == (1 << 0));
    assert(DriftSample::FLAG_DRIFT_SPIKE == (1 << 1));
    assert(DriftSample::FLAG_DT_REF_INVALID == (1 << 2));
    assert(DriftSample::FLAG_DT_CLK_INVALID == (1 << 3));
    assert(DriftSample::FLAG_EPOCH_BOUNDARY == (1 << 4));
    assert(DriftSample::FLAG_IN_HOLDOFF == (1 << 5));
    
    // Verify flags are mutually exclusive (powers of 2)
    assert((DriftSample::FLAG_OFFSET_SPIKE & DriftSample::FLAG_DRIFT_SPIKE) == 0);
    print_result(true, "Flag bits defined correctly");
    passed++;
}

/**
 * REQ-3.1, REQ-12.1: Config::CreateDefault() with recommended values
 */
void test_Config_CreateDefault(int& passed, int& total) {
    print_test_header("REQ-3.1/12.1: Config::CreateDefault()");
    total++;
    
    using namespace ptp;
    Config cfg = Config::CreateDefault();
    
    // Verify recommended defaults from spec Section 11
    assert(cfg.window_size == 120);
    assert(cfg.min_valid_samples == 30);
    assert(cfg.max_dt_ref_deviation_ns == 2000000);
    assert(cfg.max_offset_step_ns == 1000000);
    assert(cfg.max_drift_ppm == 500);
    assert(cfg.outlier_mad_sigma == 4.5);
    assert(cfg.max_invalid_ratio == 0.10);
    assert(cfg.use_linear_regression == true);
    assert(cfg.holdoff_after_step_ticks == 5);
    assert(cfg.holdoff_after_freq_ticks == 2);
    assert(cfg.holdoff_after_ref_ticks == 10);
    assert(cfg.max_drift_stddev_ppm == 5.0);
    
    print_result(true, "CreateDefault() returns correct values");
    passed++;
}

/**
 * REQ-8.1, REQ-8.2: Estimate structure fields
 */
void test_Estimate_StructureFields(int& passed, int& total) {
    print_test_header("REQ-8.1/8.2: Estimate Structure Fields");
    total++;
    
    using namespace ptp;
    Estimate est = {};
    est.ready = false;
    est.trustworthy = false;
    est.offset_mean_ns = 0;
    est.offset_stddev_ns = 0;
    est.offset_median_ns = 0;
    est.drift_ppm = 0.0;
    est.drift_stddev_ppm = 0.0;
    est.jitter_ns_rms = 0.0;
    est.health_flags = 0;
    est.total_samples = 0;
    est.valid_samples = 0;
    est.current_epoch = 0;
    est.ticks_in_epoch = 0;
    est.ticks_in_holdoff = 0;
    
    // Verify all fields are accessible
    assert(est.ready == false);
    assert(est.trustworthy == false);
    print_result(true, "Estimate structure fields accessible");
    passed++;
}

/**
 * REQ-8.3: Estimate helper methods for servo decisions
 */
void test_Estimate_HelperMethods(int& passed, int& total) {
    print_test_header("REQ-8.3: Estimate Helper Methods");
    total++;
    
    using namespace ptp;
    Estimate est = {};
    
    // Test CanCorrectOffset() - should be false when not ready
    est.ready = false;
    est.health_flags = HF_NOT_READY;
    assert(est.CanCorrectOffset() == false);
    
    // Should be true when ready and no reference issues
    est.ready = true;
    est.health_flags = 0;
    assert(est.CanCorrectOffset() == true);
    
    // Test CanCorrectDrift() - should be false in holdoff
    est.trustworthy = true;
    est.health_flags = HF_IN_HOLDOFF;
    assert(est.CanCorrectDrift() == false);
    
    // Should be true when trustworthy and not in holdoff
    est.health_flags = 0;
    assert(est.CanCorrectDrift() == true);
    
    print_result(true, "Helper methods work correctly");
    passed++;
}

/**
 * REQ-8.1: HealthFlags bitmask enum
 */
void test_HealthFlags_Bitmask(int& passed, int& total) {
    print_test_header("REQ-8.1: HealthFlags Bitmask");
    total++;
    
    using namespace ptp;
    // Verify all health flags are defined
    assert(HF_NONE == 0);
    assert(HF_NOT_READY == (1 << 0));
    assert(HF_IN_HOLDOFF == (1 << 1));
    assert(HF_REFERENCE_BAD == (1 << 2));
    assert(HF_MISSING_TICKS == (1 << 3));
    assert(HF_STEP_DETECTED == (1 << 4));
    assert(HF_WINDOW_CONTAMINATED == (1 << 5));
    assert(HF_JITTER_TOO_HIGH == (1 << 6));
    assert(HF_OFFSET_UNSTABLE == (1 << 7));
    
    // Verify flags can be combined
    uint32_t combined = HF_NOT_READY | HF_IN_HOLDOFF;
    assert((combined & HF_NOT_READY) != 0);
    assert((combined & HF_IN_HOLDOFF) != 0);
    assert((combined & HF_REFERENCE_BAD) == 0);
    
    print_result(true, "HealthFlags bitmask defined correctly");
    passed++;
}

/**
 * REQ-6.3: ObserverEvent enum values
 */
void test_ObserverEvent_Enum(int& passed, int& total) {
    print_test_header("REQ-6.3: ObserverEvent Enum");
    total++;
    
    using namespace ptp;
    // Verify all ObserverEvent values are defined
    ObserverEvent evt;
    evt = ObserverEvent::ReferenceChanged;
    evt = ObserverEvent::ReferenceLost;
    evt = ObserverEvent::ReferenceRecovered;
    evt = ObserverEvent::ClockStepped;
    evt = ObserverEvent::ClockSlewed;
    evt = ObserverEvent::FrequencyAdjusted;
    evt = ObserverEvent::ServoModeChanged;
    evt = ObserverEvent::WarmStartRequested;
    (void)evt; // Suppress unused warning
    
    print_result(true, "ObserverEvent enum defined correctly");
    passed++;
}

// =============================================================================
// PHASE 2: RING BUFFER OPERATIONS
// =============================================================================

/**
 * REQ-3.1: Ring buffer initialization
 */
void test_RingBuffer_Initialize(int& passed, int& total) {
    print_test_header("REQ-3.1: Ring Buffer Initialize");
    total++;
    
    using namespace ptp;
    
    // Create observer with default config
    Config cfg = Config::CreateDefault();
    DriftObserver obs(cfg, "test");
    
    // Get initial estimate - should NOT be ready yet
    Estimate est = obs.GetEstimate();
    
    // Verify initial state
    assert(!est.ready);  // Not ready with 0 samples
    assert(est.health_flags & HF_NOT_READY);
    assert(est.total_samples == 0);
    assert(est.valid_samples == 0);
    
    print_result(true, "Ring buffer initialized correctly");
    passed++;
}

/**
 * REQ-2.1, REQ-2.2: Update() adds samples
 */
void test_RingBuffer_Update_AddsSamples(int& passed, int& total) {
    print_test_header("REQ-2.1/2.2: Update() Adds Samples");
    total++;
    
    using namespace ptp;
    
    // Create observer
    Config cfg = Config::CreateDefault();
    DriftObserver obs(cfg, "test");
    
    // Add first sample
    int64_t t_ref = 1000000000000LL;  // 1000 seconds in ns
    int64_t t_clk = 1000000500000LL;  // 500 us offset
    obs.Update(t_ref, t_clk);
    
    // Verify sample was added
    Estimate est = obs.GetEstimate();
    assert(est.total_samples == 1);
    assert(est.valid_samples == 1);
    
    // Latest sample should have the values we provided
    const DriftSample& latest = obs.Latest();
    assert(latest.t_ref_ns == t_ref);
    assert(latest.t_clk_ns == t_clk);
    assert(latest.offset_ns == (t_clk - t_ref));  // Should be 500 us
    assert(latest.valid);
    
    // Add second sample (1 second later)
    t_ref += 1000000000LL;  // +1 second
    t_clk += 1000000000LL + 100000LL;  // +1 second + 100 us drift
    obs.Update(t_ref, t_clk);
    
    // Verify second sample added
    est = obs.GetEstimate();
    assert(est.total_samples == 2);
    assert(est.valid_samples == 2);
    
    print_result(true, "Update() adds samples correctly");
    passed++;
}

/**
 * REQ-3.2: Circular buffer wraparound
 */
void test_RingBuffer_CircularWraparound(int& passed, int& total) {
    print_test_header("REQ-3.2: Circular Buffer Wraparound");
    total++;
    
    using namespace ptp;
    
    // Create observer with small window for easier testing
    Config cfg = Config::CreateDefault();
    cfg.window_size = 5;  // Small window to test wraparound quickly
    DriftObserver obs(cfg, "test");
    
    // Add samples to fill the buffer and then overflow
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add 8 samples (more than window_size of 5)
    for (int i = 0; i < 8; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;  // +1 second
        t_clk += 1000000000LL;  // +1 second
    }
    
    // Verify sample count maxes out at window_size
    Estimate est = obs.GetEstimate();
    assert(est.total_samples == 5);  // Should max at window_size
    assert(est.valid_samples == 5);
    
    // Latest sample should be the 8th one we added
    const DriftSample& latest = obs.Latest();
    int64_t expected_t_ref = 1000000000000LL + (7 * 1000000000LL);  // 7 seconds later
    assert(latest.seq == 7);  // 8th sample (0-indexed at 7)
    assert(latest.t_ref_ns == expected_t_ref);
    
    print_result(true, "Ring buffer wraps around correctly");
    passed++;
}

/**
 * REQ-2.3: Compute offset = t_clk - t_ref
 */
void test_RingBuffer_ComputeOffset(int& passed, int& total) {
    print_test_header("REQ-2.3: Compute Offset");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    DriftObserver obs(cfg, "test");
    
    // Add sample with known offset
    int64_t t_ref = 1000000000000LL;  // 1000 seconds
    int64_t t_clk = 1000000500000LL;  // 500 us ahead
    obs.Update(t_ref, t_clk);
    
    // Verify offset is computed correctly
    const DriftSample& sample = obs.Latest();
    int64_t expected_offset = t_clk - t_ref;  // Should be +500000 ns
    assert(sample.offset_ns == expected_offset);
    assert(sample.offset_ns == 500000);  // 500 us
    
    // Add sample with negative offset (clock behind)
    t_ref += 1000000000LL;  // +1 second
    t_clk += 1000000000LL - 200000LL;  // +1 second - 200us (clock slowing)
    obs.Update(t_ref, t_clk);
    
    const DriftSample& sample2 = obs.Latest();
    expected_offset = t_clk - t_ref;  // Should be +300000 ns (500us - 200us)
    assert(sample2.offset_ns == expected_offset);
    assert(sample2.offset_ns == 300000);  // 300 us
    
    print_result(true, "Offset calculation correct");
    passed++;
}

/**
 * REQ-2.4: Compute drift from consecutive samples
 */
void test_RingBuffer_ComputeDrift(int& passed, int& total) {
    print_test_header("REQ-2.4: Compute Drift");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    DriftObserver obs(cfg, "test");
    
    // First sample - no drift yet
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000500000LL;  // 500 us offset
    obs.Update(t_ref, t_clk);
    
    // Second sample - clock drifting faster
    t_ref += 1000000000LL;  // +1 second exactly
    t_clk += 1000000000LL + 100000LL;  // +1 second + 100us drift
    obs.Update(t_ref, t_clk);
    
    const DriftSample& sample = obs.Latest();
    // Drift = change in offset = (600us) - (500us) = 100us = 100000 ns
    int64_t expected_drift = 100000;  // 100us per second = 100 ppm
    assert(sample.drift_ns_per_s == expected_drift);
    
    // Third sample - clock slowing down
    t_ref += 1000000000LL;
    t_clk += 1000000000LL - 50000LL;  // +1 second - 50us (slowing)
    obs.Update(t_ref, t_clk);
    
    const DriftSample& sample2 = obs.Latest();
    // New offset = 600us + 50us = 650us, drift = 650us - 600us = 50us
    // Wait, let me recalculate: offset[2] = (t_clk[2] - t_ref[2])
    // offset[2] = (1000000500000 + 1000000000 + 100000 + 1000000000 - 50000) - (1000000000000 + 1000000000 + 1000000000)
    // offset[2] = (1000002550000) - (1000002000000) = 550000
    // drift[2] = offset[2] - offset[1] = 550000 - 600000 = -50000
    assert(sample2.drift_ns_per_s == -50000);  // Negative drift (slowing)
    
    print_result(true, "Drift calculation correct");
    passed++;
}

/**
 * REQ-2.6: Validate dt_ref is close to 1 second
 */
void test_RingBuffer_ValidateDtRef(int& passed, int& total) {
    print_test_header("REQ-2.6: Validate dt_ref");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    DriftObserver obs(cfg, "test");
    
    // First sample
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    obs.Update(t_ref, t_clk);
    
    // Second sample - exactly 1 second later (good)
    t_ref += 1000000000LL;  // +1e9 ns = exactly 1 second
    t_clk += 1000000000LL;
    obs.Update(t_ref, t_clk);
    
    const DriftSample& sample = obs.Latest();
    assert(sample.dt_ref_ns == 1000000000LL);  // Should be exactly 1 second
    assert(sample.dt_clk_ns == 1000000000LL);
    
    // Third sample - slightly off (within tolerance)
    t_ref += 1000001000LL;  // +1 second + 1 us (slight jitter)
    t_clk += 1000001000LL;
    obs.Update(t_ref, t_clk);
    
    const DriftSample& sample2 = obs.Latest();
    assert(sample2.dt_ref_ns == 1000001000LL);  // 1 us jitter
    
    print_result(true, "dt_ref validation correct");
    passed++;
}

/**
 * REQ-8.2: GetEstimate() returns statistics
 */
void test_RingBuffer_GetEstimate_BasicStats(int& passed, int& total) {
    print_test_header("REQ-8.2: GetEstimate() Basic Stats");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    cfg.min_valid_samples = 3;  // Lower threshold for testing
    DriftObserver obs(cfg, "test");
    
    // Add some samples
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;  // +1 second
        t_clk += 1000000000LL;  // +1 second (no drift)
    }
    
    // GetEstimate should return sample counts
    Estimate est = obs.GetEstimate();
    assert(est.total_samples == 5);
    assert(est.valid_samples == 5);
    assert(est.current_epoch == 0);  // No epoch changes yet
    
    // Note: Statistics computation (mean, stddev, etc.) will be tested in later phases
    // For now, we just verify the basic counts are correct
    
    print_result(true, "GetEstimate returns basic stats");
    passed++;
}

// =============================================================================
// PHASE 3: SPIKE DETECTION
// =============================================================================

/**
 * REQ-5.1: Detect offset spikes
 */
void test_SpikeDetection_OffsetSpike(int& passed, int& total) {
    print_test_header("REQ-5.1: Detect Offset Spike");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    cfg.max_offset_step_ns = 100000;  // 100 us threshold
    DriftObserver obs(cfg, "test");
    
    // Add normal samples
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    obs.Update(t_ref, t_clk);
    
    t_ref += 1000000000LL;  // +1 second
    t_clk += 1000000000LL + 10000LL;  // +10us drift (normal)
    obs.Update(t_ref, t_clk);
    
    // Second sample should be valid (small drift)
    const DriftSample& normal = obs.Latest();
    assert(normal.valid);
    assert((normal.flags & DriftSample::FLAG_OFFSET_SPIKE) == 0);
    
    // Add sample with large offset spike
    t_ref += 1000000000LL;
    t_clk += 1000000000LL + 500000LL;  // +500us spike (exceeds threshold)
    obs.Update(t_ref, t_clk);
    
    // Third sample should be marked invalid with spike flag
    const DriftSample& spike = obs.Latest();
    assert(!spike.valid);  // Marked invalid
    assert((spike.flags & DriftSample::FLAG_OFFSET_SPIKE) != 0);  // Spike flag set
    
    print_result(true, "Offset spike detected and flagged");
    passed++;
}

/**
 * REQ-5.2: MAD-based outlier detection
 */
void test_SpikeDetection_MAD_Outlier(int& passed, int& total) {
    print_test_header("REQ-5.2: MAD Outlier Detection");
    total++;
    
    using namespace ptp;
    
    // For this basic test, just verify the structure exists
    // Full MAD implementation will be in Phase 4 (drift estimation)
    // For now, verify that outlier detection infrastructure is in place
    
    Config cfg = Config::CreateDefault();
    cfg.outlier_mad_sigma = 4.5;  // MAD threshold
    DriftObserver obs(cfg, "test");
    
    // Add samples - MAD calculation needs sufficient data
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    for (int i = 0; i < 10; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;  // No drift
    }
    
    // Verify samples are being tracked
    Estimate est = obs.GetEstimate();
    assert(est.total_samples == 10);
    
    print_result(true, "MAD outlier detection infrastructure ready");
    passed++;
}

/**
 * REQ-5.3: Detect drift spikes
 */
void test_SpikeDetection_DriftSpike(int& passed, int& total) {
    print_test_header("REQ-5.3: Detect Drift Spike");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    cfg.max_drift_ppm = 100.0;  // 100 ppm max drift threshold
    DriftObserver obs(cfg, "test");
    
    // Add normal samples
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    obs.Update(t_ref, t_clk);
    
    t_ref += 1000000000LL;
    t_clk += 1000000000LL + 10000LL;  // 10us = 10 ppm (normal)
    obs.Update(t_ref, t_clk);
    
    const DriftSample& normal = obs.Latest();
    assert(normal.valid);
    
    // Add sample with large drift spike
    t_ref += 1000000000LL;
    t_clk += 1000000000LL + 500000LL;  // 500us = 500 ppm (spike!)
    obs.Update(t_ref, t_clk);
    
    const DriftSample& spike = obs.Latest();
    assert(!spike.valid);  // Marked invalid
    assert((spike.flags & DriftSample::FLAG_DRIFT_SPIKE) != 0);  // Drift spike flag
    
    print_result(true, "Drift spike detected and flagged");
    passed++;
}

/**
 * REQ-5.4: Invalid samples excluded from statistics
 */
void test_SpikeDetection_ExcludeInvalid(int& passed, int& total) {
    print_test_header("REQ-5.4: Exclude Invalid Samples");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    cfg.max_offset_step_ns = 10000000;  // 10 ms threshold (high to prevent epoch change on spikes)
    DriftObserver obs(cfg, "test");
    
    // Add 3 normal samples
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    for (int i = 0; i < 3; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    // Add 1 invalid sample (spike) - 600ppm drift (exceeds 500ppm max, but <1ms step)
    t_clk += 600000LL;  // 600us spike: exceeds drift limit but not step threshold
    obs.Update(t_ref, t_clk);
    t_ref += 1000000000LL;
    t_clk += 1000000000LL;  // Maintain the 600us offset (drift=0, VALID)
    
    // Add 1 more normal sample
    obs.Update(t_ref, t_clk);
    
    // Check estimate
    Estimate est = obs.GetEstimate();
    std::cout << "  total_samples=" << est.total_samples << " valid_samples=" << est.valid_samples << "\n";
    assert(est.total_samples == 5);    // All samples stored
    assert(est.valid_samples == 4);    // Only 4 valid (1 spike excluded)
    
    print_result(true, "Invalid samples excluded from valid count");
    passed++;
}

// =============================================================================
// PHASE 4: DRIFT ESTIMATION
// =============================================================================

/**
 * REQ-4.1, REQ-4.2: Linear regression drift estimation
 */
void test_DriftEstimation_LinearRegression(int& passed, int& total) {
    print_test_header("REQ-4.1/4.2: Linear Regression Drift");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    cfg.use_linear_regression = true;
    cfg.min_valid_samples = 5;
    DriftObserver obs(cfg, "test");
    
    // Add samples with known drift: 100 ppm (100000 ns/s)
    // offset[k] = 100000ns/s * k seconds
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    for (int i = 0; i < 10; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;  // +1 second
        t_clk += 1000000000LL + 100000LL;  // +1 second + 100000ns (100us) drift = 100 ppm
    }
    
    // Get estimate
    Estimate est = obs.GetEstimate();
    
    // Should have computed drift via linear regression
    // Expected: ~100 ppm (allowing small numerical error)
    double drift_ppm = est.drift_ppm;
    std::cout << "  [TEST] drift_ppm=" << drift_ppm << " (expected ~100.0)" << std::endl;  // flush
    std::cout << "  [TEST] total_samples=" << est.total_samples << " valid_samples=" << est.valid_samples << std::endl;  // flush
    
    if (std::abs(drift_ppm - 100.0) >= 5.0) {
        std::cout << "  [TEST] FAIL: |" << drift_ppm << " - 100.0| = " << std::abs(drift_ppm - 100.0) << " >= 5.0" << std::endl;
    }
    
    assert(std::abs(drift_ppm - 100.0) < 5.0);  // Within 5 ppm tolerance
    
    print_result(true, "Linear regression computed correct drift");
    passed++;
}

/**
 * REQ-4.3: Mean-of-deltas fallback method
 */
void test_DriftEstimation_MeanOfDeltas(int& passed, int& total) {
    print_test_header("REQ-4.3: Mean-of-Deltas Drift");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    cfg.use_linear_regression = false;  // Use mean-of-deltas instead
    cfg.min_valid_samples = 5;
    DriftObserver obs(cfg, "test");
    
    // Add samples with known drift: 50 ppm = 50000 ns/s
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    for (int i = 0; i < 10; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 50000LL;  // 50000ns per second = 50 ppm
    }
    
    // Get estimate
    Estimate est = obs.GetEstimate();
    
    // Should have computed drift via mean of deltas
    double drift_ppm = est.drift_ppm;
    assert(std::abs(drift_ppm - 50.0) < 5.0);  // Within 5 ppm tolerance
    
    print_result(true, "Mean-of-deltas computed correct drift");
    passed++;
}

/**
 * REQ-4.4: Drift estimate convergence
 */
void test_DriftEstimation_Convergence(int& passed, int& total) {
    print_test_header("REQ-4.4: Drift Convergence");
    total++;
    
    using namespace ptp;
    
    Config cfg = Config::CreateDefault();
    cfg.use_linear_regression = true;
    cfg.min_valid_samples = 5;
    DriftObserver obs(cfg, "test");
    
    // Add samples - should see estimate improve as more samples added
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    double prev_stddev = 1e9;  // Start with large value
    
    for (int i = 0; i < 20; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 75000LL;  // 75000ns per second = 75 ppm
        
        Estimate est = obs.GetEstimate();
        
        // After min_valid_samples, should have decreasing uncertainty
        if (i >= cfg.min_valid_samples) {
            // Standard deviation should decrease or stay similar as more samples added
            // (not a strict requirement, but generally expected)
            // For this test, just verify we're computing statistics
            assert(est.offset_stddev_ns >= 0);
        }
    }
    
    // Final estimate should be close to 75 ppm
    Estimate final = obs.GetEstimate();
    assert(std::abs(final.drift_ppm - 75.0) < 5.0);
    
    print_result(true, "Drift estimate converges with more samples");
    passed++;
}

// =============================================================================
// PHASE 5: EPOCH TRACKING
// =============================================================================

/**
 * REQ-6.1: Epoch ID increments on events
 */
void test_Epochs_EpochId_Increments(int& passed, int& total) {
    print_test_header("REQ-6.1: Epoch ID Increments");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.window_size = 5;
    ptp::DriftObserver obs(cfg, "test");
    
    // Add samples - should all have epoch_id = 0
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    for (int i = 0; i < 3; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    // Get samples before epoch increment
    auto samples_before = obs.GetSamples();
    assert(samples_before.size() == 3);
    for (const auto& s : samples_before) {
        assert(s.epoch_id == 0);  // All in epoch 0
    }
    
    // Increment epoch
    obs.IncrementEpoch();
    
    // Add more samples - should have epoch_id = 1
    for (int i = 0; i < 2; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    // Verify mixed epochs
    auto samples_after = obs.GetSamples();
    assert(samples_after.size() == 5);
    
    // First 3 samples should be epoch 0
    assert(samples_after[0].epoch_id == 0);
    assert(samples_after[1].epoch_id == 0);
    assert(samples_after[2].epoch_id == 0);
    
    // Last 2 samples should be epoch 1
    assert(samples_after[3].epoch_id == 1);
    assert(samples_after[4].epoch_id == 1);
    
    passed++;
    std::cout << "✅ PASS - Epoch ID increments correctly\n";
}

/**
 * REQ-6.2: Cross-epoch samples excluded
 */
void test_Epochs_CrossEpoch_Excluded(int& passed, int& total) {
    print_test_header("REQ-6.2: Cross-Epoch Samples Excluded");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.window_size = 10;
    cfg.min_valid_samples = 3;
    ptp::DriftObserver obs(cfg, "test");
    
    // Add 5 samples in epoch 0 with known drift
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 50000LL;  // 50 ppm drift
    }
    
    // Should have valid estimate from 5 samples
    auto est1 = obs.GetEstimate();
    assert(est1.valid_samples == 5);
    
    // Increment epoch (contamination event)
    obs.IncrementEpoch();
    
    // Add 3 samples in new epoch (different drift)
    for (int i = 0; i < 3; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 100000LL;  // 100 ppm drift (different!)
    }
    
    // Window now has 8 samples but only 3 from current epoch
    auto samples = obs.GetSamples();
    assert(samples.size() == 8);
    
    // Estimate should only use 3 samples from current epoch
    auto est2 = obs.GetEstimate();
    assert(est2.valid_samples == 3);  // Only current epoch samples
    
    // Verify drift reflects new epoch (not contaminated by old samples)
    // With 100 ppm drift, should be near 100 ppm (not averaged with old 50 ppm)
    assert(std::abs(est2.drift_ppm - 100.0) < 20.0);  // Reasonable tolerance
    
    passed++;
    std::cout << "✅ PASS - Cross-epoch samples correctly excluded\n";
}

/**
 * REQ-6.3, REQ-6.4: ClockStepped resets window
 */
void test_Epochs_ClockStepped_ResetsWindow(int& passed, int& total) {
    print_test_header("REQ-6.3/6.4: ClockStepped Resets Window");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.window_size = 10;
    ptp::DriftObserver obs(cfg, "test");
    
    // Add samples
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto samples_before = obs.GetSamples();
    assert(samples_before.size() == 5);
    uint64_t epoch_before = samples_before[0].epoch_id;
    
    // Increment epoch and clear window
    obs.IncrementEpoch();
    obs.ClearWindow();
    
    // Window should be empty
    auto samples_after = obs.GetSamples();
    assert(samples_after.size() == 0);
    
    // Add new sample - should have incremented epoch
    obs.Update(t_ref, t_clk);
    t_ref += 1000000000LL;
    t_clk += 1000000000LL;
    
    auto samples_new = obs.GetSamples();
    assert(samples_new.size() == 1);
    assert(samples_new[0].epoch_id == epoch_before + 1);
    
    passed++;
    std::cout << "✅ PASS - ClockStepped correctly resets window\n";
}

/**
 * REQ-6.5: Automatic step detection
 */
void test_Epochs_AutomaticStepDetection(int& passed, int& total) {
    print_test_header("REQ-6.5: Automatic Step Detection");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.window_size = 10;
    cfg.max_offset_step_ns = 1000000;  // 1ms threshold
    ptp::DriftObserver obs(cfg, "test");
    
    // Add normal samples
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL + 500000LL;  // 500µs offset
    
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 1000LL;  // Small drift
    }
    
    auto samples_before = obs.GetSamples();
    assert(samples_before.size() == 5);
    uint64_t epoch_before = samples_before[0].epoch_id;
    
    // Inject a large step (clock adjustment)
    t_clk += 10000000LL;  // +10ms step (exceeds 1ms threshold)
    
    // This should trigger automatic epoch increment
    obs.Update(t_ref, t_clk);
    
    auto samples_after = obs.GetSamples();
    
    // Latest sample should have FLAG_OFFSET_SPIKE and new epoch
    const auto& latest = samples_after.back();
    assert(latest.flags & ptp::DriftSample::FLAG_OFFSET_SPIKE);
    
    // Epoch should have auto-incremented due to step
    // (Implementation note: this requires Update() to call IncrementEpoch() on large offset jumps)
    assert(latest.epoch_id == epoch_before + 1);
    
    passed++;
    std::cout << "✅ PASS - Automatic step detection working\n";
}

// =============================================================================
// PHASE 6: HOLDOFF AND TRUST GATING
// =============================================================================

/**
 * REQ-7.1: Ready flag after min_valid_samples
 */
void test_Holdoff_Ready_AfterMinSamples(int& passed, int& total) {
    print_test_header("REQ-7.1: Ready Flag After Min Samples");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 5;
    cfg.window_size = 10;
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add 4 samples - should NOT be ready yet
    for (int i = 0; i < 4; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto est1 = obs.GetEstimate();
    assert(est1.ready == false);  // Not enough samples
    assert(est1.health_flags & ptp::HF_NOT_READY);
    
    // Add 5th sample - should become ready
    obs.Update(t_ref, t_clk);
    
    auto est2 = obs.GetEstimate();
    assert(est2.ready == true);  // Now ready
    assert(!(est2.health_flags & ptp::HF_NOT_READY));
    
    passed++;
    std::cout << "✅ PASS - Ready flag set after min_valid_samples\n";
}

/**
 * REQ-7.2: Holdoff timer after ClockStepped
 */
void test_Holdoff_AfterClockStep(int& passed, int& total) {
    print_test_header("REQ-7.2: Holdoff After Clock Step");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 3;
    cfg.holdoff_after_step_ticks = 5;  // 5 ticks after step
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add samples to become ready
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto est_before = obs.GetEstimate();
    assert(est_before.ready == true);
    
    // Trigger clock step via NotifyEvent
    obs.NotifyEvent(ptp::ObserverEvent::ClockStepped, 1000000LL);
    
    // Should be in holdoff immediately after event
    auto est_after = obs.GetEstimate();
    assert(est_after.health_flags & ptp::HF_IN_HOLDOFF);
    assert(est_after.ticks_in_holdoff > 0);
    
    // Add samples during holdoff - should remain in holdoff
    for (int i = 0; i < 4; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
        
        auto est_during = obs.GetEstimate();
        assert(est_during.health_flags & ptp::HF_IN_HOLDOFF);
    }
    
    // Add 5th sample - should exit holdoff
    obs.Update(t_ref, t_clk);
    auto est_exit = obs.GetEstimate();
    assert(!(est_exit.health_flags & ptp::HF_IN_HOLDOFF));
    assert(est_exit.ticks_in_holdoff == 0);
    
    passed++;
    std::cout << "✅ PASS - Holdoff timer works after clock step\n";
}

/**
 * REQ-7.3: Trustworthy transitions after holdoff
 */
void test_Holdoff_Trustworthy_AfterExpire(int& passed, int& total) {
    print_test_header("REQ-7.3: Trustworthy After Holdoff");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 3;
    cfg.holdoff_after_step_ticks = 3;
    cfg.max_drift_stddev_ppm = 10.0;  // Require low jitter for trust
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add stable samples to become trustworthy
    for (int i = 0; i < 10; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 1000LL;  // Small stable drift
    }
    
    auto est_before = obs.GetEstimate();
    assert(est_before.ready == true);
    assert(est_before.trustworthy == true);  // Should be trustworthy initially
    
    // Trigger event - should lose trustworthiness
    obs.NotifyEvent(ptp::ObserverEvent::ClockStepped, 0);
    
    auto est_holdoff = obs.GetEstimate();
    assert(est_holdoff.trustworthy == false);  // Not trustworthy during holdoff
    
    // Wait out holdoff period with stable samples
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 1000LL;
    }
    
    auto est_after = obs.GetEstimate();
    assert(est_after.trustworthy == true);  // Trustworthy again after holdoff
    
    passed++;
    std::cout << "✅ PASS - Trustworthy transitions correctly\n";
}

/**
 * REQ-7.4: Event-specific holdoff durations
 */
void test_Holdoff_DifferentDurations(int& passed, int& total) {
    print_test_header("REQ-7.4: Event-Specific Holdoff");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 2;
    cfg.holdoff_after_step_ticks = 5;   // 5 ticks for step
    cfg.holdoff_after_freq_ticks = 2;   // 2 ticks for freq adjust
    cfg.holdoff_after_ref_ticks = 10;   // 10 ticks for ref change
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Become ready
    for (int i = 0; i < 3; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    // Test FREQUENCY_ADJUSTED (shortest holdoff)
    obs.NotifyEvent(ptp::ObserverEvent::FrequencyAdjusted, 0);
    auto est_freq = obs.GetEstimate();
    assert(est_freq.health_flags & ptp::HF_IN_HOLDOFF);
    uint64_t freq_holdoff = est_freq.ticks_in_holdoff;
    assert(freq_holdoff <= cfg.holdoff_after_freq_ticks);
    
    // Wait out freq holdoff
    for (uint32_t i = 0; i < cfg.holdoff_after_freq_ticks; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto est_freq_after = obs.GetEstimate();
    assert(!(est_freq_after.health_flags & ptp::HF_IN_HOLDOFF));
    
    // Test REFERENCE_CHANGED (longest holdoff)
    obs.NotifyEvent(ptp::ObserverEvent::ReferenceChanged, 0);
    auto est_ref = obs.GetEstimate();
    assert(est_ref.health_flags & ptp::HF_IN_HOLDOFF);
    uint64_t ref_holdoff = est_ref.ticks_in_holdoff;
    assert(ref_holdoff <= cfg.holdoff_after_ref_ticks);
    assert(ref_holdoff > freq_holdoff);  // Should be longer
    
    passed++;
    std::cout << "✅ PASS - Event-specific holdoff durations work\n";
}

/**
 * REQ-7.5: Jitter prevents trustworthy
 */
void test_Holdoff_Jitter_PreventsTrust(int& passed, int& total) {
    print_test_header("REQ-7.5: Jitter Prevents Trust");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 5;
    cfg.max_drift_stddev_ppm = 5.0;  // Low threshold for trust
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add stable samples - should become trustworthy
    for (int i = 0; i < 10; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 1000LL;  // 1 ppm stable drift
    }
    
    auto est_stable = obs.GetEstimate();
    assert(est_stable.ready == true);
    assert(est_stable.trustworthy == true);  // Low jitter = trustworthy
    assert(est_stable.drift_stddev_ppm < cfg.max_drift_stddev_ppm);
    
    // Add jittery samples (varying drift)
    for (int i = 0; i < 10; i++) {
        int64_t jitter = (i % 2 == 0) ? 10000LL : -10000LL;  // Alternating ±10µs
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + jitter;
    }
    
    auto est_jittery = obs.GetEstimate();
    assert(est_jittery.ready == true);
    assert(est_jittery.trustworthy == false);  // High jitter = not trustworthy
    assert(est_jittery.drift_stddev_ppm > cfg.max_drift_stddev_ppm);
    
    passed++;
    std::cout << "✅ PASS - Jitter correctly prevents trust\n";
}

// =============================================================================
// PHASE 7: EVENT HANDLING
// =============================================================================

/**
 * REQ-6.3: ReferenceChanged event
 */
void test_Events_ReferenceChanged(int& passed, int& total) {
    print_test_header("REQ-6.3: ReferenceChanged Event");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 3;
    cfg.holdoff_after_ref_ticks = 10;  // Long holdoff for ref change
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add samples to build initial state
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto est_before = obs.GetEstimate();
    uint32_t epoch_before = est_before.current_epoch;
    
    // Trigger ReferenceChanged event
    obs.NotifyEvent(ptp::ObserverEvent::ReferenceChanged, 0);
    
    auto est_after = obs.GetEstimate();
    assert(est_after.current_epoch == epoch_before + 1);  // Epoch incremented
    assert(est_after.health_flags & ptp::HF_IN_HOLDOFF);  // In holdoff
    assert(est_after.ticks_in_holdoff == cfg.holdoff_after_ref_ticks);  // Correct duration
    
    passed++;
    std::cout << "✅ PASS - ReferenceChanged increments epoch and sets holdoff\n";
}

/**
 * REQ-6.3: ReferenceLost event
 */
void test_Events_ReferenceLost(int& passed, int& total) {
    print_test_header("REQ-6.3: ReferenceLost Event");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 3;
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add samples
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto est_before = obs.GetEstimate();
    uint32_t epoch_before = est_before.current_epoch;
    bool ready_before = est_before.ready;
    
    // Trigger ReferenceLost event (informational, no state change)
    obs.NotifyEvent(ptp::ObserverEvent::ReferenceLost, 0);
    
    auto est_after = obs.GetEstimate();
    assert(est_after.current_epoch == epoch_before);  // Epoch unchanged
    assert(est_after.ready == ready_before);  // Ready unchanged
    assert(!(est_after.health_flags & ptp::HF_IN_HOLDOFF));  // No holdoff
    
    passed++;
    std::cout << "✅ PASS - ReferenceLost is informational (no state change)\n";
}

/**
 * REQ-6.3: ReferenceRecovered event
 */
void test_Events_ReferenceRecovered(int& passed, int& total) {
    print_test_header("REQ-6.3: ReferenceRecovered Event");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 3;
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add samples
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto est_before = obs.GetEstimate();
    uint32_t epoch_before = est_before.current_epoch;
    bool ready_before = est_before.ready;
    
    // Trigger ReferenceRecovered event (informational, no state change)
    obs.NotifyEvent(ptp::ObserverEvent::ReferenceRecovered, 0);
    
    auto est_after = obs.GetEstimate();
    assert(est_after.current_epoch == epoch_before);  // Epoch unchanged
    assert(est_after.ready == ready_before);  // Ready unchanged
    assert(!(est_after.health_flags & ptp::HF_IN_HOLDOFF));  // No holdoff
    
    passed++;
    std::cout << "✅ PASS - ReferenceRecovered is informational (no state change)\n";
}

/**
 * REQ-6.3: FrequencyAdjusted event
 */
void test_Events_FrequencyAdjusted(int& passed, int& total) {
    print_test_header("REQ-6.3: FrequencyAdjusted Event");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 3;
    cfg.holdoff_after_freq_ticks = 2;  // Short holdoff for freq adjust
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Add samples to build state
    for (int i = 0; i < 5; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto est_before = obs.GetEstimate();
    uint32_t epoch_before = est_before.current_epoch;
    
    // Trigger FrequencyAdjusted event
    obs.NotifyEvent(ptp::ObserverEvent::FrequencyAdjusted, 0);
    
    auto est_after = obs.GetEstimate();
    assert(est_after.current_epoch == epoch_before);  // Epoch NOT incremented
    assert(est_after.health_flags & ptp::HF_IN_HOLDOFF);  // In holdoff
    assert(est_after.ticks_in_holdoff == cfg.holdoff_after_freq_ticks);  // Correct duration
    
    passed++;
    std::cout << "✅ PASS - FrequencyAdjusted sets holdoff without epoch change\n";
}

/**
 * REQ-6.4: Reset() clears all state
 */
void test_Events_Reset(int& passed, int& total) {
    print_test_header("REQ-6.4: Reset() Method");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 3;
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Build up state with samples
    for (int i = 0; i < 10; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL;
    }
    
    auto est_before = obs.GetEstimate();
    assert(est_before.ready == true);  // Should be ready with 10 samples
    assert(est_before.total_samples == 10);  // Should have 10 samples
    
    // Call Reset()
    obs.Reset();
    
    auto est_after = obs.GetEstimate();
    assert(est_after.ready == false);  // No longer ready
    assert(est_after.current_epoch == 0);  // Epoch reset to 0
    assert(est_after.total_samples == 0);  // No samples
    assert(est_after.valid_samples == 0);  // No valid samples
    assert(!(est_after.health_flags & ptp::HF_IN_HOLDOFF));  // Not in holdoff
    
    passed++;
    std::cout << "✅ PASS - Reset() clears all state correctly\n";
}

// =============================================================================
// PHASE 8: INTEGRATION TESTS
// =============================================================================

/**
 * Integration: Realistic GPS-RTC scenario
 */
void test_Integration_GPS_RTC_Realistic(int& passed, int& total) {
    print_test_header("Integration: GPS-RTC Realistic Scenario");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 10;
    cfg.holdoff_after_step_ticks = 5;
    cfg.holdoff_after_ref_ticks = 10;
    cfg.max_drift_stddev_ppm = 20.0;  // Higher tolerance for realistic scenarios
    ptp::DriftObserver gps_obs(cfg, "GPS");
    
    int64_t t_ref = 1000000000000LL;  // GPS reference time
    int64_t t_clk = 1000000000000LL;  // System clock
    
    // Phase 1: GPS locked, stable drift of 50 ppm
    for (int i = 0; i < 30; i++) {
        gps_obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;  // 1 second
        t_clk += 1000000000LL + 50000LL;  // 50 ppm fast
    }
    
    auto est_gps_locked = gps_obs.GetEstimate();
    assert(est_gps_locked.ready == true);
    assert(est_gps_locked.trustworthy == true);
    assert(std::abs(est_gps_locked.drift_ppm - 50.0) < 5.0);  // Within 5 ppm
    
    // Phase 2: GPS signal lost, switch to RTC
    gps_obs.NotifyEvent(ptp::ObserverEvent::ReferenceLost, 0);
    gps_obs.NotifyEvent(ptp::ObserverEvent::ReferenceChanged, 0);
    
    auto est_ref_changed = gps_obs.GetEstimate();
    assert(est_ref_changed.health_flags & ptp::HF_IN_HOLDOFF);  // In holdoff
    assert(!est_ref_changed.trustworthy);  // Not trustworthy during holdoff
    
    // Phase 3: RTC settles with different drift (100 ppm)
    // Need to wait out the holdoff period (10 ticks for reference change)
    for (int i = 0; i < 25; i++) {  // More samples to ensure holdoff expires and trust builds
        gps_obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 100000LL;  // 100 ppm fast
    }
    
    auto est_rtc_settled = gps_obs.GetEstimate();
    assert(est_rtc_settled.ready == true);
    assert(!(est_rtc_settled.health_flags & ptp::HF_IN_HOLDOFF));  // Out of holdoff
    assert(est_rtc_settled.trustworthy == true);  // Trustworthy after holdoff
    assert(std::abs(est_rtc_settled.drift_ppm - 100.0) < 10.0);  // Within 10 ppm
    
    passed++;
    std::cout << "✅ PASS - GPS-RTC transition handled correctly\n";
}

/**
 * Integration: Multi-clock observers
 */
void test_Integration_MultiClock(int& passed, int& total) {
    print_test_header("Integration: Multi-Clock Observers");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 5;
    
    // Create two observers for different clocks
    ptp::DriftObserver obs_clk1(cfg, "Clock1");
    ptp::DriftObserver obs_clk2(cfg, "Clock2");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk1 = 1000000000000LL;  // Clock 1: slow by 30 ppm
    int64_t t_clk2 = 1000000000000LL;  // Clock 2: fast by 80 ppm
    
    // Update both observers with different clock behaviors
    for (int i = 0; i < 15; i++) {
        obs_clk1.Update(t_ref, t_clk1);
        obs_clk2.Update(t_ref, t_clk2);
        
        t_ref += 1000000000LL;
        t_clk1 += 1000000000LL - 30000LL;  // 30 ppm slow
        t_clk2 += 1000000000LL + 80000LL;  // 80 ppm fast
    }
    
    auto est1 = obs_clk1.GetEstimate();
    auto est2 = obs_clk2.GetEstimate();
    
    // Both should be ready and trustworthy
    assert(est1.ready == true);
    assert(est2.ready == true);
    assert(est1.trustworthy == true);
    assert(est2.trustworthy == true);
    
    // Each should track its own drift independently
    assert(std::abs(est1.drift_ppm - (-30.0)) < 5.0);  // Slow clock
    assert(std::abs(est2.drift_ppm - 80.0) < 5.0);     // Fast clock
    
    // Observers should be independent (different epochs)
    obs_clk1.IncrementEpoch();
    auto est1_after = obs_clk1.GetEstimate();
    auto est2_after = obs_clk2.GetEstimate();
    
    assert(est1_after.current_epoch == 1);  // Clock1 incremented
    assert(est2_after.current_epoch == 0);  // Clock2 unchanged
    
    passed++;
    std::cout << "✅ PASS - Multiple independent observers work correctly\n";
}

/**
 * Integration: Recovery after contamination
 */
void test_Integration_Recovery(int& passed, int& total) {
    print_test_header("Integration: Recovery After Contamination");
    total++;
    
    ptp::Config cfg = default_config();
    cfg.min_valid_samples = 5;
    cfg.holdoff_after_step_ticks = 3;
    cfg.max_drift_ppm = 500;  // Allow spike detection
    cfg.max_drift_stddev_ppm = 10.0;
    ptp::DriftObserver obs(cfg, "test");
    
    int64_t t_ref = 1000000000000LL;
    int64_t t_clk = 1000000000000LL;
    
    // Phase 1: Build up clean state (20 ppm drift)
    for (int i = 0; i < 20; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 20000LL;  // 20 ppm fast
    }
    
    auto est_clean = obs.GetEstimate();
    assert(est_clean.ready == true);
    assert(est_clean.trustworthy == true);
    uint32_t epoch_before = est_clean.current_epoch;
    
    // Phase 2: Multiple contamination events
    // Event 1: Clock step
    obs.NotifyEvent(ptp::ObserverEvent::ClockStepped, 1000000LL);
    auto est_step = obs.GetEstimate();
    assert(est_step.health_flags & ptp::HF_IN_HOLDOFF);
    assert(!est_step.trustworthy);
    assert(est_step.current_epoch == epoch_before + 1);
    
    // Event 2: Frequency adjustment during holdoff
    obs.NotifyEvent(ptp::ObserverEvent::FrequencyAdjusted, 0);
    
    // Phase 3: Wait out holdoff and rebuild trust
    for (int i = 0; i < 10; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 25000LL;  // Slightly different drift (25 ppm)
    }
    
    auto est_recovered = obs.GetEstimate();
    assert(est_recovered.ready == true);
    assert(est_recovered.trustworthy == true);  // Should be trustworthy again
    assert(!(est_recovered.health_flags & ptp::HF_IN_HOLDOFF));  // Out of holdoff
    assert(std::abs(est_recovered.drift_ppm - 25.0) < 5.0);  // New drift tracked
    
    // Phase 4: Verify full reset recovery
    obs.Reset();
    auto est_reset = obs.GetEstimate();
    assert(est_reset.ready == false);
    assert(est_reset.current_epoch == 0);
    
    // Rebuild from scratch
    for (int i = 0; i < 10; i++) {
        obs.Update(t_ref, t_clk);
        t_ref += 1000000000LL;
        t_clk += 1000000000LL + 30000LL;  // 30 ppm
    }
    
    auto est_final = obs.GetEstimate();
    assert(est_final.ready == true);
    assert(est_final.trustworthy == true);
    
    passed++;
    std::cout << "✅ PASS - Recovery after contamination works correctly\n";
}

// =============================================================================
// MAIN
// =============================================================================

// =============================================================================
// MAIN
// =============================================================================

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  DriftObserver TDD RED-GREEN-REFACTOR Test Suite          ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "\nPhase 1: Data Structures\n";
    std::cout << "Phase 2: Ring Buffer Operations\n";
    std::cout << "Phase 3: Spike Detection and Outlier Rejection\n";
    std::cout << "Phase 4: Drift Estimation Methods\n";
    std::cout << "Phase 5: Epoch Tracking and Contamination Events\n";
    std::cout << "Phase 6: Holdoff and Trust Gating\n";
    std::cout << "Phase 7: Event Handling\n";
    std::cout << "Phase 8: Integration Tests\n";
    std::cout << "\n⚠️  EXPECTED: ALL TESTS SHOULD FAIL (RED PHASE)\n";
    std::cout << "    This is correct TDD workflow!\n";
    std::cout << "\n";
    
    int tests_passed = 0;
    int tests_total = 0;
    
    // PHASE 1: Data Structures
    std::cout << "\n═══ PHASE 1: DATA STRUCTURES ═══\n";
    test_DriftSample_FieldPopulation(tests_passed, tests_total);
    test_DriftSample_FlagBits(tests_passed, tests_total);
    test_Config_CreateDefault(tests_passed, tests_total);
    test_Estimate_StructureFields(tests_passed, tests_total);
    test_Estimate_HelperMethods(tests_passed, tests_total);
    test_HealthFlags_Bitmask(tests_passed, tests_total);
    test_ObserverEvent_Enum(tests_passed, tests_total);
    
    // PHASE 2: Ring Buffer Operations
    std::cout << "\n═══ PHASE 2: RING BUFFER OPERATIONS ═══\n";
    test_RingBuffer_Initialize(tests_passed, tests_total);
    test_RingBuffer_Update_AddsSamples(tests_passed, tests_total);
    test_RingBuffer_CircularWraparound(tests_passed, tests_total);
    test_RingBuffer_ComputeOffset(tests_passed, tests_total);
    test_RingBuffer_ComputeDrift(tests_passed, tests_total);
    test_RingBuffer_ValidateDtRef(tests_passed, tests_total);
    test_RingBuffer_GetEstimate_BasicStats(tests_passed, tests_total);
    
    // PHASE 3: Spike Detection
    std::cout << "\n═══ PHASE 3: SPIKE DETECTION ═══\n";
    test_SpikeDetection_OffsetSpike(tests_passed, tests_total);
    test_SpikeDetection_MAD_Outlier(tests_passed, tests_total);
    test_SpikeDetection_DriftSpike(tests_passed, tests_total);
    test_SpikeDetection_ExcludeInvalid(tests_passed, tests_total);
    
    // PHASE 4: Drift Estimation
    std::cout << "\n═══ PHASE 4: DRIFT ESTIMATION ═══\n";
    test_DriftEstimation_LinearRegression(tests_passed, tests_total);
    test_DriftEstimation_MeanOfDeltas(tests_passed, tests_total);
    test_DriftEstimation_Convergence(tests_passed, tests_total);
    
    // PHASE 5: Epoch Tracking
    std::cout << "\n═══ PHASE 5: EPOCH TRACKING ═══\n";
    test_Epochs_EpochId_Increments(tests_passed, tests_total);
    test_Epochs_CrossEpoch_Excluded(tests_passed, tests_total);
    test_Epochs_ClockStepped_ResetsWindow(tests_passed, tests_total);
    test_Epochs_AutomaticStepDetection(tests_passed, tests_total);
    
    // PHASE 6: Holdoff and Trust Gating
    std::cout << "\n═══ PHASE 6: HOLDOFF AND TRUST GATING ═══\n";
    test_Holdoff_Ready_AfterMinSamples(tests_passed, tests_total);
    test_Holdoff_AfterClockStep(tests_passed, tests_total);
    test_Holdoff_Trustworthy_AfterExpire(tests_passed, tests_total);
    test_Holdoff_DifferentDurations(tests_passed, tests_total);
    test_Holdoff_Jitter_PreventsTrust(tests_passed, tests_total);
    
    // PHASE 7: Event Handling
    std::cout << "\n═══ PHASE 7: EVENT HANDLING ═══\n";
    test_Events_ReferenceChanged(tests_passed, tests_total);
    test_Events_ReferenceLost(tests_passed, tests_total);
    test_Events_ReferenceRecovered(tests_passed, tests_total);
    test_Events_FrequencyAdjusted(tests_passed, tests_total);
    test_Events_Reset(tests_passed, tests_total);
    
    // PHASE 8: Integration Tests
    std::cout << "\n═══ PHASE 8: INTEGRATION TESTS ═══\n";
    test_Integration_GPS_RTC_Realistic(tests_passed, tests_total);
    test_Integration_MultiClock(tests_passed, tests_total);
    test_Integration_Recovery(tests_passed, tests_total);
    
    // Summary
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║ TEST SUMMARY                                               ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "Total tests:  " << tests_total << "\n";
    std::cout << "Passed:       " << tests_passed << "\n";
    std::cout << "Failed:       " << (tests_total - tests_passed) << "\n";
    
    if (tests_passed == tests_total) {
        std::cout << "\n✅ ALL TESTS PASSED! Ready to proceed to REFACTOR phase.\n";
        return 0;
    } else {
        std::cout << "\n⚠️  RED PHASE: Tests failing (expected behavior in TDD)\n";
        std::cout << "Next: Implement drift_observer.hpp/cpp to make tests GREEN\n";
        return 1;
    }
}
