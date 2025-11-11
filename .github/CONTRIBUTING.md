# Contributing to IEEE 1588-2019 PTP Library

Thank you for your interest in contributing! This document provides guidelines for contributing to the project.

## 📋 Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [How Can I Contribute?](#how-can-i-contribute)
3. [Development Setup](#development-setup)
4. [Coding Standards](#coding-standards)
5. [Testing Requirements](#testing-requirements)
6. [Pull Request Process](#pull-request-process)
7. [Issue Guidelines](#issue-guidelines)
8. [Documentation Standards](#documentation-standards)

---

## Code of Conduct

This project adheres to a Code of Conduct (see `CODE_OF_CONDUCT.md`). By participating, you are expected to uphold this code. Please report unacceptable behavior to [conduct@example.com].

---

## How Can I Contribute?

### Reporting Bugs

Found a bug? Please use our **Bug Report template** on GitHub Issues:

1. Go to [GitHub Issues](https://github.com/[org]/IEEE_1588_2019/issues/new/choose)
2. Select "Bug Report"
3. Fill in all required sections
4. Include reproduction steps, logs, and environment details

### Suggesting Features

Have an idea? Use our **Feature Request template**:

1. Go to [GitHub Issues](https://github.com/[org]/IEEE_1588_2019/issues/new/choose)
2. Select "Feature Request"
3. Describe the problem it solves
4. Consider IEEE 1588-2019 compliance implications

### Asking Questions

- **Specific questions**: Use the "Question" issue template
- **General discussion**: Use [GitHub Discussions](https://github.com/[org]/IEEE_1588_2019/discussions)
- **Documentation issues**: File a bug report or PR to fix directly

### Contributing Code

1. **Fork** the repository
2. **Create a feature branch**: `git checkout -b feature/my-feature`
3. **Make your changes** following our standards (see below)
4. **Write tests** for new functionality
5. **Update documentation** if needed
6. **Submit a Pull Request**

---

## Development Setup

### Prerequisites

- CMake 3.20 or newer
- C++17 compatible compiler:
  - GCC 7+ (Linux)
  - Clang 5+ (Linux/macOS)
  - MSVC 2017+ (Windows)
- Git

### Clone and Build

```bash
# Clone repository
git clone https://github.com/[org]/IEEE_1588_2019.git
cd IEEE_1588_2019

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Build Options

```bash
# Enable all warnings (recommended for development)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_WARNINGS=ON

# Enable sanitizers (for memory leak detection)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON

# Build with coverage
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
```

---

## Coding Standards

### C++ Standards

**Language Version**: C++17

**Style Guide**: Based on [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with modifications

### Key Principles

1. **Hardware-agnostic**: Protocol code must be platform-independent
2. **Standards-compliant**: Follow IEEE 1588-2019 specification exactly
3. **Deterministic**: Avoid dynamic allocation in time-critical paths
4. **Testable**: Design for testability with dependency injection
5. **Documented**: All public APIs must have documentation

### Naming Conventions

```cpp
// Classes: PascalCase
class PTPSlave { };
class BMCAStateMachine { };

// Functions: snake_case
int calculate_offset(uint64_t t1, uint64_t t2);
void process_sync_message(const SyncMessage& msg);

// Variables: snake_case
uint64_t sync_timestamp;
int32_t path_delay_ns;

// Constants: UPPER_SNAKE_CASE
constexpr uint16_t PTP_ETHERTYPE = 0x88F7;
constexpr uint8_t PTP_VERSION = 0x02;

// Namespaces: PascalCase or snake_case (consistent within project)
namespace IEEE::_1588::_2019 { }

// Private members: trailing underscore
class MyClass {
private:
    int value_;
    std::string name_;
};
```

### File Organization

```cpp
// Header file: my_component.hpp
#ifndef IEEE_1588_2019_MY_COMPONENT_H
#define IEEE_1588_2019_MY_COMPONENT_H

#include <cstdint>
#include <string>

namespace IEEE {
namespace _1588 {
namespace _2019 {

/**
 * @brief Brief description of component
 * 
 * Detailed description explaining purpose, usage, and any
 * important implementation notes. Reference IEEE specification
 * sections when relevant.
 * 
 * @see IEEE 1588-2019, Section X.Y.Z
 */
class MyComponent {
public:
    /**
     * @brief Constructor
     * @param param1 Description of parameter
     */
    explicit MyComponent(int param1);
    
    /**
     * @brief Process data
     * @param data Input data
     * @return 0 on success, negative error code on failure
     */
    int process(const uint8_t* data, size_t length);
    
private:
    int value_;
};

} // namespace _2019
} // namespace _1588
} // namespace IEEE

#endif // IEEE_1588_2019_MY_COMPONENT_H
```

```cpp
// Implementation file: my_component.cpp
#include "my_component.hpp"

#include <cstring>

namespace IEEE {
namespace _1588 {
namespace _2019 {

MyComponent::MyComponent(int param1)
    : value_(param1) {
    // Initialize
}

int MyComponent::process(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return -1; // Invalid parameters
    }
    
    // Implementation per IEEE 1588-2019 Section X.Y.Z
    // ...
    
    return 0;
}

} // namespace _2019
} // namespace _1588
} // namespace IEEE
```

### Documentation Requirements

**All public APIs must have Doxygen comments**:

```cpp
/**
 * @brief Parse IEEE 1588-2019 Sync message
 * 
 * Parses a Sync message from a raw packet buffer according to
 * IEEE 1588-2019 specification Section 13.6. Validates message
 * header and extracts timestamp information.
 * 
 * @param packet_data Raw packet data (must not be NULL)
 * @param packet_length Length of packet in bytes (must be >= 44)
 * @param[out] sync_msg Output structure for parsed message
 * @return 0 on success, negative error code on failure:
 *         -1 = Invalid parameters
 *         -2 = Invalid message type
 *         -3 = Invalid version
 *         -4 = Packet too short
 * 
 * @note This function performs bounds checking and validates all
 *       fields per IEEE 1588-2019 requirements.
 * 
 * @see IEEE 1588-2019, Section 13.6 "Sync message"
 * @see IEEE 1588-2019, Table 34 "Sync message format"
 */
int parse_sync_message(const uint8_t* packet_data, 
                       size_t packet_length,
                       SyncMessage* sync_msg);
```

### Error Handling

```cpp
// Use return codes for error handling (no exceptions in critical paths)
int result = process_message(data, length);
if (result < 0) {
    // Handle error
    log_error("Failed to process message: error %d", result);
    return result;
}

// Validate inputs at function boundaries
if (data == nullptr || length == 0) {
    return -1; // EINVAL equivalent
}

// Use assert for internal invariants (debug builds only)
assert(state_ == State::INITIALIZED && "Must be initialized");
```

### Memory Management

```cpp
// Prefer static allocation for real-time paths
uint8_t packet_buffer[1024];  // ✅ Static allocation
// uint8_t* packet_buffer = new uint8_t[1024];  // ❌ Dynamic allocation

// Use const references to avoid copies
void process(const LargeStruct& data);  // ✅ Reference
// void process(LargeStruct data);      // ❌ Copy

// Initialize all variables
uint64_t timestamp = 0;  // ✅ Initialized
// uint64_t timestamp;   // ❌ Uninitialized
```

---

## Testing Requirements

### Test Coverage

- **All new code must have tests**
- **Target**: >80% line coverage
- **Critical paths**: >95% coverage

### Test Types

**Unit Tests** (Google Test):
```cpp
#include <gtest/gtest.h>
#include "my_component.hpp"

TEST(MyComponentTest, ProcessValidData) {
    MyComponent component(42);
    uint8_t data[] = {0x01, 0x02, 0x03};
    
    int result = component.process(data, sizeof(data));
    
    EXPECT_EQ(result, 0);
}

TEST(MyComponentTest, ProcessInvalidData) {
    MyComponent component(42);
    
    int result = component.process(nullptr, 0);
    
    EXPECT_EQ(result, -1);
}
```

**Integration Tests**:
```cpp
TEST(PTPIntegrationTest, SyncSequenceComplete) {
    // Setup mock HAL
    MockHAL hal;
    
    // Create PTP slave
    PTPSlave slave(&hal);
    
    // Send Announce message
    hal.inject_announce_message(master_announce);
    slave.process();
    EXPECT_EQ(slave.state(), PTPState::SLAVE);
    
    // Send Sync + Follow_Up
    hal.inject_sync_message(sync_msg);
    hal.inject_follow_up_message(follow_up_msg);
    slave.process();
    
    // Verify offset calculated
    EXPECT_LT(std::abs(slave.offset_ns()), 1000);
}
```

**Conformance Tests** (IEEE 1588-2019):
```cpp
TEST(IEEE1588ConformanceTest, SyncMessageFormat) {
    // Test that Sync message format matches IEEE 1588-2019 Table 34
    SyncMessage msg = create_sync_message();
    uint8_t buffer[64];
    
    serialize_sync_message(&msg, buffer);
    
    // Verify header fields per IEEE spec
    EXPECT_EQ(buffer[0] & 0x0F, 0x00);  // messageType = Sync (0)
    EXPECT_EQ(buffer[1] & 0x0F, 0x02);  // versionPTP = 2
    EXPECT_EQ(buffer[32] & 0x02, 0x02); // twoStepFlag = true
    // ... more checks per IEEE spec
}
```

### Running Tests

```bash
# Run all tests
ctest

# Run specific test
ctest -R MyComponentTest

# Run with verbose output
ctest --output-on-failure

# Run with coverage
cmake .. -DENABLE_COVERAGE=ON
make coverage
# View: build/coverage/index.html
```

---

## Pull Request Process

### Before Submitting

- [ ] Code compiles without warnings
- [ ] All tests pass (`ctest`)
- [ ] New tests added for new functionality
- [ ] Documentation updated (code comments, README, examples)
- [ ] Code follows style guide (run `clang-format` if available)
- [ ] Commit messages follow Conventional Commits format

### Commit Message Format

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

**Types**:
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation only
- `style:` Code style (formatting, no logic change)
- `refactor:` Code refactoring
- `perf:` Performance improvement
- `test:` Adding or updating tests
- `chore:` Build process, dependencies, tooling

**Examples**:
```
feat(bmca): implement Priority1 override per IEEE 1588-2019

Add support for Priority1 field in BMCA comparison algorithm
following IEEE 1588-2019 Section 9.3.2.5. This allows manual
override of automatic clock selection.

Closes #42
```

```
fix(hal): prevent memory leak in NetworkHAL destructor

Socket file descriptors were not being closed properly in
the NetworkHAL destructor, causing fd leak over time.

Added explicit close() call in destructor.

Fixes #67
```

### PR Description Template

When creating a PR, include:

```markdown
## Description
Brief description of changes

## Motivation
Why are these changes needed? What problem do they solve?

## Changes Made
- Change 1
- Change 2
- Change 3

## Testing
- [ ] Unit tests added/updated
- [ ] Integration tests passing
- [ ] Manual testing performed

## IEEE 1588-2019 Compliance
- [ ] No impact on standard compliance
- [ ] Implements optional feature from IEEE 1588-2019 Section X.Y
- [ ] Custom extension (maintains compatibility)

## Breaking Changes
- [ ] No breaking changes
- [ ] API changes (describe migration path)
- [ ] Configuration changes (describe migration path)

## Checklist
- [ ] Code compiles without warnings
- [ ] All tests pass
- [ ] Documentation updated
- [ ] Commit messages follow Conventional Commits
- [ ] No sensitive information in commits (passwords, API keys, etc.)

## Related Issues
Closes #XX
Related to #YY
```

### Review Process

1. **Automated Checks**: CI/CD must pass (build, tests, linting)
2. **Code Review**: At least one maintainer approval required
3. **Testing**: Reviewer may request additional testing
4. **Documentation**: Verify documentation is adequate
5. **Standards Compliance**: Verify IEEE 1588-2019 compliance maintained

### Merging

- Maintainers will merge approved PRs
- Squash merging used to keep history clean
- Release notes generated from commit messages

---

## Issue Guidelines

### Using GitHub Issues

We use **GitHub Issues** for all bug reports, feature requests, and questions:

- **Bug Reports**: Use "Bug Report" template
- **Feature Requests**: Use "Feature Request" template  
- **Questions**: Use "Question" template
- **Discussions**: Use [GitHub Discussions](https://github.com/[org]/IEEE_1588_2019/discussions) for open-ended topics

### Issue Labels

- `bug` - Something isn't working
- `enhancement` - New feature or request
- `documentation` - Documentation improvements
- `good first issue` - Good for newcomers
- `help wanted` - Extra attention needed
- `question` - Further information requested
- `wontfix` - This will not be worked on
- `duplicate` - Duplicate issue
- `invalid` - Invalid issue

### Issue Triage

Maintainers will triage issues:
1. **Label appropriately**
2. **Assign priority** (P0-P3)
3. **Assign milestone** (if scheduled)
4. **Request more information** if needed

---

## Documentation Standards

### Documentation Types

1. **Code Documentation**: Doxygen comments in headers
2. **User Documentation**: Markdown in `docs/` and `08-transition/`
3. **Examples**: Working code in `examples/` with README
4. **API Reference**: Generated from Doxygen comments

### Writing Style

- **Clear and concise**: Avoid unnecessary jargon
- **Examples**: Provide code examples where helpful
- **Standards references**: Cite IEEE 1588-2019 sections when relevant
- **Audience awareness**: Consider user's experience level

### Documentation Checklist

When adding/updating documentation:

- [ ] Spell-checked
- [ ] Grammar-checked
- [ ] Code examples tested and working
- [ ] Links verified (no broken links)
- [ ] Screenshots current (if applicable)
- [ ] Follows existing documentation structure
- [ ] Accessible to target audience

---

## Questions?

- **General questions**: [GitHub Discussions](https://github.com/[org]/IEEE_1588_2019/discussions)
- **Specific issues**: [GitHub Issues](https://github.com/[org]/IEEE_1588_2019/issues)
- **Email**: contributing@example.com

Thank you for contributing to the IEEE 1588-2019 PTP Library! 🎉
