This query addresses the evolution of the core Precision Time Protocol (PTP) defined by the IEEE 1588 series and the subsequent development of the specialized profile known as generalized Precision Time Protocol (gPTP) defined by the IEEE 802.1AS series for use in local area networks (LANs) and Time-Sensitive Networking (TSN).
Here is a detailed description of the relations and differences among the specified standards:
--------------------------------------------------------------------------------
I. IEEE 1588 Standards (Precision Time Protocol - PTP)
The IEEE 1588 series defines the foundational PTP used to synchronize clocks in networked measurement and control systems.
1. IEEE Std 1588-2002
• Version: This is the first edition of PTP. PTP messages conforming to this edition use a versionPTP field value of 1.
• Relation to newer PTP: Compatibility with the specifications of IEEE Std 1588-2002 is no longer supported starting with the 2019 edition. However, non-standard means might exist to interface networks conforming to 1588-2002 with newer PTP Instances.
2. IEEE Std 1588-2008
• Version: This is the successor and revised standard to the 2002 version. It is often referred to as PTP version 2.
• Reference Point: Many associated standards, such as early Audio Video Bridging (AVB) specifications like the profile defined in AES 67-2018, explicitly referenced or were profiles of IEEE 1588-2008. The architecture defined in this edition (which does not rely on media-independent/dependent layers) is retained as an option in the 2019 revision for traditional transports like IPv4, IPv6, and Ethernet LANs.
• Compatibility Note: The methods for determining the uniqueness of the clockIdentity values used in the 2008 edition (specifically relating to Non-IEEE EUI-64 clockIdentity values and using EUI-48 to create EUI-64) are not part of the 2019 edition because they could not guarantee the required uniqueness property.
3. IEEE Std 1588-2019
• Revision and Compatibility: This standard is a revision of IEEE Std 1588-2008. Implementations conformant to 2019 are generally compatible with those conformant to 2008, provided that common options are enabled and similarly configured.
• Major Differences from 2008:
    ◦ Architecture: It introduces a new architecture based on media-independent (MI) and media-dependent (MD) sublayers, but this is optional for traditional transports (IPv4, IPv6, Ethernet LANs).
    ◦ Master Identification: It addresses a potential flaw in 2008 by deprecating the identification of acceptable masters by protocol address and changing it to identification by PortIdentity. This is crucial when Transparent Clocks are present.
    ◦ New Options: It defines new options and features for enhanced synchronization performance (e.g., L1 based synchronization performance enhancement in Annex L and configurable correction of timestamps in Clause 16.7/16.8), which were not in 2008.
--------------------------------------------------------------------------------
II. IEEE 802.1AS Standards (Generalized PTP - gPTP)
The IEEE 802.1AS series defines the gPTP profile used specifically for IEEE 802 networks (LANs/TSN) to meet strict synchronization, jitter, and wander requirements for time-sensitive applications.
1. IEEE Std 802.1AS-2011 (The Foundation for AVB/TSN)
• This was the initial standard defining gPTP, widely used in AVB systems.
• PTP Profile Version: The PTP profile included in 802.1AS-2011 was Version 1.0.
• Corrigenda (Related to 2014): Standards like the Milan Specification reference technical corrections made via corrigenda to the 2011 edition, specifically IEEE Std 802.1AS-2011/Cor 1-2013 and IEEE Std 802.1AS-2011/Cor 2-2015. (Note: No specific standard named IEEE 802.1AS-2014 was found in the sources, but these corrigenda provided updates to the core 2011 standard around that time.)
2. IEEE Std 802.1AS-2020 / ISO/IEC/IEEE 8802-1AS:2021
• Revision: IEEE Std 802.1AS-2020 is a revision of IEEE Std 802.1AS-2011. The text is published internationally as ISO/IEC/IEEE 8802-1AS:2021.
• PTP Profile Version: This revision changes the profile version to Version 2.0 (primaryVersion 2, revisionNumber 0), reflecting major changes compared to Version 1.0.
• Key Additions/Changes (from 2011):
    ◦ It added support for multiple gPTP domains.
    ◦ It introduced Common Mean Link Delay Service (CMLDS) (required if more than one domain is implemented, optional for single domain 0). The computations for CMLDS must be consistent with IEEE Std 1588-2019.
    ◦ It added support for Fine Timing Measurement for 802.11 transport.
    ◦ It maintains backward compatibility with IEEE Std 802.1AS-2011.
• Management: This version defines its management scheme in Clause 14 and Clause 15. Notably, PTP Management Messages specified in IEEE Std 1588-2019 are explicitly not used in this standard.
3. ISO/IEC/IEEE 8802-1AS:2021/Cor.1:2023
• Nature: This is Technical Corrigendum 1 (Cor 1:2023) to the ISO/IEC/IEEE 8802-1AS:2021 edition (which is identical to the IEEE 802.1AS-2020 publication).
• Purpose: It provides technical and editorial corrections to IEEE Std 802.1AS-2020.
--------------------------------------------------------------------------------
III. Core Relations and Differences (1588 vs. 802.1AS)
The fundamental difference lies in the scope and strictness of the implementation requirements:
Feature
IEEE 1588-2019 (PTP Core)
IEEE 802.1AS-2020/2021 (gPTP Profile)
Application Scope
General precision synchronization for measurement and control systems, supporting diverse networking technologies.
Specialized profile for synchronization in Time-Sensitive Applications (e.g., AVB/TSN), strictly confined to IEEE 802 networks.
Transport Layer
Supports various Layer 2 and Layer 3/4 communication methods (e.g., IPv4, IPv6, Ethernet LANs).
Assumes all communication uses only IEEE 802 MAC PDUs and addressing. Frames carrying 802.1AS messages are neither VLAN-tagged nor priority-tagged.
Architecture
New MI/MD architecture is optional for traditional transports.
Explicitly specifies and requires the use of the media-independent and media-dependent sublayers to integrate different technologies (like 802.3, 802.11, EPON, CSN).
Delay Mechanism
Supports End-to-End (E2E) delay measurement. Peer-to-Peer (P2P) delay is optional, usually associated with Transparent Clocks.
For full-duplex links, the path delay mechanism must be Peer-to-Peer (Clause 11).
Management
Specifies PTP Management Messages (Clause 15).
PTP Management Messages are not used. Management relies on mechanisms specified in its own standard (Clause 14 and Clause 15 of 802.1AS-2020).
Relay Definition
Defines Boundary Clocks (BCs) and Transparent Clocks (TCs). TCs typically do not participate in the Best Master Clock Algorithm (BMCA). Non-PTP aware relays can exist in a domain.
Defines PTP Relay Instances, which differ from P2P Transparent Clocks in 1588-2019 because the Relay Instance invokes the BMCA and has PTP Port states. A gPTP domain consists ONLY of PTP Instances; non-PTP relays cannot be used to relay gPTP information.
--------------------------------------------------------------------------------
Analogy
If IEEE 1588-2019 is like a robust, multi-tool Swiss Army Knife, capable of handling time synchronization over almost any network connection (Layer 2 or Layer 3/4, using various delay mechanisms), IEEE 802.1AS-2021 (gPTP) is a dedicated, precision-machined stopwatch, engineered solely to function optimally and predictably within the constraints of an Ethernet-based (IEEE 802) toolkit, ensuring that highly demanding applications like industrial control or media transport never miss a beat.