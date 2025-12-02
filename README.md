<!--
  README structured per repository documentation standards.
  Preserve the phrase "Getting Started" (tests depend on it).
  No copyrighted IEEE text reproduced; only original explanatory content referencing sections.
-->

# IEEE 1588-2019 – Precision Time Protocol (PTPv2) Core Library

[![Standard](https://img.shields.io/badge/IEEE-1588--2019-blue)](https://standards.ieee.org/standard/1588-2019.html)
[![Version](https://img.shields.io/badge/Version-0.1.0--alpha-orange)](./RELEASE_NOTES.md)
[![Status](https://img.shields.io/badge/Status-Preview%20Release-yellow)](#roadmap)
[![License](https://img.shields.io/badge/License-See%20Repository-lightgrey)](#license)
[![Build](https://img.shields.io/badge/Build-CMake-informational)](#getting-started)
[![Tests](https://img.shields.io/badge/Tests-88%20passing-success)](#testing--traceability)
[![Coverage](https://img.shields.io/badge/Coverage-90.2%25-brightgreen)](./VERIFICATION_EVIDENCE.md)

> **⚠️ PREVIEW RELEASE v0.1.0** - This is an **alpha/preview release** for early feedback and community validation. **NOT PRODUCTION READY**. API stability not guaranteed. See [Release Notes](./RELEASE_NOTES.md) for details.

Hardware & OS agnostic implementation of core **IEEE 1588-2019 Precision Time Protocol (PTPv2)** logic: message formats, BMCA, dataset management, delay mechanisms, and deterministic clock servo primitives. This repository focuses on the pure protocol layer – no vendor drivers, no OS calls – enabling reuse across embedded, desktop, and data‑center environments.

> Implementation is based on understanding of IEEE 1588-2019. For authoritative requirements consult the official standard. Section numbers are referenced without reproducing copyrighted text.

## 🎉 Release v0.1.0 (December 2, 2025)

**First public preview release!** This alpha release includes:
- ✅ Core IEEE 1588-2019 protocol implementation (BMCA, datasets, messages)
- ✅ Hardware Abstraction Layer (HAL) for platform independence
- ✅ 88 test cases passing (100% pass rate, 90.2% coverage)
- ✅ Standards-compliant architecture and documentation
- ⚠️ API may change - feedback welcome!
- ⚠️ Not production-ready - see [limitations](./RELEASE_NOTES.md#known-issues)

**👉 Read the full [Release Notes](./RELEASE_NOTES.md) for details and [CHANGELOG](./CHANGELOG.md) for version history.**

**Roadmap to v1.0.0**: Target May 2026 (production-ready with certification support)

---

## Table of Contents

1. [Release Information](#-release-v010-december-2-2025)
2. [Purpose & Scope](#purpose--scope)
3. [Design Principles](#design-principles)
4. [Architecture Overview](#architecture-overview)
5. [Feature Summary](#feature-summary)
6. [Repository Layout](#repository-layout)
7. [Getting Started](#getting-started)
8. [Building](#building)
9. [Usage Examples](#usage-examples)
10. [Testing & Traceability](#testing--traceability)
11. [Roadmap](#roadmap)
12. [Contributing](#contributing)
13. [Compliance & References](#compliance--references)
14. [FAQ](#faq)
15. [License](#license)
16. [Last Updated](#last-updated)

---

## Purpose & Scope

Provide a clean, testable, specification-aligned PTPv2 (IEEE 1588-2019) protocol core suitable for integration into higher-level timing stacks (e.g., IEEE 802.1AS/gPTP, AVTP time stamping, AVDECC timestamp validation, telecom profiles, Milan). The scope intentionally excludes:

* Hardware timestamp acquisition APIs (supplied via injected interfaces)
* Network socket / driver code
* Platform-specific time discipline implementation

These belong in service / adaptation layers that depend on this library.

## Design Principles

* Standards Alignment: Data sets & state machines mirror specification concepts (Sections 8, 9, 11, 13) without copying text.
* Determinism: No dynamic allocation, no blocking waits in protocol paths; bounded loops; constant or amortized O(1) operations for hot paths.
* Clean Layering: Higher standards (802.1AS, 1722, 1722.1) consume this library; no reverse dependencies.
* Test-Driven: All externally observable behaviors have corresponding unit or scenario tests with requirement IDs (STR-*).
* Traceability: Stakeholder requirements → functional requirements → tests → evidence documented in `VERIFICATION_EVIDENCE.md`.
* Portability: Pure C++ (standard library only) for logic; all environment specifics abstracted.

## Architecture Overview

```text
Application / Control (e.g., AVDECC, Management UI)
  ↑
Higher Timing Profiles (802.1AS, Telecom G.8275.x)
  ↑
PTP Core (This Library)
  • BMCA (Sec 9.3)
  • Message Handling (Sec 13)
  • Delay Mechanisms (Sec 11.3 / 11.4)
  • Datasets (Sec 8)
  • Servo & Correction Field Processing
  ↑
Injected Interfaces (time, timestamp capture, transport send/receive)
  ↑
Hardware / OS Adapters (out of scope here)
```

## Feature Summary

Implemented / In Progress (granular tracking maintained via tests & roadmap):

| Area | Status | Notes |
|------|--------|-------|
| Core data types (clockIdentity, portIdentity, timestamps) | ✅ | Section 7 & 13 field sizing respected |
| Basic message header parsing / serialization scaffolding | ✅ | Header validation + length checks |
| BMCA comparison primitives | ✅ | Dataset-based attribute comparison rules (Sec 9.3) |
| Servo simulation tests | ✅ | Deterministic convergence evidence (adaptive loop) |
| Requirement traceability tooling | ✅ | Python scripts enforce ≥75% covered STRs |
| Multi-instance synchronization acceptance test | ✅ | Verifies isolation & BMCA selection |
| Management TLV framework | 🔄 | Structural placeholders, logic pending |
| Peer & end-to-end delay algorithms | 🔄 | Abstraction points defined |
| Full state machine coverage (all states & transitions) | 🔄 | Iterative expansion planned |
| Security extensions (Annex / optional) | ⏳ | Deferred until core stability |

Legend: ✅ complete | 🔄 partial/in progress | ⏳ planned

## Repository Layout

High-level (non-exhaustive) structure for lifecycle compliance:

```text
01-stakeholder-requirements/   # Stakeholder context & need statements (29148)
02-requirements/               # System & functional requirements specs
03-architecture/               # Architecture description & decisions (42010)
04-design/                     # Detailed design (IEEE 1016)
05-implementation/             # Source, tests, docs for implementation phase
07-verification-validation/    # Test plans, cases, results (IEEE 1012)
spec-kit-templates/            # Front matter schemas & spec templates
VERIFICATION_EVIDENCE.md       # Consolidated compliance & coverage evidence
```

Core protocol source currently resides under `src/` & `include/` while standards-specific re‑structuring towards `lib/Standards/IEEE/1588/2019/` is progressing (see Roadmap task).

## Getting Started

Minimal build workflow (Windows PowerShell & generic cross‑platform). All build artefacts stay inside `build/`.

### Prerequisites

* CMake ≥ 3.20
* C++ compiler (MSVC 2019+/Clang/GCC supporting C++17 subset used)
* Python 3 (traceability + coverage scripts)

### Quick Start

1. Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
2. Build: `cmake --build build --config Release --target all`
3. Run tests: `ctest --test-dir build -C Release --output-on-failure`
4. Generate traceability & coverage evidence (inside build): `py -3 scripts/generate-traceability-report.py`

> Windows Visual Studio users may open the generated solution in `build/` directly.

## Building

Key CMake options (add with `-D`):

* `BUILD_TESTING=ON` (default) – enable tests & traceability gates.
* `PTP_ENABLE_SECURITY=ON` – (future) include optional security extensions.
* `PTP_STRICT_COVERAGE=ON` – fail build if coverage threshold unmet (future integration).

Artifacts:

* Static library target (planned naming): `ieee1588_2019_ptp`
* Example executables under `build/examples/` (basic, realtime, time_sensitive)

## Usage Examples

Illustrative (simplified) snippet – actual APIs evolve with design specs.

```cpp
#include <ieee1588/ptp/ptp_core.hpp>

using namespace IEEE::_1588::_2019; // Planned namespace alignment

// Injected interfaces (user-supplied implementations)
struct NetIf { int (*send)(const void*, size_t); };
struct TimeIf { uint64_t (*now_ns)(); };

void example(NetIf net, TimeIf timeSrc) {
    BmcaCandidate local{};               // Populate defaultDS attributes
    BmcaCandidate foreign{};             // From received Announce
    auto result = compare_bmca(local, foreign); // BMCA attribute comparison
    (void)result;
}
```

> See `examples/` for complete, compilable demonstrations (multi‑instance sync, servo convergence, deterministic timing).

## Testing & Traceability

* Tests live under `05-implementation/tests/` & root `tests/` (transition to unified location planned).
* Requirement tags: `// @satisfies STR-ID` inside tests – parsed by Python tooling.
* Coverage Gate: Scripts enforce a minimum STR coverage threshold (currently 75%).
* Acceptance Tests: Multi-instance BMCA, resource efficiency (no unintended heap allocs), servo convergence stability.
* Evidence: Consolidated in `VERIFICATION_EVIDENCE.md` with links to raw logs.

## Roadmap

Short to mid-term milestones:

1. Namespace & directory migration to `lib/Standards/IEEE/1588/2019/` (structural compliance).
2. Complete message format parsing & serialization (all mandatory types + TLV base).
3. Full state machine implementation (Section 9.2) with exhaustive transition tests.
4. Delay mechanisms (end‑to‑end & peer) including correctionField accumulation.
5. Management messages (Section 15) – get/set dataset operations.
6. Servo refinement: integrate adaptive PI loop with bounded convergence criteria.
7. Optional security extensions & profile hooks (deferred until core stabilized).
8. Continuous performance benchmarks (deterministic latency / jitter evidence).

## Contributing

1. Discuss significant changes via an issue (reference requirement IDs if applicable).
2. Follow TDD: add a failing test first (with `@satisfies` tag) before implementation.
3. Reference IEEE 1588-2019 Section numbers in comments (no copied text).
4. Keep commits minimal & logically isolated; maintain passing tests.
5. Update traceability & evidence docs if you add or fulfill requirements.

## Compliance & References

Referenced standards (no reproduction):

* IEEE 1588-2019 (core PTP) – Sections 7, 8, 9, 11, 13, 15 referenced in code/comments.
* Related higher-layer standards (consumers): IEEE 802.1AS-2021, IEEE 1722-2016, IEEE 1722.1-2021.

Traceability Matrix & Evidence: See `VERIFICATION_EVIDENCE.md` & `07-verification-validation/`.

## FAQ

**Q: Where is hardware timestamping implemented?**  In adaptation layers outside this repo; this core only defines interfaces.

**Q: Why no OS/network code?** To keep protocol logic portable & testable (CI-friendly, deterministic unit tests).

**Q: How do I add a new requirement?** Create/update a spec in `02-requirements/`, include YAML front matter (schema validated), then add tagged tests.

**Q: Why do tests fail after editing docs?** Some tests search for anchor phrases (e.g., "Getting Started"). Ensure they remain.

## License

See repository license file. Implementation is original; all IEEE specifications retain their respective copyrights. No specification text included.

## Last Updated

2025-11-09 (README restructuring: conflict markers removed, standardized sections added.)

---

> Historical detailed implementation status blocks & legacy conflict markers were removed for clarity. Prior granular status has been superseded by the concise Roadmap + Feature Summary above.

 

