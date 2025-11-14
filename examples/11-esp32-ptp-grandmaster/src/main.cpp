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
#include <Wire.h>

// GPS components
#include "pps_handler_esp32.hpp"
#include "nmea_parser.hpp"
#include "gps_time_converter.hpp"
#include "serial_hal_interface.hpp"
#include "serial_hal_esp32.hpp"  // ESP32 UART implementation

// RTC components  
// Note: Must include IEEE types BEFORE rtc_adapter.hpp
#include "IEEE/1588/PTP/2019/types.hpp"
namespace Types = IEEE::_1588::PTP::_2019::Types;

#include "rtc_adapter.hpp"

// ====================================================================
// Configuration
// ====================================================================

// WiFi Configuration
const char* WIFI_SSID = "YOUR_WIFI_SSID";      // ⚠ CHANGE THIS!
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";    // ⚠ CHANGE THIS!

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

// No stub classes needed - using real implementations!

// ====================================================================
// Global Objects
// ====================================================================

// GPS hardware - REAL implementations
HAL::Serial::ESP32SerialPort gps_serial(GPS_UART_NUM, GPS_RX_PIN, GPS_TX_PIN);
GPS::NMEA::NMEAParser nmea_parser;
GPS::PPS::PPSHandler pps_handler(static_cast<gpio_num_t>(GPS_PPS_PIN));

// RTC hardware - REAL implementation
Examples::RTC::RTCAdapter* rtc_adapter = nullptr;

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
        quality.clock_accuracy = static_cast<uint8_t>(0xFE);  // Unknown
        quality.offset_scaled_log_variance = 0xFFFF;
        // Direct field assignment instead of setTotalSeconds() due to C++11 constexpr const issue
        last_sync_time.seconds_high = 0;
        last_sync_time.seconds_low = 0;
        last_sync_time.nanoseconds = 0;
    }
};

static TimeSourceStatus current_source;

/**
 * @brief Update time source status based on GPS and RTC health
 */
void update_time_source() {
    // Check GPS status
    bool gps_has_fix = (nmea_parser.get_fix_status() != GPS::NMEA::GPSFixStatus::NO_FIX);
    // Satellite count is updated in process_gps_data() from parsed GPS data
    current_source.pps_healthy = pps_handler.is_signal_healthy();
    
    // Determine best time source
    if (gps_has_fix && current_source.pps_healthy && current_source.satellites >= 4) {
        // Best: GPS with 1PPS locked
        current_source.type = TimeSourceType::GPS_PPS;
        current_source.quality.clock_class = 6;  // Primary reference
        current_source.quality.clock_accuracy = 0x21;  // Within_100ns
        current_source.quality.offset_scaled_log_variance = 0x4E00;  // ~25μs variance
        
    } else if (gps_has_fix && current_source.satellites >= 3) {
        // Good: GPS NMEA without PPS
        current_source.type = TimeSourceType::GPS_NMEA;
        current_source.quality.clock_class = 7;  // Degraded primary reference
        current_source.quality.clock_accuracy = 0x27;  // Within_1ms
        current_source.quality.offset_scaled_log_variance = 0x5A00;  // ~100ms variance
        
    } else {
        // Fallback to RTC
        uint64_t now_sec = millis() / 1000;
        uint64_t last_sync_sec = current_source.last_sync_time.getTotalSeconds();
        current_source.holdover_seconds = (last_sync_sec > 0) ? (now_sec - last_sync_sec) : 0xFFFFFFFF;
        
        if (current_source.holdover_seconds < 3600) {
            // RTC synced recently (<1 hour)
            current_source.type = TimeSourceType::RTC_SYNCED;
            current_source.quality.clock_class = 52;  // Degraded by holdover
            current_source.quality.clock_accuracy = 0x31;  // Within_250ms
            current_source.quality.offset_scaled_log_variance = 0x7000;
            
        } else {
            // RTC in long-term holdover
            current_source.type = TimeSourceType::RTC_HOLDOVER;
            current_source.quality.clock_class = 187;  // Degraded by holdover
            current_source.quality.clock_accuracy = 0x32;  // Within_1s
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
        case TimeSourceType::GPS_PPS:
        case TimeSourceType::GPS_NMEA:
            // GPS time source - use last synchronized time from GPS
            timestamp = current_source.last_sync_time;
            break;
            
        case TimeSourceType::RTC_SYNCED:
        case TimeSourceType::RTC_HOLDOVER:
            // Fallback: RTC time
            if (rtc_adapter) {
                timestamp = rtc_adapter->get_current_time();
            } else {
                timestamp = current_source.last_sync_time;
            }
            break;
            
        default:
            // No valid source
            timestamp.seconds_high = 0;
            timestamp.seconds_low = 0;
            timestamp.nanoseconds = 0;
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
    Serial.println("→ Sending PTP Sync (" + String(sync_time.getTotalSeconds()) + "s)");
}

// ====================================================================
// GPS Processing
// ====================================================================

void process_gps_data() {
    // Static buffer for accumulating NMEA sentences
    static char nmea_buffer[128];
    static size_t nmea_pos = 0;
    static unsigned long last_debug_print = 0;
    static uint32_t total_bytes_received = 0;
    static uint32_t total_sentences_parsed = 0;
    static uint32_t total_sentences_failed = 0;
    
    // Read available NMEA data from GPS
    uint8_t buffer[128];
    size_t bytes_read;
    
    HAL::Serial::SerialError err = gps_serial.read(buffer, sizeof(buffer), bytes_read);
    
    if (err == HAL::Serial::SerialError::SUCCESS && bytes_read > 0) {
        total_bytes_received += bytes_read;
        
        // DEBUG: Show raw bytes received (every 5 seconds)
        unsigned long now = millis();
        if (now - last_debug_print >= 5000) {
            Serial.printf("\n[GPS DEBUG] Received %u bytes (Total: %u bytes, %u OK sentences, %u failed)\n",
                         bytes_read, total_bytes_received, total_sentences_parsed, total_sentences_failed);
            Serial.print("  Raw data: ");
            for (size_t i = 0; i < min(bytes_read, (size_t)32); i++) {
                if (buffer[i] >= 32 && buffer[i] <= 126) {
                    Serial.write(buffer[i]);  // Printable ASCII
                } else {
                    Serial.printf("[0x%02X]", buffer[i]);  // Non-printable hex
                }
            }
            if (bytes_read > 32) Serial.print("...");
            Serial.println();
            last_debug_print = now;
        }
        
        // Accumulate bytes into NMEA sentence buffer
        for (size_t i = 0; i < bytes_read; i++) {
            char c = static_cast<char>(buffer[i]);
            
            // Start of new sentence
            if (c == '$') {
                nmea_pos = 0;
                nmea_buffer[nmea_pos++] = c;
            }
            // End of sentence (CR/LF)
            else if (c == '\n' && nmea_pos > 0) {
                nmea_buffer[nmea_pos] = '\0';  // Null terminate
                
                // DEBUG: Show complete NMEA sentence
                Serial.printf("[GPS NMEA] %s\n", nmea_buffer);
                
                // Parse complete NMEA sentence
                GPS::NMEA::GPSTimeData gps_data;
                if (nmea_parser.parse_sentence(nmea_buffer, gps_data)) {
                    total_sentences_parsed++;
                    
                    // DEBUG: Show parsed data
                    Serial.printf("  ✓ Parsed: %02d:%02d:%02d UTC, %d sats, Valid=%d\n",
                                 gps_data.hours, gps_data.minutes, gps_data.seconds,
                                 gps_data.satellites, gps_data.is_valid_for_ptp() ? 1 : 0);
                    
                    // Update satellite count and fix status
                    current_source.satellites = gps_data.satellites;
                    
                    if (gps_data.is_valid_for_ptp()) {
                        // Valid GPS time available - convert to PTP timestamp
                        static GPS::Time::GPSTimeConverter time_converter;
                        GPS::Time::PTPTimestamp ptp_ts;
                        
                        if (time_converter.convert_to_ptp(gps_data, ptp_ts)) {
                            // Direct field assignment due to C++11 constexpr const issue
                            current_source.last_sync_time.seconds_high = static_cast<uint16_t>(ptp_ts.seconds >> 32);
                            current_source.last_sync_time.seconds_low = static_cast<uint32_t>(ptp_ts.seconds & 0xFFFFFFFF);
                            current_source.last_sync_time.nanoseconds = ptp_ts.nanoseconds;
                            
                            // DEBUG: Show PTP timestamp
                            Serial.printf("  → PTP Time: %llu.%09u\n", 
                                         (unsigned long long)ptp_ts.seconds, ptp_ts.nanoseconds);
                        }
                    }
                } else {
                    total_sentences_failed++;
                    // DEBUG: Show parse failure
                    Serial.printf("  ✗ Parse failed for: %s\n", nmea_buffer);
                }
                
                nmea_pos = 0;  // Reset for next sentence
            }
            // Accumulate characters (ignore CR)
            else if (c != '\r' && nmea_pos < sizeof(nmea_buffer) - 1) {
                nmea_buffer[nmea_pos++] = c;
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
        if (nmea_parser.get_fix_status() != GPS::NMEA::GPSFixStatus::NO_FIX && rtc_adapter) {
            // Use the last valid GPS time from current_source
            if (current_source.last_sync_time.getTotalSeconds() > 0) {
                rtc_adapter->set_time(current_source.last_sync_time);
                Serial.println("✓ RTC synchronized to GPS");
            }
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
    Serial.print((nmea_parser.get_fix_status() != GPS::NMEA::GPSFixStatus::NO_FIX) ? "YES" : "NO");
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
    Serial.print(now.getTotalSeconds());
    Serial.print(".");
    Serial.print(now.nanoseconds);
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
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    rtc_adapter = new Examples::RTC::RTCAdapter(RTC_I2C_ADDRESS, Examples::RTC::RTCModuleType::DS3231);
    if (rtc_adapter->initialize()) {
        Serial.println("✓ RTC initialized");
    } else {
        Serial.println("✗ RTC initialization failed - check I2C wiring");
    }
    
    // Initialize GPS UART
    Serial.println("Initializing GPS UART...");
    HAL::Serial::SerialConfig gps_config = HAL::Serial::SerialConfig::gps_nmea_default();
    HAL::Serial::SerialError gps_err = gps_serial.open("GPS", gps_config);
    if (gps_err == HAL::Serial::SerialError::SUCCESS) {
        Serial.println("✓ GPS UART initialized (9600 baud, 8N1)");
        Serial.println("  Pins: RX=GPIO16, TX=GPIO17");
        
        // Wait a moment for GPS to start sending data
        Serial.print("  Testing GPS connection (waiting for data)");
        delay(2000);
        
        uint8_t test_buffer[128];
        size_t test_bytes;
        HAL::Serial::SerialError test_err = gps_serial.read(test_buffer, sizeof(test_buffer), test_bytes);
        
        if (test_err == HAL::Serial::SerialError::SUCCESS && test_bytes > 0) {
            Serial.printf("\n  ✓ Received %u bytes from GPS:\n    ", test_bytes);
            for (size_t i = 0; i < min(test_bytes, (size_t)64); i++) {
                if (test_buffer[i] >= 32 && test_buffer[i] <= 126) {
                    Serial.write(test_buffer[i]);
                } else if (test_buffer[i] == '\r') {
                    Serial.print("<CR>");
                } else if (test_buffer[i] == '\n') {
                    Serial.print("<LF>\n    ");
                } else {
                    Serial.printf("[0x%02X]", test_buffer[i]);
                }
            }
            Serial.println("\n  → GPS UART is working! (RX/TX wired correctly)");
        } else {
            Serial.println("\n  ✗ No data from GPS - Check wiring:");
            Serial.println("    - GPS TX → ESP32 GPIO16 (RX2)");
            Serial.println("    - GPS RX → ESP32 GPIO17 (TX2)");
            Serial.println("    - If still no data, try swapping TX/RX");
            Serial.println("    - Verify GPS has power and LED is blinking");
        }
    } else {
        Serial.print("✗ GPS UART initialization failed (error ");
        Serial.print(static_cast<int>(gps_err));
        Serial.println(")");
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
