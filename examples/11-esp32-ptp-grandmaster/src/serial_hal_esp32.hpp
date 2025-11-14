/**
 * @file serial_hal_esp32.hpp
 * @brief ESP32 Serial HAL Header
 */

#ifndef SERIAL_HAL_ESP32_HPP
#define SERIAL_HAL_ESP32_HPP

#include "serial_hal_interface.hpp"
#include <HardwareSerial.h>

namespace HAL {
namespace Serial {

class ESP32SerialPort : public SerialInterface {
private:
    HardwareSerial* uart;
    int uart_num;
    int rx_pin;
    int tx_pin;
    bool initialized;
    SerialConfig config;
    char port_name_[32];
    
public:
    ESP32SerialPort(int uart_number = 2, int rx_gpio = 16, int tx_gpio = 17);
    ~ESP32SerialPort() override = default;
    
    // SerialInterface required methods
    SerialError open(const char* port_name, const SerialConfig& cfg) override;
    void close() override;
    SerialError read(uint8_t* buffer, size_t size, size_t& bytes_read) override;
    SerialError write(const uint8_t* data, size_t size, size_t& bytes_written) override;
    SerialError read_line(char* buffer, size_t max_length) override;
    SerialError flush_receive() override;
    bool is_open() const override;
    const char* get_port_name() const override;
    const SerialConfig& get_config() const override;
    
    // ESP32-specific convenience methods (not in interface)
    int available();
    void flush();
};

} // namespace Serial
} // namespace HAL

#endif // SERIAL_HAL_ESP32_HPP
