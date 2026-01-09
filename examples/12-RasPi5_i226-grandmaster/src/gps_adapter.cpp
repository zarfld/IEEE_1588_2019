/**
 * @file gps_adapter.cpp
 * @brief Implementation of GPS Time Source Adapter
 * 
 * © 2026 IEEE 1588-2019 Implementation Project
 */

#include "gps_adapter.hpp"
#include <cstring>
#include <cerrno>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/timex.h>

namespace IEEE {
namespace _1588 {
namespace PTP {
namespace _2019 {
namespace Linux {

// TAI-UTC offset (37 seconds as of 2017, check leap second bulletins)
static const int64_t TAI_UTC_OFFSET = 37;

GpsAdapter::GpsAdapter(const std::string& serial_device,
                       const std::string& pps_device,
                       uint32_t baud_rate)
    : serial_device_(serial_device)
    , pps_device_(pps_device)
    , baud_rate_(baud_rate)
    , serial_fd_(-1)
    , pps_handle_(-1)
{
}

GpsAdapter::~GpsAdapter()
{
    if (serial_fd_ >= 0) {
        close(serial_fd_);
    }
    if (pps_handle_ >= 0) {
        time_pps_destroy(pps_handle_);
    }
}

bool GpsAdapter::initialize()
{
    // Open serial device
    serial_fd_ = open(serial_device_.c_str(), O_RDWR | O_NOCTTY);
    if (serial_fd_ < 0) {
        return false;
    }

    // Try common baud rates to auto-detect GPS configuration
    const speed_t baud_rates[] = { B38400, B115200, B9600, B57600, B19200 };
    const char* baud_names[] = { "38400", "115200", "9600", "57600", "19200" };
    
    bool configured = false;
    std::cout << "  Testing baud rates: ";
    
    for (size_t i = 0; i < 5; i++) {
        std::cout << baud_names[i] << "...";
        std::cout.flush();
        
        // Configure serial port
        struct termios tty{};
        if (tcgetattr(serial_fd_, &tty) != 0) {
            std::cout << "ERR ";
            continue;
        }

        // Set baud rate
        cfsetospeed(&tty, baud_rates[i]);
        cfsetispeed(&tty, baud_rates[i]);

        // 8N1, no parity, 1 stop bit
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cflag |= CREAD | CLOCAL;

        // Raw input
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
        tty.c_oflag &= ~OPOST;

        // Read timeout (short for auto-detection)
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 10; // 1 second timeout

        if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
            std::cout << "CFG_ERR ";
            continue;
        }
        
        // Flush any stale data
        tcflush(serial_fd_, TCIOFLUSH);
        usleep(100000); // 100ms settle time
        
        // Try to read data
        char test_buffer[512];
        ssize_t bytes = read(serial_fd_, test_buffer, sizeof(test_buffer) - 1);
        
        if (bytes > 0) {
            test_buffer[bytes] = '\0';
            
            // Debug: show first few bytes
            std::cout << "(" << bytes << "B:";
            for (int j = 0; j < (bytes < 4 ? bytes : 4); j++) {
                if (test_buffer[j] >= 32 && test_buffer[j] < 127) {
                    std::cout << test_buffer[j];
                } else {
                    std::cout << ".";
                }
            }
            std::cout << ") ";
            
            // Check for valid NMEA sentence marker
            if (strchr(test_buffer, '$') && (strstr(test_buffer, "GP") || strstr(test_buffer, "GN"))) {
                std::cout << "✓\n";
                std::cout << "  GPS detected at " << baud_names[i] << " baud (NMEA mode)\n";
                baud_rate_ = baud_rates[i];
                configured = true;
                break;
            }
        } else {
            std::cout << "(0B) ";
        }
    }
    std::cout << "\n";
    
    if (!configured) {
        std::cerr << "  WARNING: No NMEA data detected. GPS may be in UBX binary mode.\n";
        std::cerr << "  Sending UBX configuration to enable NMEA output...\n";
        
        // Configure for 38400 as fallback (most common for u-blox)
        struct termios tty{};
        tcgetattr(serial_fd_, &tty);
        cfsetospeed(&tty, B38400);
        cfsetispeed(&tty, B38400);
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cflag |= CREAD | CLOCAL;
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
        tty.c_oflag &= ~OPOST;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 10;
        tcsetattr(serial_fd_, TCSANOW, &tty);
        tcflush(serial_fd_, TCIOFLUSH);
        
        // UBX-CFG-MSG: Enable NMEA GGA message (ID 0xF0 0x00) on UART1
        const unsigned char ubx_enable_gga[] = {
            0xB5, 0x62,  // UBX header
            0x06, 0x01,  // CFG-MSG
            0x08, 0x00,  // Length
            0xF0, 0x00,  // NMEA GGA message class/ID
            0x00,        // I2C rate (0 = disabled)
            0x01,        // UART1 rate (1 = enabled every epoch)
            0x00,        // UART2 rate
            0x00,        // USB rate
            0x00,        // SPI rate
            0x00,        // Reserved
            0x00, 0x00   // Checksum (will calculate)
        };
        
        // UBX-CFG-MSG: Enable NMEA RMC message (ID 0xF0 0x04)
        const unsigned char ubx_enable_rmc[] = {
            0xB5, 0x62,  // UBX header
            0x06, 0x01,  // CFG-MSG
            0x08, 0x00,  // Length
            0xF0, 0x04,  // NMEA RMC message class/ID
            0x00,        // I2C rate
            0x01,        // UART1 rate (1 = enabled)
            0x00,        // UART2 rate
            0x00,        // USB rate
            0x00,        // SPI rate
            0x00,        // Reserved
            0x00, 0x00   // Checksum
        };
        
        // Calculate and set checksums for UBX messages
        auto calc_checksum = [](unsigned char* msg, size_t len) {
            unsigned char ck_a = 0, ck_b = 0;
            for (size_t i = 2; i < len - 2; i++) {  // Skip header and checksum bytes
                ck_a += msg[i];
                ck_b += ck_a;
            }
            msg[len-2] = ck_a;
            msg[len-1] = ck_b;
        };
        
        unsigned char cmd_gga[sizeof(ubx_enable_gga)];
        unsigned char cmd_rmc[sizeof(ubx_enable_rmc)];
        memcpy(cmd_gga, ubx_enable_gga, sizeof(ubx_enable_gga));
        memcpy(cmd_rmc, ubx_enable_rmc, sizeof(ubx_enable_rmc));
        calc_checksum(cmd_gga, sizeof(cmd_gga));
        calc_checksum(cmd_rmc, sizeof(cmd_rmc));
        
        // Send UBX commands
        write(serial_fd_, cmd_gga, sizeof(cmd_gga));
        usleep(100000);  // Wait 100ms
        write(serial_fd_, cmd_rmc, sizeof(cmd_rmc));
        usleep(200000);  // Wait 200ms for GPS to reconfigure
        
        tcflush(serial_fd_, TCIFLUSH);  // Flush any stale data
        
        // Verify NMEA output is now enabled
        char verify_buffer[256];
        ssize_t vbytes = read(serial_fd_, verify_buffer, sizeof(verify_buffer) - 1);
        if (vbytes > 0) {
            verify_buffer[vbytes] = '\0';
            if (strchr(verify_buffer, '$')) {
                std::cout << "  ✓ NMEA output enabled successfully\n";
                configured = true;
            } else {
                std::cerr << "  ✗ NMEA enable failed, GPS may need manual configuration\n";
                configured = true;  // Continue anyway
            }
        } else {
            std::cerr << "  ⚠ No response from GPS after UBX configuration\n";
            configured = true;  // Continue anyway
        }
        
        baud_rate_ = B38400;
    }

    // Initialize PPS
    if (!initialize_pps()) {
        return false;
    }

    return true;
}

bool GpsAdapter::update()
{
    // Read and parse NMEA sentences from GPS
    GpsData temp_gps_data = gps_data_;  // Start with current data
    
    if (read_gps_data(&temp_gps_data)) {
        gps_data_ = temp_gps_data;  // Update if successful
        return true;
    }
    
    return false;
}

bool GpsAdapter::initialize_pps()
{
    int pps_fd = open(pps_device_.c_str(), O_RDWR);
    if (pps_fd < 0) {
        return false;
    }

    // Create PPS handle
    if (time_pps_create(pps_fd, &pps_handle_) < 0) {
        close(pps_fd);
        return false;
    }

    // Configure PPS capture
    struct pps_params params{};
    params.mode = PPS_CAPTUREASSERT | PPS_OFFSETASSERT;
    params.assert_offset.tv_sec = 0;
    params.assert_offset.tv_nsec = 0;

    if (time_pps_setparams(pps_handle_, &params) < 0) {
        time_pps_destroy(pps_handle_);
        close(pps_fd);
        pps_handle_ = -1;
        return false;
    }

    return true;
}

bool GpsAdapter::parse_nmea_sentence(const char* sentence, GpsData* gps_data)
{
    if (strncmp(sentence, "$GPRMC", 6) == 0) {
        return parse_gprmc(sentence, gps_data);
    } else if (strncmp(sentence, "$GPGGA", 6) == 0) {
        return parse_gpgga(sentence, gps_data);
    }
    return false;
}

bool GpsAdapter::parse_gprmc(const char* sentence, GpsData* gps_data)
{
    // $GPRMC,hhmmss.ss,A,ddmm.mm,N,dddmm.mm,E,speed,course,ddmmyy,...
    char time_str[16], date_str[16], status;
    
    int fields = sscanf(sentence, "$GPRMC,%10[^,],%c,%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%6s",
                       time_str, &status, date_str);
    
    if (fields < 3 || status != 'A') {
        gps_data->time_valid = false;
        return false;
    }

    // Parse time: hhmmss.ss
    gps_data->hours = (time_str[0] - '0') * 10 + (time_str[1] - '0');
    gps_data->minutes = (time_str[2] - '0') * 10 + (time_str[3] - '0');
    gps_data->seconds = (time_str[4] - '0') * 10 + (time_str[5] - '0');

    // Parse date: ddmmyy
    gps_data->day = (date_str[0] - '0') * 10 + (date_str[1] - '0');
    gps_data->month = (date_str[2] - '0') * 10 + (date_str[3] - '0');
    gps_data->year = 2000 + (date_str[4] - '0') * 10 + (date_str[5] - '0');

    gps_data->time_valid = true;
    return true;
}

bool GpsAdapter::parse_gpgga(const char* sentence, GpsData* gps_data)
{
    // $GPGGA,hhmmss.ss,ddmm.mm,N,dddmm.mm,E,quality,sats,hdop,altitude,...
    char time_str[16];
    int quality, satellites;
    
    int fields = sscanf(sentence, "$GPGGA,%10[^,],%*[^,],%*[^,],%*[^,],%*[^,],%d,%d",
                       time_str, &quality, &satellites);
    
    if (fields < 3) {
        return false;
    }

    gps_data->fix_quality = static_cast<GpsFixQuality>(quality);
    gps_data->satellites = static_cast<uint8_t>(satellites);
    
    // If we have a valid fix (quality > 0), mark data as valid
    if (quality > 0) {
        gps_data->position_valid = true;
    }

    return true;
}

bool GpsAdapter::read_pps_event(PpsData* pps_data)
{
    struct pps_info info{};
    struct timespec timeout{};
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    if (time_pps_fetch(pps_handle_, PPS_TSFMT_TSPEC, &info, &timeout) < 0) {
        pps_data->valid = false;
        return false;
    }

    pps_data->assert_sec = info.assert_timestamp.tv_sec;
    pps_data->assert_nsec = info.assert_timestamp.tv_nsec;
    pps_data->sequence = info.assert_sequence;
    pps_data->valid = true;

    // Estimate jitter (simplified - should use running statistics)
    pps_data->jitter_nsec = 2000; // <2µs typical for good GPS

    return true;
}

bool GpsAdapter::get_ptp_time(uint64_t* seconds, uint32_t* nanoseconds)
{
    GpsData gps_data{};
    PpsData pps_data{};

    // Read GPS time from NMEA
    if (!read_gps_data(&gps_data) || !gps_data.time_valid) {
        return false;
    }

    // Read PPS event
    if (!read_pps_event(&pps_data) || !pps_data.valid) {
        return false;
    }

    // Convert GPS time to PTP TAI timestamp
    // PTP epoch: 1 January 1970 00:00:00 TAI
    // GPS provides UTC, convert to TAI
    
    struct tm gps_tm{};
    gps_tm.tm_year = gps_data.year - 1900;
    gps_tm.tm_mon = gps_data.month - 1;
    gps_tm.tm_mday = gps_data.day;
    gps_tm.tm_hour = gps_data.hours;
    gps_tm.tm_min = gps_data.minutes;
    gps_tm.tm_sec = gps_data.seconds;

    time_t utc_seconds = timegm(&gps_tm);
    
    // Convert UTC to TAI (add leap seconds)
    *seconds = static_cast<uint64_t>(utc_seconds) + TAI_UTC_OFFSET;
    *nanoseconds = pps_data.assert_nsec;

    return true;
}

bool GpsAdapter::read_gps_data(GpsData* gps_data)
{
    char buffer[512];
    ssize_t bytes_read = read(serial_fd_, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        return false;
    }

    buffer[bytes_read] = '\0';

    // Process all complete NMEA sentences in buffer
    bool got_valid_data = false;
    char* current = buffer;
    
    while (current && *current) {
        // Find sentence start
        char* sentence_start = strchr(current, '$');
        if (!sentence_start) break;
        
        // Find sentence end
        char* sentence_end = strchr(sentence_start, '\n');
        if (!sentence_end) break;
        
        // Null-terminate sentence
        *sentence_end = '\0';
        
        // Parse this sentence (accumulates data into gps_data)
        if (parse_nmea_sentence(sentence_start, gps_data)) {
            got_valid_data = true;
        }
        
        // Move to next sentence
        current = sentence_end + 1;
    }

    return got_valid_data;
}

bool GpsAdapter::get_ptp_clock_quality(uint8_t* clock_class, 
                                      uint8_t* clock_accuracy,
                                      uint16_t* offset_scaled_log_variance)
{
    GpsData gps_data{};
    if (!read_gps_data(&gps_data)) {
        // GPS unavailable - report holdover quality
        *clock_class = 187;  // Holdover (within spec)
        *clock_accuracy = 0x31; // >10s
        *offset_scaled_log_variance = 0xFFFF;
        return false;
    }

    // GPS available - report based on fix quality
    if (gps_data.fix_quality >= GpsFixQuality::DGPS_FIX) {
        *clock_class = 6;    // Primary reference (GPS)
        *clock_accuracy = 0x20; // 25ns
        *offset_scaled_log_variance = 0x4E5D; // Typical GPS variance
    } else if (gps_data.fix_quality == GpsFixQuality::GPS_FIX) {
        *clock_class = 7;    // Primary reference degraded
        *clock_accuracy = 0x21; // 100ns
        *offset_scaled_log_variance = 0x5000;
    } else {
        *clock_class = 52;   // Degraded reference A
        *clock_accuracy = 0x31; // >10s
        *offset_scaled_log_variance = 0xFFFF;
    }

    return true;
}

} // namespace Linux
} // namespace _2019
} // namespace PTP
} // namespace _1588
} // namespace IEEE
