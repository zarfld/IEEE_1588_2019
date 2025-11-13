/**
 * @file nmea_parser.cpp
 * @brief NMEA-0183 GPS Sentence Parser Implementation
 * 
 * Implements GPRMC and GPGGA sentence parsing with checksum validation.
 */

#include "nmea_parser.hpp"
#include <cstring>
#include <cstdlib>

namespace GPS {
namespace NMEA {

NMEAParser::NMEAParser()
    : current_fix_status_(GPSFixStatus::NO_FIX)
    , sentences_since_fix_(0)
    , gprmc_count_(0)
    , gpgga_count_(0)
    , checksum_errors_(0)
{
}

void NMEAParser::reset() {
    current_fix_status_ = GPSFixStatus::NO_FIX;
    sentences_since_fix_ = 0;
    gprmc_count_ = 0;
    gpgga_count_ = 0;
    checksum_errors_ = 0;
}

bool NMEAParser::validate_checksum(const char* sentence) {
    if (!sentence || sentence[0] != '$') {
        return false;
    }
    
    // Find checksum delimiter '*'
    const char* star = strchr(sentence, '*');
    if (!star) {
        return false;
    }
    
    // Calculate XOR checksum of data between '$' and '*'
    uint8_t calculated_checksum = 0;
    for (const char* p = sentence + 1; p < star; ++p) {
        calculated_checksum ^= static_cast<uint8_t>(*p);
    }
    
    // Parse expected checksum (2-digit hex after '*')
    char checksum_str[3] = {0};
    checksum_str[0] = *(star + 1);
    checksum_str[1] = *(star + 2);
    
    uint32_t expected_checksum = strtoul(checksum_str, nullptr, 16);
    
    return calculated_checksum == expected_checksum;
}

bool NMEAParser::extract_field(const char*& sentence, char* field_buffer, size_t max_length) {
    if (!sentence || !field_buffer || max_length == 0) {
        return false;
    }
    
    size_t i = 0;
    
    // Copy characters until comma, asterisk, or end of string
    while (*sentence && *sentence != ',' && *sentence != '*' && i < max_length - 1) {
        field_buffer[i++] = *sentence++;
    }
    
    field_buffer[i] = '\0';
    
    // Skip comma delimiter
    if (*sentence == ',') {
        ++sentence;
    }
    
    return true;
}

bool NMEAParser::parse_time(const char* time_str, uint8_t& hours, uint8_t& minutes,
                           uint8_t& seconds, uint16_t& centiseconds) {
    if (!time_str || strlen(time_str) < 6) {
        return false;
    }
    
    // Parse hhmmss (first 6 characters)
    char temp[3] = {0};
    
    // Hours
    temp[0] = time_str[0];
    temp[1] = time_str[1];
    hours = static_cast<uint8_t>(atoi(temp));
    if (hours > 23) return false;
    
    // Minutes
    temp[0] = time_str[2];
    temp[1] = time_str[3];
    minutes = static_cast<uint8_t>(atoi(temp));
    if (minutes > 59) return false;
    
    // Seconds
    temp[0] = time_str[4];
    temp[1] = time_str[5];
    seconds = static_cast<uint8_t>(atoi(temp));
    if (seconds > 59) return false;
    
    // Centiseconds (optional, after decimal point)
    centiseconds = 0;
    if (time_str[6] == '.' && strlen(time_str) >= 9) {
        temp[0] = time_str[7];
        temp[1] = time_str[8];
        centiseconds = static_cast<uint16_t>(atoi(temp));
        if (centiseconds > 99) return false;
    }
    
    return true;
}

bool NMEAParser::parse_date(const char* date_str, uint8_t& day, uint8_t& month, uint16_t& year) {
    if (!date_str || strlen(date_str) < 6) {
        return false;
    }
    
    // Parse ddmmyy
    char temp[3] = {0};
    
    // Day
    temp[0] = date_str[0];
    temp[1] = date_str[1];
    day = static_cast<uint8_t>(atoi(temp));
    if (day < 1 || day > 31) return false;
    
    // Month
    temp[0] = date_str[2];
    temp[1] = date_str[3];
    month = static_cast<uint8_t>(atoi(temp));
    if (month < 1 || month > 12) return false;
    
    // Year (2-digit, assume 2000+)
    temp[0] = date_str[4];
    temp[1] = date_str[5];
    uint8_t year_2digit = static_cast<uint8_t>(atoi(temp));
    year = 2000 + year_2digit;
    
    return true;
}

bool NMEAParser::parse_gprmc(const char* sentence, GPSTimeData& gps_data) {
    // $GPRMC,hhmmss.ss,A/V,lat,N/S,lon,E/W,speed,course,ddmmyy,mag,E/W,mode*HH
    
    char field[32];
    const char* ptr = sentence;
    
    // Skip sentence ID ($GPRMC)
    if (!extract_field(ptr, field, sizeof(field))) return false;
    
    // Field 1: Time (hhmmss.ss)
    if (!extract_field(ptr, field, sizeof(field))) return false;
    if (!parse_time(field, gps_data.hours, gps_data.minutes, 
                   gps_data.seconds, gps_data.centiseconds)) {
        return false;
    }
    gps_data.time_valid = true;
    
    // Field 2: Status (A=active/valid, V=void/warning)
    if (!extract_field(ptr, field, sizeof(field))) return false;
    GPSFixStatus new_status;
    if (field[0] == 'A') {
        new_status = GPSFixStatus::AUTONOMOUS_FIX;
    } else if (field[0] == 'V') {
        new_status = GPSFixStatus::TIME_ONLY;
    } else {
        new_status = GPSFixStatus::NO_FIX;
    }
    
    // Skip fields 3-8 (position, speed, course)
    for (int i = 0; i < 6; ++i) {
        if (!extract_field(ptr, field, sizeof(field))) return false;
    }
    
    // Field 9: Date (ddmmyy)
    if (!extract_field(ptr, field, sizeof(field))) return false;
    if (strlen(field) > 0) {
        if (parse_date(field, gps_data.day, gps_data.month, gps_data.year)) {
            gps_data.date_valid = true;
        }
    }
    
    // Update state machine
    update_fix_status(new_status);
    gps_data.fix_status = current_fix_status_;
    
    ++gprmc_count_;
    return true;
}

bool NMEAParser::parse_gpgga(const char* sentence, GPSTimeData& gps_data) {
    // $GPGGA,hhmmss.ss,lat,N/S,lon,E/W,quality,sats,hdop,alt,M,geoid,M,dgps_age,dgps_id*HH
    
    char field[32];
    const char* ptr = sentence;
    
    // Skip sentence ID ($GPGGA)
    if (!extract_field(ptr, field, sizeof(field))) return false;
    
    // Field 1: Time (hhmmss.ss)
    if (!extract_field(ptr, field, sizeof(field))) return false;
    if (!parse_time(field, gps_data.hours, gps_data.minutes,
                   gps_data.seconds, gps_data.centiseconds)) {
        return false;
    }
    gps_data.time_valid = true;
    
    // Skip fields 2-5 (position)
    for (int i = 0; i < 4; ++i) {
        if (!extract_field(ptr, field, sizeof(field))) return false;
    }
    
    // Field 6: GPS Quality
    if (!extract_field(ptr, field, sizeof(field))) return false;
    int quality_val = atoi(field);
    gps_data.quality = static_cast<GPSQuality>(quality_val);
    
    // Field 7: Number of satellites
    if (!extract_field(ptr, field, sizeof(field))) return false;
    gps_data.satellites = static_cast<uint8_t>(atoi(field));
    
    // Update fix status based on quality
    GPSFixStatus new_status;
    if (quality_val == 0) {
        new_status = GPSFixStatus::NO_FIX;
    } else if (quality_val == 2) {
        new_status = GPSFixStatus::DGPS_FIX;
    } else if (quality_val >= 1) {
        new_status = GPSFixStatus::AUTONOMOUS_FIX;
    } else {
        new_status = GPSFixStatus::TIME_ONLY;
    }
    
    update_fix_status(new_status);
    gps_data.fix_status = current_fix_status_;
    
    ++gpgga_count_;
    return true;
}

void NMEAParser::update_fix_status(GPSFixStatus new_status) {
    // State machine transitions
    if (new_status == GPSFixStatus::NO_FIX) {
        ++sentences_since_fix_;
        
        // If we had a fix and now lost it
        if (current_fix_status_ != GPSFixStatus::NO_FIX && 
            current_fix_status_ != GPSFixStatus::SIGNAL_LOST) {
            current_fix_status_ = GPSFixStatus::SIGNAL_LOST;
        } else if (sentences_since_fix_ > 10) {
            // After 10 sentences without fix, report NO_FIX
            current_fix_status_ = GPSFixStatus::NO_FIX;
        }
    } else {
        // Got a valid status
        sentences_since_fix_ = 0;
        current_fix_status_ = new_status;
    }
}

bool NMEAParser::parse_sentence(const char* sentence, GPSTimeData& gps_data) {
    if (!sentence || sentence[0] != '$') {
        return false;
    }
    
    // Validate checksum first
    if (!validate_checksum(sentence)) {
        ++checksum_errors_;
        return false;
    }
    
    // Initialize GPS data
    gps_data = GPSTimeData();
    
    // Determine sentence type and parse
    if (strncmp(sentence, "$GPRMC", 6) == 0) {
        return parse_gprmc(sentence, gps_data);
    } else if (strncmp(sentence, "$GPGGA", 6) == 0) {
        return parse_gpgga(sentence, gps_data);
    }
    
    // Unknown sentence type (not an error, just ignore)
    return false;
}

} // namespace NMEA
} // namespace GPS
