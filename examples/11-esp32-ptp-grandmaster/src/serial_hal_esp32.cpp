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

#include "serial_hal_esp32.hpp"
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

// Constructor implementation
ESP32SerialPort::ESP32SerialPort(int uart_number, int rx_gpio, int tx_gpio)
    : uart(nullptr), uart_num(uart_number), rx_pin(rx_gpio), tx_pin(tx_gpio), initialized(false) {
    
    // Allocate UART instance based on uart_number
    // Note: Must be careful with Serial macro in Arduino
    switch (uart_num) {
        case 0:
            uart = &(::Serial);    // UART0 (USB) - use :: to avoid macro issues
            break;
        case 1:
            uart = &(::Serial1);   // UART1
            break;
        case 2:
            uart = &(::Serial2);   // UART2 (recommended for GPS)
            break;
        default:
            uart = &(::Serial2);   // Default to UART2
            uart_num = 2;
            break;
    }
}
    
/**
 * @brief Open serial port with configuration
 */
SerialError ESP32SerialPort::open(const char* port_name, const SerialConfig& cfg) {
    config = cfg;
    
    // Store port name
    strncpy(port_name_, port_name ? port_name : "ESP32_UART", sizeof(port_name_) - 1);
    port_name_[sizeof(port_name_) - 1] = '\0';
    
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
    return SerialError::SUCCESS;
}
    
/**
 * @brief Close serial port
 */
void ESP32SerialPort::close() {
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
 * @return SerialError::SUCCESS if read successful
 */
SerialError ESP32SerialPort::read(uint8_t* buffer, size_t size, size_t& bytes_read) {
    if (!initialized) {
        bytes_read = 0;
        return SerialError::NOT_OPEN;
    }
    
    // Read available data (non-blocking)
    size_t avail = uart->available();
    if (avail == 0) {
        bytes_read = 0;
        return SerialError::TIMEOUT;
    }
    
    // Read up to 'size' bytes
    size_t to_read = (avail < size) ? avail : size;
    bytes_read = uart->readBytes(buffer, to_read);
    
    return SerialError::SUCCESS;
}

/**
 * @brief Write data to serial port
 * 
 * @param data Source buffer
 * @param size Bytes to write
 * @param bytes_written Output: actual bytes written
 * @return SerialError::SUCCESS if write successful
 */
SerialError ESP32SerialPort::write(const uint8_t* data, size_t size, size_t& bytes_written) {
    if (!initialized) {
        bytes_written = 0;
        return SerialError::NOT_OPEN;
    }
    
    bytes_written = uart->write(data, size);
    
    if (bytes_written != size) {
        return SerialError::BUFFER_OVERFLOW;
    }
    
    return SerialError::SUCCESS;
}

/**
 * @brief Check if data is available
 */
int ESP32SerialPort::available() {
    if (!initialized) {
        return 0;
    }
    return uart->available();
}

/**
 * @brief Flush TX buffer (wait for transmission complete)
 */
void ESP32SerialPort::flush() {
    if (initialized) {
        uart->flush();
    }
}

/**
 * @brief Check if serial port is open
 */
bool ESP32SerialPort::is_open() const {
    return initialized;
}

/**
 * @brief Read line from serial port (until \n or \r\n)
 * 
 * Useful for NMEA sentences which are line-based.
 * Strips \r\n from end and null-terminates.
 * 
 * @param buffer Destination buffer (char*)
 * @param max_length Maximum buffer size (including null terminator)
 * @return SerialError::SUCCESS if line read successfully
 */
SerialError ESP32SerialPort::read_line(char* buffer, size_t max_length) {
    if (!initialized) {
        return SerialError::NOT_OPEN;
    }
    
    // Byte-by-byte reading for real-time use
    size_t index = 0;
    unsigned long start_time = millis();
    
    while (index < max_length - 1) {  // Leave room for null terminator
        // Check timeout
        if (millis() - start_time > config.timeout_ms) {
            buffer[index] = '\0';
            return SerialError::TIMEOUT;
        }
        
        // Check if data available
        if (uart->available() > 0) {
            char c = uart->read();
            
            // Skip carriage return
            if (c == '\r') {
                continue;
            }
            
            // Check for line terminator
            if (c == '\n') {
                buffer[index] = '\0';
                return SerialError::SUCCESS;
            }
            
            // Store character
            buffer[index++] = c;
        }
        
        // Small delay to prevent CPU hogging
        delayMicroseconds(100);
    }
    
    // Buffer full without finding newline
    buffer[max_length - 1] = '\0';
    return SerialError::BUFFER_OVERFLOW;
}

/**
 * @brief Flush receive buffer
 */
SerialError ESP32SerialPort::flush_receive() {
    if (!initialized) {
        return SerialError::NOT_OPEN;
    }
    
    while (uart->available() > 0) {
        uart->read();
    }
    
    return SerialError::SUCCESS;
}

/**
 * @brief Get port name
 */
const char* ESP32SerialPort::get_port_name() const {
    return port_name_;
}

/**
 * @brief Get current configuration
 */
const SerialConfig& ESP32SerialPort::get_config() const {
    return config;
}

} // namespace Serial
} // namespace HAL

#endif // ESP32
