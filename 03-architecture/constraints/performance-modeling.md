---
specType: guidance
phase: 03-architecture
status: draft
title: "Performance Modeling Plan — IEEE 1588-2019 PTP"
author: Architecture Team
date: 2025-11-07
purpose: "Define quantitative timing models, benchmarks, and resource estimates to close Gate 03→04 conditions."
traceability:
  requirements:
    - REQ-NF-P-001
    - REQ-NF-P-002
    - REQ-NF-P-003
    - REQ-S-001
  decisions:
    - ADR-003
    - ADR-004
  architecture:
    - ARC-C-001
    - ARC-C-002
---
## Performance Modeling Plan — IEEE 1588-2019 PTP

## Objectives

- Quantify end-to-end timing for critical PTP paths (software-only vs hardware-assisted).
- Establish acceptance thresholds and reproducible benchmarks per requirement IDs.
- Provide CPU and memory budget estimates for target classes (x86-64 server, ARM Cortex-M7 embedded).

## Scope and Scenarios (baseline set)

- Sync + Follow_Up processing latency (ingress → timestamp extraction → servo update).
- Delay_Req/Delay_Resp round-trip timing and jitter.
- Announce reception and BMCA reaction time including state transition latency (ties to REQ-S-001).
- CorrectionField propagation and servo smoothing under outlier and message loss conditions.

## Quantitative Targets (initial placeholders; to be validated)

- Offset from master (software): ≤ ±1 µs P95; jitter ≤ 500 ns P95.
- Offset from master (hardware timestamp): ≤ ±100 ns P95; jitter ≤ 50 ns P95.
- Critical path WCET (software-only): ≤ 50 µs per PTP packet on reference CPU.
- Memory footprint (PTP core): ≤ 64 KiB text, ≤ 16 KiB data on embedded target.

## Measurement & Benchmark Plan

- Synthetic packet replay at controlled rates (1–128 pkt/s) with timestamp hooks at handler entry/exit.
- Servo step response evaluation with controlled offset injections.
- Long-run stability test (≥1 h) to capture jitter distribution and outliers.
- Artifact capture: raw CSV metrics, summarized percentiles, and WCET bounds.

## Estimation Methodology

- Combine microbench results with cycle-count approximations for embedded targets.
- Derive CPU utilization envelope under typical and worst-case traffic.
- Validate memory via static size reports and link-map inspection.

## Data Recording Format

- CSV columns: scenario_id, timestamp_ns, cpu_type, mode(sw|hw-ts), metric_name, value.
- Summary: median, P95, P99, max for latency/offset/jitter; WCET point estimate + margin.

## Acceptance Criteria (ties to requirements)

- Meets REQ-NF-P-001/002 thresholds for timing accuracy and WCET.
- Meets REQ-NF-P-003 resource bounds on target classes.
- Demonstrates BMCA transition responsiveness per REQ-S-001 without instability.

## Deliverables

- performance-results/ (CSV + summary.md)
- Updated thresholds recorded back into requirements and design specs.
- One-page summary embedded into Phase 04 design kickoff.

## Notes

This file is marked specType: guidance to avoid interfering with schema-gated specs while we iterate on measurements. Once finalized, figures will be folded into governed architecture/design documents.
