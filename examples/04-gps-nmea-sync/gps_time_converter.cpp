/**
 * @file gps_time_converter.cpp
 * @brief GPS Time to IEEE 1588-2019 PTP Timestamp Converter Implementation
 */

#include "gps_time_converter.hpp"

namespace GPS {
namespace Time {

GPSTimeConverter::GPSTimeConverter()
    : gps_utc_offset_(GPS_UTC_LEAP_SECONDS)
{
}

bool GPSTimeConverter::is_leap_year(uint16_t year) {
    // Leap year rules:
    // - Divisible by 4: leap year
    // - Exception: divisible by 100: not leap year
    // - Exception to exception: divisible by 400: leap year
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t GPSTimeConverter::days_in_month(uint8_t month, uint16_t year) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) {
        return 0;
    }
    
    uint8_t days_count = days[month - 1];
    
    // February in leap year
    if (month == 2 && is_leap_year(year)) {
        days_count = 29;
    }
    
    return days_count;
}

int64_t GPSTimeConverter::date_time_to_unix_timestamp(uint16_t year, uint8_t month, uint8_t day,
                                                      uint8_t hour, uint8_t minute, uint8_t second) {
    // Validate inputs
    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59) {
        return 0;
    }
    
    // Days in each month (non-leap year)
    static const uint16_t days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    
    // Calculate days since Unix epoch (1970-01-01)
    int64_t days_since_epoch = 0;
    
    // Add days for complete years
    for (uint16_t y = 1970; y < year; ++y) {
        days_since_epoch += is_leap_year(y) ? 366 : 365;
    }
    
    // Add days for complete months in current year
    days_since_epoch += days_before_month[month - 1];
    
    // Add leap day if current year is leap and month > February
    if (month > 2 && is_leap_year(year)) {
        days_since_epoch += 1;
    }
    
    // Add days in current month
    days_since_epoch += day - 1;
    
    // Convert to seconds
    int64_t timestamp = days_since_epoch * 86400LL;  // 86400 seconds per day
    timestamp += hour * 3600LL;
    timestamp += minute * 60LL;
    timestamp += second;
    
    return timestamp;
}

bool GPSTimeConverter::convert_to_ptp(const NMEA::GPSTimeData& gps_data, 
                                     PTPTimestamp& ptp_timestamp) {
    // Validate input
    if (!gps_data.time_valid || !gps_data.date_valid) {
        return false;
    }
    
    // Convert GPS time (UTC) to Unix timestamp
    int64_t utc_seconds = date_time_to_unix_timestamp(
        gps_data.year, gps_data.month, gps_data.day,
        gps_data.hours, gps_data.minutes, gps_data.seconds
    );
    
    if (utc_seconds == 0) {
        return false;
    }
    
    // GPS time is ahead of UTC by leap seconds
    // GPS_time = UTC + GPS_UTC_offset
    // So: GPS_time_in_UTC = UTC - GPS_UTC_offset (to get equivalent UTC time from GPS)
    // But NMEA reports UTC time already, so we need to add offset to get TAI
    
    // Convert UTC to TAI (IEEE 1588-2019 uses TAI)
    // TAI = UTC + TAI_UTC_offset
    int64_t tai_seconds = utc_seconds + TAI_UTC_OFFSET_SECONDS;
    
    // Convert centiseconds to nanoseconds
    // Centiseconds have 10ms resolution
    int64_t centisecond_ns = static_cast<int64_t>(gps_data.centiseconds) * 10000000LL;
    
    // Create PTP timestamp
    ptp_timestamp.seconds = static_cast<uint64_t>(tai_seconds);
    ptp_timestamp.nanoseconds = static_cast<uint32_t>(centisecond_ns);
    
    return true;
}

int64_t GPSTimeConverter::calculate_clock_offset(const PTPTimestamp& gps_time,
                                                 const PTPTimestamp& local_time) {
    // Offset = GPS_time - Local_time
    // Positive offset means local clock is behind GPS
    int64_t gps_ns = gps_time.to_nanoseconds();
    int64_t local_ns = local_time.to_nanoseconds();
    
    return gps_ns - local_ns;
}

int64_t GPSTimeConverter::estimate_time_uncertainty(const NMEA::GPSTimeData& gps_data) {
    // Base uncertainty from NMEA time resolution (centiseconds = 10ms)
    int64_t base_uncertainty_ns = 10000000LL;  // 10ms in nanoseconds
    
    // Adjust based on GPS quality
    int64_t quality_factor = 1;
    
    switch (gps_data.fix_status) {
        case NMEA::GPSFixStatus::NO_FIX:
        case NMEA::GPSFixStatus::SIGNAL_LOST:
            // No fix - very high uncertainty
            return 1000000000LL;  // 1 second
            
        case NMEA::GPSFixStatus::TIME_ONLY:
            // Time-only mode - moderate uncertainty
            quality_factor = 10;  // 100ms
            break;
            
        case NMEA::GPSFixStatus::AUTONOMOUS_FIX:
            // Standard GPS fix - good timing
            quality_factor = 5;   // 50ms
            break;
            
        case NMEA::GPSFixStatus::DGPS_FIX:
            // DGPS - excellent timing
            quality_factor = 1;   // 10ms
            break;
    }
    
    // Adjust based on number of satellites
    if (gps_data.satellites >= 8) {
        quality_factor /= 2;  // Excellent satellite coverage
    } else if (gps_data.satellites >= 5) {
        quality_factor = quality_factor;  // Good coverage
    } else if (gps_data.satellites >= 3) {
        quality_factor *= 2;  // Marginal coverage
    } else {
        quality_factor *= 5;  // Poor coverage
    }
    
    return base_uncertainty_ns * quality_factor;
}

} // namespace Time
} // namespace GPS
