/**
 * @file serial_hal_linux.cpp
 * @brief Linux/macOS Serial Port HAL Implementation using termios
 * 
 * Implements serial port communication for Linux/Unix/macOS platforms using POSIX termios.
 * Supports GPS NMEA-0183 communication over /dev/ttyUSB*, /dev/ttyS* (Linux), /dev/tty.* (macOS), etc.
 * 
 * @note POSIX-compliant implementation - uses termios API on Linux, macOS, and Unix
 * @see serial_hal_interface.hpp for interface documentation
 */

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

#include "serial_hal_interface.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <cstring>
#include <string>

namespace HAL {
namespace Serial {

/**
 * @brief Linux-specific Serial Port Implementation
 */
class SerialInterfaceLinux : public SerialInterface {
private:
    int fd_;                     ///< File descriptor for serial port
    std::string port_name_;      ///< Port name (e.g., "/dev/ttyUSB0")
    SerialConfig config_;        ///< Current configuration
    bool is_open_;               ///< Port open status
    
    /**
     * @brief Convert baud rate to termios speed_t constant
     */
    speed_t baud_to_speed(uint32_t baud) {
        switch (baud) {
            case 50:     return B50;
            case 75:     return B75;
            case 110:    return B110;
            case 134:    return B134;
            case 150:    return B150;
            case 200:    return B200;
            case 300:    return B300;
            case 600:    return B600;
            case 1200:   return B1200;
            case 1800:   return B1800;
            case 2400:   return B2400;
            case 4800:   return B4800;
            case 9600:   return B9600;
            case 19200:  return B19200;
            case 38400:  return B38400;
            case 57600:  return B57600;
            case 115200: return B115200;
            case 230400: return B230400;
            default:     return B9600;  // Default to 9600
        }
    }
    
public:
    SerialInterfaceLinux()
        : fd_(-1)
        , is_open_(false)
    {
    }
    
    ~SerialInterfaceLinux() override {
        close();
    }
    
    SerialError open(const char* port_name, const SerialConfig& config) override {
        if (is_open_) {
            return SerialError::ALREADY_OPEN;
        }
        
        if (!port_name || strlen(port_name) == 0) {
            return SerialError::INVALID_PORT;
        }
        
        // Open port (non-blocking initially)
        fd_ = ::open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd_ < 0) {
            return SerialError::OPEN_FAILED;
        }
        
        // Set blocking mode for reads
        if (fcntl(fd_, F_SETFL, 0) < 0) {
            ::close(fd_);
            fd_ = -1;
            return SerialError::CONFIG_FAILED;
        }
        
        // Get current port settings
        struct termios tty;
        if (tcgetattr(fd_, &tty) != 0) {
            ::close(fd_);
            fd_ = -1;
            return SerialError::CONFIG_FAILED;
        }
        
        // Set baud rate
        speed_t speed = baud_to_speed(config.baud_rate);
        cfsetospeed(&tty, speed);
        cfsetispeed(&tty, speed);
        
        // Configure character size
        tty.c_cflag &= ~CSIZE;
        switch (config.data_bits) {
            case 5: tty.c_cflag |= CS5; break;
            case 6: tty.c_cflag |= CS6; break;
            case 7: tty.c_cflag |= CS7; break;
            case 8: tty.c_cflag |= CS8; break;
            default: tty.c_cflag |= CS8;
        }
        
        // Configure parity
        switch (config.parity) {
            case 'N': case 'n':
                tty.c_cflag &= ~PARENB;
                break;
            case 'E': case 'e':
                tty.c_cflag |= PARENB;
                tty.c_cflag &= ~PARODD;
                break;
            case 'O': case 'o':
                tty.c_cflag |= PARENB;
                tty.c_cflag |= PARODD;
                break;
        }
        
        // Configure stop bits
        if (config.stop_bits == 2) {
            tty.c_cflag |= CSTOPB;
        } else {
            tty.c_cflag &= ~CSTOPB;
        }
        
        // Disable hardware flow control
        tty.c_cflag &= ~CRTSCTS;
        
        // Enable receiver, ignore modem control lines
        tty.c_cflag |= CREAD | CLOCAL;
        
        // Raw input mode (no processing)
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        
        // Disable software flow control
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        
        // Raw output mode
        tty.c_oflag &= ~OPOST;
        
        // Set timeouts (deciseconds)
        tty.c_cc[VMIN] = 0;                              // Non-blocking read
        tty.c_cc[VTIME] = (config.timeout_ms + 99) / 100; // Convert ms to deciseconds
        
        // Apply settings
        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            ::close(fd_);
            fd_ = -1;
            return SerialError::CONFIG_FAILED;
        }
        
        // Success
        port_name_ = port_name;
        config_ = config;
        is_open_ = true;
        
        return SerialError::SUCCESS;
    }
    
    void close() override {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        is_open_ = false;
    }
    
    bool is_open() const override {
        return is_open_;
    }
    
    SerialError read(uint8_t* buffer, size_t max_length, size_t& bytes_read) override {
        if (!is_open_) {
            return SerialError::NOT_OPEN;
        }
        
        if (!buffer || max_length == 0) {
            return SerialError::INVALID_PORT;
        }
        
        ssize_t result = ::read(fd_, buffer, max_length);
        
        if (result < 0) {
            return SerialError::READ_FAILED;
        }
        
        bytes_read = static_cast<size_t>(result);
        
        if (result == 0) {
            return SerialError::TIMEOUT;
        }
        
        return SerialError::SUCCESS;
    }
    
    SerialError read_line(char* buffer, size_t max_length) override {
        if (!is_open_) {
            return SerialError::NOT_OPEN;
        }
        
        if (!buffer || max_length < 2) {
            return SerialError::INVALID_PORT;
        }
        
        size_t pos = 0;
        bool found_newline = false;
        
        // Read character by character until newline or timeout
        while (pos < max_length - 1) {
            uint8_t ch;
            size_t bytes_read;
            
            SerialError err = read(&ch, 1, bytes_read);
            
            if (err == SerialError::TIMEOUT) {
                if (pos > 0) {
                    // Partial data received - return what we have
                    break;
                }
                return SerialError::TIMEOUT;
            }
            
            if (err != SerialError::SUCCESS) {
                return err;
            }
            
            // Check for newline (NMEA uses \r\n)
            if (ch == '\n') {
                found_newline = true;
                break;
            }
            
            // Ignore carriage return
            if (ch == '\r') {
                continue;
            }
            
            buffer[pos++] = static_cast<char>(ch);
        }
        
        // Null-terminate
        buffer[pos] = '\0';
        
        if (pos == max_length - 1 && !found_newline) {
            return SerialError::BUFFER_OVERFLOW;
        }
        
        return SerialError::SUCCESS;
    }
    
    SerialError write(const uint8_t* buffer, size_t length, size_t& bytes_written) override {
        if (!is_open_) {
            return SerialError::NOT_OPEN;
        }
        
        if (!buffer || length == 0) {
            return SerialError::INVALID_PORT;
        }
        
        ssize_t result = ::write(fd_, buffer, length);
        
        if (result < 0) {
            return SerialError::WRITE_FAILED;
        }
        
        bytes_written = static_cast<size_t>(result);
        return SerialError::SUCCESS;
    }
    
    SerialError flush_receive() override {
        if (!is_open_) {
            return SerialError::NOT_OPEN;
        }
        
        if (tcflush(fd_, TCIFLUSH) != 0) {
            return SerialError::READ_FAILED;
        }
        
        return SerialError::SUCCESS;
    }
    
    const char* get_port_name() const override {
        return port_name_.c_str();
    }
    
    const SerialConfig& get_config() const override {
        return config_;
    }
};

// Factory function implementation for Linux
SerialInterface* create_serial_interface() {
    return new SerialInterfaceLinux();
}

} // namespace Serial
} // namespace HAL

#endif // __linux__ || __unix__ || __APPLE__
