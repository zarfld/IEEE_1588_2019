The ISO/IEC/IEEE 60802 Industrial Automation (TSN-IA) profile relies heavily on the **IEEE 802.1AS** (gPTP) standard (specifically referencing 802.1AS-2019/2020/2021), which is itself a highly restricted profile of the broader IEEE 1588-2019 standard.

Since 60802 adopts the 802.1AS mechanism for clock synchronization, it inherits all the base exclusions (like prohibiting PTP security and foreign masters) established in gPTP.

The key differences for an integrator implementing 60802-2019 manifest as **tighter performance constraints** and the **mandating of certain optional 802.1AS/1588 features** required for robust industrial operation (e.g., redundancy and management access).

---

## I. Mandatory Core Requirements (Fixed Blades Welded Open) - DIFFERENCES

While 802.1AS mandates fundamental 1588 choices (like Peer-to-Peer delay), 60802 enforces tighter specifications related to accuracy, time, and multi-domain support:

| Feature Area | 60802-2019 Specific Requirement | Implication for 1588 Base (Difference from 802.1AS) | Source |
| :--- | :--- | :--- | :--- |
| **PTP Domain Support** | **Shall support four synchronization domains** (as specified in Table 11). | This mandates the implementation of the multiple PTP Instance capability of 802.1AS, which is **optional** in generic 802.1AS. | |
| **CMLDS** | The Common Mean Link Delay Service (CMLDS), specified in 16.6 of IEEE Std 1588-2019, becomes a **mandatory requirement** because support for more than one domain is required. | This shifts a conditional option in 802.1AS to a mandatory element of the PTP profile when supporting industrial domains. | |
| **Timestamp Accuracy** | The minimal **timestamp accuracy** for sync and delay messages **shall be $\le 8$ ns** for both universal time and working clock timescales. | This is a **tighter engineering constraint** compared to the granularity requirement of $\le 40$ ns specified in 802.1AS. | |
| **Convergence Speed** | The state **"in sync within <1 µs accuracy" shall be achieved in less than 1 s** per device. | This imposes a strict **time-to-sync requirement**, a performance parameter not explicitly mandated by generic 802.1AS. | |
| **Management Objects** | **Mandatory/optional IEEE 802.1AS-2019 management objects** for diagnostics and parameterization **shall be defined** (R6.17). | This ensures implementation of Clause 14/15 data sets and MIBs (like the Version 2 MIB module), but demands a **specific subset of those managed objects** be mandatory for industrial configuration and diagnostics. | |
| **Diagnostics State** | The state **"out of sync" needs to be defined** (it is noted as "not defined in .1AS"). | This necessitates defining and implementing synchronization states beyond the minimal states used in 802.1AS (DISABLED, INITIALIZE, etc.). | |

### II. Mandatory Exclusions/Required Deactivation Capabilities

The 60802 profile reinforces existing 802.1AS exclusions and adds critical deactivation requirements related to power management:

| Feature Area | Requirement in 60802-2019 | Deactivation Control Required | Source |
| :--- | :--- | :--- | :--- |
| **Flow Control** | **Shall support the capability to disable MAC control PAUSE** if it is implemented. | Must provide management mechanism to disable this feature. (This aligns with the base 802.1AS rule that flow control is not used). | |
| **Priority Flow Control** | **Shall support the capability to disable Priority-based flow control (PFC)** if it is implemented. | Must provide management mechanism to disable this feature. (This aligns with the base 802.1AS rule that PFC is prohibited). | |
| **Energy Efficiency** | **Shall support the capability to disable support of Energy Efficient Ethernet (EEE)**. | Must provide a management mechanism (or design choice) to ensure EEE, which interferes with high-accuracy clock stability, is disabled. | |

### III. Optional Industrial Features that Constrain the 1588 Base

These industrial profile requirements utilize options related to security and management originally deemed optional or excluded in the base 1588 standard, influencing the implementation context of time synchronization:

1.  **Security Integration:** The 60802 profile requires **optional support of confidentiality, integrity, availability, and authenticity** (R27). This requires the integrator to select external security tools (like 802.1X) and integrate them such that the core PTP synchronization functions are **not limited** in their real-time communication. This is notable because the underlying 802.1AS profile explicitly **excludes** the PTP integrated security mechanism (16.14 and Annex P of 1588-2019).
2.  **Management Data Modeling (YANG/MIB):** The profile shall provide a vendor-independent solution and define managed objects for configuration. This management framework must support retrieving detailed diagnostics information for TSN features and facilitate **offline configuration**. The reliance on MIBs (SNMP) and YANG models (NETCONF/RESTCONF) for remote management is supported by 802.1AS-2020 (Clause 15, optional remote management), but 60802 makes its implementation critical for certification.

In essence, the 60802-2019 profile takes the specialized PTP tool (802.1AS/gPTP) and imposes a microscope on its performance, requiring tighter accuracy ($\le 8$ ns) and faster synchronization speeds ($\le 1$ s), while mandating the implementation of multi-domain support (four domains) and external management hooks (like EEE disable) essential for dependable, configurable industrial networks.