The AES67-2018 standard (for high-performance audio over IP) implements a fundamentally different philosophy toward the IEEE 1588 base than the IEEE 802.1AS (gPTP) standards (2020/2021).

While 802.1AS dictates a highly constrained Layer 2 profile optimized for determinism (TSN), AES67 mandates interoperability across standard IP networks and Ethernet AVB networks. This difference results in entirely distinct choices from the IEEE 1588 "Swiss Army knife," particularly concerning the required transport mechanism and core operational mode.

Here are the requirements AES67-2018 imposes on the 1588 base that **differ** significantly from the 802.1AS (gPTP) profile:

---

## I. Mandatory Selections (The Essential Fixed Blades)

AES67 mandates the use of synchronization mechanisms that are explicitly excluded or treated as legacy options by the 802.1AS profile, due to its focus on IP transport.

| Feature Area | AES67-2018 Specific Requirement | Implication for 1588 Base (Difference from 802.1AS) | Source |
| :--- | :--- | :--- | :--- |
| **Required PTP Profile** | Devices, unless using AVB synchronization (802.1AS), **shall support the IEEE 1588-2008 default profiles**. | **Fundamental Conflict:** 802.1AS requires implementing the PTP profile defined within 802.1AS (gPTP). AES67 mandates the much older 1588-2008 Default profile. | |
| **Mandatory Delay Mechanism** | The core requirement is satisfied by the **Delay Request-Response (End-to-End, E2E) mechanism** defined in the 1588-2008 Default profile. | **Opposite Mechanism:** 802.1AS explicitly mandates the **Peer-to-Peer (P2P)** delay mechanism. The P2P mechanism is merely **recommended (optional)** in the AES67 Media profile. | |
| **Mandatory Transport** | The only supported message encapsulation for the AES67 Media profile is **UDP/IPv4**. PTP messages use IP multicast addressing (AF41 or EF DSCP). | **Layer 3 vs. Layer 2:** 802.1AS mandates transport via **IEEE 802 MAC PDUs** (Layer 2 multicast) and explicitly requires frames to be **untagged** (neither VLAN-tagged nor priority-tagged). AES67 strictly relies on IP/UDP encapsulation. | |
| **Management Mechanism** | Node management **shall implement the management message mechanism of IEEE 1588-2008**. | **Opposite Protocol:** 802.1AS explicitly specifies that **PTP Management Messages** defined in IEEE Std 1588-2019 **are not used**. 802.1AS relies exclusively on MIB/Data Sets (Clause 14/15). | |
| **Synchronization Intervals** | The AES67 Media profile specifies that **`portDS.logSyncInterval`** and **`portDS.logMinDelayReqInterval` are reduced** (relative to the standard 1588-2008 defaults) to **improve startup time and accuracy**. The `defaultDS.domainNumber` range is **0 to 127**. | This imposes explicit, faster interval requirements (e.g., `logAnnounceInterval` default is 1, or 2 seconds) than standard PTP default settings, prioritizing fast media synchronization. | |
| **Clock Class Extensions** | AES67 defines and utilizes **additional `clockClass` values** beyond those in standard 1588-2008 to signal AES11 physical clock specifications, supporting synchronization to an external media clock source (e.g., clockClass 13, 14, 150, 193, etc.). | These classes tie PTP quality metrics directly to professional audio clock standards (AES11). | |

### II. Mandatory Exclusions (Blades Welded Shut) - Consistency with 802.1AS

AES67 inherits many of the same exclusions as 802.1AS, although they originate from different revisions of the 1588 standard:

1.  **PTP Integrated Security:** While based on 1588-2008 (which had an experimental Annex K security), AES67 does not rely on PTP's integrated security mechanism. This is consistent with 802.1AS, which explicitly excludes the PTP integrated security mechanism defined in 1588-2019 (16.14 and Annex P).
2.  **Explicit Clock States/Features:** Since AES67 uses the older 1588-2008 base profiles, the explicit exclusions of PTP states (FAULTY, UNCALIBRATED, LISTENING, PRE\_MASTER) and the **foreign master feature** that are defined in 802.1AS, are not specifically profiled in the same way, but the general philosophy of avoiding complex PTP state management is aligned.

### III. Optional and Configurable Features (The Selectable Tuning Knobs)

AES67 provides a clear rule for managing any optional feature implemented from the 1588 base, giving the integrator maximum flexibility provided those features are controllable:

| Feature (1588 Option) | Status in AES67-2018 | Deactivation Control Requirement | Source |
| :--- | :--- | :--- | :--- |
| **PTP Unicast Negotiation** | **Optional**. Devices may implement unicast negotiation (Clause 16.1 of 1588-2008). | **Mandatory Deactivation:** This option **shall be inactive unless specifically activated by a management procedure**. | |
| **PTP Clause 17 Options** | **Optional**. Devices may implement any optional features defined in IEEE 1588-2008 Clause 17 (e.g., Alternative BMCA, Extended State Control). | **Mandatory Deactivation:** These options **shall be inactive unless specifically activated by a management procedure**. | |
| **Peer Delay Mechanism** | **Optional.** The AES67 Media Profile includes a recommendation to implement the peer delay mechanism *in addition* to the mandatory request-response mechanism. | No explicit deactivation requirement stated if implemented, but falls under the general rule that optional features must be inactive by default. | |
| **Slave-Only Mode** | Configurable. If `defaultDS.slaveOnly` is configurable, the default value **shall be FALSE**. | The capability to enable this mode (setting to TRUE) is managed, but the default state favors master/slave operation. | |

The most important difference for an integrator is that implementing AES67 requires supporting the 1588 protocol stack over **UDP/IPv4** using the **End-to-End** delay mechanism and the older 1588 **Management Messages**, whereas implementing 802.1AS requires the stack to operate over **Layer 2 MAC** addresses using the **Peer-to-Peer** delay mechanism and the 802.1AS defined **Data Sets/MIBs** for configuration.