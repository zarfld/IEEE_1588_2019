This is a comprehensive overview of the evolution of the core Precision Time Protocol (PTP) defined by the 1588 series and its specialized profile, the Generalized Precision Time Protocol (gPTP), defined by the 802.1AS series, along with a clarification on the software implementation hierarchy.
Part I: Relations and Differences of Standards
The key relationship is that the 802.1AS series defines a strict, localized profile for PTP (Generalized Precision Time Protocol, or gPTP) by selecting mandatory and optional features of the core 1588 series.
A. Core PTP Standards (IEEE 1588 Series)
The IEEE 1588 standards define the fundamental protocol for precise clock synchronization in networked control and measurement systems.
Standard
Key Characteristics & Relations
Version
IEEE Std 1588-2002
This is the initial definition of PTP. Messages conformant to this edition use a versionPTP value of 1. Compatibility with its specifications is no longer supported starting with the 2019 edition, although non-standard translation means might allow interfacing older networks.
PTP Version 1
IEEE Std 1588-2008
This revision introduced PTP Version 2. It was the foundation for early specialized profiles, particularly the initial Audio Video Bridging (AVB) synchronization defined in IEEE 802.1AS-2011. It included management messages. Certain terms and structures (like non-IEEE EUI-64 clock identity creation) from this edition were dropped in 2019 due to potential uniqueness issues.
PTP Version 2
IEEE Std 1588-2019
This is a revision of 1588-2008. It maintained compatibility with 2008 implementations, provided options are mutually supported. Key Changes: It deprecated identifying acceptable masters by protocol address in favor of identifying them by PortIdentity to address flaws exposed by Transparent Clocks. It introduced a new, layered media-independent/media-dependent (MI/MD) architecture, but this architecture is optional for traditional transport methods like IPv4, IPv6, and Ethernet LANs. It explicitly dropped support for 1588-2002.
PTP Version 2
B. Specialized gPTP Standards (IEEE 802.1AS Series)
The IEEE 802.1AS standards define the Generalized PTP (gPTP) profile, ensuring synchronization for time-sensitive applications (TSN/AVB) within IEEE 802 networks. The fundamental goal is to leverage the work of the IEEE 1588 Working Group.
Standard
Key Characteristics & Relations
PTP Profile Version
IEEE Std 802.1AS-2011
The first edition of gPTP, establishing the core synchronization profile for AVB networks. It defined a profile for use of PTP (1588-2008) on enhanced Ethernet networks.
Version 1.0
802.1AS-2014
A specific standard revision named "802.1AS-2014" is not mentioned in the sources. However, the original 2011 standard received corrigenda around that time: 802.1AS-2011/Cor 1-2013 and 802.1AS-2011/Cor 2-2015, which provided technical and editorial corrections to the original version.
Version 1.0 (via Corrigenda)
IEEE Std 802.1AS-2020
A revision of 802.1AS-2011. It specifies protocols, procedures, and managed objects for timing and synchronization in Local and Metropolitan Area Networks. Key Additions: Support for multiple gPTP domains, Common Mean Link Delay Service (CMLDS) (required for multiple domains), and Fine Timing Measurement (FTM) for 802.11 transport.
Version 2.0
ISO/IEC/IEEE 8802-1AS:2021
This is the international adoption of IEEE Std 802.1AS-2020. It references the foundational PTP standard, IEEE Std 1588-2019, for its requirements. It specifies the use of P2P delay mechanism for full-duplex Ethernet links.
Version 2.0
802.1AS2021-corr1
This refers to Technical Corrigendum 1 to the ISO/IEC/IEEE 8802-1AS:2021 standard (which is identical to IEEE 802.1AS-2020). It provides technical and editorial corrections.
Version 2.0 (Correction)
C. Major Architectural Differences (gPTP vs. PTP Core)
The 802.1AS profile strictly dictates specific choices within the flexible 1588 framework:
Feature
IEEE 1588-2019 (PTP Core)
IEEE 802.1AS-2020/2021 (gPTP Profile)
Transport Protocol
Supports Layer 2 (Ethernet) and Layer 3/4 (IPv4/IPv6).
Assumes communication is done only using IEEE 802 MAC PDUs and addressing (Layer 2).
Network Architecture
New MI/MD architecture is optional for legacy IP and Ethernet LAN transports.
Mandatory use of the media-independent and media-dependent (MD) sublayers to integrate different networking technologies (802.3, 802.11, EPON).
Path Delay Mechanism
Supports both End-to-End (E2E) and Peer-to-Peer (P2P) delay mechanisms.
Requires P2P delay mechanism for full-duplex Ethernet links.
Management
Specifies PTP Management Messages in Clause 15.
PTP Management Messages are not used. Management relies on its own mechanisms specified in Clause 14 and 15 of 802.1AS-2020.
Time Domain
Supports various domain IDs (domainNumber and sdoId).
Uses specific profile isolation (sdoId 0x100). Requires the Common Mean Link Delay Service (CMLDS) if more than one gPTP domain is implemented.
Frame Tagging
Supports various transports, including those using VLAN tags.
Frames carrying 802.1AS messages are neither VLAN-tagged nor priority-tagged (i.e., they are untagged).
--------------------------------------------------------------------------------
Part II: SW Layering Implementation
Your query asks: "If I would think about SW-layers implementation: 802.1AS will sit on top of 1588?"
The correct interpretation is that 802.1AS (gPTP) does not sit on top of PTP (1588) but rather defines the specific internal structure and operational constraints for a PTP implementation focused on IEEE 802 networks.
1. gPTP is a Profile, not a Layer: IEEE Std 802.1AS defines a PTP profile. A profile dictates the selection of features, options, attribute values, and implementation guidelines for the underlying protocol (PTP). Therefore, gPTP is the specialized manifestation of the generic PTP engine for a specific environment (TSN/AVB).
2. Shared Core Logic: The core synchronization concepts, data sets (like clockClass or timeSource), and computational formulas are derived from IEEE 1588-2019. For example, the computations for CMLDS defined in 802.1AS must be consistent with IEEE Std 1588-2019. This means the "software kernel" or foundational mathematics comes from the PTP standard.
3. Mandatory Layered Architecture: The 802.1AS standard implements the protocol logic using Media-Independent (MI) and Media-Dependent (MD) sublayers, a feature which is mandatory in 802.1AS but was only optional or retained as legacy architecture in PTP 1588-2019. The gPTP profile forces the PTP software entities to communicate using defined service primitives across the MI/MD interfaces, making it tightly coupled to the underlying physical media (MAC/PHY) (e.g., using MLME primitives for 802.11).
Analogy: If PTP (1588) is a generic operating system kernel capable of running on many types of hardware (Layer 2 or Layer 3/4), gPTP (802.1AS) is a hardened, custom build of that kernel designed exclusively for a specific set of hardware (IEEE 802 MACs). It replaces the standard network drivers and management tools with components specifically designed for real-time performance, but it fundamentally relies on the core OS architecture provided by PTP.
