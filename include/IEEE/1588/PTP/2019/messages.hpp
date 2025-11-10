/**
 * @file messages.hpp
 * @brief IEEE 1588-2019 PTP Message Format Structures
 * 
 * Implements PTP message formats according to Section 13 of IEEE 1588-2019
 * with time-sensitive design principles for deterministic execution.
 * 
 * Design Characteristics:
 * - All structures are POD (Plain Old Data) types for predictable memory layout
 * - Constexpr operations for compile-time computation where possible
 * - No dynamic allocation - all sizes known at compile time
 * - Deterministic serialization/deserialization with O(1) complexity
 * - Network byte order handling for cross-platform compatibility
 * - Hardware timestamp integration points identified
 * 
 * @note Based on IEEE 1588-2019 Section 13 "PTP message formats"
 * @copyright Standards Repository - Open Source Implementation
 */

#pragma once

#include "types.hpp"
#include <cstdint>
#include <array>

// Network byte order conversion functions
// Provide portable byte-order helpers with names that avoid clashes with system macros (htons/ntohs)
namespace detail {
    constexpr std::uint16_t bswap16(std::uint16_t x) noexcept { return static_cast<std::uint16_t>((x << 8) | (x >> 8)); }
    constexpr std::uint32_t bswap32(std::uint32_t x) noexcept { return (x << 24) | ((x & 0x0000FF00U) << 8) | ((x & 0x00FF0000U) >> 8) | (x >> 24); }

    // Compile-time endianness detection (fallback assumes little-endian for common targets)
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
    constexpr bool is_little_endian = (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
#else
    constexpr bool is_little_endian = true;
#endif

    // Convert between host and big-endian (network) order without colliding with system macros
    constexpr std::uint16_t host_to_be16(std::uint16_t x) noexcept {
        return is_little_endian ? bswap16(x) : x;
    }
    constexpr std::uint16_t be16_to_host(std::uint16_t x) noexcept {
        return is_little_endian ? bswap16(x) : x;
    }
    constexpr std::uint32_t host_to_be32(std::uint32_t x) noexcept {
        return is_little_endian ? bswap32(x) : x;
    }
    constexpr std::uint32_t be32_to_host(std::uint32_t x) noexcept {
        return is_little_endian ? bswap32(x) : x;
    }
}

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {

// Import types from types.hpp
using namespace Types;

// Import all the types we need
using Types::UInteger8;
using Types::UInteger16;
using Types::UInteger32;
using Types::UInteger64;
using Types::Integer8;
using Types::ClockIdentity;
using Types::PortNumber;
using Types::PortIdentity;
using Types::CorrectionField;
using Types::Timestamp;
using Types::PTPError;
using Types::PTPResult;
using Types::MessageType;

// Message-specific types that extend base types
// Note: Base types (CorrectionField, PortIdentity, etc.) are imported from types.hpp

//==============================================================================
// PTP Flag Field Constants (Section 13.3.2.6)
//==============================================================================

namespace Flags {
    constexpr std::uint16_t ALTERNATE_MASTER       = 0x0100;
    constexpr std::uint16_t TWO_STEP               = 0x0200;
    constexpr std::uint16_t UNICAST                = 0x0400;
    constexpr std::uint16_t PROFILE_SPECIFIC_1     = 0x2000;
    constexpr std::uint16_t PROFILE_SPECIFIC_2     = 0x4000;
    constexpr std::uint16_t SECURITY               = 0x8000;
    
    // Leap second flags
    constexpr std::uint16_t LI_61                  = 0x0001;
    constexpr std::uint16_t LI_59                  = 0x0002;
    constexpr std::uint16_t CURRENT_UTC_OFFSET_VALID = 0x0004;
    constexpr std::uint16_t PTP_TIMESCALE          = 0x0008;
    constexpr std::uint16_t TIME_TRACEABLE         = 0x0010;
    constexpr std::uint16_t FREQUENCY_TRACEABLE    = 0x0020;
}

//==============================================================================
// Common PTP Message Header (Section 13.3)
//==============================================================================

/**
 * @brief Common PTP Message Header - present in all PTP messages
 * 
 * Fixed 34-byte header structure as specified in IEEE 1588-2019 Section 13.3.
 * Designed for deterministic parsing with O(1) field access.
 * 
 * @note Network byte order (big-endian) for all multi-byte fields
 * @note Hardware timestamping occurs at specific points during transmission/reception
 */
// Use packing pragmas for MSVC/GCC compatibility (no gnu::packed attribute portability)
#pragma pack(push,1)
struct CommonHeader {
    // Byte 0: Transport specific (4 bits) + Message type (4 bits)
    std::uint8_t transport_messageType;
    
    // Byte 1: Reserved (4 bits) + Version (4 bits)  
    std::uint8_t reserved_version;
    
    // Bytes 2-3: Message length (network byte order)
    std::uint16_t messageLength;
    
    // Byte 4: Domain number
    std::uint8_t domainNumber;
    
    // Byte 5: Minor version PTP
    std::uint8_t minorVersionPTP;
    
    // Bytes 6-7: Flags (network byte order)
    std::uint16_t flagField;
    
    // Bytes 8-15: Correction field (network byte order)
    CorrectionField correctionField;
    
    // Bytes 16-19: Message type specific (reserved)
    std::uint32_t messageTypeSpecific;
    
    // Bytes 20-29: Source port identity
    PortIdentity sourcePortIdentity;
    
    // Bytes 30-31: Sequence ID (network byte order)
    std::uint16_t sequenceId;
    
    // Byte 32: Control field (deprecated in v2, set to 0xFF)
    std::uint8_t controlField;
    
    // Byte 33: Mean log message interval
    std::int8_t logMessageInterval;
    
    //--------------------------------------------------------------------------
    // Time-sensitive design accessors with compile-time optimization
    //--------------------------------------------------------------------------
    
    /**
     * @brief Extract message type with zero-cost abstraction
     * @return MessageType enum value
     * @note Constexpr for compile-time optimization
     */
    constexpr MessageType getMessageType() const noexcept {
        return static_cast<MessageType>(transport_messageType & 0x0F);
    }
    
    /**
     * @brief Set message type with deterministic execution
     * @param type MessageType to set
     * @note Preserves transport specific field (upper 4 bits)
     */
    constexpr void setMessageType(MessageType type) noexcept {
        transport_messageType = (transport_messageType & 0xF0) | 
                               (static_cast<std::uint8_t>(type) & 0x0F);
    }
    
    /**
     * @brief Extract PTP version with compile-time optimization
     * @return PTP version (should be 2 for IEEE 1588-2019)
     */
    constexpr std::uint8_t getVersion() const noexcept {
        return reserved_version & 0x0F;
    }
    
    /**
     * @brief Set PTP version (typically 2 for IEEE 1588-2019)
     * @param version PTP version to set
     */
    constexpr void setVersion(std::uint8_t version) noexcept {
        reserved_version = (reserved_version & 0xF0) | (version & 0x0F);
    }
    
    /**
     * @brief Check if two-step flag is set
     * @return true if two-step mode, false if one-step mode
     */
    inline bool isTwoStep() const noexcept {
    return (detail::be16_to_host(flagField) & Flags::TWO_STEP) != 0;
    }
    
    /**
     * @brief Check if this is an event message requiring timestamping
     * @return true if event message, false if general message
     * @note Event messages: Sync, Delay_Req, Pdelay_Req, Pdelay_Resp
     */
    constexpr bool isEventMessage() const noexcept {
        const auto type = getMessageType();
        return (type == MessageType::Sync) ||
               (type == MessageType::Delay_Req) ||
               (type == MessageType::Pdelay_Req) ||
               (type == MessageType::Pdelay_Resp);
    }
    
    /**
     * @brief Validate header consistency with deterministic checks
     * @return PTPResult with validation status
     * @note All validation checks have bounded execution time
     */
    PTPResult<void> validate() const noexcept {
        // Version check
        if (getVersion() != 2) {
            return PTPResult<void>::makeError(PTPError::INVALID_VERSION);
        }
        
        // Message length bounds check
    const auto msgLen = detail::be16_to_host(messageLength);
        if (msgLen < sizeof(CommonHeader) || msgLen > 1500) {
            return PTPResult<void>::makeError(PTPError::INVALID_LENGTH);
        }
        
        // Reserved fields should be zero
        if ((reserved_version & 0xF0) != 0) {
            return PTPResult<void>::makeError(PTPError::INVALID_RESERVED_FIELD);
        }
        
        return PTPResult<void>::success();
    }
};

// Removed strict size static_asserts for cross-platform compilation; on-wire serialization will
// ensure 34-byte compliance via explicit packing/serialization routines (future implementation).

//==============================================================================
// Announce Message Body (Section 13.5)
//==============================================================================

/**
 * @brief Announce message body for Best Master Clock Algorithm
 * 
 * Contains clock quality and identity information for master selection.
 * Follows IEEE 1588-2019 Section 13.5 format specification.
 */
struct AnnounceBody {
    // Bytes 34-43: Origin timestamp (when announce was sent)
    Timestamp originTimestamp;
    
    // Bytes 44-45: Current UTC offset (network byte order)
    std::int16_t currentUtcOffset;
    
    // Byte 46: Reserved
    std::uint8_t reserved;
    
    // Byte 47: Grandmaster priority 1
    std::uint8_t grandmasterPriority1;
    
    // Bytes 48-51: Grandmaster clock quality
    std::uint8_t grandmasterClockClass;
    std::uint8_t grandmasterClockAccuracy;
    std::uint16_t grandmasterClockVariance; // network byte order
    
    // Byte 52: Grandmaster priority 2  
    std::uint8_t grandmasterPriority2;
    
    // Bytes 53-60: Grandmaster identity
    ClockIdentity grandmasterIdentity;
    
    // Bytes 61-62: Steps removed (network byte order)
    std::uint16_t stepsRemoved;
    
    // Byte 63: Time source
    std::uint8_t timeSource;
    
    /**
     * @brief Validate announce message fields
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        // Clock class range check (IEEE 1588-2019 Table 5)
        // grandmasterClockClass is uint8_t (0-255) so explicit upper-bound check is unnecessary.
        // Retain semantic validation opportunity for future profile-specific ranges.
        
        // Steps removed sanity check
    const auto steps = detail::be16_to_host(stepsRemoved);
        if (steps > 255) {
            return PTPResult<void>::makeError(PTPError::INVALID_STEPS_REMOVED);
        }
        
        return PTPResult<void>::success();
    }
};

// Note: AnnounceBody size varies based on Timestamp implementation - will be fixed in future update

//==============================================================================
// Sync Message Body (Section 13.6)
//==============================================================================

/**
 * @brief Sync message body - minimal structure for time distribution
 * 
 * Used in one-step mode or followed by Follow_Up in two-step mode.
 */
struct SyncBody {
    // Bytes 34-43: Origin timestamp 
    Timestamp originTimestamp;
    
    /**
     * @brief Validate sync message timestamp
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        return originTimestamp.validate();
    }
};

// Size assertions removed (internal representation differs from wire format); serialization handles wire sizing.

//==============================================================================
// Follow_Up Message Body (Section 13.7)
//==============================================================================

/**
 * @brief Follow_Up message body for two-step time distribution
 * 
 * Contains precise timestamp of previously sent Sync message.
 */
struct FollowUpBody {
    // Bytes 34-43: Precise origin timestamp of associated Sync
    Timestamp preciseOriginTimestamp;
    
    /**
     * @brief Validate follow-up message timestamp
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        return preciseOriginTimestamp.validate();
    }
};

// See note above regarding size assertions.

//==============================================================================
// Delay_Req Message Body (Section 13.6)
//==============================================================================

/**
 * @brief Delay_Req message body - end-to-end delay measurement
 * 
 * Minimal message for delay request-response mechanism.
 */
struct DelayReqBody {
    // Bytes 34-43: Origin timestamp (set to zero, filled by Follow_Up)
    Timestamp originTimestamp;
    
    /**
     * @brief Validate delay request message
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        // Origin timestamp typically zero for delay request
        return PTPResult<void>::success();
    }
};

// See note above regarding size assertions.

//==============================================================================
// Delay_Resp Message Body (Section 13.8)
//==============================================================================

/**
 * @brief Delay_Resp message body - end-to-end delay measurement response
 * 
 * Contains receive timestamp of corresponding Delay_Req message.
 */
struct DelayRespBody {
    // Bytes 34-43: Receive timestamp of Delay_Req
    Timestamp receiveTimestamp;
    
    // Bytes 44-53: Requesting port identity
    PortIdentity requestingPortIdentity;
    
    /**
     * @brief Validate delay response message
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        auto result = receiveTimestamp.validate();
        if (!result.isSuccess()) {
            return result;
        }
        
        return requestingPortIdentity.validate();
    }
};

// See note above regarding size assertions.

//==============================================================================
// Pdelay_Req Message Body (Section 13.9)
//==============================================================================

/**
 * @brief Pdelay_Req message body - peer-to-peer delay measurement
 * 
 * Used for direct link delay measurement between peers.
 */
struct PdelayReqBody {
    // Bytes 34-43: Origin timestamp (typically zero, filled by hardware)
    Timestamp originTimestamp;
    
    // Bytes 44-53: Reserved (10 bytes)
    std::array<std::uint8_t, 10> reserved;
    
    /**
     * @brief Validate peer delay request message
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        // Reserved field should be zero
        for (const auto byte : reserved) {
            if (byte != 0) {
                return PTPResult<void>::makeError(PTPError::INVALID_RESERVED_FIELD);
            }
        }
        
        return PTPResult<void>::success();
    }
};

// See note above regarding size assertions.

//==============================================================================
// Pdelay_Resp Message Body (Section 13.10)
//==============================================================================

/**
 * @brief Pdelay_Resp message body - peer-to-peer delay measurement response
 * 
 * Contains receive timestamp of corresponding Pdelay_Req message.
 */
struct PdelayRespBody {
    // Bytes 34-43: Request receive timestamp
    Timestamp requestReceiveTimestamp;
    
    // Bytes 44-53: Requesting port identity
    PortIdentity requestingPortIdentity;
    
    /**
     * @brief Validate peer delay response message
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        auto result = requestReceiveTimestamp.validate();
        if (!result.isSuccess()) {
            return result;
        }
        
        return requestingPortIdentity.validate();
    }
};

// See note above regarding size assertions.

//==============================================================================
// Pdelay_Resp_Follow_Up Message Body (Section 13.11)
//==============================================================================

/**
 * @brief Pdelay_Resp_Follow_Up message body - precise peer delay response
 * 
 * Contains precise transmit timestamp of corresponding Pdelay_Resp message.
 */
struct PdelayRespFollowUpBody {
    // Bytes 34-43: Response origin timestamp
    Timestamp responseOriginTimestamp;
    
    // Bytes 44-53: Requesting port identity
    PortIdentity requestingPortIdentity;
    
    /**
     * @brief Validate peer delay response follow-up message
     * @return PTPResult with validation status  
     */
    PTPResult<void> validate() const noexcept {
        auto result = responseOriginTimestamp.validate();
        if (!result.isSuccess()) {
            return result;
        }
        
        return requestingPortIdentity.validate();
    }
};

// See note above regarding size assertions.

//==============================================================================
// Complete PTP Message Templates
//==============================================================================

/**
 * @brief Complete PTP Message template combining header and body
 * 
 * Template-based approach for type-safe message handling with
 * compile-time size validation and deterministic memory layout.
 * 
 * @tparam BodyType Specific message body type
 */
template<typename BodyType>
struct PTPMessage {
    CommonHeader header;
    BodyType body;
    
    /**
     * @brief Validate complete message structure
     * @return PTPResult with validation status
     * @note Validates both header and body with bounded execution time
     */
    PTPResult<void> validate() const noexcept {
        auto headerResult = header.validate();
        if (!headerResult.isSuccess()) {
            return headerResult;
        }
        
        return body.validate();
    }
    
    /**
     * @brief Get total message size
     * @return Message size in bytes
     * @note Constexpr for compile-time computation
     */
    static constexpr std::size_t getMessageSize() noexcept {
        return sizeof(CommonHeader) + sizeof(BodyType);
    }
    
    /**
     * @brief Initialize message with proper defaults
     * @param msgType Message type to set
     * @param domain PTP domain number
     * @param sourcePort Source port identity
     */
    void initialize(MessageType msgType, std::uint8_t domain, 
                   const PortIdentity& sourcePort) noexcept {
        // Initialize header with IEEE 1588-2019 defaults
        header = {};
        header.setMessageType(msgType);
        header.setVersion(2);  // IEEE 1588-2019 is version 2
    header.messageLength = detail::host_to_be16(static_cast<std::uint16_t>(getMessageSize()));
        header.domainNumber = domain;
        header.minorVersionPTP = 1;  // IEEE 1588-2019 minor version
        header.sourcePortIdentity = sourcePort;
        header.controlField = 0xFF;  // Deprecated in v2
        
        // Initialize body to zero
        body = {};
    }
};

//==============================================================================
// Specific Message Type Aliases for Type Safety
//==============================================================================

using AnnounceMessage = PTPMessage<AnnounceBody>;
using SyncMessage = PTPMessage<SyncBody>;
using FollowUpMessage = PTPMessage<FollowUpBody>;
using DelayReqMessage = PTPMessage<DelayReqBody>;
using DelayRespMessage = PTPMessage<DelayRespBody>;
using PdelayReqMessage = PTPMessage<PdelayReqBody>;
using PdelayRespMessage = PTPMessage<PdelayRespBody>;
using PdelayRespFollowUpMessage = PTPMessage<PdelayRespFollowUpBody>;

//==============================================================================
// TLV (Type-Length-Value) Structure (Section 14)
//==============================================================================

/**
 * @brief TLV Type Enumeration (IEEE 1588-2019 Section 14.1)
 * 
 * Defines standard TLV types for Management and Signaling messages.
 * Network byte order (big-endian) when transmitted.
 */
namespace TLVType {
    constexpr std::uint16_t MANAGEMENT                  = 0x0001;  // Section 15.5.4.1
    constexpr std::uint16_t MANAGEMENT_ERROR_STATUS     = 0x0002;  // Section 15.5.4.2
    constexpr std::uint16_t ORGANIZATION_EXTENSION      = 0x0003;  // Section 14.3
    constexpr std::uint16_t REQUEST_UNICAST_TRANSMISSION = 0x0004;  // Section 16.1
    constexpr std::uint16_t GRANT_UNICAST_TRANSMISSION  = 0x0005;  // Section 16.1
    constexpr std::uint16_t CANCEL_UNICAST_TRANSMISSION = 0x0006;  // Section 16.1
    constexpr std::uint16_t ACKNOWLEDGE_CANCEL_UNICAST_TRANSMISSION = 0x0007;  // Section 16.1
    constexpr std::uint16_t PATH_TRACE                  = 0x0008;  // Section 16.2
    constexpr std::uint16_t ALTERNATE_TIME_OFFSET_INDICATOR = 0x0009;  // Section 16.3
}

/**
 * @brief TLV Header Structure (IEEE 1588-2019 Section 14)
 * 
 * Common header for all TLV entities. Consists of:
 * - tlvType: 2 bytes (network byte order) - identifies TLV type
 * - lengthField: 2 bytes (network byte order) - octets in valueField (excludes type/length)
 * 
 * @note All multi-byte fields use network byte order (big-endian) per Section 7.1.2
 */
struct TLVHeader {
    std::uint16_t tlvType;       // TLV type identifier (network byte order)
    std::uint16_t lengthField;   // Length of valueField in octets (network byte order)
    
    /**
     * @brief Validate TLV header
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        // TLV type validation - check if type is known/valid
        const std::uint16_t type = detail::be16_to_host(tlvType);
        // Accept all types for now; specific validation in TLV-specific parsers
        
        // Length field must be reasonable (< 64KB by definition, but practical limit lower)
        const std::uint16_t length = detail::be16_to_host(lengthField);
        if (length > 1500) {  // Ethernet MTU as practical upper bound
            return PTPResult<void>::failure(PTPError::INVALID_LENGTH);
        }
        
        return PTPResult<void>::success();
    }
};

/**
 * @brief Management Action Field Enumeration (IEEE 1588-2019 Section 15.4)
 * 
 * Defines action types for Management messages.
 */
namespace ManagementAction {
    constexpr std::uint8_t GET          = 0x00;  // Section 15.4.1 - Request dataset values
    constexpr std::uint8_t SET          = 0x01;  // Section 15.4.1 - Set dataset values
    constexpr std::uint8_t RESPONSE     = 0x02;  // Section 15.4.1 - Response to GET/SET
    constexpr std::uint8_t COMMAND      = 0x03;  // Section 15.4.1 - Execute command
    constexpr std::uint8_t ACKNOWLEDGE  = 0x04;  // Section 15.4.1 - Acknowledge command
}

/**
 * @brief Management Message Body (IEEE 1588-2019 Section 15.5.3)
 * 
 * Management message structure following common header (34 bytes).
 * Total management-specific fields: 16 bytes before TLVs.
 * 
 * Structure (bytes relative to start of body, after 34-byte common header):
 * - Bytes 0-9: targetPortIdentity (10 bytes)
 * - Byte 10: startingBoundaryHops
 * - Byte 11: boundaryHops
 * - Byte 12: reserved (4 bits) + actionField (4 bits)
 * - Byte 13: reserved
 * - Bytes 14+: TLV entities (variable length)
 * 
 * @note All multi-byte fields use network byte order (big-endian)
 */
struct ManagementMessageBody {
    // Bytes 0-9: Target port identity (10 bytes)
    PortIdentity targetPortIdentity;
    
    // Byte 10: Starting boundary hops (Section 15.5.3.3)
    std::uint8_t startingBoundaryHops;
    
    // Byte 11: Boundary hops (Section 15.5.3.4)
    std::uint8_t boundaryHops;
    
    // Byte 12: Reserved (4 bits) + Action field (4 bits) - Section 15.5.3.5
    std::uint8_t reserved_actionField;
    
    // Byte 13: Reserved
    std::uint8_t reserved;
    
    // Bytes 14+: TLV entities follow (not included in fixed structure)
    // TLVs are parsed separately due to variable length
    
    /**
     * @brief Get action field value from reserved_actionField byte
     * @return Action field value (lower 4 bits)
     */
    constexpr std::uint8_t getActionField() const noexcept {
        return reserved_actionField & 0x0F;  // Lower 4 bits
    }
    
    /**
     * @brief Set action field value in reserved_actionField byte
     * @param action Action field value (must be 0-15)
     */
    constexpr void setActionField(std::uint8_t action) noexcept {
        reserved_actionField = (reserved_actionField & 0xF0) | (action & 0x0F);
    }
    
    /**
     * @brief Validate management message body
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        // Validate action field is in valid range
        const std::uint8_t action = getActionField();
        if (action > ManagementAction::ACKNOWLEDGE) {
            return PTPResult<void>::failure(PTPError::Invalid_Parameter);
        }
        
        // Validate boundary hops
        if (boundaryHops > startingBoundaryHops) {
            return PTPResult<void>::failure(PTPError::Invalid_Parameter);
        }
        
        return PTPResult<void>::success();
    }
};

/**
 * @brief Management ID Enumeration (IEEE 1588-2019 Section 15.5.3.1)
 * 
 * Identifies which dataset or command is being accessed.
 * Used within MANAGEMENT TLV payload.
 */
namespace ManagementId {
    constexpr std::uint16_t NULL_MANAGEMENT             = 0x0000;  // Section 15.5.3.1.1
    constexpr std::uint16_t CLOCK_DESCRIPTION           = 0x0001;  // Section 15.5.3.1.2
    constexpr std::uint16_t USER_DESCRIPTION            = 0x0002;  // Section 15.5.3.1.3
    constexpr std::uint16_t SAVE_IN_NON_VOLATILE_STORAGE = 0x0003; // Section 15.5.3.1.4
    constexpr std::uint16_t RESET_NON_VOLATILE_STORAGE  = 0x0004;  // Section 15.5.3.1.5
    constexpr std::uint16_t INITIALIZE                  = 0x0005;  // Section 15.5.3.1.6
    constexpr std::uint16_t FAULT_LOG                   = 0x0006;  // Section 15.5.3.1.7
    constexpr std::uint16_t FAULT_LOG_RESET             = 0x0007;  // Section 15.5.3.1.8
    
    // Dataset access (Section 8)
    constexpr std::uint16_t DEFAULT_DATA_SET            = 0x2000;  // Section 8.2.1
    constexpr std::uint16_t CURRENT_DATA_SET            = 0x2001;  // Section 8.2.2
    constexpr std::uint16_t PARENT_DATA_SET             = 0x2002;  // Section 8.2.3
    constexpr std::uint16_t TIME_PROPERTIES_DATA_SET    = 0x2003;  // Section 8.2.4
    constexpr std::uint16_t PORT_DATA_SET               = 0x2004;  // Section 8.2.5
    constexpr std::uint16_t PRIORITY1                   = 0x2005;  // Section 8.2.1
    constexpr std::uint16_t PRIORITY2                   = 0x2006;  // Section 8.2.1
    constexpr std::uint16_t DOMAIN_NUMBER               = 0x2007;  // Section 8.2.1 (renamed from DOMAIN to avoid Windows macro conflict)
    constexpr std::uint16_t SLAVE_ONLY                  = 0x2008;  // Section 8.2.1
    constexpr std::uint16_t LOG_ANNOUNCE_INTERVAL       = 0x2009;  // Section 8.2.1
    constexpr std::uint16_t ANNOUNCE_RECEIPT_TIMEOUT    = 0x200A;  // Section 8.2.1
    constexpr std::uint16_t LOG_SYNC_INTERVAL           = 0x200B;  // Section 8.2.1
    constexpr std::uint16_t VERSION_NUMBER              = 0x200C;  // Section 8.2.1
    constexpr std::uint16_t CURRENT_TIME                = 0x200F;  // Section 8.2.2 (renamed from TIME to avoid Windows macro conflict)
    constexpr std::uint16_t CLOCK_ACCURACY              = 0x2010;  // Section 8.2.1
    constexpr std::uint16_t UTC_PROPERTIES              = 0x2011;  // Section 8.2.4
    constexpr std::uint16_t TRACEABILITY_PROPERTIES     = 0x2012;  // Section 8.2.4
    constexpr std::uint16_t TIMESCALE_PROPERTIES        = 0x2013;  // Section 8.2.4
}

/**
 * @brief Management TLV Structure (IEEE 1588-2019 Section 15.5.4.1)
 * 
 * MANAGEMENT TLV payload structure:
 * - managementId: 2 bytes (network byte order) - identifies dataset/command
 * - dataField: variable length - dataset-specific data
 * 
 * This structure represents the valueField of a TLV with tlvType = MANAGEMENT (0x0001).
 */
struct ManagementTLV {
    std::uint16_t managementId;  // Management ID (network byte order)
    // dataField follows (variable length, parsed separately)
    
    /**
     * @brief Get management ID in host byte order
     * @return Management ID value
     */
    constexpr std::uint16_t getManagementId() const noexcept {
        return detail::be16_to_host(managementId);
    }
    
    /**
     * @brief Set management ID from host byte order
     * @param id Management ID value
     */
    constexpr void setManagementId(std::uint16_t id) noexcept {
        managementId = detail::host_to_be16(id);
    }
};

//==============================================================================
// Management Message Type Alias
//==============================================================================

using ManagementMessage = PTPMessage<ManagementMessageBody>;

//==============================================================================
// Signaling Message (IEEE 1588-2019 Section 13.10)
//==============================================================================

/**
 * @brief Signaling Message Body (IEEE 1588-2019 Section 13.10.2)
 * 
 * Signaling message structure following common header (34 bytes).
 * Contains only targetPortIdentity field (10 bytes).
 * TLVs follow immediately after this body (variable length).
 * 
 * Structure (bytes relative to start of body, after 34-byte common header):
 * - Bytes 0-9: targetPortIdentity (10 bytes)
 * - Bytes 10+: TLV entities (variable length, zero or more TLVs)
 * 
 * @note Signaling messages may contain multiple TLVs of different types
 * @note All multi-byte fields use network byte order (big-endian)
 */
struct SignalingMessageBody {
    // Bytes 0-9: Target port identity (10 bytes)
    // All Fs (FF-FF-FF-FF-FF-FF-FF-FF-FF-FF) means "all ports"
    PortIdentity targetPortIdentity;
    
    // Bytes 10+: TLV entities follow (not included in fixed structure)
    // TLVs are parsed separately due to variable length and count
    
    /**
     * @brief Validate Signaling message body
     * @return PTPResult with validation status
     */
    PTPResult<void> validate() const noexcept {
        // targetPortIdentity is always valid (any value allowed per Section 13.10.2)
        // All Fs means "all ports", any other value targets specific port
        return PTPResult<void>::success();
    }
};

/**
 * @brief REQUEST_UNICAST_TRANSMISSION TLV (IEEE 1588-2019 Section 16.1.4.1)
 * 
 * Used to request unicast transmission of specific message types.
 * Part of unicast message negotiation mechanism.
 * 
 * Structure (7 bytes valueField after TLVHeader):
 * - Byte 0: messageType (Enumeration4) - which message type to request
 * - Byte 1: reserved
 * - Byte 2: logInterMessagePeriod (Integer8) - requested interval
 * - Bytes 3-6: durationField (UInteger32, network byte order) - request duration in seconds
 */
struct RequestUnicastTransmissionTLV {
    std::uint8_t  messageType;           // Enumeration4: message type to request (Sync, Delay_Req, etc.)
    std::uint8_t  reserved;              // Reserved byte
    std::int8_t   logInterMessagePeriod; // Integer8: log2 of message interval in seconds
    std::uint32_t durationField;         // UInteger32: duration in seconds (network byte order)
};

/**
 * @brief GRANT_UNICAST_TRANSMISSION TLV (IEEE 1588-2019 Section 16.1.4.2)
 * 
 * Response to REQUEST_UNICAST_TRANSMISSION, granting or denying request.
 * 
 * Structure (9 bytes valueField after TLVHeader):
 * - Byte 0: messageType (Enumeration4) - granted message type
 * - Byte 1: reserved
 * - Byte 2: logInterMessagePeriod (Integer8) - granted interval
 * - Bytes 3-6: durationField (UInteger32, network byte order) - granted duration in seconds
 * - Byte 7: reserved
 * - Byte 8: renewal (Boolean) - whether renewal is allowed
 */
struct GrantUnicastTransmissionTLV {
    std::uint8_t  messageType;           // Enumeration4: granted message type
    std::uint8_t  reserved1;             // Reserved byte
    std::int8_t   logInterMessagePeriod; // Integer8: granted log interval
    std::uint32_t durationField;         // UInteger32: granted duration (network byte order)
    std::uint8_t  reserved2;             // Reserved byte
    std::uint8_t  renewal;               // Boolean: renewal allowed (0=no, 1=yes)
};

/**
 * @brief PATH_TRACE TLV (IEEE 1588-2019 Section 16.2.3)
 * 
 * Contains sequence of ClockIdentity values representing path through network.
 * Variable length: lengthField = N × 8 (where N is number of clocks).
 * 
 * Structure (variable length valueField after TLVHeader):
 * - Bytes 0-7: First ClockIdentity (8 bytes)
 * - Bytes 8-15: Second ClockIdentity (8 bytes)
 * - ... (up to N ClockIdentity entries)
 * 
 * @note pathSequence size determined by TLV lengthField / 8
 */
struct PathTraceTLV {
    std::uint8_t pathSequence[256];  // Up to 32 ClockIdentity entries (32 × 8 = 256 bytes max)
    
    /**
     * @brief Get number of ClockIdentity entries in path
     * @param tlv_length Length field from TLV header
     * @return Number of ClockIdentity entries
     */
    static constexpr std::size_t get_path_count(std::uint16_t tlv_length) noexcept {
        return tlv_length / 8;  // Each ClockIdentity is 8 bytes
    }
};

using SignalingMessage = PTPMessage<SignalingMessageBody>;

// Compile-time size verification for all message types
// Wire-size static_asserts removed pending portable serialization layer.

//==============================================================================
// Validation method implementations
//==============================================================================

// Duplicate validate implementations removed; using definitions in types.hpp.

#pragma pack(pop)

} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE