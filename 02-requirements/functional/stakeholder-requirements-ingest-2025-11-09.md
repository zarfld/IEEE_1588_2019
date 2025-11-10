---
specType: guidance
phase: "01-stakeholder-requirements"
version: "0.1.0"
author: "GitHub Copilot"
date: "2025-11-09"
status: draft
purpose: "Stakeholder requirements extraction from external standards (802.1AS, 60802, AES67)"
note: "This is a stakeholder requirements document (StR-*) that will be used to derive system requirements (REQ-*) in separate specifications"
---

# Stakeholder Requirements Ingest (802.1AS, 60802, AES67)

This document normalizes stakeholder requirements extracted from the notes in `docs/802.1AS_Stakeholderss_Requirments/` into atomic statements with stable IDs (StR-xxx). Each statement is phrased using original wording (no copyrighted text) and references the relevant standards clauses for traceability.

Notes:

- References are provided by clause/table numbers only; consult the original standards for authoritative details.
- These are stakeholder-facing needs that will be refined into system requirements and acceptance tests.


## 1) IEEE 802.1AS (gPTP) profile selections and prohibitions

- StR-001: The implementation shall use the Peer-to-Peer (P2P) path delay mechanism on full-duplex 802.3 links. (Ref: IEEE 802.1AS-2020 Clauses 8, 11; IEEE 1588-2019 11.4)
- StR-002: Time-aware system ports using IEEE 802.3 shall operate full-duplex and point-to-point; frames carrying 802.1AS messages are untagged. (Ref: 802.1AS-2020 transport constraints; 1588-2019 Annex E attribute values)
- StR-003: The Best Master Clock Algorithm (BMCA) shall be implemented per 802.1AS; for Domain 0, `externalPortConfigurationEnabled` is FALSE. (Ref: 802.1AS-2020 10.3.x)
- StR-004: The Path Trace TLV shall be processed and transmitted; `pathTraceDS.enable` is TRUE. (Ref: 802.1AS-2020; 1588-2019 16.2)
- StR-005: The PTP Port states FAULTY, UNCALIBRATED, LISTENING, PRE_MASTER and PRE_MASTER qualification are not used. (Ref: 802.1AS-2020 profile exclusions; 1588-2019 17.7)
- StR-006: The foreign master feature is not used in 802.1AS operation. (Ref: 802.1AS-2020 profile exclusions)
- StR-007: PTP Management Messages (per IEEE 1588) are not used; management shall follow 802.1AS Clause 14/15 data sets and MIB. (Ref: 802.1AS-2020 Clauses 14, 15)
- StR-008: IEEE 1588 integrated security (16.14, Annex P) is not used in 802.1AS operation. (Ref: 802.1AS-2020 exclusions)
- StR-009: The implementation shall not use MAC Control PAUSE and shall neither transmit nor honor PFC acting on 802.1AS priority; it shall provide the capability to disable these if present. (Ref: 802.1AS TSN prohibitions)
- StR-010: The LocalClock frequency offset relative to TAI shall be within ±100 ppm, with measurement granularity better than 40 ns. (Ref: 802.1AS-2020 Annex B)

## 2) IEEE 802.1AS multi-domain and options

- StR-011: Support for multiple PTP domains (additional PTP Instances with domain 1–127) is optional; if supported, it shall be consistent with 802.1AS. (Ref: 802.1AS-2020 multi-domain)
- StR-012: The Common Mean Link Delay Service (CMLDS) shall be implemented if more than one domain is supported; otherwise optional. (Ref: 802.1AS-2020; 1588-2019 16.6)
- StR-013: External Port Configuration is optional; if enabled, it shall follow 1588-2019 17.6 while defaulting to FALSE for Domain 0. (Ref: 802.1AS-2020; 1588-2019 17.6)
- StR-014: One-step transmit/receive modes are optional and controlled via managed objects; if management override flags are FALSE, state machines decide operation. (Ref: 802.1AS-2020 Clause 14)
- StR-015: Delay asymmetry modeling and compensation are optional; if implemented, enable/disable via managed objects and procedures consistent with 1588-2019. (Ref: 802.1AS-2020; 1588-2019 16.8)

## 3) IEC/IEEE 60802 Industrial Automation (TSN-IA) profile overlays

- StR-016: The implementation shall support four synchronization domains as specified by the industrial profile. (Ref: IEC/IEEE 60802 profile tables)
- StR-017: CMLDS is mandatory when implementing the industrial profile multi-domain requirement. (Ref: 60802; 1588-2019 16.6)
- StR-018: Timestamp accuracy for sync and delay messages shall be ≤ 8 ns for universal time and working clock timescales. (Ref: 60802 accuracy constraints)
- StR-019: Devices shall achieve “in sync within < 1 µs accuracy” in < 1 s per device. (Ref: 60802 convergence constraint)
- StR-020: The implementation shall provide the capability to disable EEE (Energy Efficient Ethernet). (Ref: 60802 power/latency constraints)

## 4) AES67-2018 audio over IP (contrast profile) — for interoperability scope decisions

The following derive from AES67 notes and are captured to drive interoperability decisions and potential profile support beyond 802.1AS. These will likely be handled as separate profiles or build options and are not simultaneously active with 802.1AS.

- (Info) AES67 requires IEEE 1588-2008 default profiles with UDP/IPv4 transport and End-to-End (E2E) delay; management via 1588-2008 Management Messages. It recommends (optional) P2P in its media profile and mandates optional features be inactive unless explicitly enabled. (Refs: AES67-2018 profile)

## Sources

- IEEE 802.1AS-2020/ISO/IEC/IEEE 8802-1AS:2021 (gPTP) — selections, exclusions, management
- IEEE 1588-2019 — core mechanisms (delay, TLVs, datasets), options (CMLDS, security)
- IEC/IEEE 60802 (TSN-IA) — industrial constraints (domains, convergence, accuracy, disable features)
- AES67-2018 — media profile over IP (for interoperability planning)

## Next steps

- Derive system-level requirements and acceptance criteria from each StR.
- Map StR IDs to current implementation/tests and to CAPs for any uncovered functionality.
- For conflicting profiles (e.g., AES67 vs 802.1AS), define build-time profile selection and guard acceptance tests accordingly.
