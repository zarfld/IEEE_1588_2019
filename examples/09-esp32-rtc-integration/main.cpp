/**
 * @file main.cpp
 * @brief ESP32 RTC DS3231 Integration Example with IEEE 1588-2019 PTP
 * 
 * Demonstrates real-world DS3231 RTC synchronization on ESP32 hardware
 * using the IEEE 1588-2019 PTP library with Arduino framework.
 * 
 * Hardware Requirements:
 * - ESP32 development board
 * - AZDelivery RTC DS3231 I2C module
 * - Wiring:
 *   - DS3231 VCC → ESP32 3.3V
 *   - DS3231 GND → ESP32 GND
 *   - DS3231 SDA → ESP32 GPIO21 (default)
 *   - DS3231 SCL → ESP32 GPIO22 (default)
 * 
 * Features:
 * - Read current time from DS3231 RTC
 * - Display RTC time via serial monitor
 * - Demonstrate clock quality assessment
 * - Test I2C communication with real hardware
 * - Preparation for GPS + RTC BMCA synchronization
 * 
 * @see examples/07-rtc-module/rtc_adapter.hpp for RTCAdapter class
 * @see IEEE 1588-2019 Section 7.1.3 for ClockQuality definition
 */

#include <Arduino.h>
#include <Wire.h>
#include "../07-rtc-module/rtc_adapter.hpp"

// IEEE 1588-2019 namespace
namespace Types = IEEE::_1588::PTP::_2019::Types;

// Global RTC adapter instance
static RTCAdapter* rtc_adapter = nullptr;

// ====================================================================
// Helper Functions
// ====================================================================

/**
 * @brief Convert IEEE 1588-2019 Timestamp to human-readable string
 */
String timestamp_to_string(const Types::Timestamp& ts) {
    // Convert to Unix time (seconds since 1970-01-01)
    time_t unix_time = static_cast<time_t>(ts.seconds_field);
    struct tm timeinfo;
    gmtime_r(&unix_time, &timeinfo);
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), 
             "%04d-%02d-%02d %02d:%02d:%02d.%09u UTC",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             ts.nanoseconds_field);
    
    return String(buffer);
}

/**
 * @brief Convert IEEE 1588-2019 ClockAccuracy enum to string
 */
const char* accuracy_to_string(Types::ClockAccuracy accuracy) {
    switch (accuracy) {
        case Types::ClockAccuracy::Unknown: return "Unknown";
        case Types::ClockAccuracy::Within_25ns: return "±25ns";
        case Types::ClockAccuracy::Within_100ns: return "±100ns";
        case Types::ClockAccuracy::Within_250ns: return "±250ns";
        case Types::ClockAccuracy::Within_1us: return "±1μs";
        case Types::ClockAccuracy::Within_2_5us: return "±2.5μs";
        case Types::ClockAccuracy::Within_10us: return "±10μs";
        case Types::ClockAccuracy::Within_25us: return "±25μs";
        case Types::ClockAccuracy::Within_100us: return "±100μs";
        case Types::ClockAccuracy::Within_250us: return "±250μs";
        case Types::ClockAccuracy::Within_1ms: return "±1ms";
        case Types::ClockAccuracy::Within_2_5ms: return "±2.5ms";
        case Types::ClockAccuracy::Within_10ms: return "±10ms";
        case Types::ClockAccuracy::Within_25ms: return "±25ms";
        case Types::ClockAccuracy::Within_100ms: return "±100ms";
        case Types::ClockAccuracy::Within_250ms: return "±250ms";
        case Types::ClockAccuracy::Within_1s: return "±1s";
        case Types::ClockAccuracy::Within_10s: return "±10s";
        case Types::ClockAccuracy::Greater_10s: return ">10s";
        default: return "Reserved";
    }
}

/**
 * @brief Display RTC information via serial
 */
void display_rtc_info() {
    Serial.println("\n========================================");
    Serial.println("    DS3231 RTC Status");
    Serial.println("========================================");
    
    // Get current time
    Types::Timestamp current_time = rtc_adapter->get_current_time();
    Serial.print("Current Time: ");
    Serial.println(timestamp_to_string(current_time));
    
    // Get clock quality
    Types::ClockQuality quality = rtc_adapter->get_clock_quality();
    Serial.println("\nClock Quality (IEEE 1588-2019):");
    Serial.print("  Clock Class: ");
    Serial.print(quality.clock_class);
    Serial.print(" (");
    if (quality.clock_class == 52) {
        Serial.print("Synchronized to external source");
    } else if (quality.clock_class == 187) {
        Serial.print("Free-running/holdover");
    } else if (quality.clock_class == 248) {
        Serial.print("Default/unconfigured");
    } else {
        Serial.print("Unknown class");
    }
    Serial.println(")");
    
    Serial.print("  Clock Accuracy: ");
    Serial.println(accuracy_to_string(quality.clock_accuracy));
    
    Serial.print("  Offset Scaled Log Variance: ");
    Serial.println(quality.offset_scaled_log_variance);
    
    Serial.println("========================================\n");
}

/**
 * @brief Scan I2C bus for DS3231 device
 * @return true if DS3231 found at 0x68, false otherwise
 */
bool scan_i2c_bus() {
    Serial.println("Scanning I2C bus...");
    
    Wire.begin(21, 22);  // SDA=GPIO21, SCL=GPIO22
    
    bool device_found = false;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            Serial.print(address, HEX);
            
            if (address == 0x68) {
                Serial.println(" (DS3231 RTC) ✓");
                device_found = true;
            } else if (address == 0x57) {
                Serial.println(" (DS3231 EEPROM)");
            } else {
                Serial.println();
            }
        }
    }
    
    if (!device_found) {
        Serial.println("\n⚠ WARNING: DS3231 RTC not found at 0x68!");
        Serial.println("Check wiring:");
        Serial.println("  - VCC → 3.3V");
        Serial.println("  - GND → GND");
        Serial.println("  - SDA → GPIO21");
        Serial.println("  - SCL → GPIO22");
    }
    
    return device_found;
}

// ====================================================================
// Arduino Setup
// ====================================================================

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);  // Wait for serial to stabilize
    
    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.println("║  ESP32 DS3231 RTC Integration Test    ║");
    Serial.println("║  IEEE 1588-2019 PTP Example           ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    Serial.println("Hardware: ESP32 + AZDelivery DS3231 RTC");
    Serial.println("Framework: Arduino");
    Serial.println("I2C Pins: SDA=GPIO21, SCL=GPIO22");
    Serial.println();
    
    // Scan I2C bus for DS3231
    if (!scan_i2c_bus()) {
        Serial.println("\nERROR: Cannot proceed without DS3231 RTC");
        Serial.println("Halting...");
        while (true) {
            delay(1000);
        }
    }
    
    Serial.println("\nInitializing RTC Adapter...");
    
    // Create RTC adapter for DS3231 at address 0x68
    rtc_adapter = new RTCAdapter(0x68, RTCModuleType::DS3231);
    
    if (!rtc_adapter->initialize()) {
        Serial.println("ERROR: RTC initialization failed!");
        Serial.println("Possible causes:");
        Serial.println("  1. I2C wiring issue");
        Serial.println("  2. DS3231 module defective");
        Serial.println("  3. Pull-up resistors missing");
        while (true) {
            delay(1000);
        }
    }
    
    Serial.println("✓ RTC initialized successfully");
    
    // Display initial RTC status
    display_rtc_info();
    
    // Optional: Set RTC time if not configured
    // Uncomment to set time to compile time
    /*
    Serial.println("Setting RTC to compile time...");
    Types::Timestamp compile_time;
    // Parse __DATE__ and __TIME__ macros
    // (Implementation omitted for brevity - see examples/07-rtc-module)
    rtc_adapter->set_time(compile_time);
    Serial.println("✓ Time set");
    */
    
    Serial.println("Setup complete. Starting main loop...\n");
}

// ====================================================================
// Arduino Main Loop
// ====================================================================

void loop() {
    // Update RTC adapter (reads current time from DS3231)
    rtc_adapter->update();
    
    // Display RTC information every 5 seconds
    static unsigned long last_display = 0;
    unsigned long now = millis();
    
    if (now - last_display >= 5000) {
        display_rtc_info();
        last_display = now;
    }
    
    // Small delay to prevent CPU hogging
    delay(100);
}

// ====================================================================
// Expected Output (Serial Monitor at 115200 baud)
// ====================================================================
/*

╔════════════════════════════════════════╗
║  ESP32 DS3231 RTC Integration Test    ║
║  IEEE 1588-2019 PTP Example           ║
╚════════════════════════════════════════╝

Hardware: ESP32 + AZDelivery DS3231 RTC
Framework: Arduino
I2C Pins: SDA=GPIO21, SCL=GPIO22

Scanning I2C bus...
I2C device found at address 0x68 (DS3231 RTC) ✓
I2C device found at address 0x57 (DS3231 EEPROM)

Initializing RTC Adapter...
✓ RTC initialized successfully

========================================
    DS3231 RTC Status
========================================
Current Time: 2025-11-07 14:32:15.000000000 UTC

Clock Quality (IEEE 1588-2019):
  Clock Class: 248 (Default/unconfigured)
  Clock Accuracy: ±250ms
  Offset Scaled Log Variance: 17258

========================================

Setup complete. Starting main loop...

========================================
    DS3231 RTC Status
========================================
Current Time: 2025-11-07 14:32:20.000000000 UTC

Clock Quality (IEEE 1588-2019):
  Clock Class: 248 (Default/unconfigured)
  Clock Accuracy: ±250ms
  Offset Scaled Log Variance: 17258

========================================

[Updates every 5 seconds...]

*/
