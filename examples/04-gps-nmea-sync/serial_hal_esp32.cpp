/**
 * @file serial_hal_esp32.cpp
 * @brief ESP32 Hardware Abstraction Layer for Serial/UART Communication
 * 
 * Implements SerialPortInterface for ESP32 using Arduino HardwareSerial.
 * Used for GPS NMEA-0183 communication with GT-U7 module.
 * 
 * Hardware: ESP32-WROOM-32 with Arduino framework
 * GPS Module: GT-U7 NMEA GPS (9600 baud, 8N1)
 * 
 * @note Works with PlatformIO Arduino ESP32 framework
 * @see serial_hal_interface.hpp for interface definition
 */

#ifdef ESP32

#include "serial_hal_interface.hpp"
#include <HardwareSerial.h>
#include <Arduino.h>

namespace HAL {
namespace Serial {

/**
 * @brief ESP32 Serial Port Implementation using HardwareSerial
 * 
 * ESP32 has 3 hardware UARTs:
 * - UART0: USB (usually reserved for programming/debug)
 * - UART1: Available (default: RX=GPIO9, TX=GPIO10 - often flash pins!)
 * - UART2: Available (default: RX=GPIO16, TX=GPIO17 - safe for GPS)
 * 
 * Recommended GPS Connection:
 * - Use UART2 with custom pins:
 *   - GPS TX → ESP32 RX (GPIO16)
 *   - GPS RX → ESP32 TX (GPIO17) - Optional, for commands
 *   - GPS PPS → ESP32 GPIO (e.g., GPIO4) - For precise timing
 */
class ESP32SerialPort : public SerialPortInterface {
private:
    HardwareSerial* uart;       ///< ESP32 UART instance
    int uart_num;               ///< UART number (0, 1, or 2)
    int rx_pin;                 ///< RX GPIO pin
    int tx_pin;                 ///< TX GPIO pin
    bool initialized;           ///< Initialization state
    SerialConfig config;        ///< Current configuration
    
public:
    /**
     * @brief Constructor for ESP32 serial port
     * 
     * @param uart_number UART number (0, 1, or 2)
     * @param rx_gpio RX GPIO pin number
     * @param tx_gpio TX GPIO pin number
     * 
     * Example:
     * ```cpp
     * // GPS on UART2, RX=GPIO16, TX=GPIO17
     * ESP32SerialPort gps_port(2, 16, 17);
     * ```
     */
    ESP32SerialPort(int uart_number = 2, int rx_gpio = 16, int tx_gpio = 17)
        : uart(nullptr), uart_num(uart_number), rx_pin(rx_gpio), tx_pin(tx_gpio), initialized(false) {
        
        // Allocate UART instance based on uart_number
        switch (uart_num) {
            case 0:
                uart = &Serial;    // UART0 (USB)
                break;
            case 1:
                uart = &Serial1;   // UART1
                break;
            case 2:
                uart = &Serial2;   // UART2 (recommended for GPS)
                break;
            default:
                uart = &Serial2;   // Default to UART2
                uart_num = 2;
                break;
        }
    }
    
    /**
     * @brief Open serial port with configuration
     */
    bool open(const char* port_name, const SerialConfig& cfg) override {
        config = cfg;
        
        // Configure UART with custom pins
        uart->begin(
            config.baud_rate,     // Baud rate
            SERIAL_8N1,          // Data bits, parity, stop bits (8N1)
            rx_pin,              // RX pin
            tx_pin               // TX pin
        );
        
        // Set timeout for read operations
        uart->setTimeout(config.timeout_ms);
        
        initialized = true;
        return true;
    }
    
    /**
     * @brief Close serial port
     */
    void close() override {
        if (initialized) {
            uart->end();
            initialized = false;
        }
    }
    
    /**
     * @brief Read data from serial port
     * 
     * @param buffer Destination buffer
     * @param size Maximum bytes to read
     * @param bytes_read Output: actual bytes read
     * @return SerialError::Success if read successful
     */
    SerialError read(uint8_t* buffer, size_t size, size_t& bytes_read) override {
        if (!initialized) {
            bytes_read = 0;
            return SerialError::NotOpen;
        }
        
        // Read available data (non-blocking)
        size_t available = uart->available();
        if (available == 0) {
            bytes_read = 0;
            return SerialError::Timeout;
        }
        
        // Read up to 'size' bytes
        size_t to_read = (available < size) ? available : size;
        bytes_read = uart->readBytes(buffer, to_read);
        
        return SerialError::Success;
    }
    
    /**
     * @brief Write data to serial port
     * 
     * @param data Source buffer
     * @param size Bytes to write
     * @param bytes_written Output: actual bytes written
     * @return SerialError::Success if write successful
     */
    SerialError write(const uint8_t* data, size_t size, size_t& bytes_written) override {
        if (!initialized) {
            bytes_written = 0;
            return SerialError::NotOpen;
        }
        
        bytes_written = uart->write(data, size);
        
        if (bytes_written != size) {
            return SerialError::WriteError;
        }
        
        return SerialError::Success;
    }
    
    /**
     * @brief Check if data is available
     */
    int available() override {
        if (!initialized) {
            return 0;
        }
        return uart->available();
    }
    
    /**
     * @brief Flush TX buffer (wait for transmission complete)
     */
    void flush() override {
        if (initialized) {
            uart->flush();
        }
    }
    
    /**
     * @brief Check if serial port is open
     */
    bool is_open() const override {
        return initialized;
    }
    
    /**
     * @brief Read line from serial port (until \n or \r\n)
     * 
     * Useful for NMEA sentences which are line-based.
     * 
     * @param buffer Destination buffer
     * @param size Maximum buffer size
     * @param bytes_read Output: actual bytes read (including newline)
     * @return SerialError::Success if line read successfully
     */
    SerialError read_line(uint8_t* buffer, size_t size, size_t& bytes_read) override {
        if (!initialized) {
            bytes_read = 0;
            return SerialError::NotOpen;
        }
        
        // Arduino String readStringUntil() is convenient but uses dynamic allocation
        // For real-time use, prefer byte-by-byte reading
        size_t index = 0;
        unsigned long start_time = millis();
        
        while (index < size - 1) {  // Leave room for null terminator
            // Check timeout
            if (millis() - start_time > config.timeout_ms) {
                buffer[index] = '\0';
                bytes_read = index;
                return SerialError::Timeout;
            }
            
            // Check if data available
            if (uart->available() > 0) {
                uint8_t c = uart->read();
                
                // Store character
                buffer[index++] = c;
                
                // Check for line terminator
                if (c == '\n') {
                    buffer[index] = '\0';
                    bytes_read = index;
                    return SerialError::Success;
                }
            }
            
            // Small delay to prevent CPU hogging
            delayMicroseconds(100);
        }
        
        // Buffer full without finding newline
        buffer[size - 1] = '\0';
        bytes_read = size - 1;
        return SerialError::BufferOverflow;
    }
};

} // namespace Serial
} // namespace HAL

#endif // ESP32
