---
title: "External Standards Stakeholder Requirements Matrix"
specType: requirements
standard: ISO/IEC/IEEE 29148:2018
phase: 02-requirements
version: 0.1.0
author: "standards-integration-workstream"
date: 2025-11-09
status: draft
traceability:
  stakeholderRequirements:
    - StR-EXTS-001
    - StR-EXTS-002
    - StR-EXTS-003
    - StR-EXTS-004
    - StR-EXTS-005
    - StR-EXTS-006
    - StR-EXTS-007
    - StR-EXTS-008
    - StR-EXTS-009
    - StR-EXTS-010
    - StR-EXTS-011
    - StR-EXTS-012
    - StR-EXTS-013
    - StR-EXTS-014
    - StR-EXTS-015
    - StR-EXTS-016
    - StR-EXTS-017
    - StR-EXTS-018
    - StR-EXTS-019
    - StR-EXTS-020
    - StR-EXTS-021
    - StR-EXTS-022
    - StR-EXTS-023
---

# External Standards Stakeholder Requirements Matrix

> Purpose: Consolidate interviewed external standards stakeholder expectations (IEEE 802.1AS-2020/2021 + Corrigendum 2023, IEC/IEEE 60802-2019, AES67-2018, IEEE 1588-2019 base) into uniquely identified stakeholder requirements (StR-STD-xxx) for downstream system & functional requirement derivation (REQ-F / REQ-NF). Each entry includes: intent, source, clause pointer (generic – avoids copyrighted verbatim text), priority (MoSCoW), current coverage status, corrective action, planned release, and verification method. Schema aligned to ISO/IEC/IEEE 29148 traceability (phase 02 bridging artifact).

## Legend
- Priority (MoSCoW): M = Must (MVP or compliance gate), S = Should (near-term), C = Could (later), W = Won't (explicitly deferred)
- Coverage: ✅ Covered | 🟡 Partial | ❌ Gap | ⏳ Planned
- Release Target: MVP (v1.0.0 core), R1 (first minor), R2 (second), FUTURE (deferred backlog)
- Verification: Unit, Integration, Conformance (PICS / profile), Performance, Inspection, Static Analysis.

## Stakeholder Requirement Table

| ID | Requirement Statement (Stakeholder Intent) | Source Standard(s) | Clause Ref (Indicative) | Category | Priority | Coverage | Corrective / Action Next | Release Target | Verification Method |
|----|---------------------------------------------|--------------------|-------------------------|----------|----------|----------|---------------------------|----------------|---------------------|
| StR-EXTS-001 | Mandatory Peer-to-Peer (P2P) delay mechanism for full-duplex 802.3 links | 802.1AS-2020/2021 | Delay Mechanism (P2P) | Timing | M | ❌ | Implement P2P exchange & arithmetic; add unit & integration tests; profile toggle | R1 | Unit (pdelay req/resp/fup), Integration multi-hop, Conformance (profile) |
| StR-EXTS-002 | Path Trace TLV always enabled (pathTraceDS.enable TRUE) | 802.1AS-2020/2021 | Optional TLV usage (Path Trace) | Protocol | S | ❌ | Implement TLV parse/append; size guard; tests for path chain growth | R1 | Unit TLV parse, Integration Announce chain, Inspection |
| StR-EXTS-003 | Deterministic BMCA master selection per gPTP profile constraints | 802.1AS; 1588-2019 | BMCA clauses | Timing | M | 🟡 | Complete priority vector ordering & tie handling; integrate multi-domain hooks | MVP | Unit BMCA, Integration multi-instance, Conformance (selection stability) |
| StR-EXTS-004 | Domain 0 mandatory; externalPortConfigurationEnabled FALSE by default | 802.1AS-2020/2021 | Domain control | Configuration | M | 🟡 | Enforce flag; add test verifying external control disabled on domain 0 | MVP | Unit config test, Integration domain startup |
| StR-EXTS-005 | Clock fractional frequency offset within ±100 ppm | 802.1AS Perf | Performance limits | Performance | M | ❌ | Add servo metrics & validation; performance test harness; acceptance criteria | R1 | Performance test (frequency drift), Metrics collection |
| StR-EXTS-006 | Time measurement granularity ≤40 ns | 802.1AS Perf | Granularity | Performance | S | ❌ | Define abstraction for timestamp resolution; simulate hardware capabilities | R1 | Unit granularity assertion, Static inspection |
| StR-EXTS-007 | Exclude PTP states: FAULTY, UNCALIBRATED, LISTENING, PRE_MASTER | 802.1AS-2020/2021 | State model | Protocol | M | 🟡 | Remove transitions / code paths; add negative tests ensuring states ignored | MVP | Unit state machine tests, Inspection |
| StR-EXTS-008 | Foreign master feature not used (gPTP) | 802.1AS-2020/2021 | BMCA exclusion | Protocol | M | ❌ | Introduce compile/runtime feature flag; ensure list disabled for gPTP profile | R1 | Unit disable test, Inspection |
| StR-EXTS-009 | Management via Clause 14/15 data sets & V2 MIB (no Mgmt messages) | 802.1AS-2020/2021 | Management model | Management | M | ❌ | Stub MIB accessor layer; map datasets; remove / gate 1588 Mgmt message handler | R1 | Unit dataset CRUD, Integration MIB query simulation |
| StR-EXTS-010 | Support 4 synchronization domains (Industrial profile) | IEC/IEEE 60802-2019 | Domain requirements | Scalability | S | ❌ | Multi-instance domain manager abstraction; domain config tests; performance impact | R2 | Integration multi-domain, Performance domain convergence |
| StR-EXTS-011 | CMLDS mandatory when >1 domain | 802.1AS (conditional) + 1588-2019 | CMLDS clauses | Timing | S | ❌ | Implement link mean delay service; dataset synchronization logic | R2 | Unit CMLDS math, Integration multi-domain delay consistency |
| StR-EXTS-012 | Timestamp accuracy ≤8 ns for sync & delay messages (Industrial) | IEC/IEEE 60802 | Perf constraints | Performance | M | ❌ | Define accuracy metrics; hardware abstraction flags; acceptance performance test | R2 | Performance tests, Metrics instrumentation |
| StR-EXTS-013 | Convergence <1 s to achieve <1 µs accuracy | IEC/IEEE 60802 | Convergence | Performance | S | ❌ | Add convergence measurement test scenario; optimize BMCA + servo loop | R2 | Integration timed run, Performance measurement |
| StR-EXTS-014 | Capability to disable MAC Pause, PFC, and EEE | 802.1AS + 60802 | Flow control exclusions | Configuration | S | ❌ | Add interface capability flags & tests verifying disabled semantics | R1 | Unit capability flags, Inspection |
| StR-EXTS-015 | External (non-Annex P) security integration (confidentiality, integrity, authenticity) without degrading timing | 60802 profile | Security integration | Security | C | ❌ | Draft ADR for security integration approach; stub hooks; defer heavy impl | FUTURE | ADR review, Inspection |
| StR-EXTS-016 | Diagnostics state including explicit "out of sync" | 60802 profile | Diagnostics | Observability | S | ❌ | Extend state/health enums; add metrics & test state transitions | R1 | Unit health tests, Integration trigger loss-of-sync |
| StR-EXTS-017 | AES67 End-to-End (E2E) delay mechanism support (Delay_Req/Resp) | AES67-2018 | Delay mechanism | Interop | S | 🟡 | Ensure E2E path complete; add profile abstraction to choose mechanism | R1 | Unit offset calc, Integration E2E scenario |
| StR-EXTS-018 | AES67 UDP/IPv4 multicast transport profile | AES67-2018 | Transport mapping | Interop | C | ❌ | Abstract transport layer; add IPv4 multicast simulation harness | R2 | Integration network simulation |
| StR-EXTS-019 | AES67 1588-2008 management messages support OR adaptation layer | AES67-2018 | Management 2008 | Interop | C | ❌ | Provide translation facade or limited subset; evaluate complexity; defer | FUTURE | Design review, Limited unit tests |
| StR-EXTS-020 | AES67 faster sync / delay intervals (startup optimization) | AES67-2018 | Interval tuning | Performance | C | ❌ | Parametrize interval selection; add stress startup test | R2 | Performance startup test |
| StR-EXTS-021 | AES67 additional clockClass values mapping to AES11 | AES67-2018 | Clock quality | Timing | C | ❌ | Extend clockClass enumeration; add mapping tests; non-breaking | R2 | Unit clockClass mapping |
| StR-EXTS-022 | Profile abstraction layer (gPTP, Industrial, AES67 coexist) | Cross-profile | Architecture | Architecture | M | ❌ | Define profile strategy object; dependency injection; tests selecting profile | R1 | Unit profile selection, Integration multi-profile simulation |
| StR-EXTS-023 | Automated traceability matrix & compliance updates (scripts) | Internal (process) | Tooling | Process | S | ❌ | Implement script scanning tests/REQ IDs; CI job; update gap backlog closure | R1 | Tool unit test, CI run |

## Coverage Snapshot (Initial)
Must-have MVP (Domain 0 BMCA, excluded states) partially present; majority of cross-profile & industrial performance constraints remain gaps. AES67 interop largely deferred until abstraction layer established.

## Corrective Action Queue (Extract)
- P2P Mechanism (StR-STD-001): Add new message structs (Pdelay_Req/Resp/Fup), implement arithmetic (peer mean path delay), integrate into port processing, add tests → then enable via profile selection.
- Path Trace TLV (StR-STD-002): Implement TLV parser & builder; maintain path list with size guard; integrate in Announce transmit; verify via multi-hop simulated test.
- BMCA Completion (StR-STD-003): Implement full priority vector comparison (clockClass, clockAccuracy, offsetScaledLogVariance, priority1/2, stepsRemoved, clockIdentity). Add tie injection tests (already partially present).
- Foreign Master Exclusion (StR-STD-008): Remove or gate foreign master list update; ensure BMCA selection uses only Announce messages from active domain per gPTP.
- Management Model (StR-STD-009): Provide dataset API surfaces (defaultDS/currentDS/parentDS/portDS/pathTraceDS/timePropertiesDS). Implement read accessors; stub write operations; compile-time flag for mgmt messages off.
- Multi-Domain & CMLDS (StR-STD-010/011): Domain manager with vector<DomainContext>; CMLDS computes mean link delay; ensure isolation of BMCA per domain.
- Industrial Performance (StR-STD-012/013): Add performance harness recording timestamps (T1–T4), compute accuracy & convergence; integrate metrics exporter.
- Flow Control Disable (StR-STD-014): Extend hardware abstraction capabilities struct; enforce disable semantics in initialization; add tests confirming no pause/PFC influence paths.
- Diagnostics ‘out of sync’ (StR-STD-016): Add state enumeration & transition when accuracy threshold violated for N intervals.
- Profile Abstraction (StR-STD-022): Strategy pattern + config selection; unify delay mechanism choice; constraints enforced per profile.

## Derivation Path (Traceability Plan)
Each StR-STD-xxx will map to one or more system requirements (REQ-F-* / REQ-NF-*) with acceptance criteria (Gherkin) in a forthcoming specification file (`02-requirements/external-standards-derived-requirements.md`). Tests will adopt naming pattern TEST-<Area>-<ID>-<Scenario> linking back via comments.

## Verification Strategy Overview
| Method | Purpose | Tooling | Exit Criteria |
|--------|---------|---------|---------------|
| Unit Tests | Deterministic algorithm & message format validation | CTest + GoogleTest | Pass ≥95% of new unit cases; coverage ≥80% for new modules |
| Integration Tests | Multi-domain, multi-profile orchestration | Simulated time + profile harness | All domain & profile transition scenarios pass |
| Conformance (Profile) | Validate mandatory vs excluded features (gPTP/AES67) | Scripted PICS checklist | 100% mandatory implemented; 0% excluded activated |
| Performance Tests | Accuracy, convergence, granularity | Perf harness (timing metrics) | Accuracy & convergence thresholds met or documented deviation plan |
| Process Automation | Traceability, gap closure | Scripts (to be added) | Matrix auto-updated per test headers; no orphan StR IDs |

## Release Phasing Recommendation
### MVP (v1.0.0)
Focus: Core PTP (1588-2019) + BMCA baseline + profile abstraction skeleton + excluded state enforcement (StR-STD-003, 004, 007, 022). Defer advanced accuracy & P2P until R1 to avoid schedule risk.

### R1
Add: P2P delay (001), Path Trace (002), Management model & MIB dataset interface (009), Flow control disable (014), Diagnostics state (016), Frequency accuracy metric (005), Foreign master exclusion finalization (008), Granularity (006), Traceability automation (023).

### R2
Add: Multi-domain + CMLDS (010, 011), Industrial performance constraints (012, 013), AES67 baseline transport & interval tuning (017–020), clockClass extensions (021), performance stabilization.

### Future
Security integration strategy (015), AES67 2008 management translation layer (019) – subject to architecture decision and resource assessment.

## Risks & Mitigations
| Risk | Impact | Mitigation |
|------|--------|-----------|
| P2P + E2E coexist complexity | Delays abstraction design | Implement profile strategy before adding P2P; isolate delay calculators |
| Multi-domain timing race conditions | Incorrect CMLDS results | Deterministic update ordering; lock-free single-writer pattern |
| Performance thresholds unmet on generic hardware | Compliance delay | Provide accuracy tier classification; hardware capability flags |
| AES67 & gPTP conflicting management models | Architectural churn | Introduce management facade decoupling wire protocol from dataset core |
| Traceability drift (manual updates) | Audit failure | Automate matrix update (StR-STD-023) early in R1 |

## Open Questions
1. Precision tier classification naming (e.g., Tier A ±100 ppm vs Industrial Tier ±8 ns accuracy) – confirm taxonomy.
2. Security integration baseline (TLS, MACsec, 802.1X) scope for timing path isolation – require ADR.
3. Do we enforce untagged frames strictly in all simulation harnesses for gPTP or allow VLAN for diagnostic mode?

## Next Steps
1. Derive formal system requirements spec from this matrix (create `external-standards-derived-requirements.md`).
2. Implement profile abstraction (StR-STD-022) – prerequisite for P2P vs E2E.
3. Add RED tests for P2P (001), Path Trace (002), Management dataset API (009).
4. Update gap backlog linking GAP-* items to StR IDs.

---
Copyright Notice: This matrix references standards conceptually without reproducing copyrighted text. Clause references are indicative; refer to official IEEE / IEC / AES documents for authoritative definitions.
