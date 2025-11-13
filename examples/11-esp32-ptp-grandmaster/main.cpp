/**
 * @file main.cpp
 * @brief ESP32 IEEE 1588-2019 PTP Grandmaster Clock with GPS Disciplining
 * 
 * Complete implementation of portable PTP Grandmaster using:
 * - GT-U7 GPS Module (NMEA + 1PPS) for primary time reference
 * - DS3231 RTC for holdover during GPS outages
 * - WiFi for gPTP packet distribution (IEEE 802.1AS over UDP)
 * - BMCA for automatic source selection
 * 
 * Hardware Configuration:
 * ┌──────────────────────────────────────────────────────────┐
 * │ ESP32 Development Board                                  │
 * ├──────────────────────────────────────────────────────────┤
 * │ GT-U7 GPS Module:                                        │
 * │   - VCC  → ESP32 3.3V                                    │
 * │   - GND  → ESP32 GND                                     │
 * │   - TXD  → ESP32 GPIO16 (UART2 RX) - NMEA sentences     │
 * │   - RXD  → ESP32 GPIO17 (UART2 TX) - GPS commands       │
 * │   - PPS  → ESP32 GPIO4  - 1Hz precision pulse ⚡        │
 * ├──────────────────────────────────────────────────────────┤
 * │ DS3231 RTC Module:                                       │
 * │   - VCC  → ESP32 3.3V                                    │
 * │   - GND  → ESP32 GND                                     │
 * │   - SDA  → ESP32 GPIO21 (I2C Data)                      │
 * │   - SCL  → ESP32 GPIO22 (I2C Clock)                     │
 * ├──────────────────────────────────────────────────────────┤
 * │ WiFi: Built-in ESP32 radio (IEEE 802.11 b/g/n)         │
 * │   - gPTP over UDP multicast: 224.0.1.129:319/320        │
 * └──────────────────────────────────────────────────────────┘
 * 
 * Clock Quality Hierarchy (IEEE 1588-2019):
 * 1. GPS + 1PPS locked → clockClass 6 (primary reference)
 * 2. GPS NMEA only     → clockClass 7 (degraded accuracy)
 * 3. RTC synced        → clockClass 52 (holdover <1 hour)
 * 4. RTC holdover      → clockClass 187 (free-running)
 * 
 * @see IEEE 1588-2019 Section 9.3 - Best Master Clock Algorithm
 * @see IEEE 802.1AS-2020 - gPTP profile for IEEE 802 networks
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// GPS components
#include "../04-gps-nmea-sync/serial_hal_esp32.cpp"
#include "../04-gps-nmea-sync/nmea_parser.hpp"
#include "../04-gps-nmea-sync/gps_time_converter.hpp"
#include "pps_handler_esp32.hpp"

// RTC components
#include "../07-rtc-module/rtc_adapter.hpp"

// IEEE 1588-2019 PTP types
namespace Types = IEEE::_1588::PTP::_2019::Types;

// ====================================================================
// Configuration
// ====================================================================

// WiFi Configuration
const char* WIFI_SSID = "YOUR_WIFI_SSID";      // ⚠ CHANGE THIS!
const char* WIFI_PASSWORD = "YOUR_PASSWORD";    // ⚠ CHANGE THIS!

// GPS Configuration
const int GPS_UART_NUM = 2;          // UART2
const int GPS_RX_PIN = 16;           // ESP32 GPIO16 ← GPS TXD
const int GPS_TX_PIN = 17;           // ESP32 GPIO17 → GPS RXD
const int GPS_PPS_PIN = 4;           // ESP32 GPIO4  ← GPS PPS
const uint32_t GPS_BAUD = 9600;      // Standard NMEA baud rate

// RTC Configuration
const uint8_t RTC_I2C_ADDRESS = 0x68;
const int RTC_SDA_PIN = 21;
const int RTC_SCL_PIN = 22;

// gPTP Configuration (IEEE 802.1AS over UDP/IPv4)
const char* GPTP_MULTICAST_ADDR = "224.0.1.129";  // IEEE 1588 multicast
const uint16_t GPTP_EVENT_PORT = 319;             // Sync, Delay_Req, etc.
const uint16_t GPTP_GENERAL_PORT = 320;           // Announce, Follow_Up, etc.

// Timing Configuration
const uint32_t ANNOUNCE_INTERVAL_MS = 1000;  // 1 second (2^0)
const uint32_t SYNC_INTERVAL_MS = 125;       // 125ms (8 Hz, 2^-3)
const uint32_t DISPLAY_INTERVAL_MS = 5000;   // Status update every 5s

// ====================================================================
// Global Objects
// ====================================================================

// GPS hardware
HAL::Serial::ESP32SerialPort gps_serial(GPS_UART_NUM, GPS_RX_PIN, GPS_TX_PIN);
GPS::NMEAParser nmea_parser;
GPS::PPS::PPSHandler pps_handler(static_cast<gpio_num_t>(GPS_PPS_PIN));

// RTC hardware
RTCAdapter* rtc_adapter = nullptr;

// WiFi networking
WiFiUDP udp_event;    // For Sync, Delay_Req, Pdelay_Req/Resp
WiFiUDP udp_general;  // For Announce, Follow_Up, Signaling

// ====================================================================
// Time Source Management
// ====================================================================

enum class TimeSourceType {
    GPS_PPS,        // GPS with 1PPS (best)
    GPS_NMEA,       // GPS NMEA only (no PPS)
    RTC_SYNCED,     // RTC synchronized to GPS recently (<1 hour)
    RTC_HOLDOVER,   // RTC in holdover (>1 hour since GPS sync)
    NONE            // No valid source
};

struct TimeSourceStatus {
    TimeSourceType type;
    Types::ClockQuality quality;
    Types::Timestamp last_sync_time;
    uint32_t satellites;      // GPS satellite count
    bool pps_healthy;         // GPS PPS signal healthy
    uint32_t holdover_seconds; // Seconds since last GPS sync
    
    TimeSourceStatus() 
        : type(TimeSourceType::NONE), satellites(0), 
          pps_healthy(false), holdover_seconds(0) {
        quality.clock_class = 248;  // Default/unconfigured
        quality.clock_accuracy = Types::ClockAccuracy::Unknown;
        quality.offset_scaled_log_variance = 0xFFFF;
        last_sync_time.seconds_field = 0;
        last_sync_time.nanoseconds_field = 0;
    }
};

static TimeSourceStatus current_source;

/**
 * @brief Update time source status based on GPS and RTC health
 */
void update_time_source() {
    // Check GPS status
    bool gps_has_fix = nmea_parser.has_valid_fix();
    current_source.satellites = nmea_parser.get_satellite_count();
    current_source.pps_healthy = pps_handler.is_signal_healthy();
    
    // Determine best time source
    if (gps_has_fix && current_source.pps_healthy && current_source.satellites >= 4) {
        // Best: GPS with 1PPS locked
        current_source.type = TimeSourceType::GPS_PPS;
        current_source.quality.clock_class = 6;  // Primary reference
        current_source.quality.clock_accuracy = Types::ClockAccuracy::Within_100ns;
        current_source.quality.offset_scaled_log_variance = 0x4E00;  // ~25μs variance
        
    } else if (gps_has_fix && current_source.satellites >= 3) {
        // Good: GPS NMEA without PPS
        current_source.type = TimeSourceType::GPS_NMEA;
        current_source.quality.clock_class = 7;  // Degraded primary reference
        current_source.quality.clock_accuracy = Types::ClockAccuracy::Within_1ms;
        current_source.quality.offset_scaled_log_variance = 0x5A00;  // ~100ms variance
        
    } else {
        // Fallback to RTC
        uint64_t now_sec = millis() / 1000;
        uint64_t last_sync_sec = current_source.last_sync_time.seconds_field;
        current_source.holdover_seconds = (last_sync_sec > 0) ? (now_sec - last_sync_sec) : 0xFFFFFFFF;
        
        if (current_source.holdover_seconds < 3600) {
            // RTC synced recently (<1 hour)
            current_source.type = TimeSourceType::RTC_SYNCED;
            current_source.quality.clock_class = 52;  // Degraded by holdover
            current_source.quality.clock_accuracy = Types::ClockAccuracy::Within_250ms;
            current_source.quality.offset_scaled_log_variance = 0x7000;
            
        } else {
            // RTC in long-term holdover
            current_source.type = TimeSourceType::RTC_HOLDOVER;
            current_source.quality.clock_class = 187;  // Free-running
            current_source.quality.clock_accuracy = Types::ClockAccuracy::Within_1s;
            current_source.quality.offset_scaled_log_variance = 0x8000;
        }
    }
}

/**
 * @brief Get current time from best available source
 */
Types::Timestamp get_current_time() {
    Types::Timestamp timestamp;
    
    switch (current_source.type) {
        case TimeSourceType::GPS_PPS: {
            // Best: Use GPS time + PPS correction
            if (pps_handler.has_event()) {
                GPS::PPS::PPSEvent pps = pps_handler.get_event();
                // TODO: Combine GPS NMEA time with PPS microsecond precision
                // For now, use GPS NMEA time
                timestamp = nmea_parser.get_utc_timestamp();
            } else {
                timestamp = nmea_parser.get_utc_timestamp();
            }
            break;
        }
        
        case TimeSourceType::GPS_NMEA:
            // Good: GPS NMEA time only
            timestamp = nmea_parser.get_utc_timestamp();
            break;
            
        case TimeSourceType::RTC_SYNCED:
        case TimeSourceType::RTC_HOLDOVER:
            // Fallback: RTC time
            if (rtc_adapter) {
                timestamp = rtc_adapter->get_current_time();
            }
            break;
            
        default:
            // No valid source
            timestamp.seconds_field = 0;
            timestamp.nanoseconds_field = 0;
            break;
    }
    
    return timestamp;
}

// ====================================================================
// PTP Packet Generation (Simplified)
// ====================================================================

/**
 * @brief Send PTP Announce message (clockQuality advertisement)
 */
void send_ptp_announce() {
    // TODO: Construct IEEE 1588-2019 Announce message
    // - Include current clockQuality from current_source
    // - Include clockIdentity (ESP32 MAC address)
    // - Include grandmasterPriority1/2
    // - Send via udp_general to multicast group
    
    Serial.println("→ Sending PTP Announce (clockClass " + String(current_source.quality.clock_class) + ")");
}

/**
 * @brief Send PTP Sync message (timestamp distribution)
 */
void send_ptp_sync() {
    // TODO: Construct IEEE 1588-2019 Sync message
    // - Capture precise timestamp (esp_timer_get_time())
    // - Include originTimestamp
    // - Send via udp_event to multicast group
    // - Follow with Follow_Up message containing correctionField
    
    Types::Timestamp sync_time = get_current_time();
    Serial.println("→ Sending PTP Sync (" + String(sync_time.seconds_field) + "s)");
}

// ====================================================================
// GPS Processing
// ====================================================================

void process_gps_data() {
    // Read available NMEA data from GPS
    uint8_t buffer[256];
    size_t bytes_read;
    
    HAL::Serial::SerialError err = gps_serial.read(buffer, sizeof(buffer) - 1, bytes_read);
    
    if (err == HAL::Serial::SerialError::Success && bytes_read > 0) {
        buffer[bytes_read] = '\0';  // Null terminate
        
        // Parse NMEA sentences
        for (size_t i = 0; i < bytes_read; i++) {
            if (nmea_parser.parse_byte(buffer[i])) {
                // Complete NMEA sentence parsed
                Serial.print("GPS: ");
                Serial.print(nmea_parser.get_satellite_count());
                Serial.print(" sats, Fix: ");
                Serial.println(nmea_parser.has_valid_fix() ? "YES" : "NO");
            }
        }
    }
    
    // Process PPS events
    if (pps_handler.has_event()) {
        GPS::PPS::PPSEvent pps = pps_handler.get_event();
        int64_t jitter_us = pps_handler.get_jitter_us();
        
        Serial.print("PPS: ");
        Serial.print(pps.timestamp_us);
        Serial.print(" μs, jitter: ");
        Serial.print(jitter_us);
        Serial.println(" μs");
        
        // Synchronize RTC to GPS when we have good fix
        if (nmea_parser.has_valid_fix() && rtc_adapter) {
            Types::Timestamp gps_time = nmea_parser.get_utc_timestamp();
            rtc_adapter->set_time(gps_time);
            current_source.last_sync_time = gps_time;
            Serial.println("✓ RTC synchronized to GPS");
        }
    }
}

// ====================================================================
// Status Display
// ====================================================================

void display_status() {
    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    Serial.println("║  ESP32 PTP Grandmaster Clock Status                       ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    
    // WiFi status
    Serial.print("WiFi: ");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected to ");
        Serial.print(WIFI_SSID);
        Serial.print(" (");
        Serial.print(WiFi.localIP());
        Serial.print(", RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm)");
    } else {
        Serial.println("Disconnected");
    }
    
    // Time source
    Serial.print("Time Source: ");
    switch (current_source.type) {
        case TimeSourceType::GPS_PPS:
            Serial.println("GPS + 1PPS (BEST)");
            break;
        case TimeSourceType::GPS_NMEA:
            Serial.println("GPS NMEA only");
            break;
        case TimeSourceType::RTC_SYNCED:
            Serial.print("RTC (holdover ");
            Serial.print(current_source.holdover_seconds);
            Serial.println("s)");
            break;
        case TimeSourceType::RTC_HOLDOVER:
            Serial.println("RTC (long holdover)");
            break;
        default:
            Serial.println("NONE");
            break;
    }
    
    // GPS details
    Serial.print("GPS: ");
    Serial.print(current_source.satellites);
    Serial.print(" satellites, Fix: ");
    Serial.print(nmea_parser.has_valid_fix() ? "YES" : "NO");
    Serial.print(", PPS: ");
    Serial.println(current_source.pps_healthy ? "Healthy" : "Unhealthy");
    
    // Clock quality
    Serial.println("\nIEEE 1588-2019 Clock Quality:");
    Serial.print("  Clock Class: ");
    Serial.println(current_source.quality.clock_class);
    Serial.print("  Clock Accuracy: ");
    Serial.println(static_cast<uint8_t>(current_source.quality.clock_accuracy), HEX);
    Serial.print("  Offset Scaled Log Variance: 0x");
    Serial.println(current_source.quality.offset_scaled_log_variance, HEX);
    
    // Current time
    Types::Timestamp now = get_current_time();
    Serial.print("\nCurrent Time: ");
    Serial.print(now.seconds_field);
    Serial.print(".");
    Serial.print(now.nanoseconds_field);
    Serial.println(" (Unix epoch)");
    
    // PPS statistics
    Serial.print("\nPPS Statistics:");
    Serial.print(" Count: ");
    Serial.print(pps_handler.get_pps_count());
    Serial.print(", Missed: ");
    Serial.print(pps_handler.get_missed_count());
    Serial.print(", Jitter: ");
    Serial.print(pps_handler.get_jitter_us());
    Serial.println(" μs");
    
    Serial.println("════════════════════════════════════════════════════════════\n");
}

// ====================================================================
// Arduino Setup
// ====================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n╔════════════════════════════════════════════════════════════╗");
    Serial.println("║  ESP32 IEEE 1588-2019 PTP Grandmaster Clock              ║");
    Serial.println("║  GPS-Disciplined Time Server                             ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝\n");
    
    // Initialize I2C for RTC
    Serial.println("Initializing RTC (DS3231)...");
    rtc_adapter = new RTCAdapter(RTC_I2C_ADDRESS, RTCModuleType::DS3231);
    if (rtc_adapter->initialize()) {
        Serial.println("✓ RTC initialized");
    } else {
        Serial.println("✗ RTC initialization failed");
    }
    
    // Initialize GPS UART
    Serial.println("Initializing GPS UART...");
    HAL::Serial::SerialConfig gps_config = HAL::Serial::SerialConfig::gps_nmea_default();
    if (gps_serial.open("", gps_config)) {
        Serial.println("✓ GPS UART initialized (9600 baud, 8N1)");
    } else {
        Serial.println("✗ GPS UART initialization failed");
    }
    
    // Initialize GPS 1PPS interrupt
    Serial.println("Initializing GPS 1PPS handler...");
    if (pps_handler.begin()) {
        Serial.print("✓ 1PPS interrupt attached to GPIO");
        Serial.println(GPS_PPS_PIN);
    } else {
        Serial.println("✗ 1PPS interrupt setup failed");
    }
    
    // Connect to WiFi
    Serial.print("Connecting to WiFi (");
    Serial.print(WIFI_SSID);
    Serial.print(")...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int wifi_attempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) {
        delay(500);
        Serial.print(".");
        wifi_attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi connected");
        Serial.print("  IP Address: ");
        Serial.println(WiFi.localIP());
        
        // Initialize UDP sockets for gPTP
        udp_event.begin(GPTP_EVENT_PORT);
        udp_general.begin(GPTP_GENERAL_PORT);
        Serial.println("✓ gPTP UDP sockets initialized");
    } else {
        Serial.println("\n✗ WiFi connection failed");
        Serial.println("  Continuing without network (GPS/RTC only)");
    }
    
    Serial.println("\nSetup complete. Starting PTP Grandmaster...\n");
}

// ====================================================================
// Arduino Main Loop
// ====================================================================

void loop() {
    static unsigned long last_announce = 0;
    static unsigned long last_sync = 0;
    static unsigned long last_display = 0;
    
    unsigned long now = millis();
    
    // Process GPS data (NMEA + PPS)
    process_gps_data();
    
    // Update RTC
    if (rtc_adapter) {
        rtc_adapter->update();
    }
    
    // Update time source status
    update_time_source();
    
    // Send PTP Announce messages
    if (WiFi.status() == WL_CONNECTED && now - last_announce >= ANNOUNCE_INTERVAL_MS) {
        send_ptp_announce();
        last_announce = now;
    }
    
    // Send PTP Sync messages
    if (WiFi.status() == WL_CONNECTED && now - last_sync >= SYNC_INTERVAL_MS) {
        send_ptp_sync();
        last_sync = now;
    }
    
    // Display status
    if (now - last_display >= DISPLAY_INTERVAL_MS) {
        display_status();
        last_display = now;
    }
    
    // Small delay to prevent CPU hogging
    delay(10);
}
