As an integrator implementing IEEE 802.1AS (also known as generalized Precision Time Protocol or gPTP), your implementation must conform to the requirements specified in the 802.1AS standard, which functions as a **PTP profile** leveraged from the IEEE 1588 base.

A time-aware system implementation **shall support at least one IEEE 1588 Precision Time Protocol (PTP) Instance**. The 802.1AS standard takes specific mandatory and optional choices regarding the features and mechanisms defined in IEEE Std 1588-2019.

Here are the key requirements related to the IEEE 1588 base that your 802.1AS implementation must adhere to, based on the defined profile:

### Core PTP Functionality Requirements (Mandatory)

1.  **PTP Instance Implementation:** The system must implement the core requirements of a PTP Instance, including the generalized precision time protocol (gPTP) requirements specified in Clause 8 of 802.1AS.
2.  **Path Delay Mechanism:** The mandated path delay mechanism for the 802.1AS PTP profile is the **peer-to-peer delay mechanism**.
3.  **Transport Mechanism:** The required transport mechanism is **full-duplex and point-to-point**, utilizing specific attribute values described in Annex E of IEEE Std 1588-2019 for IEEE 802.3 Ethernet. An implementation with IEEE 802.3 MAC services to physical ports shall support **full-duplex operation**.
4.  **Best Master Clock Algorithm (BMCA):** The implementation must support the BMCA requirements specified in 802.1AS (Clauses 10.3.2 through 10.3.6, 10.3.8, and 10.3.10).
5.  **PTP States (Exclusions):** The 802.1AS profile explicitly specifies that certain PTP states defined in IEEE 1588-2019 are **not used**. These excluded states/features include:
    *   The **FAULTY** state.
    *   The **UNCALIBRATED** state.
    *   The **LISTENING** state.
    *   The **PRE\_MASTER** state and PRE\_MASTER qualification.
    *   The **foreign master feature**.
6.  **Clock Characteristics:** The implementation must define PTP attributes as specified by 802.1AS, which often align with or constrain the 1588 requirements:
    *   The `clockIdentity` attribute shall be as specified in IEEE Std 1588-2019.
    *   The fractional frequency offset of the LocalClock relative to the TAI frequency **shall be within ± 100 ppm**.
    *   The granularity of the local clock must be **40 ns or better**.

### Management and Configuration Requirements

1.  **Management Mechanism:** The mandated management mechanism for the 802.1AS profile is the mechanism specified in Clause 14 (Data Sets) and Clause 15 (MIBs) of 802.1AS. This management information model is constructed as a set of managed objects defined in data sets, which are based on the core IEEE 1588 data sets (like `defaultDS`, `currentDS`, and `portDS`).
2.  **MIB Conformance:** Conformance to the required or optional capabilities in the current 802.1AS standard requires support of the **version 2 MIB module** (IEEE8021-AS-V2 MIB), which supersedes the version 1 MIB module from 802.1AS-2011.
3.  **Data Sets:** You must maintain the following PTP Instance data sets locally as the basis for protocol decisions: the **Default Parameter Data Set (defaultDS)**, the **Current Parameter Data Set (currentDS)**, and the **Parent Parameter Data Set (parentDS)**. You will also need to maintain the **Port Parameter Data Set (portDS)** for each PTP Port.
4.  **Security (Non-Use):** The 802.1AS profile specifies that the **security mechanism of 16.14 of IEEE Std 1588-2019 and security annex (Annex P) of IEEE Std 1588-2019 are not used**.

### Domain and Advanced Feature Requirements (Optional/Conditional)

1.  **Domain Numbering:** IEEE 802.1AS typically uses **Domain 0**.
    *   The BMCA implementation for Domain 0 must conform to specifications where the **`externalPortConfigurationEnabled` value is FALSE**.
    *   Support for additional PTP Instances with domain numbers in the range 1 to 127 is optional.
2.  **Common Mean Link Delay Service (CMLDS):** This service, specified in 16.6 of IEEE Std 1588-2019, is:
    *   **Required** if **more than one domain is implemented**.
    *   **Optional** if only one domain (Domain 0) is implemented.
3.  **External Port Configuration:** This feature (defined in 17.6 of IEEE Std 1588-2019) is **optional** in the 802.1AS profile.
4.  **Acceptable Master Table:** This feature is provided in 802.1AS (see Clause 14.7/14.11 in 802.1AS-2020), which is based on the IEEE 1588 acceptable master table mechanism (see 17.5 in 1588-2019). Note that IEEE Std 1588-2019 revised the definition, requiring acceptable masters to be identified by `portIdentity` rather than protocol address to correct potential flaws when Transparent Clocks are present. The 802.1AS management data sets rely on the `PortIdentity` approach for the acceptable master mechanism.

Integrating 802.1AS means conforming to a precise blueprint (the gPTP profile) built upon the massive toolbox that is IEEE 1588. You take the core PTP synchronization engine, but you restrict its operation to use peer-to-peer delay measurement, operate primarily in domain 0, and use a simplified set of clock states and management tools defined specifically within the 802.1AS standard, while explicitly excluding complex options like security or certain complex state machine behaviors defined in the broader IEEE 1588 standard.