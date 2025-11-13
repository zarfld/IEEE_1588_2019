/**
 * @file gps_time_converter.hpp
 * @brief GPS Time to IEEE 1588-2019 PTP Timestamp Converter
 * 
 * Converts GPS NMEA time (HHMMSS.SS + DDMMYY) to IEEE 1588-2019 PTP timestamps
 * with GPS-UTC leap second correction.
 * 
 * GPS time is ahead of UTC by leap seconds (currently 18 seconds as of 2017).
 * PTP uses TAI (International Atomic Time) which is UTC + 37 seconds.
 * 
 * @see IEEE 1588-2019, Section 7.2 "Timescales"
 * @see GPS_NMEA_Specification_Refinement.md for time conversion details
 */

#ifndef GPS_TIME_CONVERTER_HPP
#define GPS_TIME_CONVERTER_HPP

#include "nmea_parser.hpp"
#include <cstdint>

namespace GPS {
namespace Time {

/**
 * @brief GPS-UTC Leap Second Offset
 * 
 * GPS time was synchronized with UTC at 1980-01-06 00:00:00.
 * Since then, UTC has added leap seconds while GPS time continues monotonically.
 * 
 * Current offset: 18 seconds (as of 2017-01-01, no new leap seconds announced)
 * 
 * @note This value must be updated when new leap seconds are announced by IERS
 */
constexpr int32_t GPS_UTC_LEAP_SECONDS = 18;

/**
 * @brief TAI-UTC Offset (seconds)
 * 
 * TAI (International Atomic Time) is ahead of UTC by 37 seconds (as of 2017).
 * IEEE 1588-2019 PTP uses TAI as its timescale.
 * 
 * @note This value changes when leap seconds are added to UTC
 */
constexpr int32_t TAI_UTC_OFFSET_SECONDS = 37;

/**
 * @brief PTP Epoch (TAI) - January 1, 1970 00:00:00
 * 
 * IEEE 1588-2019 uses TAI epoch same as Unix epoch (1970-01-01).
 */
constexpr int64_t PTP_EPOCH_YEAR = 1970;

/**
 * @brief Nanoseconds per second
 */
constexpr int64_t NANOSECONDS_PER_SECOND = 1000000000LL;

/**
 * @brief IEEE 1588-2019 PTP Timestamp
 * 
 * Represents absolute time in TAI timescale.
 */
struct PTPTimestamp {
    uint64_t seconds;      ///< Seconds since PTP epoch (TAI)
    uint32_t nanoseconds;  ///< Nanoseconds within second (0-999999999)
    
    /**
     * @brief Constructor - initializes to zero
     */
    PTPTimestamp() : seconds(0), nanoseconds(0) {}
    
    /**
     * @brief Constructor with values
     */
    PTPTimestamp(uint64_t sec, uint32_t nsec) 
        : seconds(sec), nanoseconds(nsec) {}
    
    /**
     * @brief Convert to nanoseconds since epoch
     */
    int64_t to_nanoseconds() const {
        return static_cast<int64_t>(seconds) * NANOSECONDS_PER_SECOND + 
               static_cast<int64_t>(nanoseconds);
    }
    
    /**
     * @brief Create from nanoseconds since epoch
     */
    static PTPTimestamp from_nanoseconds(int64_t ns) {
        PTPTimestamp ts;
        ts.seconds = static_cast<uint64_t>(ns / NANOSECONDS_PER_SECOND);
        ts.nanoseconds = static_cast<uint32_t>(ns % NANOSECONDS_PER_SECOND);
        return ts;
    }
};

/**
 * @brief GPS Time Converter
 * 
 * Converts GPS NMEA time to IEEE 1588-2019 PTP timestamps.
 * Handles GPS-UTC leap second correction and TAI conversion.
 */
class GPSTimeConverter {
public:
    /**
     * @brief Constructor
     */
    GPSTimeConverter();
    
    /**
     * @brief Destructor
     */
    ~GPSTimeConverter() = default;
    
    /**
     * @brief Convert GPS NMEA time to PTP timestamp
     * 
     * Process:
     * 1. Convert NMEA time (UTC) to Unix timestamp
     * 2. Apply GPS-UTC leap second correction (currently +18s)
     * 3. Convert to TAI by adding TAI-UTC offset (+37s)
     * 4. Interpolate centiseconds to nanoseconds (10ms resolution → 1ns)
     * 
     * @param gps_data GPS time data from NMEA parser
     * @param[out] ptp_timestamp IEEE 1588-2019 PTP timestamp (TAI)
     * @return true if conversion successful
     * 
     * @note Requires valid date and time in gps_data
     * @note PTP timestamp is in TAI timescale, not UTC
     * 
     * @example
     * @code
     * GPSTimeConverter converter;
     * GPSTimeData gps_data;  // From NMEA parser
     * PTPTimestamp ptp_ts;
     * 
     * if (converter.convert_to_ptp(gps_data, ptp_ts)) {
     *     // Use ptp_ts for clock synchronization
     * }
     * @endcode
     */
    bool convert_to_ptp(const NMEA::GPSTimeData& gps_data, PTPTimestamp& ptp_timestamp);
    
    /**
     * @brief Calculate clock offset between local PTP clock and GPS time
     * 
     * Offset = GPS_time - Local_time
     * Positive offset means local clock is behind GPS.
     * 
     * @param gps_time GPS time as PTP timestamp
     * @param local_time Local PTP clock time
     * @return Clock offset in nanoseconds (GPS - Local)
     * 
     * @note Use this offset to adjust local PTP clock
     */
    int64_t calculate_clock_offset(const PTPTimestamp& gps_time, 
                                   const PTPTimestamp& local_time);
    
    /**
     * @brief Set custom GPS-UTC leap second offset
     * 
     * Use this if leap second information is available from GPS receiver
     * or when IERS announces new leap second.
     * 
     * @param leap_seconds GPS-UTC offset in seconds (positive)
     * 
     * @note Default is 18 seconds (as of 2017)
     */
    void set_leap_seconds(int32_t leap_seconds) {
        gps_utc_offset_ = leap_seconds;
    }
    
    /**
     * @brief Get current GPS-UTC leap second offset
     * 
     * @return Current offset in seconds
     */
    int32_t get_leap_seconds() const {
        return gps_utc_offset_;
    }
    
    /**
     * @brief Estimate time uncertainty from GPS data
     * 
     * Calculates estimated timing uncertainty based on:
     * - Number of satellites in view
     * - GPS fix quality
     * - NMEA time resolution (centiseconds = 10ms)
     * 
     * @param gps_data GPS data from NMEA parser
     * @return Estimated uncertainty in nanoseconds
     * 
     * @note Typical GPS timing accuracy: 100ns - 1μs with good fix
     */
    int64_t estimate_time_uncertainty(const NMEA::GPSTimeData& gps_data);
    
private:
    /**
     * @brief Convert calendar date/time to Unix timestamp (seconds since epoch)
     * 
     * @param year Year (e.g., 2025)
     * @param month Month (1-12)
     * @param day Day (1-31)
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @param second Second (0-59)
     * @return Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
     */
    int64_t date_time_to_unix_timestamp(uint16_t year, uint8_t month, uint8_t day,
                                        uint8_t hour, uint8_t minute, uint8_t second);
    
    /**
     * @brief Check if year is a leap year
     * 
     * @param year Year to check
     * @return true if leap year
     */
    bool is_leap_year(uint16_t year);
    
    /**
     * @brief Get days in month
     * 
     * @param month Month (1-12)
     * @param year Year (for leap year calculation)
     * @return Number of days in month
     */
    uint8_t days_in_month(uint8_t month, uint16_t year);
    
    int32_t gps_utc_offset_;  ///< Current GPS-UTC leap second offset
};

} // namespace Time
} // namespace GPS

#endif // GPS_TIME_CONVERTER_HPP
