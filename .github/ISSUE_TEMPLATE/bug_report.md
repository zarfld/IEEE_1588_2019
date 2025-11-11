---
name: Bug Report
about: Report a bug or unexpected behavior in the IEEE 1588-2019 PTP library
title: '[BUG] '
labels: bug, needs-triage
assignees: ''
---

## Bug Description

**Clear and concise description of the bug:**


**Expected behavior:**


**Actual behavior:**


---

## Reproduction Steps

**Steps to reproduce the behavior:**

1. Configure system with...
2. Run command '...'
3. Observe error at...
4. See error

**Frequency:** (Does this happen every time, intermittently, or once?)


**Minimal reproducible example** (if possible):
```cpp
// Code that demonstrates the issue

```

---

## Environment

**Operating System:**
- [ ] Linux (distribution: ____________, version: ____________)
- [ ] Windows (version: ____________)
- [ ] FreeRTOS (version: ____________)
- [ ] Bare-metal (platform: ____________)

**Hardware Platform:**
- CPU architecture: (e.g., x86_64, ARM Cortex-M7, ARM Cortex-A53)
- Network hardware: (e.g., Intel i210, Broadcom BCM5789)
- Hardware timestamping support: [ ] Yes [ ] No

**Library Version:**
- IEEE 1588-2019 PTP library version: (e.g., 1.0.0-MVP)
- Commit hash (if building from source): 

**Build Configuration:**
- CMake version: 
- Compiler: (e.g., GCC 11.2, MSVC 19.29, Clang 14.0)
- Build type: [ ] Release [ ] Debug [ ] RelWithDebInfo
- CMake options used: 

**Network Configuration:**
- Network interface: (e.g., eth0, enp3s0, Ethernet 2)
- Network topology: (single switch, multi-switch, VLAN configuration)
- PTP transport: [ ] Layer 2 (Ethernet) [ ] UDP/IPv4 [ ] UDP/IPv6
- Hardware timestamping: [ ] Enabled [ ] Disabled [ ] Not supported

---

## Logs and Diagnostics

**Relevant log output:**
```
Paste log output here (redact sensitive information like IP addresses if needed)


```

**PTP status output** (if applicable):
```bash
# Linux example:
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
sudo pmc -u -b 0 'GET PARENT_DATA_SET'

# Or application-specific status command
./ptp-status-tool --full
```

```
Paste output here


```

**System diagnostics:**
```bash
# Network interface status
ethtool eth0
ip addr show eth0

# CPU/Memory usage
top -b -n 1 | head -20

# Kernel messages (if relevant)
dmesg | tail -50
```

```
Paste output here


```

---

## Impact Assessment

**Severity** (select one):
- [ ] **Critical (P0)**: System completely broken, production outage
- [ ] **High (P1)**: Major functionality broken, significant impact
- [ ] **Medium (P2)**: Feature not working as expected, workaround available
- [ ] **Low (P3)**: Minor issue, cosmetic problem, or enhancement request

**Impact on your system:**
- Number of systems affected: 
- PTP synchronization accuracy: (e.g., offset >100μs, normally <1μs)
- Business impact: (e.g., audio/video sync lost, industrial control impacted)

**Workaround** (if found):


---

## Additional Context

**Recent changes:**
- Did this work in a previous version? [ ] Yes (version: ______) [ ] No [ ] Unknown
- Recent system changes: (OS updates, network changes, configuration modifications)


**Related issues:**
- Link to similar issues (if any): 

**Screenshots or diagrams** (if applicable):
<!-- Drag and drop images here, or use markdown syntax: ![description](image-url) -->


**Anything else we should know:**


---

## Checklist

Before submitting, please verify:

- [ ] I have searched existing issues to avoid duplicates
- [ ] I have provided all requested environment information
- [ ] I have included relevant logs and diagnostics
- [ ] I have described clear reproduction steps (or explained why they're not available)
- [ ] I have redacted any sensitive information (IP addresses, hostnames, credentials)
- [ ] I have tested with the latest library version (if possible)

---

**Note**: This issue will be triaged by maintainers. Please be patient and provide any additional information requested. For urgent production issues (P0), also contact support via emergency channels (see Operations Manual, Section 4.4).
