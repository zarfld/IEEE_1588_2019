/**
 * @file serial_hal_windows.cpp
 * @brief Windows Serial Port HAL Implementation using Win32 API
 * 
 * Implements serial port communication for Windows platforms using Win32 CreateFile/ReadFile/WriteFile.
 * Supports GPS NMEA-0183 communication over COM ports and USB-Serial adapters.
 * 
 * @note Windows-specific implementation - not portable
 * @see serial_hal_interface.hpp for interface documentation
 */

#ifdef _WIN32

#include "serial_hal_interface.hpp"
#include <windows.h>
#include <string>
#include <cstring>

namespace HAL {
namespace Serial {

/**
 * @brief Windows-specific Serial Port Implementation
 */
class SerialInterfaceWindows : public SerialInterface {
private:
    HANDLE port_handle_;          ///< Windows file handle for COM port
    std::string port_name_;       ///< Port name (e.g., "COM3")
    SerialConfig config_;         ///< Current configuration
    bool is_open_;                ///< Port open status
    
public:
    SerialInterfaceWindows()
        : port_handle_(INVALID_HANDLE_VALUE)
        , is_open_(false)
    {
    }
    
    ~SerialInterfaceWindows() override {
        close();
    }
    
    SerialError open(const char* port_name, const SerialConfig& config) override {
        if (is_open_) {
            return SerialError::ALREADY_OPEN;
        }
        
        if (!port_name || strlen(port_name) == 0) {
            return SerialError::INVALID_PORT;
        }
        
        // Windows requires "\\.\\" prefix for COM ports > COM9
        std::string full_port_name = "\\\\.\\";
        full_port_name += port_name;
        
        // Open COM port
        port_handle_ = CreateFileA(
            full_port_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,                          // No sharing
            NULL,                       // No security attributes
            OPEN_EXISTING,             // Port must exist
            FILE_ATTRIBUTE_NORMAL,     // Normal file
            NULL                       // No template
        );
        
        if (port_handle_ == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            // Common errors:
            // ERROR_FILE_NOT_FOUND (2): Port doesn't exist
            // ERROR_ACCESS_DENIED (5): Port already open
            return SerialError::OPEN_FAILED;
        }
        
        // Configure port
        DCB dcb = {0};
        dcb.DCBlength = sizeof(DCB);
        
        if (!GetCommState(port_handle_, &dcb)) {
            CloseHandle(port_handle_);
            port_handle_ = INVALID_HANDLE_VALUE;
            return SerialError::CONFIG_FAILED;
        }
        
        // Set parameters
        dcb.BaudRate = config.baud_rate;
        dcb.ByteSize = config.data_bits;
        dcb.StopBits = (config.stop_bits == 1) ? ONESTOPBIT : TWOSTOPBITS;
        
        switch (config.parity) {
            case 'N': case 'n':
                dcb.Parity = NOPARITY;
                break;
            case 'E': case 'e':
                dcb.Parity = EVENPARITY;
                break;
            case 'O': case 'o':
                dcb.Parity = ODDPARITY;
                break;
            default:
                dcb.Parity = NOPARITY;
        }
        
        // Disable flow control for GPS (standard NMEA)
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        
        if (!SetCommState(port_handle_, &dcb)) {
            CloseHandle(port_handle_);
            port_handle_ = INVALID_HANDLE_VALUE;
            return SerialError::CONFIG_FAILED;
        }
        
        // Set timeouts
        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;                  // Max time between chars (ms)
        timeouts.ReadTotalTimeoutMultiplier = 10;           // Multiplier per byte
        timeouts.ReadTotalTimeoutConstant = config.timeout_ms; // Base timeout
        timeouts.WriteTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 1000;
        
        if (!SetCommTimeouts(port_handle_, &timeouts)) {
            CloseHandle(port_handle_);
            port_handle_ = INVALID_HANDLE_VALUE;
            return SerialError::CONFIG_FAILED;
        }
        
        // Success
        port_name_ = port_name;
        config_ = config;
        is_open_ = true;
        
        return SerialError::SUCCESS;
    }
    
    void close() override {
        if (port_handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(port_handle_);
            port_handle_ = INVALID_HANDLE_VALUE;
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
        
        DWORD read = 0;
        if (!ReadFile(port_handle_, buffer, static_cast<DWORD>(max_length), &read, NULL)) {
            DWORD error = GetLastError();
            return SerialError::READ_FAILED;
        }
        
        bytes_read = static_cast<size_t>(read);
        
        if (read == 0) {
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
                // Timeout without complete line
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
        
        DWORD written = 0;
        if (!WriteFile(port_handle_, buffer, static_cast<DWORD>(length), &written, NULL)) {
            return SerialError::WRITE_FAILED;
        }
        
        bytes_written = static_cast<size_t>(written);
        return SerialError::SUCCESS;
    }
    
    SerialError flush_receive() override {
        if (!is_open_) {
            return SerialError::NOT_OPEN;
        }
        
        if (!PurgeComm(port_handle_, PURGE_RXCLEAR)) {
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

// Factory function implementation for Windows
SerialInterface* create_serial_interface() {
    return new SerialInterfaceWindows();
}

} // namespace Serial
} // namespace HAL

#endif // _WIN32
