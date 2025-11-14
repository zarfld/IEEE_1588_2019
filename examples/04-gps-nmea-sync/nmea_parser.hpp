/**
 * @file nmea_parser.hpp
 * @brief NMEA-0183 GPS Sentence Parser
 * 
 * Implements parser for NMEA-0183 GPRMC and GPGGA sentences with checksum
 * validation and state machine for GPS fix status tracking.
 * 
 * Supports IEEE 1588-2019 PTP synchronization with GPS reference time.
 * 
 * @see GPS_NMEA_Specification_Refinement.md for parser design
 * @see NMEA-0183 Standard for sentence format specification
 */

#ifndef GPS_NMEA_PARSER_HPP
#define GPS_NMEA_PARSER_HPP

#include <cstdint>
#include <cstddef>  // for size_t

namespace GPS {
namespace NMEA {

/**
 * @brief GPS Fix Status
 */
enum class GPSFixStatus {
    NO_FIX,              ///< No GPS fix available
    TIME_ONLY,           ///< Valid time, no position fix (GPRMC 'V' status)
    AUTONOMOUS_FIX,      ///< Autonomous GPS fix (GPRMC 'A' status)
    DGPS_FIX,            ///< Differential GPS fix (GPGGA quality 2)
    SIGNAL_LOST          ///< GPS signal lost after previous fix
};

/**
 * @brief GPS Quality Indicator (from GPGGA)
 */
enum class GPSQuality {
    INVALID = 0,         ///< Fix not available
    GPS_FIX = 1,         ///< GPS SPS mode
    DGPS_FIX = 2,        ///< Differential GPS SPS mode
    PPS_FIX = 3,         ///< GPS PPS mode
    RTK_FIX = 4,         ///< Real-Time Kinematic
    FLOAT_RTK = 5,       ///< Float RTK
    ESTIMATED = 6,       ///< Estimated (dead reckoning)
    MANUAL = 7,          ///< Manual input mode
    SIMULATION = 8       ///< Simulation mode
};

/**
 * @brief Parsed GPS Time Data
 * 
 * Contains time extracted from NMEA sentences with centisecond precision.
 * Supports IEEE 1588-2019 PTP timestamp generation.
 */
struct GPSTimeData {
    // Time fields (from GPRMC or GPGGA)
    uint8_t  hours;          ///< UTC hours (0-23)
    uint8_t  minutes;        ///< UTC minutes (0-59)
    uint8_t  seconds;        ///< UTC seconds (0-59)
    uint16_t centiseconds;   ///< Centiseconds (0-99), 10ms resolution
    
    // Date fields (from GPRMC only)
    uint8_t  day;            ///< UTC day (1-31)
    uint8_t  month;          ///< UTC month (1-12)
    uint16_t year;           ///< UTC year (4-digit, e.g., 2025)
    
    // GPS status
    GPSFixStatus fix_status; ///< Current GPS fix status
    GPSQuality   quality;    ///< GPS quality indicator (from GPGGA)
    uint8_t      satellites; ///< Number of satellites in use (from GPGGA)
    
    // Validity flags
    bool time_valid;         ///< Time data is valid
    bool date_valid;         ///< Date data is valid
    
    /**
     * @brief Constructor - initializes to invalid state
     */
    GPSTimeData()
        : hours(0)
        , minutes(0)
        , seconds(0)
        , centiseconds(0)
        , day(0)
        , month(0)
        , year(0)
        , fix_status(GPSFixStatus::NO_FIX)
        , quality(GPSQuality::INVALID)
        , satellites(0)
        , time_valid(false)
        , date_valid(false)
    {
    }
    
    /**
     * @brief Check if GPS data is usable for PTP synchronization
     * 
     * Time-only mode (GPRMC 'V' status) is sufficient for PTP sync.
     * Position fix is not required.
     * 
     * @return true if time is valid for PTP use
     */
    bool is_valid_for_ptp() const {
        return time_valid && (fix_status != GPSFixStatus::NO_FIX);
    }
};

/**
 * @brief NMEA-0183 Parser
 * 
 * Parses GPRMC and GPGGA sentences with checksum validation.
 * Maintains state machine for GPS fix status tracking.
 */
class NMEAParser {
public:
    /**
     * @brief Constructor
     */
    NMEAParser();
    
    /**
     * @brief Destructor
     */
    ~NMEAParser() = default;
    
    /**
     * @brief Parse NMEA sentence and extract GPS data
     * 
     * Supports:
     * - $GPRMC - Recommended Minimum Specific GPS/Transit Data
     * - $GPGGA - Global Positioning System Fix Data
     * 
     * Validates checksum before processing.
     * 
     * @param sentence NMEA sentence string (null-terminated)
     * @param[out] gps_data Parsed GPS time data
     * @return true if sentence was valid and parsed successfully
     * 
     * @note Updates internal state machine for fix status tracking
     * 
     * @example
     * @code
     * NMEAParser parser;
     * GPSTimeData gps_data;
     * 
     * if (parser.parse_sentence("$GPRMC,083218.00,V,,,,,,,131125,,,N*78", gps_data)) {
     *     if (gps_data.is_valid_for_ptp()) {
     *         // Use GPS time for PTP synchronization
     *     }
     * }
     * @endcode
     */
    bool parse_sentence(const char* sentence, GPSTimeData& gps_data);
    
    /**
     * @brief Get current GPS fix status
     * 
     * @return Current fix status from state machine
     */
    GPSFixStatus get_fix_status() const { return current_fix_status_; }
    
    /**
     * @brief Reset parser state machine
     * 
     * Clears fix status and sentence counters.
     */
    void reset();
    
private:
    /**
     * @brief Validate NMEA checksum
     * 
     * NMEA checksum is XOR of all characters between '$' and '*'.
     * Format: $...DATA...*HH\r\n where HH is 2-digit hex checksum.
     * 
     * @param sentence NMEA sentence string
     * @return true if checksum is valid
     */
    bool validate_checksum(const char* sentence);
    
    /**
     * @brief Parse GPRMC sentence
     * 
     * Format: $GPRMC,hhmmss.ss,A/V,lat,N/S,lon,E/W,speed,course,ddmmyy,mag,E/W,mode*HH
     * 
     * @param sentence GPRMC sentence (checksum already validated)
     * @param[out] gps_data Parsed GPS data
     * @return true if parsed successfully
     */
    bool parse_gprmc(const char* sentence, GPSTimeData& gps_data);
    
    /**
     * @brief Parse GPGGA sentence
     * 
     * Format: $GPGGA,hhmmss.ss,lat,N/S,lon,E/W,quality,sats,hdop,alt,M,geoid,M,dgps_age,dgps_id*HH
     * 
     * @param sentence GPGGA sentence (checksum already validated)
     * @param[out] gps_data Parsed GPS data
     * @return true if parsed successfully
     */
    bool parse_gpgga(const char* sentence, GPSTimeData& gps_data);
    
    /**
     * @brief Parse NMEA time field (hhmmss.ss)
     * 
     * @param time_str Time string (e.g., "083218.00")
     * @param[out] hours Hours (0-23)
     * @param[out] minutes Minutes (0-59)
     * @param[out] seconds Seconds (0-59)
     * @param[out] centiseconds Centiseconds (0-99)
     * @return true if parsed successfully
     */
    bool parse_time(const char* time_str, uint8_t& hours, uint8_t& minutes,
                   uint8_t& seconds, uint16_t& centiseconds);
    
    /**
     * @brief Parse NMEA date field (ddmmyy)
     * 
     * @param date_str Date string (e.g., "131125")
     * @param[out] day Day (1-31)
     * @param[out] month Month (1-12)
     * @param[out] year Year (4-digit, e.g., 2025)
     * @return true if parsed successfully
     */
    bool parse_date(const char* date_str, uint8_t& day, uint8_t& month, uint16_t& year);
    
    /**
     * @brief Update state machine based on GPS fix status
     * 
     * @param new_status New fix status from parsed sentence
     */
    void update_fix_status(GPSFixStatus new_status);
    
    /**
     * @brief Extract next field from comma-separated NMEA sentence
     * 
     * @param[in,out] sentence Pointer to current position in sentence (updated to next field)
     * @param[out] field_buffer Buffer to store extracted field (null-terminated)
     * @param max_length Maximum length of field_buffer
     * @return true if field extracted successfully
     */
    bool extract_field(const char*& sentence, char* field_buffer, size_t max_length);
    
    // State machine variables
    GPSFixStatus current_fix_status_;  ///< Current GPS fix status
    uint32_t sentences_since_fix_;     ///< Number of sentences since last valid fix
    
    // Statistics
    uint32_t gprmc_count_;             ///< Number of GPRMC sentences parsed
    uint32_t gpgga_count_;             ///< Number of GPGGA sentences parsed
    uint32_t checksum_errors_;         ///< Number of checksum validation failures
};

} // namespace NMEA
} // namespace GPS

#endif // GPS_NMEA_PARSER_HPP
