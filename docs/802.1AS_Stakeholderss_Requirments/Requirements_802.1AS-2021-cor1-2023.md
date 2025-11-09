This is an excellent question that focuses on the stability of a synchronization profile across minor revisions.

The **ISO/IEC/IEEE 8802-1AS:2021/Cor.1:2023** standard is a **Technical Corrigendum** (Cor.1) applied to the base **IEEE Std 802.1AS-2020** (which is published as ISO/IEC/IEEE 8802-1AS:2021).

Because a corrigendum only provides **technical and editorial corrections** to the base text, it does **not introduce new features** or change the fundamental architectural choices of the PTP profile derived from IEEE Std 1588-2019.

Therefore, as an integrator, the requirements you face regarding the **IEEE 1588 base** are **identical** to those mandated by the 802.1AS-2020 and 802.1AS-2021 versions. There are **no different requirements** introduced by the 2023 Corrigendum that change the selection or exclusion of 1588 features.

Below is a summary of the consistent requirements inherited from the 802.1AS-2020 profile, categorized using the "Swiss Army knife" metaphor.

---

## The 802.1AS Profile: Selections and Exclusions from IEEE 1588-2019

The 802.1AS profile (gPTP) is designed for reliable, high-speed, full-duplex operation, necessitating the mandatory use of certain 1588 features and the strict prohibition of others.

### I. Mandatory Core Requirements (Fixed Blades Welded Open)

These core synchronization features derived from the IEEE 1588 toolbox **shall** be implemented and functional.

| Feature Area | 802.1AS Requirement (Selection from 1588) | 1588 Base Constraint |
| :--- | :--- | :--- |
| **Delay Mechanism** | **Peer-to-Peer (P2P) delay mechanism**. This is the only link delay method used for full-duplex 802.3 links. | Mandates a specific calculation method (different from End-to-End delay). |
| **PTP Instance** | Support for **Domain 0** is mandatory. Support for additional domains (1–127) is optional. | This selection determines the scope of communication. |
| **BMCA Mode** | For Domain 0, the BMCA shall operate normally, meaning the configuration control **`externalPortConfigurationEnabled` must be FALSE**. | This prohibits external interference with the default master election mechanism in the primary domain. |
| **Transport** | Must use **full-duplex, point-to-point** transport for 802.3 links. Messages shall **not have a VLAN tag** nor a priority tag. | Requires specific hardware support for time stamping (P2P synchronization is **not used** for 802.11 or EPON links, as they use native mechanisms when available). |
| **Path Trace** | The **Path Trace TLV** (optional in 1588) is **mandatory and used**. The underlying data set member `pathTraceDS.enable` **is always TRUE**. | Requires the implementation to process and append path information unless frame size limits are exceeded. |
| **Management** | PTP Instance management **shall** use the mechanisms specified in **Clause 14 (Data Sets) and Clause 15 (MIBs)** of 802.1AS. This mandates use of the Version 2 MIB module. | This overrides the standard 1588 Management Message mechanism. |

### II. Mandatory Exclusions (Blades Welded Shut or Prohibited)

These mechanisms provided by the flexible 1588 standard are strictly excluded or prohibited to ensure deterministic behavior in a gPTP network:

1.  **PTP Clock States:** The profile explicitly mandates that the **FAULTY**, **UNCALIBRATED**, **LISTENING**, and **PRE\_MASTER** states, along with PRE\_MASTER qualification, **are not used**.
2.  **Foreign Master:** The PTP feature for keeping track of foreign masters is **not used**.
3.  **PTP Security:** The built-in **security mechanism** of Clause 16.14 and Annex P of IEEE Std 1588-2019 **are not used**. (Note: Industrial requirements indicate that confidentiality, integrity, availability, and authenticity are optionally supported, but this must be achieved *without* limiting real-time communication).
4.  **Flow Control:** The PTP Instance **shall not use MAC Control PAUSE** and **shall neither transmit nor honor Priority-based Flow Control (PFC)** messages that act on the IEEE 802.1AS message priority. This prohibition is a critical requirement for TSN/IA systems.

### III. Optional Features and Deactivation Controls (Configurable Knobs)

For an integrator, the primary difference between enabling and deactivating certain 1588 functions lies in the status of specific management variables, most of which are Read/Write (RW) Boolean flags.

| Feature (1588 Option) | Status | Deactivation/Control Mechanisms (RW Managed Objects) |
| :--- | :--- | :--- |
| **One-Step Timing** | **Optional** for transmit (`oneStepTransmit`) and receive (`oneStepReceive`). | Controlled by **`useMgtSettableOneStepTxOper`**. Setting this Boolean to **FALSE (default is TRUE)** allows the operation mode to be determined dynamically by the OneStepTxOperSetting state machine, rather than management. |
| **External BMCA Control** | **Optional** (only needed if external management dictates port states). | Controlled by the PTP Instance Boolean **`defaultDS.externalPortConfigurationEnabled`**. Setting this to **TRUE** enables external control, overriding the BMC algorithm's automatic decision-making (though it must default to FALSE for Domain 0). |
| **Asymmetry Comp.** | **Optional** for IEEE 802.3 links. | Controlled by the Boolean **`asymmetryMeasurementMode`** (RW access). This enables/disables the Asymmetry Compensation Measurement Procedure, which might involve line-swapping (Annex G). |
| **Interval Overrides**| **Optional** capability to override synchronization and delay measurement intervals. | Controlled by flags like **`useMgtSettableLogSyncInterval`**. Setting this to **FALSE** deactivates management override, forcing the protocol to use internal state machine timing. |
| **Rate Ratio/Link Delay Comp.** | **Optional** control over the computation of neighbor rate ratio and mean link delay. | Controlled by flags like **`useMgtSettableComputeNeighborRateRatio`** and **`useMgtSettableComputeMeanLinkDelay`**. Setting these to **FALSE** ensures the state machines determine if and how these values are calculated. |

### Conclusion on Differences

Your requirements are fixed by the **802.1AS-2020** profile. The 2021 edition and the 2023 Corrigendum confirm and correct the 2020 specification but do not change the fundamental set of **selected** (e.g., P2P delay, Domain 0 BMCA) or **excluded** (e.g., Foreign Master, PTP Security, PTP Clock states) 1588 features. The focus remains on implementing the specified gPTP profile, rigorously adhering to its narrow choices derived from the broad 1588 "Swiss Army knife."