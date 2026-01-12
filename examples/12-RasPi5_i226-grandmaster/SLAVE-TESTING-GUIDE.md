# PTP Slave Device Testing Instructions

## Prerequisites

1. **Two Raspberry Pi 5 devices** with Intel i226 NICs
2. **Direct Ethernet connection** between them (or same network switch)
3. **Grandmaster running** on first device
4. **ptp4l installed** on second device (slave)

---

## Slave Device Setup

### 1. Verify Network Connection

```bash
# Check which interface is connected (look for "UP" and "LOWER_UP"):
ip link show

# Example output:
# 1: lo: <LOOPBACK,UP,LOWER_UP> ...
# 2: eth0: <NO-CARRIER,BROADCAST,MULTICAST,UP> ...  ← Link down!
# 3: eth1: <BROADCAST,MULTICAST,UP,LOWER_UP> ...    ← Use this one!
```

**Use the interface that shows "UP,LOWER_UP" (cable connected)**

### 2. Verify PHC Device Mapping

```bash
# Find which PTP device corresponds to your interface:
readlink -f /sys/class/net/eth1/ptp

# Example output:
# /sys/class/ptp/ptp1  ← Use /dev/ptp1

# Verify device exists:
ls -l /dev/ptp1
# Should show: crw------- 1 root root 249, 1 ...
```

### 3. Install ptp4l (If Not Already Installed)

```bash
# Debian/Ubuntu/Raspberry Pi OS:
sudo apt update
sudo apt install linuxptp

# Verify installation:
ptp4l -v
# Should show: ptp4l 3.x or 4.x
```

---

## Running Slave Tests

### Test 1: Basic Slave Synchronization

```bash
# Replace eth1 with your connected interface (from Step 1)
sudo ptp4l -i eth1 -s -m

# Flags explained:
# -i eth1  : Use eth1 interface (change if needed!)
# -s       : Slave mode only (don't try to become master)
# -m       : Print monitoring statistics every second
```

**Expected Output (Initial 10-20 seconds)**:
```
ptp4l[xxxxx.xxx]: selected /dev/ptp1 as PTP clock
ptp4l[xxxxx.xxx]: port 1 (eth1): INITIALIZING to LISTENING on INIT_COMPLETE
ptp4l[xxxxx.xxx]: port 1 (eth1): LISTENING to UNCALIBRATED on RS_MASTER
ptp4l[xxxxx.xxx]: selected best master clock 000000.fffe.000001
ptp4l[xxxxx.xxx]: port 1 (eth1): UNCALIBRATED to SLAVE on MASTER_CLOCK_SELECTED
```

**Expected Output (After 30-60 seconds - LOCKED)**:
```
ptp4l[xxxxx.xxx]: rms    234 max    456 freq  +12345 +/-  123 delay    456 +/-   12
                   ^^^     ^^^       ^^^^^^
                   RMS    Peak      Frequency   Path
                   Offset Offset    Correction  Delay

# SUCCESS CRITERIA:
# - rms < ±1000 ns  (sub-microsecond RMS synchronization)
# - rms < ±100 ns   (excellent - better than expected!)
# - freq stabilizes (stops changing rapidly after 60s)
# - delay shows reasonable value (< 10 µs for direct connection)
```

### Test 2: Verbose Slave (See Details)

```bash
sudo ptp4l -i eth1 -s -m -l 7

# Additional flag:
# -l 7  : Debug level 7 (most verbose)
```

**Shows detailed PTP message processing**:
```
ptp4l[xxxxx.xxx]: port 1 (eth1): received Announce message
ptp4l[xxxxx.xxx]: port 1 (eth1): received Sync message
ptp4l[xxxxx.xxx]: port 1 (eth1): received Follow_Up message
ptp4l[xxxxx.xxx]: master offset      -234 s2 freq  +12345 path delay    456
```

### Test 3: Hardware Timestamping Verification

```bash
# Verify that ptp4l is using hardware timestamping:
sudo ptp4l -i eth1 -s -m 2>&1 | grep -i "time stamp"

# Expected output:
# timestamping: hardware
# tx_timestamp_timeout: 10  ← Hardware TX timestamping working
```

---

## Interpreting Results

### Excellent Performance ✅
```
ptp4l[xxxxx.xxx]: rms     45 max     89 freq   +1234 +/-   23 delay    123 +/-    5
                          ^^            ^^^^
                   < 100 ns RMS    Freq stable
```
- **RMS offset < 100 ns**: Sub-100 nanosecond synchronization!
- **Frequency stable**: Servo locked, minimal corrections needed
- **Low jitter**: `+/-` values small and consistent

### Good Performance ✅
```
ptp4l[xxxxx.xxx]: rms    456 max    892 freq   +5678 +/-   89 delay    234 +/-   12
                         ^^^            ^^^^
                   < 1 µs RMS     Converging
```
- **RMS offset < 1000 ns**: Sub-microsecond synchronization (goal achieved!)
- **Frequency converging**: Getting better over time
- **Moderate jitter**: Normal for Ethernet path delay variations

### Needs Investigation ⚠️
```
ptp4l[xxxxx.xxx]: rms  12345 max  45678 freq  +56789 +/-  234 delay   1234 +/-   89
                      ^^^^^        ^^^^^^
                   > 10 µs        Unstable
```
- **RMS offset > 10 µs**: Something wrong (check network, grandmaster status)
- **Frequency unstable**: Servo not converging (check for interference)
- **High jitter**: Network issues or packet loss

---

## Troubleshooting

### Issue: "link down" Error

**Symptom**:
```
ptp4l[xxxxx.xxx]: port 1 (eth0): link down
ptp4l[xxxxx.xxx]: port 1 (eth0): FAULTY
```

**Solution**: Wrong interface! Check connected interface:
```bash
ip link show
# Use the interface that shows "UP,LOWER_UP"
```

### Issue: "No PTP master found"

**Symptom**: Slave stays in LISTENING state forever

**Checks**:
1. **Grandmaster running?**
   ```bash
   # On grandmaster device:
   ps aux | grep ptp_grandmaster
   # Should show running process
   ```

2. **Network connectivity?**
   ```bash
   # From slave, ping grandmaster:
   ping 192.168.1.100  # (replace with grandmaster IP)
   ```

3. **Firewall blocking?**
   ```bash
   # Temporarily disable firewall for testing:
   sudo ufw disable
   ```

### Issue: Large Offset (> 100 µs)

**Symptom**: RMS offset stays above 100 µs

**Checks**:
1. **Grandmaster PHC locked to GPS?**
   ```bash
   # Check grandmaster output for:
   # [PHC Discipline] ✓ Locked to GPS (offset < 1µs)
   ```

2. **Network congestion?**
   ```bash
   # Check packet loss:
   ping -c 100 <grandmaster_ip>
   # Should show 0% packet loss
   ```

3. **PTP traffic on wrong VLAN?**
   ```bash
   # Check VLAN configuration if using managed switches
   ```

---

## Advanced Testing

### Test 4: PTP Management Client (pmc)

```bash
# Query slave status:
sudo pmc -u -b 0 'GET TIME_STATUS_NP'

# Expected output:
# TIME_STATUS_NP
#   master_offset              -234
#   ingress_time               1768173300123456789
#   cumulativeScaledRateOffset +1.000012345
#   scaledLastGmPhaseChange    0
#   gmTimeBaseIndicator        0
#   lastGmPhaseChange          0x0000'0000000000000000.0000
#   gmPresent                  true
#   gmIdentity                 000000.fffe.000001
```

### Test 5: Continuous Monitoring

```bash
# Log to file for analysis:
sudo ptp4l -i eth1 -s -m | tee /tmp/ptp_slave.log

# In another terminal, monitor performance:
tail -f /tmp/ptp_slave.log | grep "rms"
```

### Test 6: Compare Clocks Directly

```bash
# On slave:
sudo phc_ctl /dev/ptp1 get

# On grandmaster:
sudo phc_ctl /dev/ptp0 get

# Compare timestamps - should be within microseconds!
```

---

## Success Criteria Summary

| Metric | Excellent | Good | Needs Work |
|--------|-----------|------|------------|
| **RMS Offset** | < 100 ns | < 1 µs | > 10 µs |
| **Max Offset** | < 200 ns | < 2 µs | > 20 µs |
| **Frequency Stability** | ±100 ppb | ±1000 ppb | >±5000 ppb |
| **Path Delay** | < 1 µs | < 10 µs | > 100 µs |
| **Time to Lock** | < 30s | < 60s | > 120s |

---

## Next Steps After Successful Sync

Once slave shows **RMS offset < 1 µs**:

1. ✅ **Bug #9 FULLY VERIFIED** - Sub-microsecond sync achieved!
2. ⏳ **Implement Delay mechanism** (Delay_Req/Delay_Resp) in grandmaster
3. ⏳ **Test path delay measurement** accuracy
4. ⏳ **Multi-slave testing** (3+ slaves to same grandmaster)
5. ⏳ **Long-term stability** (24+ hour continuous operation)

---

## Quick Reference Commands

```bash
# Start slave (basic):
sudo ptp4l -i eth1 -s -m

# Start slave (verbose):
sudo ptp4l -i eth1 -s -m -l 7

# Check status:
sudo pmc -u -b 0 'GET TIME_STATUS_NP'

# Compare clocks:
sudo phc_ctl /dev/ptp1 get  # slave
sudo phc_ctl /dev/ptp0 get  # grandmaster (on other device)

# Monitor continuously:
sudo ptp4l -i eth1 -s -m | tee /tmp/ptp_slave.log
```
