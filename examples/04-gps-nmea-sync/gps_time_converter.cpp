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

GPSTimeConverter::ClockQualityAttributes 
GPSTimeConverter::update_clock_quality(
    NMEA::GPSFixStatus gps_fix_status,
    uint8_t pps_state) {
    
    // IEEE 1588-2019 Table 5: clockClass values
    // IEEE 1588-2019 Table 6: clockAccuracy values
    // IEEE 1588-2019 Table 8-2: timeSource enumeration
    
    ClockQualityAttributes quality;
    
    // Determine timeSource and clockClass based on GPS fix
    switch (gps_fix_status) {
        case NMEA::GPSFixStatus::NO_FIX:
        case NMEA::GPSFixStatus::SIGNAL_LOST:
            // No GPS lock - running on internal oscillator
            quality.time_source = 0xA0;   // INTERNAL_OSCILLATOR (IEEE Table 8-2)
            quality.clock_class = 248;    // Default (not traceable, IEEE Table 5)
            quality.clock_accuracy = 0xFE; // Unknown (IEEE Table 6)
            quality.offset_scaled_log_variance = 0xFFFF; // Maximum variance (worst)
            break;
            
        case NMEA::GPSFixStatus::TIME_ONLY:
            // Time-only fix - GPS available but no position
            quality.time_source = 0x20;   // GPS (IEEE Table 8-2)
            quality.clock_class = 248;    // Default (time-only not fully traceable)
            
            // Accuracy depends on PPS availability
            if (pps_state == 2) {  // Locked (DetectionState::Locked)
                quality.clock_accuracy = 0x21;  // 100ns (PPS + NMEA, IEEE Table 6)
                quality.offset_scaled_log_variance = 0x4E5D;  // Good stability
            } else {
                quality.clock_accuracy = 0x31;  // 10ms (NMEA only, IEEE Table 6)
                quality.offset_scaled_log_variance = 0x8000;  // Moderate stability
            }
            break;
            
        case NMEA::GPSFixStatus::AUTONOMOUS_FIX:
            // Standard GPS fix (3D fix, 4+ satellites)
            quality.time_source = 0x20;   // GPS (IEEE Table 8-2)
            quality.clock_class = 6;      // Primary reference (traceable to primary time source)
            
            // Accuracy depends on PPS availability
            if (pps_state == 2) {  // Locked (DetectionState::Locked)
                quality.clock_accuracy = 0x21;  // 100ns (PPS hardware timestamping)
                quality.offset_scaled_log_variance = 0x4E5D;  // Good stability
            } else {
                quality.clock_accuracy = 0x31;  // 10ms (NMEA only)
                quality.offset_scaled_log_variance = 0x8000;  // Moderate stability
            }
            break;
            
        case NMEA::GPSFixStatus::DGPS_FIX:
            // Differential GPS fix - excellent accuracy
            quality.time_source = 0x20;   // GPS (IEEE Table 8-2)
            quality.clock_class = 6;      // Primary reference (traceable)
            
            // Accuracy depends on PPS availability
            if (pps_state == 2) {  // Locked (DetectionState::Locked)
                quality.clock_accuracy = 0x20;  // 25ns (DGPS + PPS, best case)
                quality.offset_scaled_log_variance = 0x4000;  // Excellent stability
            } else {
                quality.clock_accuracy = 0x22;  // 250ns (DGPS without PPS)
                quality.offset_scaled_log_variance = 0x6000;  // Good stability
            }
            break;
    }
    
    // Priority values - typically static, but could be adjusted based on quality
    // Higher quality GPS could use lower priority1 (higher priority in BMCA)
    if (quality.clock_class == 6 && pps_state == 2) {
        quality.priority1 = 100;  // High priority for GPS-locked with PPS
    } else {
        quality.priority1 = 128;  // Default priority
    }
    quality.priority2 = 128;  // Default (typically not changed)
    
    // Store updated quality
    clock_quality_ = quality;
    
    return quality;
}

} // namespace Time
} // namespace GPS
