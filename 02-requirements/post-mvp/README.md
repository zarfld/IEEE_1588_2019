# Post-MVP Requirements (Phase 02 - Cross-Standards Integration)

**Status**: Deferred to Phase 02 (Post-MVP)  
**Date**: 2025-11-07  
**Reason**: Focus Phase 01 MVP on IEEE 1588-2019 PTP only per project roadmap

---

## Overview

This directory contains requirements that are **out of scope** for Phase 01 MVP but will be addressed in Phase 02 (post-MVP) once the core IEEE 1588-2019 PTP implementation is stable and validated.

## Deferred Requirements

### 1. Cross-Standards Architecture Integration (58 requirements)

**File**: `cross-standards-architecture-integration-requirements.md`

**Scope**: Integration with related IEEE/AES/AVnu standards:
- IEEE 802.1AS (gPTP) - Generalized Precision Time Protocol
- IEEE 1722 (AVTP) - Audio Video Transport Protocol
- IEEE 1722.1 (AVDECC) - Audio Video Device Discovery, Enumeration, and Control
- AES67 - Audio-over-IP interoperability
- AVnu Milan - Professional audio profiles

**Rationale**: Cross-standards integration requires stable IEEE 1588-2019 foundation first. These requirements add significant complexity (protocol mapping, multi-standard state machines, cross-layer dependencies) inappropriate for MVP.

**MVP Boundary**: Phase 01 delivers **IEEE 1588-2019 PTP only** as a hardware-agnostic library. Cross-standards features require Phase 01 completion for proper architectural foundation.

### 2. Cross-Standard Dependency Analysis (5 requirements)

**File**: `cross-standard-dependency-analysis.md`

**Scope**: Dependency management across multiple IEEE/AES standards:
- Timestamp correlation between PTP and AVTP
- Clock domain synchronization across 802.1AS and 1588-2019
- Security mechanisms spanning multiple protocols
- Configuration management for multi-standard deployments

**Rationale**: Dependency analysis presumes multi-standard deployment. MVP focuses on single-standard (IEEE 1588-2019) deployments only. Cross-standard dependencies introduce testing complexity requiring dedicated conformance infrastructure.

## Phase 02 Planning

### Target Completion: Q2-Q3 2026 (Post-MVP)

**Prerequisites for Phase 02**:
1. ✅ Phase 01 MVP complete with IEEE 1588-2019 PTP core
2. ✅ Conformance testing suite operational (>95% test coverage)
3. ✅ Reference HAL implementations for 3+ platforms
4. ✅ Production deployments validated (stability, performance, interoperability)
5. ✅ Architectural refactoring complete (prepare for multi-standard support)

### Phase 02 Milestones

**Phase 02A** (Weeks 1-12): IEEE 802.1AS (gPTP) Integration
- Extend PTP core with 802.1AS-specific features (peer-to-peer delay, gPTP profiles)
- Implement gPTP state machines and message extensions
- Conformance testing against commercial gPTP devices

**Phase 02B** (Weeks 13-24): AVTP/AVDECC Integration
- IEEE 1722 AVTP transport layer integration
- IEEE 1722.1 AVDECC discovery and control protocols
- Stream synchronization with PTP timestamps

**Phase 02C** (Weeks 25-36): AES67/Milan Profiles
- AES67 audio-over-IP profile implementation
- AVnu Milan professional audio extensions
- Multi-vendor interoperability testing

## Stakeholder Communication

**Message to Stakeholders**: "Phase 01 MVP delivers robust, hardware-agnostic IEEE 1588-2019 PTP implementation. Cross-standards integration (802.1AS, AVTP, AVDECC, AES67, Milan) is planned for Phase 02 post-MVP after core PTP is production-validated. This phased approach ensures quality foundation before adding complexity."

**Benefits of Deferral**:
- ✅ **Faster MVP delivery** - Focus on 9 core requirements vs. 72 total requirements
- ✅ **Reduced risk** - Validate single-standard implementation before multi-standard complexity
- ✅ **Better architecture** - Learn from MVP deployments, inform cross-standards design
- ✅ **Clear milestones** - Phase 01 success criteria unambiguous (IEEE 1588-2019 conformance only)

## Cross-Standards Requirements Traceability

When Phase 02 begins, these requirements will be:

1. **Re-validated** against current stakeholder needs (priorities may shift)
2. **Refined** based on Phase 01 MVP lessons learned
3. **Linked** to Phase 01 architecture (extend existing components vs. new components)
4. **Tested** with cross-standards conformance suites (AVnu, AES)

**Traceability Preservation**: All deferred requirements maintain their original IDs (REQ-F-CROSSARCH-###, REQ-NF-CROSSSTD-###) for future reference. When activated in Phase 02, they will receive Phase 02 requirement IDs while preserving backward traceability.

---

**Document Owner**: Requirements Engineering Team  
**Last Updated**: 2025-11-07  
**Next Review**: Phase 01 MVP completion + 30 days
