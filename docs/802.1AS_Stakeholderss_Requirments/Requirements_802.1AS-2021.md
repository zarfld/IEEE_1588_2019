The ISO/IEC/IEEE 8802-1AS:2021 standard (which incorporates IEEE Std 802.1AS-2020), defines a strict **PTP profile** built upon the vast set of options provided by **IEEE Std 1588-2019**.

If IEEE 1588 is likened to a comprehensive **Swiss Army knife** containing every possible synchronization tool, 802.1AS-2021 acts as the mandatory blueprint, specifying exactly which blades you must weld permanently open, which you must permanently disable, and which you can leave configurable.

As an integrator, your requirements map directly to these three categories:

---

## I. Mandatory Selections (The Essential Fixed Blades)

These features derived from the IEEE 1588 base **shall** be implemented and used according to the gPTP specification (Clause 8 of 802.1AS).

| Feature | 802.1AS Requirement (PTP Profile) | Source Reference |
| :--- | :--- | :--- |
| **Path Delay Mechanism** | Mandated choice is the **peer-to-peer delay mechanism**. | |
| **PTP Instance** | Implementation **shall** support at least one PTP Instance (Domain 0). | |
| **Transport** | **Full-duplex point-to-point** operation is mandatory for IEEE 802.3 MAC services ports. Frames carrying 802.1AS messages **shall be untagged** (neither VLAN-tagged nor priority-tagged). | |
| **BMCA Implementation** | The Best Master Clock Algorithm (BMCA) **shall be implemented**. | |
| **BMCA Constraint (Domain 0)** | For Domain 0, the BMCA shall be implemented such that the `externalPortConfigurationEnabled` value is **FALSE** (disabling external control over BMCA decision making for the primary domain). | |
| **Path Trace TLV** | The Path Trace feature (optional in 1588-2019) **is mandatory and used**. The data set member `pathTraceDS.enable` **shall be TRUE**. | |
| **Management** | The management mechanism **shall** be based on Clause 14 (Data Sets) and Clause 15 (MIBs) defined in 802.1AS. Conformance requires support of the **Version 2 MIB module** (IEEE8021-AS-V2 MIB). | |
| **Clock Accuracy** | The fractional frequency offset of the LocalClock relative to TAI **shall be within $\pm 100$ ppm**. | |

---

## II. Mandatory Exclusions (The Prohibited or Disabled Blades)

The following specific components of the 1588 standard are **not used** or **prohibited**, simplifying the protocol stack and ensuring deterministic operation necessary for time-sensitive applications:

1.  **PTP Clock States:** Several optional PTP Port states defined in IEEE Std 1588-2019 (17.7) are explicitly **not used** in 802.1AS: **FAULTY, UNCALIBRATED, LISTENING,** and **PRE\_MASTER** (including PRE\_MASTER qualification).
2.  **Foreign Master Feature:** The **foreign master feature** is **not used**.
3.  **PTP Management Messages:** The specific PTP Management Messages defined in IEEE Std 1588-2019 are **not used**.
4.  **PTP Integrated Security:** The **security mechanism of 16.14** of IEEE Std 1588-2019 and the security annex (Annex P) are **not used**.
5.  **Unicast Flag:** The `unicastFlag` in the common PTP header is **not used** in this standard; it must be transmitted as FALSE and ignored on reception.
6.  **Flow Control Prohibitions (Requires Deactivation/Capability to Disable):** PTP Instances **shall not use** MAC Control PAUSE operation. They **shall neither transmit nor honor** priority-based flow control (PFC) messages that act on the IEEE 802.1AS message priority. This requires the integrator to ensure the device has the **capability to disable MAC control PAUSE** if implemented, and the capability to disable **Priority-based flow control** if implemented.

---

## III. Optional and Configurable Features (The Selectable Tuning Knobs)

These features, usually managed via the R/W objects in the Clause 14 Data Sets, allow you to enable (or implicitly disable) advanced functionality:

| Feature | Status | Management Control (Activation/Deactivation) | Source Reference |
| :--- | :--- | :--- | :--- |
| **Multiple Domains** | **Optional**. | N/A (Enabled by supporting multiple PTP Instances) | |
| **External BMCA Control** | **Optional**. If supported, implementation requires enabling `externalPortConfigurationEnabled` and setting the port `desiredState`. | Controlled by the PTP Instance Boolean `defaultDS.externalPortConfigurationEnabled`. | |
| **Common Mean Link Delay Service (CMLDS)** | **Required** if multiple domains are implemented. **Optional** if only Domain 0 is implemented. | Managed through Common Mean Link Delay Service Default Parameter Data Set (`cmldsDefaultDS`). | |
| **One-Step Timing** | **Optional** for receive (`oneStepReceive`) and transmit (`oneStepTransmit`). | Controlled by `portDS.useMgtSettableOneStepTxOper` (RW), which defaults to TRUE (enabled). Setting this to FALSE returns control to the `OneStepTxOperSetting` state machine. | |
| **Asymmetry Compensation** | **Optional** for IEEE 802.3 links, relying on the 1588 mechanism (16.8). | Controlled by the Boolean **`asymmetryMeasurementMode`** (RW access). | |
| **Acceptable Master Table** | **Used** (supported), especially required for EPON links to enforce OLT/ONU roles. | Controlled by the Boolean `acceptableMasterPortDS.acceptableMasterTableEnabled` (RW access). | |
| **Interval Overrides** | **Optional** capability to override the synchronization intervals determined by the state machines. | Controlled by multiple `useMgtSettable...` Booleans (e.g., `useMgtSettableLogSyncInterval`) (RW access). Setting the `useMgtSettable...` flag to TRUE enables the corresponding `mgtSettable...` value to take effect. | |

The integrator's role is thus to utilize the **gPTP profile** as a highly optimized, specialized toolkit where redundant or complex 1588 features are minimized, and core performance features (P2P timing, BMCA for Domain 0, management visibility via MIBs) are standardized.