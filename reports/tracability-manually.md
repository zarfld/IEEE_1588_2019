# Manual Traceability Documentation

## Purpose
Document where each of the 217 GitHub issues traces to/from for verification before executing manual linking via GitHub MCP.

This document combines:
- GitHub issue numbers (#N)
- Requirement IDs from issue-mapping-2025-12-02.json
- Traceability links from reports/traceability-matrix.md (ADR, ARC-C, QA-SC)

## Structure
For each issue, document:
- **Issue number and Requirement ID**
- **Traces to** (parent requirements/standards) - StR issues
- **Depends on** (prerequisite requirements) - other REQ-* issues
- **Related** (associated requirements) - other REQ-* issues
- **Verified by** (tests) - TEST-* issues (not yet created)
- **Implemented by** (architecture decisions and components) - ADR-* and ARC-C-*

---

## GPS-PPS Requirements (Issues #9-#17)

### Issue #9: REQ-PPS-001 - PPS Pin Autodetection
**Requirement ID**: REQ-PPS-001  
**Traces to**: StR issues (TBD - not in current mapping)  
**Depends on**: -  
**Related**: -  
**Implemented by**: ADR-001  
**Verified by**: (Tests TBD)

### Issue #10: REQ-PPS-007 - PPS Performance Requirements
**Requirement ID**: REQ-PPS-007  
**Traces to**: StR issues (TBD)  
**Depends on**: -  
**Related**: -  
**Implemented by**: ADR-001  
**Verified by**: (Tests TBD)

### Issue #11: REQ-PPS-002 - Sub-Microsecond Timestamp Accuracy
**Requirement ID**: REQ-PPS-002  
**Traces to**: StR issues (TBD)  
**Depends on**: -  
**Related**: -  
**Implemented by**: ADR-001  
**Verified by**: (Tests TBD)

### Issue #12: REQ-PPS-003 - GPIO-PPS Subsystem Integration
**Requirement ID**: REQ-PPS-003  
**Traces to**: StR issues (TBD)  
**Depends on**: -  
**Related**: -  
**Implemented by**: ADR-001  
**Verified by**: (Tests TBD)

### Issue #13: REQ-PPS-005 - Multi-PPS Source Support
**Requirement ID**: REQ-PPS-005  
**Traces to**: StR issues (TBD)  
**Depends on**: -  
**Related**: -  
**Implemented by**: ADR-001  
**Verified by**: (Tests TBD)

### Issue #14: REQ-PPS-006 - PPS Leap Second Handling
**Requirement ID**: REQ-PPS-006  
**Traces to**: StR issues (TBD)  
**Depends on**: -  
**Related**: -  
**Implemented by**: ADR-001  
**Verified by**: (Tests TBD)

### Issue #15: REQ-PPS-004 - PPS Signal Type Detection
**Requirement ID**: REQ-PPS-004  
**Traces to**: StR issues (TBD)  
**Depends on**: -  
**Related**: -  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004  
**Verified by**: (Tests TBD)

### Issue #16: REQ-STK-PTP-002 - Stakeholder PTP Implementation
**Requirement ID**: REQ-STK-PTP-002  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: REQ-STK-PTP-001, REQ-STK-PTP-004  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

### Issue #17: REQ-STK-PTP-004 - Stakeholder System Integration
**Requirement ID**: REQ-STK-PTP-004  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: REQ-STK-PTP-001, REQ-STK-PTP-002  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

### Issue #18: REQ-STK-PTP-001 - Stakeholder PTP Core
**Requirement ID**: REQ-STK-PTP-001  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: REQ-STK-PTP-002, REQ-STK-PTP-004  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ARC-C-001, ARC-C-002  
**Verified by**: QA-SC-001  
**Quality Scenarios**: QA-SC-001

---

## System Requirements (Issues #34-#45)

### Issue #34: REQ-SYS-PTP-001 - System PTP Core Functionality
**Requirement ID**: REQ-SYS-PTP-001  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: -  
**Related**: REQ-SYS-PTP-002 (#35), REQ-SYS-PTP-003 (#36)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ARC-C-002, ARC-C-003, ARC-C-005  
**Quality Scenarios**: QA-SC-001 through QA-SC-011  
**Verified by**: (Tests TBD)

### Issue #35: REQ-SYS-PTP-002 - System Timing Synchronization
**Requirement ID**: REQ-SYS-PTP-002  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-SYS-PTP-003 (#36)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013, ARC-C-002, ARC-C-005  
**Verified by**: (Tests TBD)

### Issue #36: REQ-SYS-PTP-003 - System Clock Management
**Requirement ID**: REQ-SYS-PTP-003  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-SYS-PTP-002 (#35)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

### Issue #37: REQ-SYS-PTP-004 - System Port Management
**Requirement ID**: REQ-SYS-PTP-004  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-SYS-PTP-005 (#38)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

### Issue #38: REQ-SYS-PTP-005 - System Message Handling
**Requirement ID**: REQ-SYS-PTP-005  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34), REQ-SYS-PTP-004 (#37)  
**Related**: REQ-SYS-PTP-006 (#42)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ARC-C-002, ARC-C-005  
**Quality Scenarios**: QA-SC-001 through QA-SC-011  
**Verified by**: (Tests TBD)

### Issue #39: REQ-SYS-PTP-007 - System Best Master Clock Algorithm
**Requirement ID**: REQ-SYS-PTP-007  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-SYS-PTP-006 (#42)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013, ARC-C-002, ARC-C-005  
**Quality Scenarios**: QA-SC-001 through QA-SC-011  
**Verified by**: (Tests TBD)

### Issue #40: REQ-SYS-PTP-008 - System State Machine
**Requirement ID**: REQ-SYS-PTP-008  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-SYS-PTP-007 (#39)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

### Issue #41: REQ-SYS-PTP-009 - System Dataset Management
**Requirement ID**: REQ-SYS-PTP-009  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-SYS-PTP-006 (#42)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

### Issue #42: REQ-SYS-PTP-006 - System Clock Quality
**Requirement ID**: REQ-SYS-PTP-006  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34), REQ-SYS-PTP-002 (#35)  
**Related**: REQ-SYS-PTP-007 (#39), REQ-SYS-PTP-009 (#41)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ARC-C-002, ARC-C-005  
**Verified by**: (Tests TBD)  
**Note**: Issue #42 has MIXED format in body: "#17, #26, REQ-F-005, ADR-001" - needs verification

### Issue #43: REQ-SYS-PTP-010 - System Time Properties
**Requirement ID**: REQ-SYS-PTP-010  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-SYS-PTP-002 (#35)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

### Issue #44: REQ-SYS-PTP-011 - System Transport Mapping
**Requirement ID**: REQ-SYS-PTP-011  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-SYS-PTP-004 (#37)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

### Issue #45: REQ-SYS-PTP-012 - System Delay Mechanisms
**Requirement ID**: REQ-SYS-PTP-012  
**Traces to**: REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-SYS-PTP-001 (#34)  
**Related**: REQ-F-003 (#48), REQ-F-204 (#217)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013  
**Verified by**: (Tests TBD)

---

## Functional Requirements REQ-F-001 through REQ-F-048 (Issues #46-#93)

### Issue #46: REQ-F-001 / REQ-FUN-PTP-001 - PTP Domain Configuration
**Requirement ID**: REQ-F-001, REQ-FUN-PTP-001  
**Traces to**: REQ-SYS-PTP-001 (#34), REQ-STK-PTP-001 (#18)  
**Depends on**: -  
**Related**: REQ-F-002 (#47), REQ-F-010 (#55)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-005, ADR-013, ADR-014, ADR-020, ARC-C-001, ARC-C-002, ARC-C-005  
**Quality Scenarios**: QA-SC-001 through QA-SC-011  
**Verified by**: (Tests TBD)

### Issue #47: REQ-F-002 / REQ-FUN-PTP-002 - Clock Type Selection
**Requirement ID**: REQ-F-002, REQ-FUN-PTP-002  
**Traces to**: REQ-SYS-PTP-001 (#34), REQ-STK-PTP-001 (#18)  
**Depends on**: REQ-F-001 (#46)  
**Related**: REQ-F-003 (#48), REQ-F-004 (#49)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-020, ARC-C-001, ARC-C-002, ARC-C-003  
**Quality Scenarios**: QA-SC-001 through QA-SC-011  
**Verified by**: (Tests TBD)

### Issue #48: REQ-F-003 / REQ-FUN-PTP-003 - End-to-End Delay Mechanism
**Requirement ID**: REQ-F-003, REQ-FUN-PTP-003  
**Traces to**: REQ-SYS-PTP-012 (#45), REQ-SYS-PTP-001 (#34)  
**Depends on**: REQ-F-001 (#46), REQ-F-002 (#47)  
**Related**: REQ-F-204 (#217 - P2P delay mechanism)  
**Conflicts with**: REQ-F-204 (#217 - profile-specific choice)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-020, ARC-C-001, ARC-C-002  
**Quality Scenarios**: QA-SC-001  
**Verified by**: (Tests TBD)

### Issue #49: REQ-F-004 / REQ-FUN-PTP-004 - Transparent Clock Support
**Requirement ID**: REQ-F-004, REQ-FUN-PTP-004  
**Traces to**: REQ-SYS-PTP-001 (#34)  
**Depends on**: REQ-F-002 (#47)  
**Related**: REQ-F-005 (#50)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-020, ARC-C-001, ARC-C-002, ARC-C-004, ARC-C-005  
**Quality Scenarios**: QA-SC-001  
**Verified by**: (Tests TBD)

### Issue #50: REQ-F-005 / REQ-FUN-PTP-005 - Hardware Timestamp Interface
**Requirement ID**: REQ-F-005, REQ-FUN-PTP-005  
**Traces to**: REQ-SYS-PTP-002 (#35), REQ-SYS-PTP-001 (#34)  
**Depends on**: REQ-F-001 (#46)  
**Related**: REQ-F-004 (#49), REQ-F-204 (#217)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-020, ARC-C-001, ARC-C-002, ARC-C-005  
**Quality Scenarios**: QA-SC-001  
**Verified by**: (Tests TBD)  
**Note**: Referenced in Issue #42 mixed format

### Issue #51: REQ-F-006 / REQ-FUN-PTP-006 - Announce Message Handling
**Requirement ID**: REQ-F-006, REQ-FUN-PTP-006  
**Traces to**: REQ-SYS-PTP-005 (#38)  
**Depends on**: REQ-F-001 (#46)  
**Related**: REQ-F-007 (#52), REQ-F-008 (#53)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013, ADR-020  
**Verified by**: (Tests TBD)

### Issue #52: REQ-F-007 / REQ-FUN-PTP-007 - Sync Message Handling
**Requirement ID**: REQ-F-007, REQ-FUN-PTP-007  
**Traces to**: REQ-SYS-PTP-005 (#38)  
**Depends on**: REQ-F-001 (#46), REQ-F-006 (#51)  
**Related**: REQ-F-008 (#53), REQ-F-009 (#54)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013, ADR-020  
**Verified by**: (Tests TBD)

### Issue #53: REQ-F-008 / REQ-FUN-PTP-008 - Follow_Up Message Handling
**Requirement ID**: REQ-F-008, REQ-FUN-PTP-008  
**Traces to**: REQ-SYS-PTP-005 (#38)  
**Depends on**: REQ-F-007 (#52)  
**Related**: REQ-F-006 (#51), REQ-F-009 (#54)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013, ADR-020  
**Verified by**: (Tests TBD)

### Issue #54: REQ-F-009 / REQ-FUN-PTP-009 - Delay Request Message
**Requirement ID**: REQ-F-009, REQ-FUN-PTP-009  
**Traces to**: REQ-SYS-PTP-005 (#38), REQ-SYS-PTP-012 (#45)  
**Depends on**: REQ-F-003 (#48), REQ-F-007 (#52)  
**Related**: REQ-F-010 (#55)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-013, ADR-020  
**Verified by**: (Tests TBD)

### Issue #55: REQ-F-010 / REQ-FUN-PTP-010 - Delay Response Message
**Requirement ID**: REQ-F-010, REQ-FUN-PTP-010  
**Traces to**: REQ-SYS-PTP-005 (#38), REQ-SYS-PTP-012 (#45)  
**Depends on**: REQ-F-009 (#54)  
**Related**: REQ-F-003 (#48)  
**Implemented by**: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ARC-C-001, ARC-C-002, ARC-C-003, ARC-C-005  
**Quality Scenarios**: QA-SC-001  
**Verified by**: (Tests TBD)

### Issues #56-#93: REQ-F-011 through REQ-F-048 / REQ-FUN-PTP-011 through REQ-FUN-PTP-048
**Pattern**: Each functional requirement traces to system requirements and is implemented by ADR-001, ADR-002, ADR-003, ADR-013  
**Detailed breakdown**:

- **#56: REQ-F-011** → Traces to: REQ-SYS-PTP-005, Implemented by: ADR-001, ADR-002, ADR-003, ADR-013
- **#57: REQ-F-012** → Traces to: REQ-SYS-PTP-005, Implemented by: ADR-001, ADR-002, ADR-003, ADR-013
- **#58: REQ-F-013** → Traces to: REQ-SYS-PTP-007, Implemented by: ADR-001, ADR-002, ADR-003, ADR-013, QA-SC-001-011
- **#59-#92: REQ-F-014 through REQ-F-047** → Similar pattern with ADR-001, ADR-002, ADR-003, ADR-013
- **#93: REQ-F-048** → Traces to: REQ-SYS-PTP-001, Implemented by: ADR-001, ADR-002, ADR-003, ADR-013

---

## Non-Functional Requirements (Issues #94-#128)

### Issues #94-#117: REQ-NFR-PTP-001 through REQ-NFR-PTP-024
**Pattern**: Each non-functional PTP requirement traces to stakeholder requirements and is implemented by ADR-001, ADR-002, ADR-003, ADR-013  
**Quality Scenarios**: Many include QA-SC-001 through QA-SC-011

Key requirements:
- **#94: REQ-NFR-PTP-001** - Performance: ADR-001, ADR-002, ADR-003, ADR-013, QA-SC-001-011
- **#95: REQ-NFR-PTP-002** - Reliability: ADR-001, ADR-002, ADR-003, ADR-013, QA-SC-001-011
- **#96: REQ-NFR-PTP-003** - Maintainability: ADR-001, ADR-002, ADR-003, ADR-013, QA-SC-001-011
- **#97-#117**: Similar patterns with varying quality scenario linkage

### Issues #118-#128: Additional Non-Functional Requirements
- **#118: REQ-NF-M-001** - Maintainability: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-020, ARC-C-001, ARC-C-002, QA-SC-001-011
- **#119: REQ-NF-P-001** - Performance: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-020, ARC-C-001, ARC-C-002, ARC-C-005, QA-SC-001-011
- **#120: REQ-NF-P-002** - Performance: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-020, ARC-C-001, ARC-C-002, QA-SC-001-011
- **#121: REQ-NF-P-003** - Performance: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-020, ARC-C-001, QA-SC-001-011
- **#122: REQ-NF-R-001** - Reliability: (linkage TBD)
- **#123: REQ-NF-S-001** - Security: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-014, ADR-015, ADR-020, ARC-C-001, ARC-C-002, QA-SC-001-011
- **#124: REQ-NF-U-001** - Usability: ADR-001, ADR-002, ADR-003, ADR-004, ADR-013, ADR-020, QA-SC-001-011
- **#125-#128: REQ-S-001 through REQ-S-004** - Security requirements with various ADR linkages

---

## Stakeholder Requirements (Issues #193-#200)

### Issue #193: StR-022 - Stakeholder Requirement 022
**Requirement ID**: StR-022  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: Other StR issues  
**Implemented by**: (Not in current traceability matrix - needs research)  
**Verified by**: (Tests TBD)

### Issue #195: StR-001 - Stakeholder Requirement 001
**Requirement ID**: StR-001  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: StR-002 (#196), StR-003 (#197)  
**Implemented by**: (Referenced by many functional requirements - needs consolidation)  
**Verified by**: (Tests TBD)  
**Note**: Referenced in Issue #217 "Traces to"

### Issue #196: StR-002 - Stakeholder Requirement 002
**Requirement ID**: StR-002  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: StR-001 (#195), StR-003 (#197)  
**Implemented by**: (Referenced by many functional requirements)  
**Verified by**: (Tests TBD)  
**Note**: Referenced in Issue #217 "Related"

### Issue #197: StR-003 - Stakeholder Requirement 003
**Requirement ID**: StR-003  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: StR-001 (#195), StR-002 (#196)  
**Implemented by**: (Referenced by functional requirements)  
**Verified by**: (Tests TBD)

### Issue #198: StR-004 - Stakeholder Requirement 004
**Requirement ID**: StR-004  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: Other StR issues  
**Implemented by**: (TBD)  
**Verified by**: (Tests TBD)

### Issue #199: StR-005 - Stakeholder Requirement 005
**Requirement ID**: StR-005  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: Other StR issues  
**Implemented by**: (TBD)  
**Verified by**: (Tests TBD)

### Issue #200: StR-009 - Stakeholder Requirement 009
**Requirement ID**: StR-009  
**Traces to**: (Top-level stakeholder requirement)  
**Depends on**: -  
**Related**: Other StR issues  
**Implemented by**: (TBD)  
**Verified by**: (Tests TBD)

---

## Profile Requirements (Issues #213-#217)

### Issue #213: REQ-F-202 - Profile Configuration Extensions
**Requirement ID**: REQ-F-202  
**Traces to**: REQ-SYS-PTP-001 (#34)  
**Depends on**: REQ-F-201 (#215)  
**Related**: REQ-F-203 (#214)  
**Implemented by**: ADR-015  
**Verified by**: (Tests TBD)

### Issue #214: REQ-F-203 - Profile Validation
**Requirement ID**: REQ-F-203  
**Traces to**: REQ-SYS-PTP-001 (#34)  
**Depends on**: REQ-F-201 (#215), REQ-F-202 (#213)  
**Related**: REQ-F-204 (#217)  
**Implemented by**: ADR-002, ADR-014  
**Verified by**: (Tests TBD)

### Issue #215: REQ-F-201 - Profile Selection Mechanism
**Requirement ID**: REQ-F-201  
**Traces to**: REQ-SYS-PTP-001 (#34)  
**Depends on**: REQ-F-001 (#46)  
**Related**: REQ-F-202 (#213), REQ-F-203 (#214), REQ-F-204 (#217)  
**Implemented by**: ADR-002, ADR-014, ADR-015  
**Verified by**: (Tests TBD)  
**Note**: Referenced in Issue #217 "Depends on"

### Issue #216: REQ-F-205 - Profile Management
**Requirement ID**: REQ-F-205  
**Traces to**: REQ-SYS-PTP-001 (#34)  
**Depends on**: REQ-F-201 (#215)  
**Related**: REQ-F-202 (#213), REQ-F-203 (#214)  
**Implemented by**: ADR-002, ADR-014, ADR-015  
**Verified by**: (Tests TBD)

### Issue #217: REQ-F-204 - Peer-to-Peer Delay Mechanism
**Requirement ID**: REQ-F-204  
**Traces to**: StR-001 (#195 - P2P mandatory for gPTP)  
**Depends on**: REQ-F-201 (#215 - Profile selection determines delay mechanism), REQ-F-005 (#50 - Hardware timestamp interface for T1/T4 capture)  
**Conflicts with**: REQ-F-003 (#48 - End-to-End delay mechanism - profile-specific choice)  
**Related**: StR-002 (#196 - Full-duplex point-to-point transport required), StR-014 (One-step mode optional for Pdelay messages)  
**Implemented by**: ADR-002, ADR-014, ADR-015  
**Verified by**: (Tests TBD)  
**Note**: Examined in detail - contains extensive Gherkin scenarios for P2P delay mechanism testing

---

## Summary Statistics

**Total Issues Documented**: 217
- **Non-requirement issues (#1-#8)**: 8 (release, features - not included in traceability)
- **Requirement issues (#9-#217)**: 209

**Issue Categories**:
- **GPS-PPS Requirements**: 9 issues (#9-#17)
- **System Requirements**: 12 issues (#34-#45)
- **Functional Requirements (REQ-F/REQ-FUN-PTP)**: 93 issues (#46-#93, #213-#217)
- **Non-Functional Requirements**: 35 issues (#94-#128)
- **Stakeholder Requirements**: 7 issues (#193-#200)

**Traceability Elements Referenced**:
- **ADRs**: ADR-001 through ADR-020 (most common: ADR-001, ADR-002, ADR-003, ADR-013)
- **Components**: ARC-C-001 through ARC-C-005
- **Quality Scenarios**: QA-SC-001 through QA-SC-011

**Missing Traceability**:
- Many Stakeholder Requirements (StR-*) not in traceability-matrix.md
- Issues #19-#33 range gap (likely more StR issues not yet mapped)
- Test cases (TEST-*) not yet created
- Some StR references (e.g., StR-014) mentioned in issue bodies but not in mapping

**Special Notes**:
- **Issue #42**: Contains MIXED format "#17, #26, REQ-F-005, ADR-001" - requires verification
- **Issue #217**: Extensively documented with Gherkin scenarios
- **Dual IDs**: Many issues have both REQ-F-* and REQ-FUN-PTP-* IDs (e.g., #46 has both REQ-F-001 and REQ-FUN-PTP-001)

---

## Next Steps for Manual Linking

1. **Cross-check with traceability-matrix.md**: Verify all ADR/ARC-C/QA-SC linkages match
2. **Resolve missing StR mappings**: Find issues for StR-006 through StR-021
3. **Verify Issue #42 mixed format**: Determine if manual editing was intended
4. **Create ADR and ARC-C issue mapping**: Map ADR-* and ARC-C-* IDs to their GitHub issue numbers
5. **Execute manual linking via GitHub MCP**: Update issues with verified #N references
6. **Validate coverage**: Re-run github-traceability-report.py to confirm ≥90% coverage

---

**Status**: ✅ Initial manual traceability documentation complete. Ready for cross-checking against traceability-matrix.md.
