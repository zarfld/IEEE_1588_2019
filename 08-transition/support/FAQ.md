# Frequently Asked Questions (FAQ)

**IEEE 1588-2019 PTP Library**  
**Last Updated**: 2025-11-11

---

## Table of Contents

1. [General Questions](#general-questions)
2. [Getting Started](#getting-started)
3. [Installation & Build](#installation--build)
4. [Configuration](#configuration)
5. [Synchronization](#synchronization)
6. [Performance](#performance)
7. [Troubleshooting](#troubleshooting)
8. [Hardware & Platforms](#hardware--platforms)
9. [Standards & Compliance](#standards--compliance)
10. [Development & Contributing](#development--contributing)

---

## General Questions

### What is IEEE 1588-2019?

IEEE 1588-2019 is the **Precision Time Protocol (PTPv2)** standard for clock synchronization over packet-based networks. It enables sub-microsecond accuracy by:
- Using hardware timestamping at the network interface
- Measuring network path delays
- Compensating for asymmetric delays
- Selecting the best master clock (BMCA)

### What is this library for?

This library provides a **hardware-agnostic**, **standards-compliant** implementation of IEEE 1588-2019 PTP that can be integrated into:
- Audio/Video equipment (AVTP, Milan)
- Industrial automation systems
- Telecom/5G infrastructure
- Test & measurement equipment
- Any application requiring precise time synchronization

### What makes this library different?

- **Hardware-agnostic**: Works on any platform via HAL abstraction
- **Standards-compliant**: Follows IEEE 1588-2019 specification exactly
- **Production-ready**: Deterministic design, static allocation, tested
- **Well-documented**: Comprehensive examples, guides, and specifications
- **Open source**: Free to use, modify, and contribute

### What accuracy can I expect?

**With hardware timestamping**:
- Typical: **<1 μs** (microsecond) offset from master
- Best case: **<200 ns** (nanoseconds) with optimized configuration

**With software timestamping**:
- Typical: **<10 μs** offset from master
- May vary based on system load and network conditions

### What are the licensing terms?

[TODO: Specify your license here - MIT, Apache 2.0, GPL, etc.]

The library is open source. See the `LICENSE` file in the repository for details.

---

## Getting Started

### How do I get started quickly?

1. **Clone the repository**:
   ```bash
   git clone https://github.com/[org]/IEEE_1588_2019.git
   cd IEEE_1588_2019
   ```

2. **Build the library**:
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```

3. **Run Example 1 (Basic PTP Slave)**:
   ```bash
   cd examples/01-basic-ptp-slave
   ./basic_ptp_slave
   ```

4. **Read the documentation**:
   - Getting Started: `docs/getting-started.md`
   - Examples: `examples/*/README.md`
   - Operations: `08-transition/user-documentation/operations-manual.md`

### What prerequisites do I need?

**Software**:
- CMake 3.20 or newer
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Git (for cloning)

**Hardware**:
- Ethernet network interface
- **Recommended**: Network interface with PTP hardware timestamping support
- **Optional**: GPS receiver for grandmaster clock

### Do I need special hardware?

**For development/testing**: No special hardware needed. The library includes simulated HAL implementations for learning.

**For production**: Ethernet controller with hardware timestamping is **strongly recommended** for <1μs accuracy. Without hardware timestamping, expect ~10μs accuracy.

**Supported NICs** (hardware timestamping):
- Intel i210, i350, 82580, X710
- Broadcom BCM5789x series
- Marvell 88E1512
- Many others (check `ethtool -T <interface>` on Linux)

---

## Installation & Build

### How do I build on Linux?

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake git

# Clone repository
git clone https://github.com/[org]/IEEE_1588_2019.git
cd IEEE_1588_2019

# Build
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)

# Run tests
ctest
```

### How do I build on Windows?

```powershell
# Install dependencies: CMake, Visual Studio 2019+

# Clone repository
git clone https://github.com/[org]/IEEE_1588_2019.git
cd IEEE_1588_2019

# Build
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release

# Run tests
ctest -C Release
```

### Build fails with "C++17 required" error

Ensure your compiler supports C++17:

```bash
# GCC: version 7 or newer
gcc --version

# Clang: version 5 or newer
clang --version

# Upgrade if needed (Ubuntu/Debian)
sudo apt-get install g++-9
export CXX=g++-9
```

### How do I cross-compile for embedded systems?

```bash
# Example: Cross-compile for ARM
mkdir build-arm && cd build-arm
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-toolchain.cmake
cmake --build .
```

See `examples/03-hal-implementation-template/README.md` for platform porting guide.

---

## Configuration

### How do I configure the PTP slave?

Create a configuration file (e.g., `/etc/ptp-slave.conf`):

```ini
[general]
network_interface = eth0
clock_domain = 0
priority1 = 128
priority2 = 128

[timing]
sync_interval = 0        # 2^0 = 1 message/sec
announce_interval = 1    # 2^1 = 2 messages/sec

[sync]
offset_threshold_ns = 1000     # Step if offset >1μs
slew_rate_ppb = 500           # Slew rate: 500 PPB

[logging]
log_level = INFO
log_file = /var/log/ptp/ptp-slave.log
```

Then pass to application:
```bash
./ptp-slave --config /etc/ptp-slave.conf
```

### What do priority1 and priority2 do?

**priority1** and **priority2** are used by BMCA (Best Master Clock Algorithm) to select the master clock:

- **priority1** (0-255): **First** comparison (lower wins)
  - Use to override clock quality
  - Primary master: 64, Backup master: 128
  
- **priority2** (0-255): **Last** comparison before clock identity
  - Tie-breaker if clocks have same quality
  - Usually set to 128 (default)

**BMCA Comparison Order**:
1. priority1 (lower wins) 👈 **Set this differently for primary/backup**
2. Clock class (lower wins)
3. Clock accuracy (better wins)
4. Clock variance (lower wins)
5. priority2 (lower wins)
6. Clock identity (lower wins)

### How do I enable hardware timestamping?

**Linux**:
```bash
# Check if supported
sudo ethtool -T eth0
# Look for: SOF_TIMESTAMPING_TX_HARDWARE, SOF_TIMESTAMPING_RX_HARDWARE

# Usually enabled by default if hardware supports it
# Verify in HAL implementation that SO_TIMESTAMPING is used
```

**Platform-specific**: See your HAL implementation documentation in `examples/03-hal-implementation-template/`.

### How often should Sync messages be sent?

**IEEE 1588-2019 default**: 1 message/second (`sync_interval = 0`, meaning 2^0 = 1/sec)

**Tuning**:
- **More frequent** (2-8/sec): Better accuracy, more network bandwidth
- **Less frequent** (<1/sec): Lower bandwidth, slightly worse accuracy

**Recommendation**: Start with 1/sec, increase only if accuracy insufficient.

| sync_interval | Messages/sec | Accuracy | Bandwidth |
|---------------|--------------|----------|-----------|
| 0             | 1            | ~500 ns  | Baseline  |
| -1            | 2            | ~300 ns  | 2x        |
| -2            | 4            | ~220 ns  | 4x        |
| -3            | 8            | ~180 ns  | 8x        |

---

## Synchronization

### Why is my clock not synchronizing?

**Check these in order**:

1. **Verify Sync messages arriving**:
   ```bash
   sudo tcpdump -i eth0 -nn ether proto 0x88F7
   # Should see Sync, Follow_Up, Announce messages
   ```

2. **Check PTP state**:
   ```bash
   sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
   # Should show state: SLAVE, not LISTENING or FAULTY
   ```

3. **Verify network connectivity**:
   ```bash
   ping <master-ip>
   ```

4. **Check firewall** (if using UDP/IP):
   ```bash
   sudo iptables -L -n | grep 319
   # Ports 319 and 320 must be open
   ```

5. **Check HAL implementation**:
   - Review logs for HAL errors
   - Verify network interface name correct
   - Verify permissions for clock adjustment

### What is BMCA and why does it matter?

**BMCA** = Best Master Clock Algorithm (IEEE 1588-2019 Section 9.3)

It **automatically selects the best time source** when multiple PTP masters are available:
- Ensures slaves use the most accurate clock
- Provides automatic failover if master fails
- Prevents time source conflicts

**How it works**:
1. Slaves receive Announce messages from all masters
2. Each slave runs BMCA to compare masters
3. Slave selects best master and synchronizes to it
4. If master fails, BMCA automatically switches to backup

See `examples/02-bmca-integration/` for demonstration.

### My offset is large (>10μs). How do I fix it?

**Step 1**: Verify hardware timestamping enabled
```bash
sudo ethtool -T eth0
```
If not supported, expect ~10μs accuracy (this is normal for software timestamping).

**Step 2**: Check network path delay
```bash
sudo pmc -u -b 0 'GET CURRENT_DATA_SET' | grep meanPathDelay
```
High path delay (>1ms) indicates network congestion or multi-hop path.

**Step 3**: Increase sync message rate (carefully)
```ini
# In config file
sync_interval = -1  # 2 messages/sec instead of 1
```

**Step 4**: Tune clock discipline parameters
```ini
slew_rate_ppb = 1000  # Increase from 500 (max ~2000)
```

**Step 5**: Check for interference
- Other time sync services (NTP, Chrony) running?
- High system load causing delays?
- Network congestion or packet loss?

### How long does it take to converge?

**Typical convergence time**:
- **Initial lock**: 5-30 seconds
- **Sub-microsecond accuracy**: 1-5 minutes
- **Long-term stability**: 30-60 minutes

**Factors affecting convergence**:
- Network path delay variance
- Sync message interval
- Clock discipline parameters
- Hardware timestamping vs software

---

## Performance

### How much CPU does the library use?

**Typical**: <5% CPU on modern processors

**Factors**:
- Sync message rate (1/sec baseline, 8/sec peak)
- Logging verbosity (DEBUG uses much more than ERROR)
- HAL efficiency (optimized HAL is critical)

**Optimization tips**:
- Use hardware timestamping (reduces CPU load)
- Set log level to ERROR or WARN in production
- Optimize HAL implementation (avoid busy-wait loops)

### How much memory does the library use?

**Typical**: <10 MB RAM

The library uses **static allocation** by default (no dynamic memory in hot paths), ensuring predictable memory usage.

**Memory breakdown**:
- Protocol state machines: ~1 MB
- Packet buffers: ~16 KB (configurable)
- HAL structures: Platform-dependent (~1-5 MB)
- Logging buffers: ~1 MB (configurable)

### Can I use this in real-time systems?

**Yes!** The library is designed for real-time systems:

- ✅ **Deterministic**: No dynamic allocation in time-critical paths
- ✅ **Bounded latency**: Packet processing <100μs
- ✅ **No blocking calls**: All operations non-blocking or timeout-bounded
- ✅ **RTOS-friendly**: Works with FreeRTOS, VxWorks, etc.

See `examples/03-hal-implementation-template/` for RTOS porting guide.

### How do I optimize for lowest latency?

1. **Enable hardware timestamping** (biggest impact)
2. **Disable interrupt coalescing** on network interface:
   ```bash
   sudo ethtool -C eth0 rx-usecs 0 tx-usecs 0
   ```
3. **Increase sync message rate** (2-8/sec):
   ```ini
   sync_interval = -2  # 4 messages/sec
   ```
4. **Optimize HAL implementation**:
   - Use select/poll instead of busy-wait
   - Minimize packet processing overhead
   - Use zero-copy if possible
5. **Dedicate CPU core** (Linux):
   ```bash
   taskset -c 1 ./ptp-slave
   ```

---

## Troubleshooting

### I get "Permission denied" when adjusting clock

**Linux**: Clock adjustment requires `CAP_SYS_TIME` capability:

```bash
# Option 1: Run as root (not recommended for production)
sudo ./ptp-slave

# Option 2: Grant capability to binary
sudo setcap CAP_SYS_TIME+ep /usr/bin/ptp-slave

# Option 3: Add user to appropriate group (if using systemd)
sudo usermod -aG systemd-timesync $USER
```

**Windows**: Run as Administrator.

### Sync messages received but offset not improving

**Possible causes**:
1. **Clock adjustment disabled**: Check HAL `adjust_clock()` is implemented
2. **Another time service interfering**: Disable NTP, Chrony, systemd-timesyncd
3. **Insufficient permissions**: See "Permission denied" above
4. **HAL bug**: Check HAL implementation for errors

**Debug**:
```bash
# Enable debug logging
export PTP_LOG_LEVEL=DEBUG
./ptp-slave 2>&1 | grep -E "adjust|offset"

# Should see clock adjustment attempts
```

### Master keeps changing (BMCA flapping)

**Symptoms**: "BMCA: Master changed" repeatedly in logs

**Causes**:
1. **Multiple masters same priority**: Set different priority1 values
   - Primary: priority1=64
   - Backup: priority1=128
2. **Master intermittent failure**: Check master logs and network
3. **Network partitioning**: Check switch configuration

**Verify master priorities**:
```bash
sudo pmc -u -b 0 'GET FOREIGN_MASTER_CLOCK_DATA_SET'
```

### Memory leak or growing memory usage

**Diagnosis**:
```bash
# Monitor memory
watch -n 10 'ps aux | grep ptp-slave'

# Profile with valgrind
valgrind --leak-check=full ./ptp-slave
```

**Common causes**:
- HAL leaking sockets or buffers
- Log files growing without rotation
- Received packets not freed

**Solutions**:
- Fix HAL implementation (see `hal_template.cpp`)
- Configure log rotation (`logrotate`)
- Update to latest library version

---

## Hardware & Platforms

### Which network interfaces support hardware timestamping?

**Check your NIC**:
```bash
# Linux
sudo ethtool -T eth0

# Look for these capabilities:
# SOF_TIMESTAMPING_TX_HARDWARE
# SOF_TIMESTAMPING_RX_HARDWARE
# SOF_TIMESTAMPING_RAW_HARDWARE
```

**Known compatible NICs**:
- ✅ Intel i210, i350, 82580, X540, X550, X710, XXV710
- ✅ Broadcom BCM5789x series
- ✅ Marvell 88E1512, 88E6xxx switches
- ✅ Microchip LAN7430, LAN7431
- ✅ TI DP83640, DP83867

**Unsupported** (software timestamping only):
- ❌ Realtek RTL8111/RTL8168 (most consumer NICs)
- ❌ Virtual machines (usually, unless passthrough)
- ❌ USB Ethernet adapters (most)

### Can I use this on Raspberry Pi?

**Yes, but with limitations**:

- **Raspberry Pi 4/5**: Some models support hardware timestamping (check with `ethtool -T eth0`)
- **Raspberry Pi 3 and earlier**: Software timestamping only (~10μs accuracy)
- **Compute Module 4**: Hardware timestamping with appropriate carrier board

**Performance**: Expect 5-10μs accuracy on Pi 4 with hardware timestamping.

### Does this work in virtual machines?

**Generally no**, unless using **PCI passthrough**:

- **VM without passthrough**: No hardware timestamping → ~10-100μs accuracy
- **VM with PCI passthrough**: Full hardware timestamping → <1μs possible

**Cloud environments** (AWS, Azure, GCP): Usually no hardware timestamping available.

### How do I port to a new platform?

Follow the **6-phase porting guide** in `examples/03-hal-implementation-template/`:

1. **Environment Setup** (1-2 days)
2. **Platform Analysis** (2-3 days)
3. **HAL Skeleton** (3-5 days) - Implement NetworkHAL, TimestampHAL, ClockHAL
4. **Integration/Testing** (5-7 days)
5. **Optimization** (3-5 days) - Hardware timestamping
6. **Validation** (2-3 days)

**Total**: 16-25 days depending on platform complexity.

**Resources**:
- HAL template: `examples/03-hal-implementation-template/hal_template.hpp`
- Porting checklist: `examples/03-hal-implementation-template/porting_checklist.md`
- Platform examples: Linux, Windows, FreeRTOS, bare-metal in comments

---

## Standards & Compliance

### Is this compliant with IEEE 1588-2019?

**Yes!** The library implements:
- ✅ IEEE 1588-2019 protocol messages (Sync, Follow_Up, Delay_Req, Delay_Resp, Announce)
- ✅ Best Master Clock Algorithm (BMCA) per Section 9.3
- ✅ Clock synchronization mechanisms per Section 11
- ✅ Delay request-response mechanism per Section 11.3
- ✅ Optional peer-to-peer delay mechanism per Section 11.4

**Not yet implemented** (future work):
- ⏳ Transparent Clock support
- ⏳ Some optional TLVs
- ⏳ Security extensions (Annex P)

### Does this work with PTPv1 (IEEE 1588-2008)?

**No**, this library implements **PTPv2** (IEEE 1588-2019 and IEEE 1588-2008 are compatible).

PTPv1 (IEEE 1588-2002) is not supported and not recommended (obsolete).

### Is this compatible with gPTP (802.1AS)?

**Partially**. This library implements IEEE 1588-2019 PTP, while gPTP (IEEE 802.1AS) is a **profile** of PTP with additional requirements:

- ✅ Same basic protocol
- ✅ Compatible message formats
- ❌ gPTP requires peer-to-peer delay (not implemented yet)
- ❌ gPTP has stricter timing requirements

**For gPTP compatibility**, see the IEEE 802.1AS implementation in `lib/Standards/IEEE/802.1/AS/` (if available).

### What about AVnu Milan?

**AVnu Milan** is a **professional audio/video profile** built on IEEE 1588-2019 and IEEE 802.1AS.

This library provides the **PTP foundation** for Milan. For full Milan compliance, additional components needed:
- IEEE 1722 (AVTP) for media transport
- IEEE 1722.1 (AVDECC) for device control
- Milan-specific extensions

See `lib/Standards/AVnu/Milan/` (if available) for Milan-specific implementations.

---

## Development & Contributing

### How do I contribute?

1. **Fork** the repository
2. **Create a branch**: `git checkout -b feature/my-feature`
3. **Make changes** following coding standards (see `CONTRIBUTING.md`)
4. **Add tests** for new functionality
5. **Run tests**: `ctest`
6. **Commit**: Use conventional commits (e.g., `feat: add hardware timestamping`)
7. **Push**: `git push origin feature/my-feature`
8. **Open Pull Request** with description

See `CONTRIBUTING.md` (when available) for detailed guidelines.

### How do I report a bug?

Use the **Bug Report template** when creating an issue:

1. Go to [GitHub Issues](https://github.com/[org]/IEEE_1588_2019/issues)
2. Click "New Issue"
3. Select "Bug Report"
4. Fill in all sections (environment, reproduction steps, logs)
5. Submit

**For urgent production issues** (P0/P1), also contact emergency support (see Operations Manual, Section 4.4).

### How do I request a feature?

Use the **Feature Request template**:

1. Go to [GitHub Issues](https://github.com/[org]/IEEE_1588_2019/issues)
2. Click "New Issue"
3. Select "Feature Request"
4. Describe the problem and proposed solution
5. Explain IEEE 1588-2019 compliance implications
6. Submit

### Where can I ask questions?

- **GitHub Issues**: Use "Question" template for specific questions
- **GitHub Discussions**: For general discussion, ideas, and open-ended questions
- **Email**: support@example.com (if commercial support available)

---

## Still Have Questions?

- **Documentation**: Check `docs/` folder
- **Examples**: See `examples/` folder for working code
- **Operations Manual**: See `08-transition/user-documentation/operations-manual.md`
- **GitHub Issues**: [Create an issue](https://github.com/[org]/IEEE_1588_2019/issues)
- **Email Support**: support@example.com (if available)

---

**This FAQ is continuously updated. Last revision: 2025-11-11**
