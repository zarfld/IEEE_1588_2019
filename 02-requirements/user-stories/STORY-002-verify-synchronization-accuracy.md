---
specType: requirements
standard: "29148"
phase: "02-requirements"
version: "1.0.0"
author: "Requirements Engineering Team"
date: "2025-11-07"
status: "draft"
traceability:
  stakeholderRequirements:
    - StR-001  # STR-STD-001: IEEE 1588-2019 Protocol Compliance
    - StR-005  # STR-PERF-001: Synchronization Accuracy <1µs
    - StR-006  # STR-PERF-002: Timing Determinism
---

# User Story: STORY-002 - Verify Synchronization Accuracy

**Story ID**: STORY-002  
**Story Title**: Verify PTP Synchronization Accuracy Meets <1µs Requirement  
**Author**: Requirements Engineering Team  
**Date**: 2025-11-07  
**Version**: 1.0.0

---

## 1. User Story Statement

**As** a system integrator or QA engineer  
**I want** comprehensive test procedures and tools to verify PTP synchronization accuracy meets the <1µs requirement  
**So that** I can validate product performance before production deployment and generate compliance reports for customers

---

## 2. Personas and Context

### Primary Persona: System Integrator / QA Lead

- **Name**: Sarah Thompson
- **Role**: Senior QA Engineer & Certification Specialist
- **Company**: Industrial networking equipment manufacturer
- **Experience**: 12 years test engineering, 5 years PTP/TSN validation
- **Goals**:
  - Validate synchronization accuracy <1µs (P95) per REQ-NF-P-001
  - Generate compliance report for customer acceptance
  - Identify performance regressions before production release
  - Establish repeatable test methodology for future products
- **Pain Points**:
  - Expensive test equipment (Calnex Sentinel, Meinberg M1000)
  - Complex setup with GPS-disciplined grandmaster
  - Difficult to isolate timing issues (network jitter vs. PTP bugs)
  - Manual data collection and analysis time-consuming
- **Success Criteria**:
  - Automated test suite runs in <2 hours
  - Generates PDF compliance report with statistical analysis
  - P50/P95/P99 offset metrics clearly documented
  - Pass/fail decision objective and repeatable

### Secondary Persona: Customer Acceptance Engineer

- **Name**: David Park
- **Role**: Technical Buyer for automotive OEM
- **Concerns**:
  - Vendor claims of <1µs accuracy must be verified independently
  - Compliance report must reference IEEE 1588-2019 standard
  - Test methodology must be transparent and reproducible
- **Acceptance Criteria**:
  - Test report includes raw data (not just summary statistics)
  - Test environment documented (equipment, topology, configuration)
  - Independent third-party lab validation available

---

## 3. Acceptance Criteria (Gherkin Format)

```gherkin
Feature: Verify PTP Synchronization Accuracy
  As a QA engineer
  I want to measure and validate PTP offset accuracy against <1µs requirement
  So that I can certify product performance and generate compliance reports

  Background:
    Given test environment is set up:
      | Component              | Specification                          |
      | Grandmaster            | GPS-disciplined, ±15ns holdover       |
      | Device Under Test (DUT)| PTP slave with hardware timestamping  |
      | Measurement Tool       | Calnex Sentinel or equivalent         |
      | Network                | Gigabit Ethernet switch, <100µs delay |
      | Test Duration          | 10 minutes minimum (600 samples at 1Hz)|
    And DUT is running PTP stack integrated per STORY-001
    And measurement tool is synchronized to GPS reference

  Scenario: Verify P50 offset <500ns
    Given DUT has been synchronizing for 5 minutes (warm-up period)
    When 600 offset measurements are collected over 10 minutes:
      """
      Measurement tool captures timestamps:
      - T_gps: GPS-disciplined reference timestamp
      - T_dut: DUT local clock timestamp
      - offset = T_dut - T_gps (nanoseconds)
      """
    And offset dataset is sorted ascending
    And P50 (median) is calculated: offset[300] (middle value)
    Then P50 offset shall be less than 500 nanoseconds absolute value
    And measurement report shall include P50 value with units

  Scenario: Verify P95 offset <1µs (primary requirement)
    Given 600 offset measurements collected per previous scenario
    When P95 percentile is calculated: offset at index 570 (95% of 600)
    Then P95 offset shall be less than 1000 nanoseconds (1 microsecond)
    And this result shall be highlighted in compliance report as PASS
    And if P95 ≥ 1000ns, report shall indicate FAIL with root cause analysis

  Scenario: Verify P99 offset <2µs (stretch goal)
    Given 600 offset measurements collected per previous scenario
    When P99 percentile is calculated: offset at index 594 (99% of 600)
    Then P99 offset shall be less than 2000 nanoseconds (2 microseconds)
    And if P99 ≥ 2µs, investigate outliers but do not fail test

  Scenario: Generate statistical analysis of offset distribution
    Given 600 offset measurements collected
    When statistical metrics are calculated:
      | Metric               | Formula                          |
      | Mean                 | sum(offset) / count              |
      | Standard Deviation   | sqrt(variance)                   |
      | Min                  | min(offset)                      |
      | Max                  | max(offset)                      |
      | Range                | max - min                        |
    Then report shall include all metrics in table format
    And histogram of offset distribution shall be generated (bin width 50ns)
    And normal distribution fit shall be shown on histogram

  Scenario: Verify synchronization stability over time
    Given offset measurements span 10 minutes (600 samples)
    When time-series plot is generated (offset vs. time)
    Then plot shall show no systematic drift (mean offset stable)
    And variance shall remain bounded (no increasing jitter)
    And no outliers exceeding ±10µs (indicates timestamp errors)
    And plot shall be included in compliance report

  Scenario: Test under network stress (high traffic load)
    Given baseline accuracy established (P95 <1µs under no load)
    When background network traffic is injected:
      | Traffic Type    | Rate        | Duration   |
      | TCP bulk        | 800 Mbps    | 5 minutes  |
      | UDP multicast   | 100 Mbps    | 5 minutes  |
      | PTP packets     | 8 Hz        | 5 minutes  |
    And 300 offset measurements collected during stress test
    Then P95 offset shall remain less than 1.5 microseconds
    And degradation from baseline shall be less than 500ns
    And report shall include "under load" test results

  Scenario: Test with asymmetric network delay
    Given test setup includes asymmetric delay simulator:
      | Direction       | Delay       |
      | Master to Slave | 50µs        |
      | Slave to Master | 200µs       |
    When offset measurements are collected over 10 minutes
    Then PTP stack shall handle asymmetry gracefully (no crash)
    And offset accuracy may degrade (document degradation level)
    And report shall note asymmetry impact on performance

  Scenario: Generate compliance report in PDF format
    Given all test scenarios have been executed
    When compliance report is generated using report template
    Then report shall include:
      | Section                     | Content                                 |
      | Executive Summary           | Pass/Fail, P50/P95/P99 metrics          |
      | Test Configuration          | Equipment, topology, DUT firmware       |
      | Measurement Methodology     | IEEE 1588-2019 references, procedures   |
      | Statistical Analysis        | Metrics table, histogram, time-series   |
      | Stress Test Results         | Performance under load                  |
      | Raw Data Appendix           | CSV file with all 600 measurements      |
      | Compliance Statement        | REQ-NF-P-001 satisfied (if PASS)        |
    And report shall be digitally signed (SHA-256 hash)
    And report shall be reproducible (seed all random tests)

  Scenario: Automate test execution with Python script
    Given test automation script `test_ptp_accuracy.py` is available
    When script is executed with configuration file:
      """yaml
      test_config:
        dut_ip: 192.168.1.100
        grandmaster_ip: 192.168.1.1
        measurement_duration_sec: 600
        sync_rate_hz: 1
        output_report: compliance_report.pdf
      """
    Then script shall:
      - Configure DUT via management interface
      - Start measurement tool data capture
      - Wait for test duration (10 minutes)
      - Collect measurements via measurement tool API
      - Calculate statistics (P50, P95, P99)
      - Generate PDF report
      - Return exit code 0 (pass) or 1 (fail)
    And entire process shall complete in less than 15 minutes
    And script shall log all actions for traceability
```

---

## 4. Test Environment Setup

### 4.1 Reference Test Topology

```
                   GPS Antenna
                        |
                   ┌────▼────┐
                   │   GPS   │
                   │Receiver │ (PPS + NMEA)
                   └────┬────┘
                        |
                   ┌────▼────────────┐
                   │  Grandmaster    │
                   │(GPS-disciplined)│
                   │  PTP Clock      │
                   └────┬────────────┘
                        | (Ethernet, PTP Sync/Follow_Up)
                        |
        ┌───────────────┼───────────────┐
        |               |               |
   ┌────▼────┐     ┌───▼───┐      ┌───▼──────────────┐
   │ Ethernet│     │ DUT   │      │Measurement Tool  │
   │ Switch  │────▶│(Slave)│◀─────│(Calnex/Meinberg) │
   └─────────┘     └───────┘      └──────────────────┘
                        |                    |
                        └────────┬───────────┘
                                 |
                            Monitor Port
                        (capture DUT timestamps)
```

### 4.2 Equipment Requirements

#### Grandmaster Clock

**Purpose**: Provide GPS-disciplined reference time for DUT synchronization

**Specifications**:
- **Accuracy**: ±15ns holdover (GPS locked), ±100ns/hour (GPS unlocked)
- **PTP Profile**: IEEE 1588-2019 default profile
- **Transport**: Ethernet Layer 2 (Ethertype 0x88F7)
- **Sync Rate**: Configurable (1 Hz typical)
- **Examples**: Meinberg LANTIME M1000, Microsemi SyncServer S650

**Configuration**:
```yaml
grandmaster_config:
  clock_class: 6        # GPS locked
  clock_accuracy: 0x21  # Accuracy <25ns
  priority1: 64         # High priority master
  domain_number: 0      # Default domain
  sync_interval: 0      # 1 Hz (2^0 seconds)
```

#### Device Under Test (DUT)

**Purpose**: PTP slave device being validated

**Requirements**:
- PTP stack integrated per STORY-001
- Hardware timestamping capable (±8ns preferred)
- Management interface for configuration and status query
- Stable network connection (no packet loss)

**Configuration**:
```yaml
dut_config:
  device_type: ordinary_clock_slave
  priority1: 255        # Never become master
  domain_number: 0
  sync_interval: 0      # Match grandmaster (1 Hz)
```

#### Measurement Tool

**Purpose**: Independent measurement of DUT offset from GPS reference

**Options**:

1. **Calnex Sentinel** (High-End, ~$50k USD)
   - ±1ns measurement accuracy
   - Hardware timestamping
   - Automated test scripts via REST API
   - PDF report generation built-in

2. **Meinberg M1000** (Mid-Range, ~$10k USD)
   - ±10ns measurement accuracy
   - PTP monitoring and analysis
   - CSV data export for offline analysis

3. **Linux PTP + GPS Receiver** (Budget, ~$500 USD)
   - ±50ns measurement accuracy (software timestamps)
   - Open-source tools (ptp4l, phc2sys, gpsd)
   - Custom Python scripts for analysis

**Recommended**: Calnex Sentinel for production certification, Linux PTP for development

### 4.3 Network Configuration

#### Ethernet Switch Requirements

- **Switching Delay**: <10µs (low-latency switch preferred)
- **PTP Transparent Clock**: Optional (improves accuracy if switch supports)
- **Priority Queuing**: PTP packets on high-priority queue (VLAN priority 7)
- **No Packet Loss**: <0.001% loss rate acceptable

#### Network Isolation

- **Dedicated VLAN**: Isolate PTP traffic from other protocols
- **No Spanning Tree**: Disable STP to avoid topology changes during test
- **Static ARP**: Prevent ARP flooding during measurement

---

## 5. Test Procedures

### 5.1 Cold Start Convergence Test

**Objective**: Verify DUT achieves synchronization within 60 seconds from power-on

**Procedure**:
1. Power off DUT
2. Start grandmaster and measurement tool (verify GPS lock)
3. Power on DUT
4. Monitor offset via measurement tool
5. Record time to convergence (offset <1µs)
6. Verify convergence time <60 seconds

**Pass Criteria**: Convergence time ≤60 seconds, offset <1µs at convergence

### 5.2 Steady-State Accuracy Test (PRIMARY)

**Objective**: Verify P95 offset <1µs per REQ-NF-P-001

**Procedure**:
1. Allow DUT to synchronize for 5 minutes (warm-up)
2. Capture 600 offset measurements at 1 Hz (10 minutes)
3. Calculate P50, P95, P99 percentiles
4. Generate statistical report

**Pass Criteria**:
- P50 offset <500ns
- **P95 offset <1µs** (CRITICAL)
- P99 offset <2µs

**Measurement Script**:
```python
import numpy as np

# Capture offset measurements from measurement tool
offsets_ns = measurement_tool.capture_offsets(duration_sec=600, rate_hz=1)

# Calculate percentiles
p50 = np.percentile(np.abs(offsets_ns), 50)
p95 = np.percentile(np.abs(offsets_ns), 95)
p99 = np.percentile(np.abs(offsets_ns), 99)

# Pass/Fail decision
if p95 < 1000:
    print(f"PASS: P95 offset = {p95:.1f} ns < 1000 ns")
else:
    print(f"FAIL: P95 offset = {p95:.1f} ns ≥ 1000 ns")
```

### 5.3 Stability Test (Long Duration)

**Objective**: Verify synchronization remains stable over extended operation

**Procedure**:
1. Run DUT synchronized for 1 hour
2. Capture offset every 10 seconds (360 samples)
3. Plot offset vs. time
4. Verify no drift or increasing variance

**Pass Criteria**:
- Mean offset stable (no systematic drift >100ns/hour)
- Variance stable (no increasing jitter trend)
- No outliers >10µs

### 5.4 Network Stress Test

**Objective**: Verify performance degrades gracefully under network load

**Procedure**:
1. Establish baseline accuracy (P95 <1µs, no load)
2. Inject background traffic (800 Mbps TCP, 100 Mbps UDP)
3. Measure offset for 5 minutes under load
4. Calculate P95 offset under load
5. Compare to baseline (acceptable degradation <500ns)

**Pass Criteria**:
- P95 offset under load <1.5µs
- Degradation from baseline <500ns

---

## 6. Compliance Report Template

### 6.1 Report Structure

```markdown
# PTP Synchronization Accuracy Compliance Report

**Product**: [Device Model]
**Firmware Version**: [Version Number]
**Test Date**: [Date]
**Test Engineer**: [Name]
**Test Equipment**: [Grandmaster Model, Measurement Tool Model]

---

## Executive Summary

- **Test Result**: PASS / FAIL
- **Requirement**: REQ-NF-P-001 - Synchronization accuracy <1µs (P95)
- **Measured P95 Offset**: XXX ns
- **Test Duration**: 10 minutes (600 samples at 1 Hz)

---

## Test Configuration

### Device Under Test (DUT)
- Model: [Model]
- Firmware: [Version]
- MAC Address: [MAC]
- PTP Configuration: [Domain, Priority, Sync Interval]

### Test Equipment
- Grandmaster: [Model] (GPS-locked, clock class 6)
- Measurement Tool: [Model]
- Network Switch: [Model]

### Test Topology
[Diagram showing grandmaster, DUT, measurement tool]

---

## Measurement Results

### Statistical Summary

| Metric | Value (ns) | Requirement | Pass/Fail |
|--------|-----------|-------------|-----------|
| P50    | XXX       | <500ns      | PASS/FAIL |
| P95    | XXX       | <1000ns     | PASS/FAIL |
| P99    | XXX       | <2000ns     | PASS/FAIL |
| Mean   | XXX       | N/A         | -         |
| Std Dev| XXX       | N/A         | -         |
| Min    | XXX       | N/A         | -         |
| Max    | XXX       | N/A         | -         |

### Offset Distribution Histogram
[Histogram image: offset (ns) vs. count, bin width 50ns]

### Time-Series Plot
[Plot: offset (ns) vs. time (seconds), 0-600 seconds]

---

## Test Scenarios

### 1. Cold Start Convergence
- Convergence Time: XX seconds
- Pass Criteria: ≤60 seconds
- Result: PASS/FAIL

### 2. Steady-State Accuracy
- P95 Offset: XXX ns
- Pass Criteria: <1000ns
- Result: PASS/FAIL

### 3. Network Stress Test
- Baseline P95: XXX ns
- Under Load P95: XXX ns
- Degradation: XXX ns
- Pass Criteria: Degradation <500ns
- Result: PASS/FAIL

---

## Compliance Statement

[If PASS]
The Device Under Test (DUT) meets the requirement REQ-NF-P-001 for 
synchronization accuracy <1µs (P95) as specified in IEEE 1588-2019 
Section 11.3 delay request-response mechanism. Test methodology follows 
IEEE 1588-2019 conformance guidelines.

[If FAIL]
The DUT does not meet REQ-NF-P-001. Root cause analysis:
- [Investigation findings]
- [Recommended corrective actions]

---

## Raw Data Appendix

Raw measurement data (600 samples) attached as CSV file:
- Columns: timestamp, offset_ns, delay_ns
- File: ptp_measurements_YYYYMMDD_HHMMSS.csv

---

## Signatures

Test Engineer: ___________________  Date: __________
QA Manager: ______________________  Date: __________

Report Hash (SHA-256): [hash for integrity verification]
```

---

## 7. Automation Script Example

### 7.1 Python Test Automation Script

```python
#!/usr/bin/env python3
"""
PTP Accuracy Test Automation Script
Validates synchronization accuracy per REQ-NF-P-001
"""

import time
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
from fpdf import FPDF  # PDF report generation

class PTPAccuracyTest:
    def __init__(self, config):
        self.dut_ip = config['dut_ip']
        self.gm_ip = config['grandmaster_ip']
        self.duration_sec = config['measurement_duration_sec']
        self.sync_rate_hz = config['sync_rate_hz']
        self.output_report = config['output_report']
        
    def capture_offsets(self):
        """Capture offset measurements from measurement tool"""
        num_samples = self.duration_sec * self.sync_rate_hz
        offsets_ns = []
        
        for i in range(num_samples):
            # Query measurement tool for offset
            offset = self.measurement_tool.get_offset()  # API call
            offsets_ns.append(offset)
            time.sleep(1.0 / self.sync_rate_hz)
            
        return np.array(offsets_ns)
    
    def calculate_statistics(self, offsets_ns):
        """Calculate P50/P95/P99 and other metrics"""
        abs_offsets = np.abs(offsets_ns)
        
        stats = {
            'p50': np.percentile(abs_offsets, 50),
            'p95': np.percentile(abs_offsets, 95),
            'p99': np.percentile(abs_offsets, 99),
            'mean': np.mean(offsets_ns),
            'std': np.std(offsets_ns),
            'min': np.min(offsets_ns),
            'max': np.max(offsets_ns)
        }
        
        return stats
    
    def generate_report(self, stats, offsets_ns):
        """Generate PDF compliance report"""
        pdf = FPDF()
        pdf.add_page()
        pdf.set_font("Arial", "B", 16)
        pdf.cell(0, 10, "PTP Synchronization Accuracy Report", ln=True)
        
        # Executive Summary
        pdf.set_font("Arial", "", 12)
        result = "PASS" if stats['p95'] < 1000 else "FAIL"
        pdf.cell(0, 10, f"Test Result: {result}", ln=True)
        pdf.cell(0, 10, f"P95 Offset: {stats['p95']:.1f} ns", ln=True)
        
        # [Additional report sections...]
        
        pdf.output(self.output_report)
        print(f"Report generated: {self.output_report}")
    
    def run(self):
        """Execute test and generate report"""
        print(f"Starting PTP accuracy test: {self.duration_sec} seconds")
        
        # Capture measurements
        offsets_ns = self.capture_offsets()
        
        # Calculate statistics
        stats = self.calculate_statistics(offsets_ns)
        
        # Generate report
        self.generate_report(stats, offsets_ns)
        
        # Pass/Fail decision
        if stats['p95'] < 1000:
            print(f"PASS: P95 offset = {stats['p95']:.1f} ns < 1000 ns")
            return 0  # Exit code success
        else:
            print(f"FAIL: P95 offset = {stats['p95']:.1f} ns ≥ 1000 ns")
            return 1  # Exit code failure

# Run test
if __name__ == "__main__":
    config = {
        'dut_ip': '192.168.1.100',
        'grandmaster_ip': '192.168.1.1',
        'measurement_duration_sec': 600,
        'sync_rate_hz': 1,
        'output_report': 'ptp_compliance_report.pdf'
    }
    
    test = PTPAccuracyTest(config)
    exit_code = test.run()
    exit(exit_code)
```

---

## 8. Definition of Done

- [ ] **Test environment set up**: Grandmaster, DUT, measurement tool configured
- [ ] **Cold start test passed**: Convergence within 60 seconds
- [ ] **Steady-state accuracy verified**: P95 offset <1µs (PRIMARY)
- [ ] **Statistical analysis complete**: P50/P95/P99 calculated, histogram generated
- [ ] **Time-series plot reviewed**: No drift or increasing variance
- [ ] **Network stress test passed**: Degradation under load <500ns
- [ ] **Compliance report generated**: PDF report with all required sections
- [ ] **Raw data archived**: CSV file with 600 measurements stored
- [ ] **Test automation script validated**: Python script runs end-to-end
- [ ] **QA manager approval**: Report reviewed and signed

---

## 9. Dependencies and Blockers

### Dependencies

- **STORY-001 Complete**: PTP stack integrated into DUT
- **Test Equipment Available**: Grandmaster, measurement tool procured
- **GPS Lock**: GPS antenna with clear sky view for grandmaster
- **Stable Network**: Dedicated test VLAN, low packet loss

### Potential Blockers

- **Equipment Procurement Delay**: Calnex/Meinberg lead time 4-8 weeks
- **GPS Signal Issues**: Indoor lab without GPS antenna
- **Network Jitter**: Shared network degrades baseline accuracy
- **DUT Firmware Bugs**: PTP stack instability prevents convergence

---

## 10. Traceability Matrix

| Story Element | Requirement ID | Description |
|---------------|----------------|-------------|
| P95 Offset <1µs | REQ-NF-P-001 | Primary synchronization accuracy requirement |
| Offset Measurement | REQ-F-003 | Delay request-response mechanism (UC-003) |
| Servo Convergence | REQ-F-004 | PI controller convergence (UC-004) |
| Deterministic Behavior | REQ-NF-P-002 | Test under real-time constraints |
| IEEE 1588-2019 Compliance | REQ-F-001, REQ-F-002 | Validate protocol compliance |

---

## 11. Related Use Cases and Stories

- **UC-001**: Synchronize as Ordinary Clock Slave
- **UC-003**: Measure Clock Offset
- **UC-004**: Adjust Clock Frequency
- **STORY-001**: Integrate PTP into RTOS Application (prerequisite)
- **STORY-003**: Port PTP to Custom NIC

---

## 12. Notes and Comments

### Cost-Benefit Analysis

| Test Approach | Equipment Cost | Setup Time | Accuracy | Automation |
|---------------|---------------|-----------|----------|------------|
| Calnex Sentinel | $50k | 1 day | ±1ns | Excellent |
| Meinberg M1000 | $10k | 2 days | ±10ns | Good |
| Linux PTP + GPS | $500 | 1 week | ±50ns | Manual |

**Recommendation**: Calnex for production, Linux PTP for development

### Common Test Pitfalls

1. **Insufficient Warm-Up**: Measuring before servo converges (wait 5 minutes)
2. **Network Jitter**: Shared network causes false failures
3. **GPS Unlock**: Grandmaster loses GPS lock during test
4. **Sample Size Too Small**: <100 samples insufficient for P95 calculation
5. **Timestamp Errors**: Measurement tool misconfigured (wrong PTP domain)

---

**Document Control**:
- **Created**: 2025-11-07
- **Last Updated**: 2025-11-07
- **Review Status**: Draft - Pending technical review
- **Approved By**: TBD (Requirements Review Board)
- **Next Review**: 2025-11-14
