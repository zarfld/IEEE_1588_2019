/**
 * @file profile.hpp
 * @brief Generic PTP profile abstraction supporting multiple external standards (IEEE 1588-2019 core, IEEE 802.1AS gPTP, IEC/IEEE 60802, AES67 media profile).
 *
 * Provides a hardware-agnostic configurational layer selecting protocol feature sets
 * without duplicating implementation logic. This enables compilation/runtime selection
 * of profile constraints (delay mechanism, management model, mandatory TLVs, domains).
 *
 * References (section numbers only; no reproduction of copyrighted text):
 * - IEEE 1588-2019 (Sections 8,11,13,16,17)
 * - IEEE 802.1AS-2020 (Clauses 8,10,11,14,15)
 * - IEC/IEEE 60802 (industrial profile tables)
 * - AES67-2018 (PTP profile over UDP/IPv4)
 */

#ifndef IEEE_1588_PTP_2019_PROFILE_HPP
#define IEEE_1588_PTP_2019_PROFILE_HPP

#include <cstdint>
#include <string>

namespace IEEE {
namespace _1588 {
namespace _2019 {

/**
 * @enum DelayMechanism
 * @brief Enumerates supported path delay mechanisms.
 */
enum class DelayMechanism : uint8_t {
    PeerToPeer,   ///< P2P mechanism (802.1AS mandatory on full-duplex 802.3)
    EndToEnd      ///< Request-Response mechanism (IEEE 1588 default, AES67 mandatory)
};

/**
 * @enum ManagementModel
 * @brief Enumerates management approaches.
 */
enum class ManagementModel : uint8_t {
    PtpMessages,      ///< IEEE 1588 Management messages (Clause 15)
    DataSetsMib       ///< 802.1AS Clause 14/15 data sets + MIB (no 1588 mgmt messages)
};

/**
 * @struct ProfileConfig
 * @brief Feature flag configuration describing a PTP profile selection.
 */
struct ProfileConfig {
    std::string name;                 ///< Human-readable profile identifier
    DelayMechanism delayMechanism;    ///< Selected delay mechanism
    ManagementModel management;       ///< Management model selection
    bool pathTraceMandatory;          ///< Path Trace TLV always enabled
    bool securityEnabled;             ///< Integrated security (Annex P) active
    bool multiDomainSupport;          ///< Supports >1 domain instances
    bool cmldsRequired;               ///< CMLDS mandatory (industrial multi-domain)
    bool externalPortConfigAllowed;   ///< External port state configuration feature
    bool oneStepTxOptional;           ///< One-step transmit mode optionally selectable
    bool asymmetryCompensationOptional; ///< Asymmetry compensation measurement available
    bool pauseDisallowed;             ///< MAC Control PAUSE must be disabled
    bool pfcDisallowed;               ///< Priority Flow Control must be disabled
    bool eeeDisableCapability;        ///< Capability to disable Energy Efficient Ethernet
    bool usesUdpTransport;            ///< Uses UDP/IPv4 transport (AES67)
    bool framesUntagged;              ///< Frames must be untagged (802.1AS requirement)
};

/**
 * @brief Factory helpers for standard profiles.
 */
struct ProfileFactory {
    static ProfileConfig Gptp8021AS() {
        return ProfileConfig{
            "gPTP-802.1AS",
            DelayMechanism::PeerToPeer,
            ManagementModel::DataSetsMib,
            /*pathTraceMandatory*/ true,
            /*securityEnabled*/ false,
            /*multiDomainSupport*/ false, // domain 0 only baseline
            /*cmldsRequired*/ false,
            /*externalPortConfigAllowed*/ false,
            /*oneStepTxOptional*/ true,
            /*asymmetryCompensationOptional*/ true,
            /*pauseDisallowed*/ true,
            /*pfcDisallowed*/ true,
            /*eeeDisableCapability*/ false,
            /*usesUdpTransport*/ false,
            /*framesUntagged*/ true
        };
    }
    static ProfileConfig Industrial60802() {
        return ProfileConfig{
            "TSN-Industrial-60802",
            DelayMechanism::PeerToPeer,
            ManagementModel::DataSetsMib,
            /*pathTraceMandatory*/ true,
            /*securityEnabled*/ false, // integrated security excluded; external security separate
            /*multiDomainSupport*/ true,
            /*cmldsRequired*/ true,
            /*externalPortConfigAllowed*/ false,
            /*oneStepTxOptional*/ true,
            /*asymmetryCompensationOptional*/ true,
            /*pauseDisallowed*/ true,
            /*pfcDisallowed*/ true,
            /*eeeDisableCapability*/ true,
            /*usesUdpTransport*/ false,
            /*framesUntagged*/ true
        };
    }
    static ProfileConfig AES67Media() {
        return ProfileConfig{
            "AES67-Media",
            DelayMechanism::EndToEnd,
            ManagementModel::PtpMessages,
            /*pathTraceMandatory*/ false,
            /*securityEnabled*/ false,
            /*multiDomainSupport*/ true,
            /*cmldsRequired*/ false,
            /*externalPortConfigAllowed*/ false,
            /*oneStepTxOptional*/ true,
            /*asymmetryCompensationOptional*/ false,
            /*pauseDisallowed*/ false,
            /*pfcDisallowed*/ false,
            /*eeeDisableCapability*/ false,
            /*usesUdpTransport*/ true,
            /*framesUntagged*/ false
        };
    }
};

} // namespace _2019
} // namespace _1588
} // namespace IEEE

#endif // IEEE_1588_PTP_2019_PROFILE_HPP
