# Remote Debugging Guide - Raspberry Pi 5 PTP Grandmaster

**Target**: Raspberry Pi 5 (zarfld@GPSdisciplinedRTC)  
**Development**: Windows/Linux workstation  
**Tools**: GDB, VS Code Remote-SSH, Performance Profilers

---

## 🎯 Setup Overview

Three remote development approaches:

1. **GDB Remote Debugging** - Breakpoints, step debugging, variable inspection
2. **VS Code Remote-SSH** - Full IDE experience on remote machine
3. **Performance Profiling** - Identify bottlenecks, measure timing

---

## 1️⃣ GDB Remote Debugging

**⚠️ Prerequisites**: Source code must exist on BOTH machines:
- **Windows**: `D:\Repos\IEEE_1588_2019` (for editing/GDB client)
- **Raspberry Pi**: `~/IEEE_1588_2019` (for building/running)

**First-time setup on Raspberry Pi**:
```bash
# Option A: Clone repository
cd ~
git clone https://github.com/zarfld/IEEE_1588_2019.git

# Option B: Copy from Windows (from WSL)
rsync -avz --exclude 'build' --exclude '.git' \
  /mnt/d/Repos/IEEE_1588_2019 zarfld@<raspberry-pi-ip>:~/
```

### On Raspberry Pi (Target)

```bash
# Install gdbserver if not present
sudo apt install gdbserver

# Build with debug symbols
cd ~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# Start application under gdbserver
sudo gdbserver :2345 ./ptp_grandmaster --interface=eth1 --verbose
```

**Output**:
```
Process ./ptp_grandmaster created; pid = 1234
Listening on port 2345
```

### On Development Machine (Host)

#### Windows

**⚠️ Note**: `gdb-multiarch` is not available natively on Windows. Choose one option:

**Option A: Use WSL (Recommended for GDB)**
```powershell
# Install WSL if not present (one-time setup)
wsl --install

# Enter WSL environment
wsl

# Inside WSL terminal (one-time setup):
sudo apt update
sudo apt install gdb-multiarch

# For each debugging session:
cd /mnt/d/Repos/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster

# Connect to remote gdbserver (no local binary needed)
gdb-multiarch
(gdb) target remote <raspberry-pi-ip>:2345
(gdb) set sysroot
(gdb) break main
(gdb) continue
```

**Option B: Use MSYS2**
```powershell
# Install MSYS2 from https://www.msys2.org/
# In MSYS2 terminal:
pacman -S mingw-w64-x86_64-gdb-multiarch

# Start GDB
gdb-multiarch /d/Repos/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build/ptp_grandmaster
```

**Option C: Use VS Code Remote-SSH (Easiest - see Section 2 below)**

**GDB Commands** (once connected):
```gdb
(gdb) target remote <raspi-ip>:2345
(gdb) break main
(gdb) continue
(gdb) next
(gdb) print variable_name
(gdb) backtrace
```

#### Linux

```bash
# Install GDB with ARM64 support
sudo apt install gdb-multiarch

# Connect to target
gdb-multiarch ./ptp_grandmaster
(gdb) target remote 192.168.x.x:2345
(gdb) break LinuxPtpHal::send_message
(gdb) continue
```

### VS Code Debug Configuration

Create `.vscode/launch.json`:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Remote Debug RasPi PTP",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/examples/12-RasPi5_i226-grandmaster/build/ptp_grandmaster",
            "args": ["--interface=eth1", "--verbose"],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "miDebuggerPath": "gdb-multiarch",
            "miDebuggerServerAddress": "192.168.x.x:2345",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                },
                {
                    "description": "Set Disassembly Flavor to Intel",
                    "text": "-gdb-set disassembly-flavor intel",
                    "ignoreFailures": true
                }
            ],
            "sourceFileMap": {
                "/home/zarfld/IEEE_1588_2019": "${workspaceFolder}"
            }
        }
    ]
}
```

**Usage**:
1. Start `gdbserver` on Raspberry Pi
2. Open VS Code on development machine
3. Press F5 or Run → Start Debugging
4. Set breakpoints, inspect variables, step through code

---

## 2️⃣ VS Code Remote-SSH (Recommended)

**✨ Advantage**: Edits files directly on Raspberry Pi - no file syncing needed!

**⚠️ First-time**: Clone repository on Raspberry Pi (see Option 1 above)

### Setup

1. **Install VS Code Extensions**:
   - Remote - SSH (ms-vscode-remote.remote-ssh)
   - C/C++ Extension Pack (ms-vscode.cpptools-extension-pack)
   - CMake Tools (ms-vscode.cmake-tools)

2. **Configure SSH Connection**:
   - Press `F1` → "Remote-SSH: Connect to Host"
   - Enter: `zarfld@192.168.x.x` (or hostname)
   - Enter password when prompted
   - Wait for connection to establish

3. **Open Project on Raspberry Pi**:
   ```
   File → Open Folder → /home/zarfld/IEEE_1588_2019
   ```

4. **Install Extensions on Remote**:
   - VS Code will prompt to install C++ extensions on remote
   - Click "Install in SSH: raspberry-pi"

### Development Workflow

```bash
# All operations run directly on Raspberry Pi
# No need for file synchronization!

# Configure project
cmake --preset=default

# Build
cmake --build build

# Debug (local on Pi, faster than remote GDB)
gdb ./build/ptp_grandmaster
(gdb) run --interface=eth1
```

### VS Code Tasks for Remote

Create `.vscode/tasks.json`:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build PTP Grandmaster",
            "type": "shell",
            "command": "cmake --build build -j4",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Run PTP Grandmaster",
            "type": "shell",
            "command": "sudo ./build/ptp_grandmaster --interface=eth1 --verbose",
            "group": {
                "kind": "test",
                "isDefault": true
            }
        },
        {
            "label": "Monitor PTP State",
            "type": "shell",
            "command": "sudo pmc -u -b 0 'GET CURRENT_DATA_SET'",
            "problemMatcher": []
        },
        {
            "label": "Check GPS PPS",
            "type": "shell",
            "command": "sudo timeout 5 ppstest /dev/pps0",
            "problemMatcher": []
        }
    ]
}
```

**Usage**:
- Build: `Ctrl+Shift+B`
- Run: `Ctrl+Shift+P` → "Tasks: Run Test Task"

---

## 3️⃣ Performance Profiling

### Linux perf (System-wide Profiling)

```bash
# Install perf tools
sudo apt install linux-perf linux-tools-generic

# Record performance data
sudo perf record -F 999 -g ./ptp_grandmaster --interface=eth1 &
PID=$!
sleep 60  # Run for 60 seconds
sudo kill -SIGINT $PID

# Generate report
sudo perf report

# Annotate source code
sudo perf annotate linux_ptp_hal::send_message
```

### Real-Time Latency Testing

```bash
# Install cyclictest
sudo apt install rt-tests

# Test real-time latency (while PTP grandmaster running)
sudo cyclictest -p 80 -t 1 -n -i 1000 -l 100000

# Expected output:
# T: 0 ( 1234) P:80 I:1000 C: 100000 Min:  2 Act:  5 Avg:  4 Max: 15
```

### Hardware Timestamping Verification

```bash
# Verify hardware timestamping enabled
ethtool -T eth1

# Expected output:
# Hardware transmit timestamp modes:
#   hardware-transmit  (SOF_TIMESTAMPING_TX_HARDWARE)
# Hardware receive filter modes:
#   ptpv2-event
```

### PTP Packet Capture

```bash
# Capture PTP packets
sudo tcpdump -i eth1 -nn ether proto 0x88f7 -w ptp_capture.pcap

# Analyze with Wireshark
# Filter: ptp
# Verify: Announce, Sync, Follow_Up, Delay_Req, Delay_Resp messages
```

---

## 🔍 Debugging Common Issues

### Issue: Cannot Connect to gdbserver

**Problem**: `Connection refused` when connecting to port 2345

**Solutions**:
```bash
# Check if gdbserver is running
ps aux | grep gdbserver

# Check firewall
sudo ufw status
sudo ufw allow 2345/tcp

# Check network connectivity
ping 192.168.x.x

# Verify gdbserver listening
sudo netstat -tulpn | grep 2345
```

### Issue: Breakpoints Not Hit

**Problem**: GDB says breakpoint set, but never hits

**Solutions**:
```bash
# Ensure debug symbols
file ptp_grandmaster
# Should show: "not stripped"

# Rebuild with debug info
cmake .. -DCMAKE_BUILD_TYPE=Debug
make clean && make

# Verify source file mapping
(gdb) info sources
(gdb) set substitute-path /remote/path /local/path
```

### Issue: VS Code Can't Find Sources

**Problem**: "Source file not found" in debug view

**Solution** - Update `sourceFileMap` in `launch.json`:
```json
"sourceFileMap": {
    "/home/zarfld/IEEE_1588_2019": "d:\\Repos\\IEEE_1588_2019"
}
```

### Issue: High PTP Latency

**Tools to Diagnose**:
```bash
# Check CPU load
top -p $(pgrep ptp_grandmaster)

# Check interrupt latency
sudo cat /proc/interrupts | grep eth1

# Check network buffer
ethtool -S eth1 | grep -i error

# Check system time offset
chronyc tracking
```

---

## 📊 Monitoring Dashboard

### Terminal Multiplexer (tmux) Layout

```bash
# Install tmux
sudo apt install tmux

# Create monitoring session
tmux new -s ptp-debug

# Split panes
Ctrl+b %    # Split horizontal
Ctrl+b "    # Split vertical

# Pane 1: Application logs
sudo journalctl -u ptp-grandmaster -f

# Pane 2: PTP state
watch -n 1 'sudo pmc -u -b 0 "GET CURRENT_DATA_SET"'

# Pane 3: GPS/PPS status
watch -n 1 'sudo timeout 2 ppstest /dev/pps0 | head -n 5'

# Pane 4: PHC offset
watch -n 1 'sudo phc2sys -s CLOCK_REALTIME -c /dev/ptp0 -O 0 -m'
```

### Web-Based Monitoring (Optional Future Enhancement)

```bash
# Install Grafana + Prometheus
# Export PTP metrics
# Create dashboards for:
#   - Clock offset over time
#   - Packet rate (Announce, Sync)
#   - GPS fix quality
#   - PPS jitter
```

---

## 🛠️ Quick Reference Commands

### Raspberry Pi Commands

```bash
# Build & Run
cd ~/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/build
cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j4
sudo ./ptp_grandmaster --interface=eth1 --verbose

# Debug with gdbserver
sudo gdbserver :2345 ./ptp_grandmaster --interface=eth1

# Check PTP status
sudo pmc -u -b 0 'GET CURRENT_DATA_SET'
sudo pmc -u -b 0 'GET TIME_STATUS_NP'

# Monitor GPS
sudo cat /dev/ttyACM0
sudo ppstest /dev/pps0

# Check PHC
sudo phc_ctl /dev/ptp0 get
```

### Development Machine Commands

```bash
# Connect via SSH
ssh zarfld@192.168.x.x

# Remote debug (from WSL)
cd /mnt/d/Repos/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster
gdb-multiarch
(gdb) target remote 192.168.x.x:2345

# Sync entire project to Pi (from WSL)
rsync -avz --exclude 'build' \
  /mnt/d/Repos/IEEE_1588_2019 \
  zarfld@192.168.x.x:~/

# Copy single file to Pi
scp file.cpp zarfld@192.168.x.x:/home/zarfld/IEEE_1588_2019/examples/12-RasPi5_i226-grandmaster/src/

# VS Code Remote-SSH
code --folder-uri vscode-remote://ssh-remote+192.168.x.x/home/zarfld/IEEE_1588_2019
```

---

## 📚 Resources

### Documentation
- [GDB Manual](https://sourceware.org/gdb/documentation/)
- [VS Code Remote Development](https://code.visualstudio.com/docs/remote/ssh)
- [Linux perf Tutorial](https://perf.wiki.kernel.org/index.php/Tutorial)

### Tools
- GDB Multiarch: ARM64 debugging from x86
- VS Code Remote-SSH: Full IDE on remote machine
- tmux: Terminal multiplexer for monitoring
- Wireshark: PTP packet analysis

---

**Ready to Debug!** 🚀

Choose your workflow:
- **Quick Tests**: VS Code Remote-SSH (recommended for active development)
- **Deep Debugging**: GDB Remote (breakpoints, variable inspection)
- **Performance**: Linux perf + cyclictest (identify bottlenecks)
