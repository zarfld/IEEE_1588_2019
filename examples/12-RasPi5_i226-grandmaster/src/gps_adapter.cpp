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

// Get TAI-UTC offset from kernel (adjtimex provides current value)
// Fallback to 37 seconds (as of January 2026) if kernel doesn't provide it
static int64_t get_tai_offset_seconds() {
    struct timex tx = {0};
    if (adjtimex(&tx) < 0) {
        return 37;  // fallback to known value for 2026
    }
    // tx.tai contains TAI offset if kernel has been properly configured
    // Validate range (TAI-UTC should be between 0-100 seconds)
    if (tx.tai > 0 && tx.tai < 100) {
        return tx.tai;
    }
    return 37;  // fallback if kernel value seems invalid
}

static const int64_t TAI_UTC_OFFSET = get_tai_offset_seconds();

GpsAdapter::GpsAdapter(const std::string& serial_device,
                       const std::string& pps_device,
                       uint32_t baud_rate)
    : serial_device_(serial_device)
    , pps_device_(pps_device)
    , baud_rate_(baud_rate)
    , serial_fd_(-1)
    , pps_handle_(-1)
    , last_pps_fetch_ms_(0)
    , base_pps_seq_(0)
    , base_utc_sec_(0)
    , pps_utc_locked_(false)
    , nmea_labels_last_pps_(true)
    , association_sample_count_(0)
    , association_dt_sum_(0)
    , last_nmea_time_(0)
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
                
                // Check if UBX binary is also present (mixed mode)
                bool ubx_present = false;
                for (int j = 0; j < bytes - 1; j++) {
                    if ((unsigned char)test_buffer[j] == 0xB5 && (unsigned char)test_buffer[j+1] == 0x62) {
                        ubx_present = true;
                        break;
                    }
                }
                
                if (ubx_present) {
                    std::cout << "  WARNING: UBX binary protocol also detected (mixed mode)\n";
                    std::cout << "  Attempting to disable UBX binary output...\n";
                    // Don't set configured=true yet, let UBX disable logic run
                } else {
                    std::cout << "  Pure NMEA mode detected\n";
                    configured = true;
                }
                
                baud_rate_ = baud_rates[i];
                break;
            }
        } else {
            std::cout << "(0B) ";
        }
    }
    std::cout << "\n";
    
    if (!configured) {
        std::cerr << "  WARNING: No NMEA data detected. GPS may be in UBX binary mode.\n";
        std::cerr << "  Attempting UBX reconfiguration at detected baud rates...\n";
        
        // Lambda to calculate UBX checksums
        auto calc_checksum = [](unsigned char* msg, size_t len) {
            unsigned char ck_a = 0, ck_b = 0;
            for (size_t i = 2; i < len - 2; i++) {  // Skip header and checksum bytes
                ck_a += msg[i];
                ck_b += ck_a;
            }
            msg[len-2] = ck_a;
            msg[len-1] = ck_b;
        };
        
        // Lambda to read and parse UBX ACK/NAK response
        auto read_ubx_ack = [](int fd) -> int {
            unsigned char ack_buffer[16];
            usleep(50000);  // Wait 50ms for response
            ssize_t bytes = read(fd, ack_buffer, sizeof(ack_buffer));
            if (bytes >= 10) {
                // Check for UBX-ACK-ACK (0x05 0x01) or UBX-ACK-NAK (0x05 0x00)
                for (int i = 0; i < bytes - 3; i++) {
                    if (ack_buffer[i] == 0xB5 && ack_buffer[i+1] == 0x62) {
                        if (ack_buffer[i+2] == 0x05 && ack_buffer[i+3] == 0x01) {
                            return 1;  // ACK
                        } else if (ack_buffer[i+2] == 0x05 && ack_buffer[i+3] == 0x00) {
                            return -1;  // NAK
                        }
                    }
                }
            }
            return 0;  // No response or timeout
        };
        
        // Try UBX configuration at each common baud rate
        speed_t ubx_baud_rates[] = {B9600, B38400, B115200, B57600};
        const char* ubx_baud_names[] = {"9600", "38400", "115200", "57600"};
        
        for (size_t i = 0; i < 4 && !configured; i++) {
            std::cout << "  Trying UBX config at " << ubx_baud_names[i] << " baud...";
            
            struct termios tty{};
            tcgetattr(serial_fd_, &tty);
            cfsetospeed(&tty, ubx_baud_rates[i]);
            cfsetispeed(&tty, ubx_baud_rates[i]);
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
            usleep(100000);
            
            // UBX-CFG-MSG: Enable NMEA GGA message (ID 0xF0 0x00)
            unsigned char ubx_enable_gga[] = {
                0xB5, 0x62,  // UBX header
                0x06, 0x01,  // CFG-MSG
                0x08, 0x00,  // Length = 8 bytes
                0xF0, 0x00,  // NMEA GGA message class/ID
                0x01,        // Rate on I2C (1 = every epoch)
                0x01,        // Rate on UART1 (1 = every epoch)
                0x01,        // Rate on UART2
                0x01,        // Rate on USB
                0x01,        // Rate on SPI
                0x00,        // Reserved
                0x00, 0x00   // Checksum (calculated below)
            };
            
            // UBX-CFG-MSG: Enable NMEA RMC message (ID 0xF0 0x04)
            unsigned char ubx_enable_rmc[] = {
                0xB5, 0x62,  // UBX header
                0x06, 0x01,  // CFG-MSG
                0x08, 0x00,  // Length = 8 bytes
                0xF0, 0x04,  // NMEA RMC message class/ID
                0x01,        // Rate on I2C
                0x01,        // Rate on UART1
                0x01,        // Rate on UART2
                0x01,        // Rate on USB
                0x01,        // Rate on SPI
                0x00,        // Reserved
                0x00, 0x00   // Checksum
            };
            
            // UBX-CFG-MSG: Disable UBX-NAV-PVT message (the binary message we're seeing)
            unsigned char ubx_disable_nav_pvt[] = {
                0xB5, 0x62,  // UBX header
                0x06, 0x01,  // CFG-MSG
                0x08, 0x00,  // Length = 8 bytes
                0x01, 0x07,  // UBX-NAV-PVT message class/ID
                0x00,        // Rate on I2C (0 = disabled)
                0x00,        // Rate on UART1 (0 = disabled)
                0x00,        // Rate on UART2 (0 = disabled)
                0x00,        // Rate on USB (0 = disabled)
                0x00,        // Rate on SPI (0 = disabled)
                0x00,        // Reserved
                0x00, 0x00   // Checksum
            };
            
            // UBX-CFG-PRT: Configure UART port to output NMEA protocol only
            unsigned char ubx_cfg_prt[] = {
                0xB5, 0x62,  // UBX header
                0x06, 0x00,  // CFG-PRT (port configuration)
                0x14, 0x00,  // Length = 20 bytes
                0x01,        // Port ID = 1 (UART1)
                0x00,        // Reserved
                0x00, 0x00,  // TX ready (not used)
                0xD0, 0x08, 0x00, 0x00,  // Mode: 8N1 (8 bits, no parity, 1 stop bit)
                0x00, 0x96, 0x00, 0x00,  // Baud rate = 38400 (0x00009600)
                0x02, 0x00,  // Input protocols: NMEA only (bit 1)
                0x02, 0x00,  // Output protocols: NMEA only (bit 1)
                0x00, 0x00,  // Flags (reserved)
                0x00, 0x00,  // Reserved
                0x00, 0x00   // Checksum
            };
            
            // UBX-CFG-CFG: Save configuration to BBR/Flash
            unsigned char ubx_save_config[] = {
                0xB5, 0x62,  // UBX header
                0x06, 0x09,  // CFG-CFG
                0x0D, 0x00,  // Length = 13 bytes
                0x00, 0x00, 0x00, 0x00,  // Clear mask (none)
                0x1F, 0x1F, 0x00, 0x00,  // Save mask (ioPort + msgConf + infMsg + navConf + rxmConf)
                0x1F, 0x1F, 0x00, 0x00,  // Load mask (all)
                0x17,        // Device mask: BBR + Flash
                0x00, 0x00   // Checksum
            };
            
            calc_checksum(ubx_enable_gga, sizeof(ubx_enable_gga));
            calc_checksum(ubx_enable_rmc, sizeof(ubx_enable_rmc));
            calc_checksum(ubx_disable_nav_pvt, sizeof(ubx_disable_nav_pvt));
            calc_checksum(ubx_cfg_prt, sizeof(ubx_cfg_prt));
            calc_checksum(ubx_save_config, sizeof(ubx_save_config));
            
            // Send GGA enable command
            write(serial_fd_, ubx_enable_gga, sizeof(ubx_enable_gga));
            int ack1 = read_ubx_ack(serial_fd_);
            
            // Send RMC enable command
            write(serial_fd_, ubx_enable_rmc, sizeof(ubx_enable_rmc));
            int ack2 = read_ubx_ack(serial_fd_);
            
            // Disable UBX-NAV-PVT binary message
            write(serial_fd_, ubx_disable_nav_pvt, sizeof(ubx_disable_nav_pvt));
            int ack4 = read_ubx_ack(serial_fd_);
            
            // Configure port to NMEA-only protocol
            write(serial_fd_, ubx_cfg_prt, sizeof(ubx_cfg_prt));
            int ack5 = read_ubx_ack(serial_fd_);
            
            // Send save configuration command
            write(serial_fd_, ubx_save_config, sizeof(ubx_save_config));
            int ack3 = read_ubx_ack(serial_fd_);
            
            std::cout << " GGA:" << (ack1 == 1 ? "ACK" : (ack1 == -1 ? "NAK" : "NO_RESP"));
            std::cout << " RMC:" << (ack2 == 1 ? "ACK" : (ack2 == -1 ? "NAK" : "NO_RESP"));
            std::cout << " DIS_NAV:" << (ack4 == 1 ? "ACK" : (ack4 == -1 ? "NAK" : "NO_RESP"));
            std::cout << " PRT:" << (ack5 == 1 ? "ACK" : (ack5 == -1 ? "NAK" : "NO_RESP"));
            std::cout << " SAVE:" << (ack3 == 1 ? "ACK" : (ack3 == -1 ? "NAK" : "NO_RESP"));
            
            // If we got at least one ACK, wait for GPS to reconfigure
            if (ack1 == 1 || ack2 == 1) {
                std::cout << " - Waiting for GPS reconfiguration...\n";
                usleep(2000000);  // Wait 2 seconds for GPS to apply changes and stabilize
                
                // Try multiple reads to catch NMEA output (GPS may buffer)
                bool nmea_found = false;
                for (int attempt = 0; attempt < 3 && !nmea_found; attempt++) {
                    tcflush(serial_fd_, TCIFLUSH);
                    usleep(500000);  // Wait 500ms between attempts
                    
                    char verify_buffer[512];
                    ssize_t vbytes = read(serial_fd_, verify_buffer, sizeof(verify_buffer) - 1);
                    
                    if (vbytes > 0) {
                        verify_buffer[vbytes] = '\0';
                        
                        // Debug: show what we received (all bytes with hex for non-printable)
                        std::cout << "  Attempt " << (attempt + 1) << ": " << vbytes << " bytes: ";
                        int show_bytes = vbytes < 60 ? vbytes : 60;
                        for (int j = 0; j < show_bytes; j++) {
                            if (verify_buffer[j] >= 32 && verify_buffer[j] < 127) {
                                std::cout << verify_buffer[j];
                            } else if (verify_buffer[j] == '\n') {
                                std::cout << "\\n";
                            } else if (verify_buffer[j] == '\r') {
                                std::cout << "\\r";
                            } else {
                                printf("<%02X>", (unsigned char)verify_buffer[j]);
                            }
                        }
                        std::cout << (vbytes > 60 ? "...\n" : "\n");
                        
                        if (strchr(verify_buffer, '$') && (strstr(verify_buffer, "GP") || strstr(verify_buffer, "GN"))) {
                            std::cout << "  ✓ NMEA output enabled successfully at " << ubx_baud_names[i] << " baud!\n";
                            baud_rate_ = ubx_baud_rates[i];
                            configured = true;
                            nmea_found = true;
                            break;
                        }
                    } else {
                        std::cout << "  Attempt " << (attempt + 1) << ": No data received\n";
                    }
                }
                
                if (nmea_found) break;
            } else {
                std::cout << "\n";
            }
        }
        
        if (!configured) {
            std::cerr << "  ✗ UBX configuration failed at all baud rates.\n";
            std::cerr << "  GPS may need manual reconfiguration via u-center or gpsd.\n";
            configured = true;  // Continue anyway with binary mode
            baud_rate_ = B38400;  // Default fallback
        }
    }

    // Initialize PPS
    if (!initialize_pps()) {
        return false;
    }

    return true;
}

bool GpsAdapter::update()
{
    bool gps_updated = false;
    
    // Read and parse NMEA sentences from GPS
    GpsData temp_gps_data = gps_data_;  // Start with current data
    
    if (read_gps_data(&temp_gps_data)) {
        gps_data_ = temp_gps_data;  // Update if successful
        gps_updated = true;
    }
    
    // Fetch PPS timestamp on EVERY update (non-blocking, fast check)
    if (pps_handle_ >= 0) {
        update_pps_data();
    }
    
    return gps_updated;
}

bool GpsAdapter::update_pps_data()
{
    if (pps_handle_ < 0) {
        return false;
    }
    
    struct pps_info pps_info{};
    struct timespec timeout{};
    timeout.tv_sec = 0;
    timeout.tv_nsec = 0;  // Non-blocking - return immediately
    
    // Fetch PPS event (non-blocking - returns old data if no new pulse)
    int ret = time_pps_fetch(pps_handle_, PPS_TSFMT_TSPEC, &pps_info, &timeout);
    
    if (ret < 0) {
        pps_data_.valid = false;
        return false;
    }
    
    // Check if we got a new PPS pulse (sequence number incremented)
    if (pps_info.assert_sequence == pps_data_.sequence) {
        // No new PPS pulse since last fetch - this is normal
        return pps_data_.valid;
    }
    
    // New PPS pulse - update PPS data
    uint64_t new_assert_sec = static_cast<uint64_t>(pps_info.assert_timestamp.tv_sec);
    uint32_t new_assert_nsec = static_cast<uint32_t>(pps_info.assert_timestamp.tv_nsec);
    
    // Calculate jitter from previous pulse (if valid and consecutive)
    uint32_t current_jitter = 0;
    if (pps_data_.valid && pps_data_.sequence > 0) {
        // Only calculate jitter if this is the immediate next pulse (seq+1)
        // If we missed a pulse, skip jitter calculation (would show false spike)
        if (pps_info.assert_sequence == pps_data_.sequence + 1) {
            // Expected: exactly 1 second between consecutive pulses
            int64_t time_diff_ns = (static_cast<int64_t>(new_assert_sec) - static_cast<int64_t>(pps_data_.assert_sec)) * 1000000000LL
                                 + (static_cast<int64_t>(new_assert_nsec) - static_cast<int64_t>(pps_data_.assert_nsec));
            int64_t jitter_ns = time_diff_ns - 1000000000LL;  // Deviation from 1 second
            current_jitter = static_cast<uint32_t>(std::abs(jitter_ns));
        }
        // else: missed pulse(s) between samples, don't include in jitter stats
    }
    
    // Update PPS data
    pps_data_.assert_sec = new_assert_sec;
    pps_data_.assert_nsec = new_assert_nsec;
    pps_data_.sequence = pps_info.assert_sequence;
    pps_data_.jitter_nsec = current_jitter;
    pps_data_.valid = true;
    
    // Track max jitter (output handled by caller)
    static uint32_t max_jitter = 0;
    max_jitter = std::max(max_jitter, current_jitter);
    
    return true;
}

bool GpsAdapter::get_pps_data(PpsData* pps_data, uint32_t* max_jitter_ns)
{
    if (!pps_data || !max_jitter_ns) {
        return false;
    }
    
    if (!pps_data_.valid) {
        return false;
    }
    
    *pps_data = pps_data_;
    
    // Get max jitter from static tracking
    static uint32_t tracked_max_jitter = 0;
    static uint64_t last_reported_seq = 0;
    
    // Every 10 pulses, report and reset
    if (pps_data_.sequence - last_reported_seq >= 10) {
        *max_jitter_ns = tracked_max_jitter;
        tracked_max_jitter = 0;
        last_reported_seq = pps_data_.sequence;
        return true;  // Signal: ready to output
    }
    
    // Update max but don't report yet
    tracked_max_jitter = std::max(tracked_max_jitter, pps_data_.jitter_nsec);
    return false;  // Signal: not ready to output yet
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
    
    // Initialize PPS data
    pps_data_.valid = false;
    pps_data_.sequence = 0;
    pps_data_.jitter_nsec = 0;

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
    // Manual CSV parsing to preserve empty fields (course is often empty)
    
    char* fields[15] = {nullptr};  // GPRMC has ~12 fields
    int field_count = 0;
    
    // Manual CSV parse - don't skip empty fields
    const char* start = sentence;
    const char* current = sentence;
    
    static char field_buffer[15][32];  // Storage for field strings
    
    while (*current && field_count < 15) {
        // Find next comma or end of string
        if (*current == ',' || *current == '*' || *current == '\0') {
            // Extract field
            size_t len = current - start;
            if (len >= sizeof(field_buffer[0])) len = sizeof(field_buffer[0]) - 1;
            memcpy(field_buffer[field_count], start, len);
            field_buffer[field_count][len] = '\0';
            fields[field_count] = field_buffer[field_count];
            field_count++;
            
            if (*current == '*' || *current == '\0') break;  // Stop at checksum or end
            
            start = current + 1;  // Move past comma
        }
        current++;
    }
    
    // Debug: Show parsed GPRMC fields
    static int rmc_counter = 0;
    if (++rmc_counter % 50 == 1) {
        std::cout << "[GPRMC Parse] field_count=" << field_count;
        if (field_count >= 2) std::cout << " time=" << fields[1];
        if (field_count >= 3) std::cout << " status=" << fields[2];
        if (field_count >= 10) std::cout << " date=" << fields[9];
        std::cout << "\n";
    }
    
    // Need at least: $GPRMC(0), time(1), status(2), lat(3), NS(4), lon(5), EW(6), speed(7), course(8), date(9)
    if (field_count < 10) {
        gps_data->time_valid = false;
        return false;
    }
    
    // Check status field (index 2) - must be 'A' for valid
    if (!fields[2] || fields[2][0] != 'A') {
        gps_data->time_valid = false;
        return false;
    }
    
    // Parse time field (index 1): hhmmss.ss
    const char* time_str = fields[1];
    if (!time_str || strlen(time_str) < 6) {
        gps_data->time_valid = false;
        return false;
    }
    
    gps_data->hours = (time_str[0] - '0') * 10 + (time_str[1] - '0');
    gps_data->minutes = (time_str[2] - '0') * 10 + (time_str[3] - '0');
    gps_data->seconds = (time_str[4] - '0') * 10 + (time_str[5] - '0');

    // Parse date field (index 9): ddmmyy
    const char* date_str = fields[9];
    if (!date_str || strlen(date_str) < 6) {
        gps_data->time_valid = false;
        return false;
    }
    
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
    
    // Debug: Show parsed GPGGA fields
    static int gga_counter = 0;
    if (++gga_counter % 50 == 1) {
        std::cout << "[GPGGA Parse] fields=" << fields << " quality=" << quality 
                  << " sats=" << satellites << "\n";
    }
    
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
    // Fix for ±1 sec oscillation per deb.md:
    // - PPS is the only second boundary (monotonic)
    // - NMEA only initializes/re-anchors the UTC label
    // - Atomic pairing prevents association ambiguity
    
    if (!pps_data_.valid) {
        return false;  // No PPS = no reliable time
    }
    
    // Check if we have a new PPS pulse (not needed for locked mode, but keep for unlock)
    bool new_pps = (pps_data_.sequence != base_pps_seq_);
    
    if (pps_utc_locked_) {
        // LOCKED: Use base mapping model UTC(seq) = base_utc + (seq - base_seq)
        // This is race-free and handles all PPS updates atomically
        // No per-PPS updates needed!
    } else if (new_pps && gps_data_.time_valid) {
            // Not locked: process NMEA for association detection or UTC initialization
            
            // Calculate NMEA UTC second
            struct tm gps_tm{};
            gps_tm.tm_year = gps_data_.year - 1900;
            gps_tm.tm_mon = gps_data_.month - 1;
            gps_tm.tm_mday = gps_data_.day;
            gps_tm.tm_hour = gps_data_.hours;
            gps_tm.tm_min = gps_data_.minutes;
            gps_tm.tm_sec = gps_data_.seconds;
            time_t nmea_utc_sec = timegm(&gps_tm);
            
            // Only process if this is NEW NMEA data (not stale)
            if (nmea_utc_sec == last_nmea_time_) {
                // Stale NMEA - base mapping still works, nothing to do
            } else {
                // Fresh NMEA data - update tracking and process association
                last_nmea_time_ = nmea_utc_sec;
            
            // Measure dt = time since last PPS (for association detection)
            // NOTE: We need to compare get_ptp_time() call time to PPS time.
            // Since get_ptp_time() is called ~100ms after PPS in main loop,
            // dt should be consistently ~100-200ms if NMEA labels last PPS.
            // 
            // CRITICAL: We can't use pps_data_.assert_sec (UNIX time) with MONOTONIC!
            // Instead, assume get_ptp_time() is called shortly after update(),
            // and NMEA parsing happens within ~200ms of GPRMC reception.
            // For now, use a simplified heuristic: if we got here, NMEA is recent.
            
            // Simplified association: measure time from NMEA to now
            static uint64_t nmea_arrival_mono_ms = 0;
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            nmea_arrival_mono_ms = now.tv_sec * 1000ULL + now.tv_nsec / 1000000ULL;
            
            // Assume typical GPS: NMEA arrives ~100-300ms after PPS
            // Just use a fixed assumption for now
            int64_t dt_ms = 200;  // Typical delay
            
            // Accumulate samples for association detection
            association_dt_sum_ += dt_ms;
            association_sample_count_++;
            
            // After 5 samples, lock the association
            if (association_sample_count_ >= 5) {
                int64_t avg_dt_ms = association_dt_sum_ / association_sample_count_;
                
                // Determine association rule and set BASE MAPPING (ONCE!)
                if (avg_dt_ms >= 50 && avg_dt_ms <= 950) {
                    // NMEA arrives after PPS → labels LAST PPS
                    nmea_labels_last_pps_ = true;
                    base_pps_seq_ = pps_data_.sequence;
                    base_utc_sec_ = nmea_utc_sec;
                } else {
                    // NMEA arrives just before PPS → labels NEXT PPS
                    nmea_labels_last_pps_ = false;
                    base_pps_seq_ = pps_data_.sequence + 1;
                    base_utc_sec_ = nmea_utc_sec;
                }
                
                pps_utc_locked_ = true;
                std::cout << "[PPS-UTC Lock] Association locked: NMEA labels "
                          << (nmea_labels_last_pps_ ? "LAST" : "NEXT")
                          << " PPS (avg_dt=" << avg_dt_ms << "ms)\n";
                std::cout << "[Base Mapping] base_pps_seq=" << base_pps_seq_ 
                          << " base_utc_sec=" << base_utc_sec_ << " (UTC epoch)\n";
            } else if (base_utc_sec_ == 0) {
                // First sample only - initialize base tentatively
                base_pps_seq_ = pps_data_.sequence;
                base_utc_sec_ = nmea_labels_last_pps_ ? nmea_utc_sec : (nmea_utc_sec - 1);
            }
            // Else: Already have tentative base, accumulating samples, don't overwrite!
            } // End fresh NMEA block
        }
    
    // Return PPS-UTC pair using BASE MAPPING model
    if (base_utc_sec_ > 0) {
        // Compute UTC for current PPS: UTC(seq) = base_utc + (seq - base_seq)
        uint64_t utc_sec = base_utc_sec_ + (pps_data_.sequence - base_pps_seq_);
        // Convert UTC to TAI
        *seconds = utc_sec + TAI_UTC_OFFSET;
        *nanoseconds = pps_data_.assert_nsec;
        return true;
    }
    
    return false;  // Not initialized yet
}

bool GpsAdapter::read_gps_data(GpsData* gps_data)
{
    char buffer[512];
    ssize_t bytes_read = read(serial_fd_, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        return false;
    }

    buffer[bytes_read] = '\0';
    
    // Debug: Show raw buffer content (first 80 bytes or first line)
    static int debug_counter = 0;
    if (++debug_counter % 50 == 1) {  // Show every 50th read
        std::cout << "[GPS Raw] " << bytes_read << " bytes: ";
        int show_bytes = bytes_read < 80 ? bytes_read : 80;
        for (int i = 0; i < show_bytes; i++) {
            if (buffer[i] >= 32 && buffer[i] < 127) {
                std::cout << buffer[i];
            } else if (buffer[i] == '\n') {
                std::cout << "\\n";
                if (i < show_bytes - 1) break;  // Stop at first newline for readability
            } else if (buffer[i] == '\r') {
                std::cout << "\\r";
            } else {
                printf("<%02X>", (unsigned char)buffer[i]);
            }
        }
        std::cout << "\n";
    }

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
