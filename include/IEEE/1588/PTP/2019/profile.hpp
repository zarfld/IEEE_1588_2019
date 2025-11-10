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
 *
 * @req REQ-F-201 Profile Strategy Selection (gPTP, Industrial, AES67)
 */

#ifndef IEEE_1588_PTP_2019_PROFILE_HPP
#define IEEE_1588_PTP_2019_PROFILE_HPP

#include <cstdint>
#include <string>

// Include clocks.hpp for PortConfiguration
#include "../../../../clocks.hpp"

namespace IEEE {
namespace _1588 {
namespace _2019 {

/**
 * @enum DelayMechanism
 * @brief Enumerates supported path delay mechanisms.
 */
enum class DelayMechanism : uint8_t {
    E2E,          ///< End-to-End delay request-response mechanism (IEEE 1588-2019 default)
    P2P,          ///< Peer-to-Peer delay mechanism (802.1AS mandatory, Power profile)
    PeerToPeer = P2P,   ///< Alias for backward compatibility
    EndToEnd = E2E      ///< Alias for backward compatibility
};

/**
 * @enum PTPProfile
 * @brief IEEE 1588-2019 Annex I profile types
 */
enum class PTPProfile : uint8_t {
    DEFAULT_PROFILE,  ///< Default PTP profile (Annex I.2) - E2E delay mechanism
    POWER_PROFILE,    ///< Power profile (Annex I.3) - P2P delay for power utility systems
    CUSTOM_PROFILE    ///< User-defined custom profile
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

/**
 * @brief Simple Result type for profile operations
 */
struct ProfileResult {
    bool success;
    
    bool is_success() const { return success; }
    
    static ProfileResult ok() { return ProfileResult{true}; }
    static ProfileResult fail() { return ProfileResult{false}; }
};

/**
 * @struct ProfileConfiguration
 * @brief IEEE 1588-2019 Annex I/J profile configuration parameters
 * 
 * Defines complete profile configuration per IEEE 1588-2019 Annex I (Default and Power profiles)
 * and Annex J (profile template). Each profile specifies timing parameters, delay mechanisms,
 * and operational constraints for specific application domains.
 * 
 * @see IEEE 1588-2019, Annex I.2 "Default PTP profile"
 * @see IEEE 1588-2019, Annex I.3 "Power profile"
 * @see IEEE 1588-2019, Annex J "PTP profile template"
 */
struct ProfileConfiguration {
    PTPProfile profile_type;          ///< Profile type (Default, Power, Custom)
    DelayMechanism delay_mechanism;   ///< Delay mechanism (E2E or P2P)
    uint8_t domain_number_min;        ///< Minimum domain number allowed
    uint8_t domain_number_max;        ///< Maximum domain number allowed
    uint8_t network_protocol;         ///< Network protocol (0=UDP/IPv4, 1=UDP/IPv6, 2=IEEE 802.3)
    int8_t announce_interval;         ///< Announce interval (log2 seconds, range -3 to 4)
    int8_t sync_interval;             ///< Sync interval (log2 seconds, range -7 to 4)
    int8_t delay_req_interval;        ///< Delay_Req interval for E2E (log2 seconds)
    int8_t pdelay_req_interval;       ///< Pdelay_Req interval for P2P (log2 seconds)
    uint8_t announce_receipt_timeout; ///< Announce receipt timeout (2-255, typically 3)

    /**
     * @brief Validate profile configuration parameters
     * @return ProfileResult indicating validation success or failure
     * 
     * Validates:
     * - Delay mechanism consistency (E2E uses delay_req, P2P uses pdelay_req)
     * - Domain number range validity (0-127 per IEEE 1588-2019)
     * - Message interval ranges per specification
     * - Announce receipt timeout >= 2 per specification
     */
    ProfileResult validate() const {
        // Validate domain numbers
        if (domain_number_max > 127) return ProfileResult::fail();
        if (domain_number_min > domain_number_max) return ProfileResult::fail();

        // Validate announce receipt timeout
        if (announce_receipt_timeout < 2) return ProfileResult::fail();

        // Validate message intervals
        if (announce_interval < -3 || announce_interval > 4) return ProfileResult::fail();
        if (sync_interval < -7 || sync_interval > 4) return ProfileResult::fail();
        if (delay_req_interval < -7 || delay_req_interval > 4) return ProfileResult::fail();
        if (pdelay_req_interval < -7 || pdelay_req_interval > 4) return ProfileResult::fail();

        // Validate delay mechanism consistency
        if (delay_mechanism == DelayMechanism::E2E) {
            // E2E should use delay_req, not pdelay_req
            if (profile_type == PTPProfile::POWER_PROFILE) return ProfileResult::fail(); // Power profile must use P2P
        } else if (delay_mechanism == DelayMechanism::P2P) {
            // P2P should use pdelay_req, not delay_req
            if (profile_type == PTPProfile::DEFAULT_PROFILE) return ProfileResult::fail(); // Default profile must use E2E
        }

        return ProfileResult::ok();
    }
};

/**
 * @brief Get IEEE 1588-2019 Annex I.2 Default PTP profile configuration
 * @return ProfileConfiguration for Default profile
 * 
 * Default profile characteristics per IEEE 1588-2019 Annex I.2:
 * - Delay mechanism: End-to-End (E2E)
 * - Domain numbers: 0-127 (all domains allowed)
 * - Network protocol: Any (UDP/IPv4, UDP/IPv6, or IEEE 802.3)
 * - Announce interval: 1 (2 seconds)
 * - Sync interval: 0 (1 second)
 * - Delay_Req interval: 0 (1 second)
 * - Announce receipt timeout: 3
 * 
 * @see IEEE 1588-2019, Annex I.2 "Default PTP profile"
 */
inline ProfileConfiguration get_default_profile() {
    ProfileConfiguration config;
    config.profile_type = PTPProfile::DEFAULT_PROFILE;
    config.delay_mechanism = DelayMechanism::E2E;
    config.domain_number_min = 0;
    config.domain_number_max = 127;
    config.network_protocol = 0; // UDP/IPv4 (can be any)
    config.announce_interval = 1;  // 2 seconds
    config.sync_interval = 0;      // 1 second
    config.delay_req_interval = 0; // 1 second
    config.pdelay_req_interval = 0; // Not used in E2E
    config.announce_receipt_timeout = 3;
    return config;
}

/**
 * @brief Get IEEE 1588-2019 Annex I.3 Power profile configuration
 * @return ProfileConfiguration for Power profile
 * 
 * Power profile characteristics per IEEE 1588-2019 Annex I.3:
 * - Delay mechanism: Peer-to-Peer (P2P)
 * - Domain number: 0 only (restricted for power utility systems)
 * - Network protocol: IEEE 802.3 (Ethernet Layer 2)
 * - Announce interval: 1 (2 seconds)
 * - Sync interval: -4 (16 messages per second, 62.5ms period)
 * - Pdelay_Req interval: 0 (1 second)
 * - Announce receipt timeout: 3
 * 
 * @see IEEE 1588-2019, Annex I.3 "Power profile"
 * @see IEC 61850-9-3 for power utility time synchronization requirements
 */
inline ProfileConfiguration get_power_profile() {
    ProfileConfiguration config;
    config.profile_type = PTPProfile::POWER_PROFILE;
    config.delay_mechanism = DelayMechanism::P2P;
    config.domain_number_min = 0;
    config.domain_number_max = 0; // Power profile: domain 0 only
    config.network_protocol = 2; // IEEE 802.3 (Ethernet)
    config.announce_interval = 1;  // 2 seconds
    config.sync_interval = -4;     // 16 messages/sec (62.5ms)
    config.delay_req_interval = 0; // Not used in P2P
    config.pdelay_req_interval = 0; // 1 second
    config.announce_receipt_timeout = 3;
    return config;
}

/**
 * @brief Validate profile parameters against IEEE 1588-2019 constraints
 * @param config Profile configuration to validate
 * @return ProfileResult indicating validation success or failure
 * 
 * Performs comprehensive validation:
 * - Calls ProfileConfiguration::validate() for basic checks
 * - Verifies profile-specific constraints (Default vs Power)
 * - Validates delay mechanism matches profile requirements
 * 
 * @see IEEE 1588-2019, Annex I "PTP profiles"
 * @see IEEE 1588-2019, Annex J "PTP profile template"
 */
inline ProfileResult validate_profile_parameters(const ProfileConfiguration& config) {
    return config.validate();
}

} // namespace _2019
} // namespace _1588
} // namespace IEEE

/**
 * @brief Apply IEEE 1588-2019 profile configuration to PortConfiguration
 * @param port_config Target PortConfiguration to apply profile to
 * @param profile Source ProfileConfiguration to apply
 * @return ProfileResult indicating success or failure
 * 
 * Free function outside IEEE namespace to access Clocks::PortConfiguration.
 * Copies profile parameters to PortConfiguration structure:
 * - Sets delay_mechanism_p2p based on profile's delay mechanism
 * - Copies message intervals (announce, sync, delay_req)
 * - Validates profile parameters before applying
 * 
 * @see IEEE 1588-2019, Section 7.6.2.3 "logMessageInterval"
 * @see IEEE 1588-2019, Section 8.2.17 "portDS.logMinDelayReqInterval"
 * @see IEEE 1588-2019, Section 8.2.18 "portDS.logMinPdelayReqInterval"
 */
inline IEEE::_1588::_2019::ProfileResult apply_profile(IEEE::_1588::PTP::_2019::Clocks::PortConfiguration& port_config, 
                                                      const IEEE::_1588::_2019::ProfileConfiguration& profile) {
    // Validate profile before applying
    auto validation_result = profile.validate();
    if (!validation_result.is_success()) {
        return validation_result;
    }

    // Apply delay mechanism
    port_config.delay_mechanism_p2p = (profile.delay_mechanism == IEEE::_1588::_2019::DelayMechanism::P2P);

    // Apply message intervals
    port_config.announce_interval = profile.announce_interval;
    port_config.sync_interval = profile.sync_interval;
    port_config.delay_req_interval = profile.delay_req_interval;
    port_config.announce_receipt_timeout = profile.announce_receipt_timeout;

    return IEEE::_1588::_2019::ProfileResult::ok();
}

#endif // IEEE_1588_PTP_2019_PROFILE_HPP
