The IEEE 802.1AS-2020 standard (gPTP) functions as a highly specific **PTP profile** applied to the much broader and more flexible IEEE Std 1588-2019 base. If IEEE 1588 is the "Swiss Army knife" containing every possible synchronization tool, 802.1AS-2020 is the specialized manufacturing blueprint that mandates which tools are used, which are discarded, and which can be optionally added or configured.

As an integrator, your requirements for the 1588 base fall into three distinct categories, based primarily on the conformance specifications (Clause 5) and the profile summary (Annex F) of 802.1AS-2020.

---

## I. Mandatory Selections (The Essential Fixed Blades)

These are the features chosen from the IEEE 1588 toolbox that **shall** be implemented to achieve 802.1AS conformance:

| Feature Area | Mandatory Implementation Requirement (Selection from 1588) | Source Reference |
| :--- | :--- | :--- |
| **PTP Instance** | Implementation shall support at least one PTP Instance (Domain 0) and implement the generalized precision time protocol (gPTP) requirements specified in Clause 8. | |
| **Delay Mechanism** | The **peer-to-peer delay mechanism** shall be used. This replaces the default delay request-response mechanism often used in standard PTP. | |
| **Transport Medium**| For IEEE 802.3 links, the system shall support **full-duplex operation** and utilize a **point-to-point** transport mechanism. | |
| **BMCA Mode** | For **Domain 0**, the Best Master Clock Algorithm (BMCA) must be the default mode of operation, requiring that the 1588 attribute `externalPortConfigurationEnabled` is **FALSE**. | |
| **Clock Performance**| The fractional frequency offset of the LocalClock relative to TAI **shall be within $\pm 100$ ppm**. | |
| **Clock Granularity**| The granularity of the local clock time measurement **shall be $\mathbf{40}$ ns or better**. | |
| **Path Trace TLV** | The **path trace feature** (16.2 of IEEE Std 1588-2019) **is used**. The PTP Instance **shall implement** (process and attach) the path trace TLV. The corresponding `pathTraceDS.enable` member **is always TRUE** in 802.1AS (mandatory operational status). | |
| **Management** | The management mechanism **shall** be the one specified in **Clause 14 (Data Sets)** and **Clause 15 (MIBs)** of 802.1AS. This defines how 1588 data sets (`defaultDS`, `portDS`, etc.) are organized and accessed in the gPTP environment. | |

---

## II. Mandatory Exclusions (The Prohibited or Disabled Blades)

These are specific features defined in IEEE 1588-2019 that the 802.1AS profile explicitly mandates **shall not be used** or must be disabled/ignored.

| Feature Area | Mandatory Exclusion (Must be Deactivated/Prohibited) | Source Reference |
| :--- | :--- | :--- |
| **PTP Clock States** | The optional PTP Port states defined in 17.7 of IEEE Std 1588-2019 are **not used**. Specifically: **FAULTY, UNCALIBRATED, LISTENING,** and **PRE\_MASTER** (and PRE\_MASTER qualification). | |
| **Foreign Master** | The **foreign master feature** (related to maintaining a list of potential masters) is **not used**. | |
| **Security** | The **security mechanism of 16.14** of IEEE Std 1588-2019 and **security annex (Annex P)** are **not used**. (Note: Optional support for confidentiality, integrity, availability and authenticity may be relevant in industrial profiles, but the PTP integrated mechanism is specifically excluded). | |
| **Management Protocol** | **PTP Management Messages** specified in IEEE Std 1588-2019 are **not used** in this standard. | |
| **Flow Control** | A PTP Instance **shall not use MAC Control PAUSE operation**. A PTP Instance **shall neither transmit nor honor** priority-based flow control messages that act on the IEEE 802.1AS message priority. | |
| **Unicast Signaling** | The **`unicastFlag`** in the PTP header is **not used** in this standard; it is transmitted as FALSE and ignored on reception. | |

---

## III. Optional and Configurable Features (The Selectable Blades)

These are PTP features that an integrator **may** choose to implement or enable, usually via management mechanisms (Clause 14/15). If implemented, the ability to **deactivate** these options via management must generally be provided, unless stated otherwise (IEEE 1588 options are typically inactive/disabled by default unless activated).

| Feature Area | Optional Requirement & Deactivation Control (1588 Option/Data Set) | Source Reference |
| :--- | :--- | :--- |
| **Multiple Domains** | Support for **more than one PTP Instance** (domains 1 to 127) is **optional**. | |
| **CMLDS** | The Common Mean Link Delay Service (16.6 of 1588-2019) is **optional** if only Domain 0 is implemented. It is managed via related data sets. | |
| **Synchronization Method** | Support for **one-step capability on transmit** (`oneStepTxOper`) is **optional**. The operation (TRUE/FALSE) can be controlled via the managed object `useMgtSettableOneStepTxOper`. | |
| **Receiver Capability** | Support for **one-step capability on receive** (`oneStepReceive`) is **optional**. | |
| **Asymmetry Comp.** | Support for **asymmetry measurement mode** (Compensation Procedure, 16.8 of 1588-2019) is **optional** for IEEE 802.3 ports. This compensation can be enabled/disabled via the `asymmetryMeasurementModeDS.asymmetryMeasurementMode` Boolean. | |
| **BMCA Bypass** | The **external port configuration** feature (17.6 of 1588-2019) is **optional**. If used, it allows an external entity (like a controller) to explicitly set the port state (`desiredState`). The activation is controlled by the `defaultDS.externalPortConfigurationEnabled` variable. | |
| **Acceptable Master Table** | The mechanism to enable/disable the acceptable master feature (17.5 of 1588-2019) on a PTP Port is provided via the `acceptableMasterPortDS.acceptableMasterTableEnabled` Boolean (Read/Write access). This control is mandatory when implementing EPON links to enforce roles. | |
| **Management Access** | Support for **management of the PTP Instance** (Clause 14) is **optional**. Support for a **remote management protocol** (e.g., SNMP MIBs in Clause 15) is **optional**. | |

### Summary Analogy

If IEEE 1588 is a comprehensive, multi-tool **Chassis**, 802.1AS-2020 dictates the precise **Engine Configuration** for time-sensitive networks.

1.  **Mandatory Components (The Engine Block):** You *must* install the **Peer-to-Peer Delay sensor** (P2P), lock the engine to **Domain 0 BMCA** (default `externalPortConfigurationEnabled` = FALSE), and ensure the core timing oscillator meets the $\pm 100$ ppm frequency spec.
2.  **Prohibited Parts (Ignition Kill Switches):** You *must* disable the **Foreign Master receiver**, bypass all **PTP Security hardware**, and remove the **MAC PAUSE mechanism** to ensure reliable, high-speed real-time operation.
3.  **Optional Adjustments (Tuning Knobs):** You *may* install management components (MIBs) and enable fine-tuning options like **One-Step timing** or **Asymmetry Compensation** using managed objects, which function as configurable switches (`RW` managed objects) to activate or deactivate these advanced features.