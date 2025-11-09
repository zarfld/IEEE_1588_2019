The relationship between the various standards revolves around the evolution of the Precision Time Protocol (PTP) defined by the IEEE 1588 series, and the creation of the specialized profile for time-sensitive networking, known as generalized Precision Time Protocol (gPTP), defined by the IEEE 802.1AS series.
Here is a description of the relations and differences between the listed standards:
--------------------------------------------------------------------------------
I. IEEE 1588 Standards (PTP)
The IEEE 1588 standards define the core Precision Time Protocol (PTP), which generates a master-slave relationship among PTP Instances to synchronize clocks in networked measurement and control systems.
1. IEEE Std 1588-2002
• Foundation: This standard defined the initial PTP protocol for precise synchronization of clocks in measurement and control systems communicating over networks.
• Version: PTP messages conforming to this edition use a versionPTP field value of 1.
• Compatibility: Starting with the IEEE Std 1588-2019 edition, compatibility with IEEE Std 1588-2002 is no longer supported. However, implementations conformant to 1588-2002 can potentially interface with newer networks using implementation-specific translation means.
2. IEEE Std 1588-2019
• Revision: This standard is a revision of IEEE Std 1588-2008.
• Compatibility: PTP Instances conforming to 1588-2019 are generally compatible with implementations conforming to the older 1588-2008 edition, provided that options used are either compatible or mutually supported.
• Architectural Changes (Difference from earlier PTP versions): IEEE Std 1588-2019 introduced an architecture based on media-independent and media-dependent sublayers. However, this new architecture is optional for traditional transport methods like IPv4, IPv6, Ethernet LANs, and industrial automation control protocols; the previous 1588-2008 architecture is retained for these.
• Data Structure Updates: This edition changed how acceptable masters are identified, moving from the deprecated protocol address method to using portIdentity (to resolve issues when Transparent Clocks are present).
• Management: PTP management messages are specified in IEEE Std 1588-2019 and are used to access data set members and generate events.
--------------------------------------------------------------------------------
II. IEEE 802.1AS Standards (gPTP)
The IEEE 802.1AS standards, including the 2019/2021 versions, specify protocols, procedures, and managed objects for timing and synchronization in the context of IEEE 802 networks for time-sensitive applications (gPTP). They leverage IEEE 1588 specifications but add necessary specifications to meet synchronization requirements.
1. IEEE 802.1AS-2019 (Context for Industrial Use)
• This version is specifically referenced as the standard that shall apply for clock synchronization selection within the IEC/IEEE 60802 Industrial Automation profile.
• Requirements drawn from this context include needing synchronization for both universal time and working clock timescales.
• The minimal timestamp accuracy for sync and delay messages must be ≤ 8ns for universal time and working clock.
2. IEEE 802.1AS-2021 (ISO/IEC/IEEE 8802-1AS:2021, derived from IEEE Std 802.1AS-2020)
• Revision History: This document is the adoption of IEEE Std 802.1AS-2020, which is a revision of IEEE Std 802.1AS-2011. This profile is referred to as version 2.0 (primaryVersion 2, revisionNumber 0) relative to the 2011 version.
• Purpose: It specifies protocols for gPTP (generalized Precision Time Protocol) enabling systems to meet jitter, wander, and time-synchronization requirements for time-sensitive applications like audio, video, and control.
III. Key Relations and Differences (1588-2019 vs. 802.1AS-2021)
While gPTP (802.1AS) is a profile or adaptation of PTP (1588), there are several fundamental differences specified in the sources.
Feature
IEEE Std 1588-2019 (PTP)
IEEE Std 802.1AS-2021 (gPTP)
Communication Layer
Supports various Layer 2 and Layer 3-4 communication methods.
Assumes communication uses only IEEE 802 MAC PDUs and addressing.
PTP Architecture
Media-independent/media-dependent layers are optional for common transports (retains 1588-2008 architecture for IPv4, IPv6, Ethernet LANs).
Specifies mandatory media-independent and media-dependent sublayers to integrate different networking technologies.
Delay Mechanism (Full-Duplex)
Allows use of End-to-End (E2E) delay measurement.
Requires the use of the Peer-to-Peer (P2P) delay mechanism for full-duplex Ethernet links.
Relays
Allows the use of non-IEEE-1588-aware relays in a domain (though this slows timing convergence).
A gPTP domain consists ONLY of PTP Instances (PTP Relay Instances); non-PTP Relay Instances cannot be used to relay gPTP information.
Transparent Clocks
Specifies E2E and P2P Transparent Clocks.
Defines a PTP Relay Instance, which differs from a P2P Transparent Clock in 1588-2019 because the Relay Instance invokes the Best Master Clock Algorithm (BMCA) and has PTP Port states.
Management Protocol
Specifies PTP Management Messages.
PTP Management Messages are not used; uses mechanisms specified in Clause 14 and 15 for management.
Multiple Domains/CMLDS
Supports multiple domains (identified by domainNumber and sdoId).
Requires the Common Mean Link Delay Service (CMLDS) (specified in 1588-2019) if more than one domain is implemented, and it is optional if only domain 0 is implemented.
This dynamic relationship is typical in standardization, where PTP (1588) provides the foundation, and gPTP (802.1AS) acts like a highly specialized, tightly constrained tool tailored specifically for reliable synchronization within IEEE 802 networks.
--------------------------------------------------------------------------------
To clarify the difference between the foundational PTP standard and its specialized gPTP profile, consider them like maps: IEEE 1588 (PTP) is the comprehensive world atlas, providing detailed specifications and optional routes (Layer 2, Layer 3-4, E2E, P2P options) usable anywhere in the network universe. IEEE 802.1AS (gPTP) is a regional map (for IEEE 802 networks) that locks down specific routes (Layer 2 only) and highly precise measuring tools (P2P mechanism mandatory on full-duplex links) to ensure mission-critical timing is met efficiently within its specialized territory.