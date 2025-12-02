# Changelog

All notable changes to the IEEE 1588-2019 PTP implementation will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2025-12-02

### 🎉 Initial Preview Release

First public preview release of the hardware-agnostic IEEE 1588-2019 Precision Time Protocol (PTPv2) implementation. This is an **alpha/preview release** for early feedback and community validation.

**Release Quality**: All CI quality gates pass - builds clean on all platforms (Linux/Windows/macOS), 100% test pass rate, requirements traceability validated.

### ⚠️ Release Status

**NOT PRODUCTION READY** - This is an early preview release with:
- Core protocol implementation in progress
- API stability not guaranteed
- Partial feature completeness
- No certification claims

### ✨ Added - Core Features

#### Protocol Implementation
- IEEE 1588-2019 core data structures (ClockIdentity, PortIdentity, Timestamp)
- Message type definitions (Sync, Follow_Up, Delay_Req, Delay_Resp, Announce)
- Best Master Clock Algorithm (BMCA) implementation
- Dataset management framework (defaultDS, currentDS, parentDS, portDS)
- Message header parsing and validation
- Basic servo controller implementation (PI controller)

#### Architecture & Design
- Hardware Abstraction Layer (HAL) with function pointer interfaces
- Platform-agnostic design (no vendor/OS dependencies in protocol layer)
- Clean separation: Protocol layer → HAL interface → Hardware adapters
- Cross-standard integration architecture (802.1AS, 1722, 1722.1 ready)

#### Testing & Quality
- 88 comprehensive test cases (100% passing)
- 90.2% line coverage (exceeds 80% target)
- 6,200+ test iterations with zero failures
- Test-Driven Development (TDD) approach
- Traceability framework linking requirements → tests → evidence

#### Build System
- CMake-based build system
- Cross-platform support (Windows, Linux, macOS, embedded targets)
- Google Test integration for unit testing
- Automated CI/CD pipeline setup
- Multi-platform CI validation (Ubuntu, Windows MSVC, macOS Clang)

#### GPS PPS Autodetection (Example Implementation)
- Automatic PPS signal detection on RS-232 pins (DCD/CTS/DSR)
- Sub-microsecond timestamp accuracy (50-200ns typical)
- 1Hz frequency validation with graceful NMEA-only fallback
- Platform abstraction for Windows/Linux/embedded systems
- Hardware validation tests and mock interfaces

#### Documentation
- IEEE/ISO/IEC standards-compliant documentation structure
- 9-phase software lifecycle documentation
- Architecture Decision Records (ADRs)
- Requirements traceability matrix
- Verification & Validation reports

### 📋 Requirements Coverage

**Verified Requirements** (from System Requirements Specification v1.0.0):
- ✅ REQ-F-001: PTP message types (Sync, Follow_Up, Delay_Req, Delay_Resp, Announce)
- ✅ REQ-F-002: Best Master Clock Algorithm (BMCA)
- ✅ REQ-F-003: Offset calculation (E2E delay mechanism)
- ✅ REQ-F-004: PI servo controller
- ✅ REQ-F-005: Hardware Abstraction Layer (HAL)
- ✅ REQ-NF-P-001: Synchronization accuracy <1µs (P95)
- ✅ REQ-NF-P-002: Timing determinism (zero malloc, bounded WCET)
- ✅ REQ-NF-S-001: Memory safety (no buffer overflows)
- ✅ REQ-NF-S-002: Input validation (all external data)
- ✅ REQ-NF-M-001: Modular architecture
- ✅ REQ-NF-M-002: Hardware abstraction

**Partially Verified** (5 requirements need additional evidence):
- ⚠️ REQ-S-001: Fault detection (partial implementation)
- ⚠️ REQ-S-002: Graceful degradation (GPS PPS fallback implemented, master failover pending)
- ⚠️ REQ-NF-P-003: Resource efficiency (partial verification)
- ⚠️ REQ-NF-U-001: API usability (needs user testing)

### 🐛 Fixed - Critical Bug Fixes (16 commits)

#### Compiler Warnings Eliminated
- Fixed all GCC/Clang warnings on Linux and macOS builds (299/299 targets clean)
- Fixed MSVC warnings on Windows build
- Corrected format specifiers (%lld → %ld for time_t on 32-bit platforms)
- Fixed misleading indentation in reliability tools (srg_fit.cpp)
- Marked unused test helper functions and variables with [[maybe_unused]]
- Fixed Timestamp struct initialization (requires 3 fields: seconds_high, seconds_low, nanoseconds)

#### Platform Support
- **macOS Support**: Added __APPLE__ preprocessor support to serial HAL
- Fixed serial_hal_linux.cpp to work on macOS (uses same termios API as Linux)
- GPS PPS example now builds and links on macOS (previously Linux-only)

#### Build System Fixes
- Added missing `#include <thread>` in pps_detector.hpp
- Added missing `#include <cstring>` for strerror() in test_pps_hardware.cpp
- Removed invalid [[maybe_unused]] attribute from using declarations (C++ standard violation)
- Fixed CMake conditional: Changed `UNIX AND NOT APPLE` to `UNIX` for serial HAL

#### Standards Compliance
- Added YAML front matter to context-map.md for spec validation (62/62 specs now valid)
- Created formal GPS PPS requirements specification (REQ-PPS-001 through REQ-PPS-007)
- Added REQ-S-002 (Fault Recovery and Graceful Degradation) to system requirements
- Fixed requirements traceability validation (ISO/IEC/IEEE 29148 compliance)
- All requirements now trace to stakeholder → architecture → design → implementation → tests

### 🚧 Known Limitations

#### Feature Completeness
- Management TLV framework: Structural placeholders only, logic pending
- Peer-to-peer delay mechanism: Abstraction points defined, full implementation pending
- State machine coverage: Core states implemented, edge cases in progress
- Security extensions: Deferred to post-MVP (Annex/optional features)

#### Verification Gaps
- 67% requirements fully verified (10/15 from System Requirements Specification)
- 33% requirements partially verified (needs additional evidence)
- No external certification (IEEE P1588, AVnu Milan certification pending)
- No production deployment validation yet

#### API Stability
- Public API may change based on community feedback
- No backward compatibility guarantees in 0.x versions
- Breaking changes possible in future 0.x releases

### 📊 Quality Metrics

| Metric | Target | Actual | Status |
|--------|--------|--------|--------|
| Test Pass Rate | 100% | 100% (88/88) | ✅ |
| Line Coverage | >80% | 90.2% | ✅ |
| Branch Coverage | >70% | ~85% | ✅ |
| Test Stability | >95% | 100% (6200 iterations) | ✅ |
| MTBF (95% conf.) | >100 | ≥1669 | ✅ |
| Requirements Verified | 100% | 67% fully | ⚠️ |

### 🎯 Use Cases

This preview release supports:
1. **Protocol Research** - Study IEEE 1588-2019 reference implementation
2. **Early Integration** - Test HAL integration patterns
3. **Academic Use** - Teaching/learning PTP protocol internals
4. **Proof of Concept** - Validate feasibility for your platform

**Not Yet Suitable For**:
- ❌ Production deployments
- ❌ Safety-critical applications
- ❌ Certification testing
- ❌ Mission-critical timing systems

### 🗺️ Roadmap to 1.0.0

Target: May 2026 (26-week MVP completion)

**Planned for 1.0.0**:
- Complete state machine implementation (all states/transitions)
- Full management TLV support
- Peer-to-peer delay mechanism completion
- IEEE P1588 Working Group engagement
- AVnu Milan certification process initiation
- Production validation in real systems
- API stability commitment
- External security audit
- Complete documentation (user guides, tutorials, examples)

### 🤝 Contributing

We welcome early adopters and contributors! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Feedback Needed**:
- HAL interface usability
- API design feedback
- Platform integration experiences
- Bug reports and edge cases
- Documentation clarity

### 📚 Documentation

- **Getting Started**: See [README.md](README.md#getting-started)
- **Architecture**: `03-architecture/ieee-1588-2019-ptpv2-architecture-spec.md`
- **Requirements**: `02-requirements/system-requirements-specification.md`
- **V&V Report**: `07-verification-validation/vv-summary-report-2025-11-11.md`
- **Roadmap**: `01-stakeholder-requirements/roadmap.md`

### 🔗 Standards References

Implementation based on understanding of:
- **IEEE 1588-2019** - Precision Time Protocol (PTPv2)
- **ISO/IEC/IEEE 12207:2017** - Software life cycle processes
- **ISO/IEC/IEEE 29148:2018** - Requirements engineering
- **IEEE 1016-2009** - Software design descriptions
- **IEEE 1012-2016** - Verification and validation

No copyrighted content from these specifications is reproduced. Implementation is original work achieving compliance through understanding of specification requirements.

### 📄 License

See repository root for license information.

---

## Version History

### 0.1.0 (2025-12-02)
- Initial preview release
- Core protocol implementation
- Testing framework established
- Documentation structure complete

---

**Note**: This is version 0.1.0 - an early preview release. For production use, please wait for 1.0.0 release (Target: May 2026) which will include complete features, certification readiness, and API stability guarantees.
