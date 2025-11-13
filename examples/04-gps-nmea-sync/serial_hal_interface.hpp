/**
 * @file serial_hal_interface.hpp
 * @brief Hardware Abstraction Layer for Serial Port Communication
 * 
 * Provides platform-independent interface for serial port operations.
 * Used for GPS NMEA-0183 communication over RS-232/USB-Serial.
 * 
 * @note This follows IEEE 1588-2019 hardware abstraction principles
 * @see GPS_NMEA_Integration_Spec.md
 */

#ifndef SERIAL_HAL_INTERFACE_HPP
#define SERIAL_HAL_INTERFACE_HPP

#include <cstddef>
#include <cstdint>

namespace HAL {
namespace Serial {

/**
 * @brief Serial port configuration parameters
 * 
 * Standard GPS NMEA configuration: 9600 baud, 8 data bits, no parity, 1 stop bit (8N1)
 */
struct SerialConfig {
    uint32_t baud_rate;      ///< Baud rate (e.g., 9600, 115200)
    uint8_t data_bits;       ///< Data bits: 7 or 8
    uint8_t stop_bits;       ///< Stop bits: 1 or 2
    char parity;             ///< Parity: 'N' (none), 'E' (even), 'O' (odd)
    uint32_t timeout_ms;     ///< Read timeout in milliseconds
    
    /**
     * @brief Get default GPS NMEA configuration
     * 
     * Standard: 9600 baud, 8N1, 1 second timeout
     */
    static SerialConfig gps_nmea_default() {
        SerialConfig config;
        config.baud_rate = 9600;
        config.data_bits = 8;
        config.stop_bits = 1;
        config.parity = 'N';
        config.timeout_ms = 1000;
        return config;
    }
};

/**
 * @brief Serial port error codes
 */
enum class SerialError {
    SUCCESS = 0,              ///< Operation successful
    INVALID_PORT = -1,        ///< Invalid port name or handle
    OPEN_FAILED = -2,         ///< Failed to open serial port
    CONFIG_FAILED = -3,       ///< Failed to configure serial port
    READ_FAILED = -4,         ///< Read operation failed
    WRITE_FAILED = -5,        ///< Write operation failed
    TIMEOUT = -6,             ///< Operation timed out
    BUFFER_OVERFLOW = -7,     ///< Buffer too small for data
    NOT_OPEN = -8,            ///< Port not opened
    ALREADY_OPEN = -9         ///< Port already opened
};

/**
 * @brief Abstract Serial Port Interface
 * 
 * Platform-independent serial communication interface.
 * Implementations:
 * - Windows: serial_hal_windows.cpp (Win32 API)
 * - Linux: serial_hal_linux.cpp (termios)
 * - Embedded: User-provided (UART drivers)
 */
class SerialInterface {
public:
    virtual ~SerialInterface() = default;
    
    /**
     * @brief Open serial port with configuration
     * 
     * @param port_name Platform-specific port name:
     *                  - Windows: "COM3", "COM4", etc.
     *                  - Linux: "/dev/ttyUSB0", "/dev/ttyS0", etc.
     * @param config Serial port configuration (baud, parity, etc.)
     * @return SerialError::SUCCESS on success, error code otherwise
     * 
     * @note Must be called before any read/write operations
     */
    virtual SerialError open(const char* port_name, const SerialConfig& config) = 0;
    
    /**
     * @brief Close serial port
     * 
     * Releases port resources. Safe to call multiple times.
     */
    virtual void close() = 0;
    
    /**
     * @brief Check if port is open and ready
     * 
     * @return true if port is open, false otherwise
     */
    virtual bool is_open() const = 0;
    
    /**
     * @brief Read data from serial port (non-blocking with timeout)
     * 
     * @param buffer Output buffer for received data
     * @param max_length Maximum bytes to read
     * @param bytes_read [out] Actual number of bytes read
     * @return SerialError::SUCCESS on success, error code otherwise
     * 
     * @note Timeout is specified in SerialConfig
     * @note Returns SerialError::TIMEOUT if no data within timeout period
     */
    virtual SerialError read(uint8_t* buffer, size_t max_length, size_t& bytes_read) = 0;
    
    /**
     * @brief Read complete line from serial port
     * 
     * Reads until newline character (\\n) or timeout.
     * For GPS NMEA: Sentences end with \\r\\n
     * 
     * @param buffer Output buffer for line (null-terminated)
     * @param max_length Maximum bytes to read (including null terminator)
     * @return SerialError::SUCCESS on success, error code otherwise
     * 
     * @note Strips \\r\\n from end of line
     * @note Adds null terminator
     * @note Returns SerialError::BUFFER_OVERFLOW if line too long
     */
    virtual SerialError read_line(char* buffer, size_t max_length) = 0;
    
    /**
     * @brief Write data to serial port
     * 
     * @param buffer Data to transmit
     * @param length Number of bytes to write
     * @param bytes_written [out] Actual number of bytes written
     * @return SerialError::SUCCESS on success, error code otherwise
     * 
     * @note Not typically used for GPS receivers (read-only)
     */
    virtual SerialError write(const uint8_t* buffer, size_t length, size_t& bytes_written) = 0;
    
    /**
     * @brief Flush receive buffer
     * 
     * Discards any unread data in receive buffer.
     * Useful for synchronizing to start of NMEA sentence.
     * 
     * @return SerialError::SUCCESS on success, error code otherwise
     */
    virtual SerialError flush_receive() = 0;
    
    /**
     * @brief Get port name
     * 
     * @return Port name string (e.g., "COM3", "/dev/ttyUSB0")
     */
    virtual const char* get_port_name() const = 0;
    
    /**
     * @brief Get current configuration
     * 
     * @return Current serial port configuration
     */
    virtual const SerialConfig& get_config() const = 0;
};

/**
 * @brief Create platform-specific serial interface
 * 
 * Factory function that returns appropriate implementation:
 * - Windows: SerialInterfaceWindows
 * - Linux: SerialInterfaceLinux
 * - Embedded: User must provide implementation
 * 
 * @return Pointer to SerialInterface implementation
 * 
 * @note Caller is responsible for deleting returned pointer
 * 
 * @example
 * @code
 * auto serial = HAL::Serial::create_serial_interface();
 * SerialConfig config = SerialConfig::gps_nmea_default();
 * if (serial->open("COM3", config) == SerialError::SUCCESS) {
 *     char line[256];
 *     if (serial->read_line(line, sizeof(line)) == SerialError::SUCCESS) {
 *         printf("Received: %s\\n", line);
 *     }
 *     serial->close();
 * }
 * delete serial;
 * @endcode
 */
SerialInterface* create_serial_interface();

/**
 * @brief Convert SerialError to string
 * 
 * @param error Error code
 * @return Human-readable error description
 */
inline const char* error_to_string(SerialError error) {
    switch (error) {
        case SerialError::SUCCESS:          return "Success";
        case SerialError::INVALID_PORT:     return "Invalid port";
        case SerialError::OPEN_FAILED:      return "Failed to open port";
        case SerialError::CONFIG_FAILED:    return "Failed to configure port";
        case SerialError::READ_FAILED:      return "Read failed";
        case SerialError::WRITE_FAILED:     return "Write failed";
        case SerialError::TIMEOUT:          return "Timeout";
        case SerialError::BUFFER_OVERFLOW:  return "Buffer overflow";
        case SerialError::NOT_OPEN:         return "Port not open";
        case SerialError::ALREADY_OPEN:     return "Port already open";
        default:                            return "Unknown error";
    }
}

} // namespace Serial
} // namespace HAL

#endif // SERIAL_HAL_INTERFACE_HPP
