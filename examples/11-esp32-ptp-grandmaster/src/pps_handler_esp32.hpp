/**
 * @file pps_handler_esp32.hpp
 * @brief ESP32 Hardware Interrupt Handler for GPS 1PPS Signal
 * 
 * Implements high-precision 1PPS (Pulse Per Second) capture using ESP32 GPIO interrupts.
 * The 1PPS signal from GPS provides sub-microsecond timing reference for IEEE 1588-2019.
 * 
 * Hardware: GT-U7 GPS Module PPS pin → ESP32 GPIO
 * Timing: Rising edge triggered, <1μs interrupt latency
 * 
 * @note Critical for achieving IEEE 1588-2019 sub-microsecond synchronization accuracy
 * @see IEEE 1588-2019 Section 7.3.4 - Timestamp point requirements
 */

#ifndef PPS_HANDLER_ESP32_HPP
#define PPS_HANDLER_ESP32_HPP

#ifdef ESP32

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_timer.h>

namespace GPS {
namespace PPS {

/**
 * @brief PPS event data captured on interrupt
 * 
 * Captured atomically in ISR, processed in main loop.
 */
struct PPSEvent {
    uint64_t timestamp_us;      ///< Microsecond timestamp from esp_timer_get_time()
    uint32_t millis_at_pps;     ///< Arduino millis() at PPS event (for correlation)
    volatile bool valid;        ///< Event is valid and not yet processed
    
    PPSEvent() : timestamp_us(0), millis_at_pps(0), valid(false) {}
};

/**
 * @brief ESP32 GPS 1PPS Interrupt Handler
 * 
 * Hardware Timing Characteristics:
 * - GT-U7 PPS pulse: 100ms high, 900ms low (10% duty cycle)
 * - Rising edge aligned to UTC second boundary
 * - Timing accuracy: ±1μs (GPS locked), ±10μs (holdover)
 * - ESP32 interrupt latency: ~500ns to 2μs
 * 
 * Usage:
 * ```cpp
 * PPSHandler pps(GPIO_NUM_4);  // PPS on GPIO4
 * pps.begin();
 * 
 * void loop() {
 *     if (pps.has_event()) {
 *         PPSEvent event = pps.get_event();
 *         uint64_t precise_time = event.timestamp_us;
 *         // Synchronize system clock to GPS 1PPS
 *     }
 * }
 * ```
 */
class PPSHandler {
private:
    gpio_num_t pps_pin;                 ///< GPIO pin for PPS input
    volatile PPSEvent current_event;    ///< Latest PPS event (shared with ISR)
    volatile uint32_t pps_count;        ///< Total PPS pulses received
    volatile uint32_t missed_count;     ///< Missed PPS events (processed too slowly)
    
    // Statistics for monitoring
    volatile uint64_t last_pps_us;      ///< Previous PPS timestamp
    volatile int64_t last_interval_us;  ///< Interval between last two PPS pulses
    volatile bool initialized;
    
    /**
     * @brief GPIO ISR handler (IRAM_ATTR for fast execution)
     * 
     * CRITICAL: This runs in interrupt context with interrupts disabled.
     * - Keep as short as possible (<10μs execution time)
     * - No Serial.print(), no delay(), no malloc()
     * - Only atomic operations and simple assignments
     * 
     * Timing budget:
     * - esp_timer_get_time(): ~300ns
     * - millis(): ~100ns
     * - Variable assignments: ~50ns
     * - Total: <500ns typical, <2μs worst case
     */
    static void ARDUINO_ISR_ATTR pps_isr_handler(void* arg) {
        PPSHandler* handler = reinterpret_cast<PPSHandler*>(arg);
        if (handler == nullptr) return;
        
        // Capture timestamp IMMEDIATELY (highest priority)
        uint64_t timestamp_us = esp_timer_get_time();
        uint32_t millis_now = millis();
        
        // Calculate interval since last PPS
        uint64_t last = handler->last_pps_us;
        if (last != 0) {
            handler->last_interval_us = (int64_t)(timestamp_us - last);
        }
        handler->last_pps_us = timestamp_us;
        
        // Check if previous event was processed
        if (handler->current_event.valid) {
            handler->missed_count++;  // Previous event not yet consumed
        }
        
        // Store new event
        handler->current_event.timestamp_us = timestamp_us;
        handler->current_event.millis_at_pps = millis_now;
        handler->current_event.valid = true;
        
        handler->pps_count++;
    }
    
public:
    /**
     * @brief Constructor
     * @param pin GPIO pin number for PPS input (e.g., GPIO_NUM_4)
     */
    explicit PPSHandler(gpio_num_t pin = GPIO_NUM_4) 
        : pps_pin(pin), pps_count(0), missed_count(0), 
          last_pps_us(0), last_interval_us(0), initialized(false) {
        current_event.valid = false;
    }
    
    /**
     * @brief Initialize GPIO and attach interrupt
     * @return true if successful, false on error
     */
    bool begin() {
        if (initialized) {
            return true;
        }
        
        // Configure GPIO as input with pull-down
        // (PPS signal is HIGH pulse, idle LOW)
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_POSEDGE;      // Trigger on rising edge
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << pps_pin);
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            return false;
        }
        
        // Install GPIO ISR service (if not already installed)
        gpio_install_isr_service(0);
        
        // Attach our ISR handler
        err = gpio_isr_handler_add(pps_pin, pps_isr_handler, this);
        if (err != ESP_OK) {
            return false;
        }
        
        initialized = true;
        return true;
    }
    
    /**
     * @brief Check if new PPS event is available
     * @return true if event ready for processing
     */
    bool has_event() const {
        return current_event.valid;
    }
    
    /**
     * @brief Get and consume latest PPS event
     * @return PPS event data (marks event as consumed)
     */
    PPSEvent get_event() {
        PPSEvent event;
        
        // Disable interrupts briefly to safely copy event
        portENTER_CRITICAL(&mux);
        // Manually copy fields to avoid issues with volatile assignment
        event.timestamp_us = current_event.timestamp_us;
        event.millis_at_pps = current_event.millis_at_pps;
        event.valid = current_event.valid;
        current_event.valid = false;  // Mark as consumed
        portEXIT_CRITICAL(&mux);
        
        return event;
    }
    
    /**
     * @brief Get total PPS pulses received since initialization
     */
    uint32_t get_pps_count() const {
        return pps_count;
    }
    
    /**
     * @brief Get number of missed PPS events (processed too slowly)
     */
    uint32_t get_missed_count() const {
        return missed_count;
    }
    
    /**
     * @brief Get interval between last two PPS pulses in microseconds
     * @return Interval in μs (should be ~1,000,000 μs = 1 second)
     */
    int64_t get_last_interval_us() const {
        return last_interval_us;
    }
    
    /**
     * @brief Check if PPS signal is healthy
     * @return true if interval is within ±100ms of 1 second
     */
    bool is_signal_healthy() const {
        if (last_interval_us == 0) {
            return false;  // No PPS received yet
        }
        
        // Expected: 1,000,000 μs ± 100,000 μs (±10%)
        const int64_t expected = 1000000;
        const int64_t tolerance = 100000;
        
        int64_t diff = last_interval_us - expected;
        return (diff >= -tolerance && diff <= tolerance);
    }
    
    /**
     * @brief Get jitter statistics (standard deviation from 1 second)
     * @return Jitter in microseconds
     */
    int64_t get_jitter_us() const {
        if (last_interval_us == 0) {
            return 0;
        }
        return last_interval_us - 1000000;  // Deviation from perfect 1 second
    }
    
    /**
     * @brief Cleanup and detach interrupt
     */
    void end() {
        if (initialized) {
            gpio_isr_handler_remove(pps_pin);
            initialized = false;
        }
    }
    
    /**
     * @brief Destructor
     */
    ~PPSHandler() {
        end();
    }
    
private:
    static portMUX_TYPE mux;  ///< Spinlock for critical sections
};

// Initialize static spinlock
portMUX_TYPE PPSHandler::mux = portMUX_INITIALIZER_UNLOCKED;

} // namespace PPS
} // namespace GPS

#endif // ESP32
#endif // PPS_HANDLER_ESP32_HPP
