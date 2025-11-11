# Security Policy

## Supported Versions

The following versions of the IEEE 1588-2019 PTP library are currently supported with security updates:

| Version | Supported          | End of Support |
| ------- | ------------------ | -------------- |
| 1.0.x   | :white_check_mark: | TBD            |
| < 1.0   | :x:                | N/A (pre-release) |

**Note**: We recommend always using the latest stable release to ensure you have the most recent security patches.

## Security Considerations

### Time Synchronization Security

**Important**: The IEEE 1588-2019 PTP protocol itself has limited built-in security mechanisms. Please be aware of the following security considerations:

#### Network Security
- **PTP messages are unencrypted by default**: An attacker with network access can:
  - Observe timing information
  - Inject false timing messages
  - Disrupt synchronization
- **Mitigation**: Use network segmentation and VLANs to isolate PTP traffic on trusted networks

#### Spoofing Attacks
- **False Master Attack**: An attacker could inject Announce messages claiming to be a superior master
- **Mitigation**: 
  - Use IEEE 1588-2019 security extensions (Annex P) if available
  - Implement network-level access controls
  - Use static master configuration instead of BMCA in high-security environments

#### Denial of Service
- **Message Flooding**: Excessive PTP messages could consume CPU/network resources
- **Mitigation**:
  - Rate limiting at network layer
  - Monitor message rates
  - Use Quality of Service (QoS) to prioritize legitimate traffic

### Implementation Security

This library is designed with security best practices:

- ✅ **No dynamic allocation in critical paths** - Prevents heap exhaustion attacks
- ✅ **Bounds checking** - All packet parsing validates buffer sizes
- ✅ **Input validation** - Validates all protocol fields per IEEE spec
- ✅ **No unsafe functions** - Avoids `strcpy`, `sprintf`, etc.
- ✅ **Const correctness** - Prevents unintended modifications

### Known Limitations

1. **No encryption support**: This library does not currently implement IEEE 1588-2019 Annex P (security extensions)
2. **No authentication**: PTP messages are not authenticated
3. **Trust model**: Assumes network infrastructure is trusted

## Reporting a Vulnerability

**We take security seriously.** If you discover a security vulnerability in the IEEE 1588-2019 PTP library, please report it responsibly.

### Reporting Process

1. **Do NOT create a public GitHub issue** for security vulnerabilities
2. **Email security reports to**: [security@example.com](mailto:security@example.com)
3. **Use PGP encryption** if possible (PGP key available at [URL])

### What to Include

Please include the following information in your report:

- **Description**: Clear description of the vulnerability
- **Impact**: What could an attacker do? What systems are affected?
- **Reproduction**: Step-by-step instructions to reproduce the issue
- **Proof of Concept**: Code or commands demonstrating the vulnerability (if available)
- **Suggested Fix**: If you have ideas for how to fix it (optional)
- **Environment**: 
  - Library version
  - Operating system
  - Network configuration
  - Hardware platform

### Example Report Template

```
Subject: [SECURITY] Buffer overflow in parse_sync_message()

Description:
A buffer overflow vulnerability exists in the parse_sync_message() 
function when processing malformed Sync messages with invalid length fields.

Impact:
An attacker with network access could send crafted Sync messages to:
- Crash the PTP slave process (DoS)
- Potentially execute arbitrary code (RCE)

Reproduction Steps:
1. Configure PTP slave on interface eth0
2. Send crafted Sync message with length field = 0xFFFF
3. Observe crash in parse_sync_message() at line 234

Proof of Concept:
[Attach PoC code or packet capture]

Environment:
- Library version: 1.0.0
- OS: Linux Ubuntu 22.04
- Architecture: x86_64
- NIC: Intel i210
```

### Response Timeline

- **Initial Response**: Within 48 hours of receipt
- **Triage**: Within 1 week - confirm vulnerability and assess severity
- **Fix Development**: Depends on severity:
  - Critical (P0): Patch within 72 hours
  - High (P1): Patch within 2 weeks
  - Medium (P2): Patch within 1 month
  - Low (P3): Patch in next scheduled release
- **Public Disclosure**: After fix is released and users have time to update (typically 2-4 weeks)

### Severity Classification

We use the following severity levels:

**Critical (P0)**:
- Remote code execution
- Authentication bypass
- Data exfiltration
- Complete loss of time synchronization

**High (P1)**:
- Denial of service
- Privilege escalation
- Significant timing attack surface

**Medium (P2)**:
- Information disclosure
- Local privilege escalation
- Moderate timing vulnerabilities

**Low (P3)**:
- Minor information leaks
- Edge case vulnerabilities
- Theoretical attacks requiring impractical conditions

## Disclosure Policy

### Coordinated Disclosure

We follow **coordinated vulnerability disclosure**:

1. **Private Disclosure**: Reporter notifies maintainers privately
2. **Acknowledgment**: Maintainers confirm receipt and begin investigation
3. **Fix Development**: Maintainers develop and test fix
4. **Advance Notice**: Optional - notify major users before public disclosure
5. **Public Disclosure**: Release fix and publish security advisory
6. **Credit**: Reporter credited in advisory (unless they prefer anonymity)

### Security Advisories

Security advisories will be published:
- **GitHub Security Advisories**: [https://github.com/[org]/IEEE_1588_2019/security/advisories](https://github.com/[org]/IEEE_1588_2019/security/advisories)
- **Mailing List**: security-announce@example.com (subscribe at [URL])
- **Release Notes**: Included in release notes with CVE if assigned

### CVE Assignment

For significant vulnerabilities, we will:
- Request CVE (Common Vulnerabilities and Exposures) ID
- Include CVE in security advisory
- Report to NVD (National Vulnerability Database)

## Security Best Practices for Users

### Deployment Security

When deploying the IEEE 1588-2019 PTP library:

1. **Network Isolation**
   - Deploy PTP on isolated/dedicated network (VLAN)
   - Use firewalls to restrict access to PTP traffic
   - Avoid mixing PTP with untrusted traffic

2. **Access Control**
   - Restrict network access to known PTP masters
   - Use ACLs to filter PTP messages by source MAC/IP
   - Monitor for unauthorized PTP devices

3. **Monitoring**
   - Monitor for unusual BMCA state changes
   - Alert on large offset changes
   - Log all PTP events for forensics

4. **Updates**
   - **Apply security patches promptly** (within 48 hours for critical)
   - Subscribe to security advisory mailing list
   - Test patches in staging before production

5. **Configuration**
   - Use static master configuration (disable BMCA) if in high-security environment
   - Set appropriate priority values to prevent rogue masters
   - Disable unused PTP features

### Secure Coding Practices for Contributors

If contributing code:

- Validate all inputs at trust boundaries
- Use safe string functions (`strncpy`, `snprintf`)
- Check array bounds before access
- Avoid integer overflows in size calculations
- Use `const` for data that shouldn't change
- Initialize all variables
- Review code for security issues before submitting PR

## Security-Related Configuration

### Hardening PTP Slave

```ini
# /etc/ptp-slave.conf

[security]
# Reject Announce messages from unknown masters
accept_unknown_masters = false

# List of allowed master clock identities (if accept_unknown_masters=false)
allowed_masters = 00:11:22:33:44:55:66:77, 88:99:AA:BB:CC:DD:EE:FF

# Maximum allowed clock step (prevent large time jumps)
max_clock_step_ns = 1000000  # 1ms

# Enable audit logging of all BMCA decisions
audit_bmca = true
```

### Network-Level Protections

```bash
# Linux iptables: Allow PTP only from known master
sudo iptables -A INPUT -p udp --dport 319 -s 192.168.1.100 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 319 -j DROP

sudo iptables -A INPUT -p udp --dport 320 -s 192.168.1.100 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 320 -j DROP

# Or for Layer 2 PTP, use ebtables
sudo ebtables -A INPUT -p 0x88F7 -s 00:11:22:33:44:55 -j ACCEPT
sudo ebtables -A INPUT -p 0x88F7 -j DROP
```

## Contact

- **Security Issues**: [security@example.com](mailto:security@example.com)
- **General Questions**: [GitHub Issues](https://github.com/[org]/IEEE_1588_2019/issues) (for non-security topics)
- **Security Mailing List**: security-announce@example.com

**PGP Key**: Available at [URL] (Key ID: [ID])

---

**Thank you for helping keep the IEEE 1588-2019 PTP library secure!**
