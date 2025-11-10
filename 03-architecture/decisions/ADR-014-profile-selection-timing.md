---
specType: architecture
standard: 42010
phase: 03-architecture
version: 1.0.0
author: Architecture Team
date: "2025-11-09"
status: draft
traceability:
  requirements:
    - REQ-F-201
    - REQ-F-203
    - REQ-F-204
    - REQ-F-205
---

# ADR-014: Profile Selection Timing (Build-time vs. Runtime)

## Metadata

```yaml
adrId: ADR-014
status: proposed
relatedRequirements:
  - REQ-F-201  # Profile abstraction
  - REQ-F-203  # Domain 0 default with external control disabled
  - REQ-F-204  # P2P delay mechanism
  - REQ-F-205  # Dataset/MIB coherence
relatedComponents:
  - DES-C-010  # Clock/Port state machines
  - DES-C-003  # BMCA engine
  - ProfileAdapter
supersedes: []
supersededBy: null
author: Architecture Team
date: 2025-11-09
reviewers: []
```

## Context
We must support multiple PTP profiles (e.g., IEEE 802.1AS/gPTP, AES67, industrial IEC/IEEE 60802) with conflicting defaults (P2P vs E2E, announce/sync intervals, management). The decision is when to bind a profile: at build-time (compile-time flags) or at runtime (config/API). Stakeholder needs include easy integration (Makers), interoperability (System Integrators), and deterministic behavior (gPTP). Non-functional constraints: platform-agnostic library, no vendor/OS code, deterministic execution.

## Decision
We will support runtime profile selection via a small Profile Adapter API, with optional build-time defaults. Runtime selection applies during port initialization and may be re-applied at state reset boundaries. Build-time flags can set safe defaults per artifact (e.g., examples), but the standards layer remains runtime-configurable.

## Status
Proposed; minimal adapter exists (`applyProfileToPortConfig`). Further wiring and tests needed to validate re-application and dataset invariants.

## Rationale
- Flexibility: Consumers select profiles without recompiling.
- Determinism: Adapter maps to immutable port config fields before start; avoids mid-flight churn.
- Testability: TDD across profiles in one binary.
- Compliance: Clean separation keeps standards layer hardware-agnostic.

## Considered Alternatives
| Alternative | Summary | Pros | Cons | Reason Not Chosen |
|------------|---------|------|------|-------------------|
| Build-time only | Compile for a single profile | Simple, small footprint | Inflexible, multiple binaries | Limits integration and testing |
| Mixed: runtime for some, build-time for others | Hybrid | Targeted optimization | Inconsistent behavior | Complexity without clear gain |

## Consequences
### Positive
- One binary covers multiple profiles.
- Easier CI matrix; simpler docs for users.

### Negative / Liabilities
- Slightly more API surface (adapter + config).
- Need guards to prevent runtime flipping post-start.

### Neutral / Follow-ups
- Document re-apply points (initialize/reset only).
- Provide example configs for common profiles.

## Quality Attribute Impact Matrix
| Quality Attribute | Impact | Notes |
|-------------------|--------|-------|
| Performance | 0 | Adapter runs at init only.
| Scalability | + | One binary, many deployments.
| Security | 0 | No change.
| Maintainability | + | Centralized mapping.

## Risks
| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| Misconfiguration at runtime | Medium | Medium | Validate profile config; defaults + schema.
| Mid-flight changes | Low | High | Disallow profile changes after start; enforce in API.

## Compliance Mapping
| Standard Clause | How Addressed |
|-----------------|---------------|
| ISO/IEC/IEEE 42010 (Rationale) | Decision + rationale documented |
| ISO/IEC/IEEE 29148 (Req trace) | Linked to REQ-F-201/204/205 |

## Implementation Notes
- Keep `PortConfiguration` as the single source for profile mapping (e.g., `delay_mechanism_p2p`).
- Expose `applyProfileToPortConfig(profile, config)`; call prior to `initialize/start`.
- Add guard: reject profile changes while port is not in Initializing.
- Provide example configs for gPTP (P2P default), AES67 (E2E default).

## Validation Plan
- Unit tests per profile applying adapter produce expected dataset invariants (e.g., P2P suppresses E2E delay updates).
- Acceptance tests for profile defaults and prohibiting mid-flight changes.

## References
- ADR-002 IEEE standards layering
- Derived requirements REQ-F-201..205
