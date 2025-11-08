# Contributing to IEEE_1588_2019

Thanks for your interest in contributing! This project implements standards-compliant, hardware-agnostic protocol libraries. Please follow these guidelines to keep quality high and changes traceable.

## Principles

- Standards-only in the Standards layer (no OS/hardware/vendor code)
- Test-Driven Development (write or update tests with each change)
- Small, focused pull requests
- Maintain requirements→design→code→tests traceability
- Keep build reproducible; avoid adding heavy dependencies

## Development Flow

1. Fork and create a feature branch
2. Link your change to a requirement ID (e.g., `STR-*`) in code/tests using `// @satisfies` annotations
3. Add or update tests under `05-implementation/tests/` (or appropriate phase)
4. Build and run tests locally
5. Update docs if behavior or APIs change (README and relevant phase docs)
6. Submit a PR with a clear description and references to IEEE clause numbers when applicable

## Building and Testing

- Windows (PowerShell): use the existing CMake preset or open the generated Visual Studio solution under `build/`
- Tests are registered with CTest; CI runs them on each PR

## Coding Standards

- C++14 baseline (guard C++17 features behind `__cplusplus >= 201703L`)
- No dynamic allocation in critical paths; no blocking calls
- Deterministic data structures (POD where possible)
- Include headers using the project’s namespace paths (e.g., `include/IEEE/...`)

## Commit Messages

- Be descriptive and reference relevant requirement IDs and IEEE section numbers (by number only)
- Example: `PTP: clamp offset (FM-002), refs IEEE 1588-2019 11.3; @satisfies STR-SEC-002`

## Pull Request Checklist

- [ ] Tests added/updated and passing locally
- [ ] Traceability annotations present (`// @satisfies STR-...`)
- [ ] No platform-specific code in standards layer
- [ ] Docs updated (README, phase docs) if needed
- [ ] Small, reviewable diff

## Reporting Issues

Please include:

- Repro steps, expected vs actual behavior
- Environment info (OS, compiler, build config)
- Links to any relevant requirement IDs and test logs

Thanks for helping make the project robust and standards-compliant!
