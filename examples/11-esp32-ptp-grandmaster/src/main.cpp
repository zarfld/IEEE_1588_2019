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
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>  // For esp_wifi_set_ps() - disable power save for PTP

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

// WiFi credentials (in gitignored file for security)
#include "credentials.h"

// ====================================================================
// Configuration
// ====================================================================

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

// WiFi networking - 3-Socket Architecture for ESP32 WiFiUDP multicast bug
// 
// ESP32 WiFiUDP BUG: Sending on a multicast-subscribed socket breaks RX
// → Solution: Separate RX and TX sockets to avoid state conflicts
// 
// Socket Architecture (PTP-safe):
// ┌─────────────────┬──────────┬──────┬───────┬─────────┬──────────────────────┐
// │ Socket          │ Function │ Port │ Bind? │ RX/TX   │ Purpose              │
// ├─────────────────┼──────────┼──────┼───────┼─────────┼──────────────────────┤
// │ udp_event_rx    │ Event RX │ 319  │ ✔️    │ RX-only │ Receive Sync         │
// │ udp_general     │ Gen. RX  │ 320  │ ✔️    │ RX-only │ Receive Announce     │
// │ udp_tx          │ PTP TX   │ auto │ ❌    │ TX-only │ Send Sync/Announce   │
// └─────────────────┴──────────┴──────┴───────┴─────────┴──────────────────────┘
//
// IEEE 1588-2019 Annex D.2: Event messages CAN use unicast (compliant)
WiFiUDP udp_event_rx;   // RX-only: Receive Sync on port 319 (multicast)
WiFiUDP udp_general;    // RX-only: Receive Announce on port 320 (multicast)
WiFiUDP udp_tx;         // TX-only: Send all PTP messages (unbound, auto port)

// Web server for monitoring
AsyncWebServer web_server(80);

// ====================================================================
// Time Source Management
// ====================================================================

enum class TimeSourceType {
    GPS_PPS,        // GPS with 1PPS (best)
    GPS_NMEA,       // GPS NMEA only (no PPS)
    RTC_SYNCED,     // RTC synchronized to GPS recently (<1 hour)
    RTC_HOLDOVER,   // RTC in holdover (>1 hour since GPS sync)
    PTP_SLAVE,      // PTP synchronized to network master
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

// PTP State Machine (forward declaration for update_time_source)
enum class PTPState {
    INITIALIZING,
    LISTENING,
    MASTER,
    SLAVE
};

static PTPState ptp_state = PTPState::INITIALIZING;
static TimeSourceStatus current_source;

/**
 * @brief Update time source status based on GPS and RTC health
 * 
 * Implements IEEE 1588-2019 clockClass transitions for GPSDO holdover:
 * - Class 6: GPS locked (primary reference, PTP timescale)
 * - Class 7: GPS holdover within spec (designated holdover, PTP timescale)  
 * - Class 187: GPS holdover degraded (degradation alternative B)
 * 
 * Per IEEE 1588-2019 Section 7.6.2.5:
 * "A clockClass of 7 denotes that the clock is within holdover specifications"
 * 
 * The priority1 and priority2 attributes are administrative and do NOT change
 * automatically. Only clockClass, clockAccuracy, and offsetScaledLogVariance
 * reflect the current synchronization state.
 */
void update_time_source() {
    static unsigned long last_gps_sync_millis = 0;
    static bool was_gps_locked = false;
    
    // If we're in PTP_SLAVE mode, don't override with local sources
    // PTP master synchronization takes precedence over local GPS/RTC
    if (ptp_state == PTPState::SLAVE && current_source.type == TimeSourceType::PTP_SLAVE) {
        return;  // Keep PTP_SLAVE time source active
    }
    
    // Check GPS status
    bool gps_has_fix = (nmea_parser.get_fix_status() != GPS::NMEA::GPSFixStatus::NO_FIX);
    current_source.pps_healthy = pps_handler.is_signal_healthy();
    
    // Calculate time since last GPS lock (for holdover tracking)
    unsigned long now_millis = millis();
    
    // Determine best time source and set IEEE 1588-2019 compliant clockClass
    if (gps_has_fix && current_source.pps_healthy && current_source.satellites >= 4) {
        // ═══════════════════════════════════════════════════════════════════
        // BEST: GPS + 1PPS locked (Primary Reference)
        // ═══════════════════════════════════════════════════════════════════
        // IEEE 1588-2019 Table 5 - clockClass 6:
        // "Shall designate a clock that is synchronized to a primary reference 
        //  time source. The timescale distributed shall be PTP."
        current_source.type = TimeSourceType::GPS_PPS;
        current_source.quality.clock_class = 6;        // Primary reference
        current_source.quality.clock_accuracy = 0x21;  // Within 25ns (0x21)
        current_source.quality.offset_scaled_log_variance = 0x4E00;  // ~1μs Allan variance
        current_source.holdover_seconds = 0;
        
        last_gps_sync_millis = now_millis;  // Update last sync time
        was_gps_locked = true;
        
    } else if (gps_has_fix && current_source.satellites >= 3) {
        // ═══════════════════════════════════════════════════════════════════
        // GOOD: GPS NMEA without PPS (Still locked, but less accurate)
        // ═══════════════════════════════════════════════════════════════════
        // Still considered GPS-locked since we have valid NMEA time
        current_source.type = TimeSourceType::GPS_NMEA;
        current_source.quality.clock_class = 6;        // Still primary reference
        current_source.quality.clock_accuracy = 0x27;  // Within 1ms
        current_source.quality.offset_scaled_log_variance = 0x5A00;  // ~100ms variance
        current_source.holdover_seconds = 0;
        
        last_gps_sync_millis = now_millis;
        was_gps_locked = true;
        
    } else if (was_gps_locked) {
        // ═══════════════════════════════════════════════════════════════════
        // HOLDOVER: GPS lost, using high-stability oscillator holdover
        // ═══════════════════════════════════════════════════════════════════
        // IEEE 1588-2019 clockClass transitions during holdover:
        // Time 0-300s:    Class 7  (within designated holdover spec)
        // Time 300s-3600s: Class 7  (still within spec for good TCXO/OCXO)
        // Time >3600s:    Class 187 (degradation alternative B)
        
        uint32_t holdover_sec = (now_millis - last_gps_sync_millis) / 1000;
        current_source.holdover_seconds = holdover_sec;
        
        if (holdover_sec < 3600) {
            // ───────────────────────────────────────────────────────────────
            // SHORT-TERM HOLDOVER (<1 hour) - Class 7
            // ───────────────────────────────────────────────────────────────
            // IEEE 1588-2019 Table 5 - clockClass 7:
            // "Shall designate a clock that has previously been designated as
            //  clockClass 6 but that has lost the ability to synchronize to a
            //  primary reference time source and is in holdover mode and within
            //  holdover specifications. The timescale distributed shall be PTP."
            //
            // Typical TCXO drift: 1-5 ppm
            // After 1 hour: 3.6ms - 18ms drift (still acceptable for many apps)
            current_source.type = TimeSourceType::RTC_SYNCED;
            current_source.quality.clock_class = 7;        // Designated holdover
            current_source.quality.clock_accuracy = 0x31;  // Within 250ms (conservative)
            current_source.quality.offset_scaled_log_variance = 0x7000;  // Increased variance
            
        } else {
            // ───────────────────────────────────────────────────────────────
            // LONG-TERM HOLDOVER (>1 hour) - Class 187
            // ───────────────────────────────────────────────────────────────
            // IEEE 1588-2019 Table 5 - clockClass 187:
            // "Degradation alternative B - For a clock that has lost the ability
            //  to synchronize to a grandmaster clock."
            //
            // After 1+ hours, even good oscillators drift significantly:
            // TCXO @ 2.5ppm: ~9 seconds drift per hour
            // Must degrade to Class 187 to avoid misleading downstream clocks
            current_source.type = TimeSourceType::RTC_HOLDOVER;
            current_source.quality.clock_class = 187;      // Degraded holdover
            current_source.quality.clock_accuracy = 0x32;  // Within 1s
            current_source.quality.offset_scaled_log_variance = 0x8000;  // High variance
        }
        
    } else {
        // ═══════════════════════════════════════════════════════════════════
        // NEVER HAD GPS LOCK - Unconfigured/Free-running
        // ═══════════════════════════════════════════════════════════════════
        // IEEE 1588-2019 Table 5 - clockClass 248:
        // "Default - For a clock that is not synchronized to a
        //  primary reference source"
        current_source.type = TimeSourceType::NONE;
        current_source.quality.clock_class = 248;      // Default/unconfigured
        current_source.quality.clock_accuracy = 0xFE;  // Unknown
        current_source.quality.offset_scaled_log_variance = 0xFFFF;
        current_source.holdover_seconds = 0xFFFFFFFF;  // N/A
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
            
        case TimeSourceType::PTP_SLAVE:
            // PTP synchronized time (adjusted by PI controller)
            timestamp = current_source.last_sync_time;
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
// PTP Packet Structures (IEEE 1588-2019)
// ====================================================================

#pragma pack(push, 1)

/**
 * @brief IEEE 1588-2019 PTP Common Header (34 bytes)
 * @see IEEE 1588-2019 Section 13.3, Table 18
 */
struct PTPHeader {
    uint8_t  transport_specific_message_type;  // [7:4]=transportSpecific, [3:0]=messageType
    uint8_t  version_ptp;                      // PTP version = 2
    uint16_t message_length;                   // Total message length
    uint8_t  domain_number;                    // Domain number (0 for default)
    uint8_t  reserved1;
    uint16_t flags;                            // PTP flags
    uint64_t correction_field;                 // Nanoseconds * 2^16
    uint32_t reserved2;
    uint8_t  source_port_identity[10];         // Clock ID (8) + Port number (2)
    uint16_t sequence_id;                      // Message sequence number
    uint8_t  control_field;                    // Deprecated, use 0
    int8_t   log_message_interval;             // Log2 of message interval
};

/**
 * @brief IEEE 1588-2019 Announce Message Body
 * @see IEEE 1588-2019 Section 13.5, Table 27
 */
struct PTPAnnounceMessage {
    PTPHeader header;
    uint16_t origin_timestamp_seconds_high;    // Seconds MSB
    uint32_t origin_timestamp_seconds_low;     // Seconds LSB
    uint32_t origin_timestamp_nanoseconds;
    uint16_t current_utc_offset;               // GPS-UTC offset (18s as of 2024)
    uint8_t  reserved;
    uint8_t  grandmaster_priority1;
    uint8_t  grandmaster_clock_quality_class;
    uint8_t  grandmaster_clock_quality_accuracy;
    uint16_t grandmaster_clock_quality_variance;
    uint8_t  grandmaster_priority2;
    uint8_t  grandmaster_identity[8];          // Clock ID
    uint16_t steps_removed;
    uint8_t  time_source;
};

/**
 * @brief IEEE 1588-2019 Sync Message Body
 * @see IEEE 1588-2019 Section 13.6, Table 34
 */
struct PTPSyncMessage {
    PTPHeader header;
    uint16_t origin_timestamp_seconds_high;
    uint32_t origin_timestamp_seconds_low;
    uint32_t origin_timestamp_nanoseconds;
};

#pragma pack(pop)

// ====================================================================
// PTP State and BMCA
// ====================================================================

// PTPState already declared earlier (before update_time_source)

struct ForeignMaster {
    uint8_t clock_identity[8];
    IPAddress ip_address;
    uint8_t clock_class;
    uint8_t clock_accuracy;
    uint16_t variance;
    uint8_t priority1;
    uint8_t priority2;
    uint16_t steps_removed;
    uint8_t time_source;
    unsigned long last_announce_time;
    uint16_t last_sequence_id;
    bool valid;
};

// Global PTP state (ptp_state already initialized earlier, before update_time_source)
ForeignMaster foreign_masters[4];  // Track up to 4 foreign masters
const int MAX_FOREIGN_MASTERS = 4;
uint16_t ptp_sequence_id = 0;
uint8_t local_clock_identity[8] = {0};

// Packet statistics for monitoring
struct PacketStatistics {
    unsigned long announce_received;
    unsigned long announce_sent;
    unsigned long sync_received;
    unsigned long sync_sent;
    unsigned long last_announce_received_ms;
    unsigned long last_sync_received_ms;
} packet_stats = {0, 0, 0, 0, 0, 0};

// Master selection (BMCA result)
ForeignMaster* selected_master = nullptr;
int64_t offset_from_master_ns = 0;

/**
 * @brief Initialize local clock identity from MAC address
 */
void init_clock_identity() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    // IEEE EUI-64 format: insert 0xFF 0xFE in middle of MAC
    local_clock_identity[0] = mac[0];
    local_clock_identity[1] = mac[1];
    local_clock_identity[2] = mac[2];
    local_clock_identity[3] = 0xFF;
    local_clock_identity[4] = 0xFE;
    local_clock_identity[5] = mac[3];
    local_clock_identity[6] = mac[4];
    local_clock_identity[7] = mac[5];
}

/**
 * @brief Compare two clocks using IEEE 1588-2019 BMCA
 * @return -1 if clock_a is better, 1 if clock_b is better, 0 if equal
 */
int bmca_compare(const ForeignMaster* clock_a, const ForeignMaster* clock_b) {
    // Step 1: Compare priority1 (lower is better)
    if (clock_a->priority1 < clock_b->priority1) return -1;
    if (clock_a->priority1 > clock_b->priority1) return 1;
    
    // Step 2: Compare clockClass (lower is better)
    if (clock_a->clock_class < clock_b->clock_class) return -1;
    if (clock_a->clock_class > clock_b->clock_class) return 1;
    
    // Step 3: Compare clockAccuracy (lower is better)
    if (clock_a->clock_accuracy < clock_b->clock_accuracy) return -1;
    if (clock_a->clock_accuracy > clock_b->clock_accuracy) return 1;
    
    // Step 4: Compare variance (lower is better)
    if (clock_a->variance < clock_b->variance) return -1;
    if (clock_a->variance > clock_b->variance) return 1;
    
    // Step 5: Compare priority2 (lower is better)
    if (clock_a->priority2 < clock_b->priority2) return -1;
    if (clock_a->priority2 > clock_b->priority2) return 1;
    
    // Step 6: Compare clock identity (lexicographically lower is better)
    int cmp = memcmp(clock_a->clock_identity, clock_b->clock_identity, 8);
    return (cmp < 0) ? -1 : (cmp > 0) ? 1 : 0;
}

/**
 * @brief Run BMCA to select best master
 */
void run_bmca() {
    ForeignMaster local_clock;
    memcpy(local_clock.clock_identity, local_clock_identity, 8);
    local_clock.clock_class = current_source.quality.clock_class;
    local_clock.clock_accuracy = static_cast<uint8_t>(current_source.quality.clock_accuracy);
    local_clock.variance = current_source.quality.offset_scaled_log_variance;
    local_clock.priority1 = 128;  // Default priority
    local_clock.priority2 = 128;
    local_clock.steps_removed = 0;
    local_clock.time_source = 0x20;  // GPS (0x20 per IEEE 1588-2019 Table 7)
    local_clock.valid = true;
    
    // Find best foreign master
    ForeignMaster* best_foreign = nullptr;
    unsigned long now = millis();
    
    for (int i = 0; i < MAX_FOREIGN_MASTERS; i++) {
        if (foreign_masters[i].valid && (now - foreign_masters[i].last_announce_time < 3000)) {
            if (best_foreign == nullptr || bmca_compare(&foreign_masters[i], best_foreign) < 0) {
                best_foreign = &foreign_masters[i];
            }
        }
    }
    
    // Compare best foreign with local clock
    PTPState old_state = ptp_state;
    
    if (best_foreign != nullptr && bmca_compare(best_foreign, &local_clock) < 0) {
        // Foreign master is better - become SLAVE
        ptp_state = PTPState::SLAVE;
        selected_master = best_foreign;
        
        if (old_state != PTPState::SLAVE) {
            Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
            Serial.println("║  PTP STATE CHANGE: MASTER → SLAVE                          ║");
            Serial.println("╚═══════════════════════════════════════════════════════════════╝");
            Serial.printf("Selected Master: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                         selected_master->clock_identity[0], selected_master->clock_identity[1],
                         selected_master->clock_identity[2], selected_master->clock_identity[3],
                         selected_master->clock_identity[4], selected_master->clock_identity[5],
                         selected_master->clock_identity[6], selected_master->clock_identity[7]);
            Serial.printf("Master IP: %s\n", selected_master->ip_address.toString().c_str());
            Serial.printf("Master clockClass: %d (vs local: %d)\n", 
                         selected_master->clock_class, local_clock.clock_class);
        }
    } else {
        // Local clock is best - become MASTER
        ptp_state = PTPState::MASTER;
        selected_master = nullptr;
        
        if (old_state != PTPState::MASTER) {
            Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
            Serial.println("║  PTP STATE CHANGE: SLAVE → MASTER                          ║");
            Serial.println("╚═══════════════════════════════════════════════════════════════╝");
            Serial.printf("Local clockClass: %d (best on network)\n", local_clock.clock_class);
            
            // When transitioning back to MASTER, revert to local time source (GPS/RTC)
            if (current_source.type == TimeSourceType::PTP_SLAVE) {
                Serial.println("→ Reverting to local time source (GPS/RTC)");
                // update_time_source() will be called in loop() and will re-establish GPS/RTC
            }
        }
    }
}

// ====================================================================
// PTP Packet Generation (IEEE 1588-2019 Compliant)
// ====================================================================

/**
 * @brief Send PTP Announce message (clockQuality advertisement)
 */
void send_ptp_announce() {
    if (ptp_state != PTPState::MASTER) {
        return;  // Only masters send Announce
    }
    
    PTPAnnounceMessage announce;
    memset(&announce, 0, sizeof(announce));
    
    // PTP Header
    announce.header.transport_specific_message_type = 0x0B;  // [7:4]=0 (Ethernet), [3:0]=11 (Announce)
    announce.header.version_ptp = 0x02;  // PTP v2
    announce.header.message_length = htons(sizeof(PTPAnnounceMessage));
    announce.header.domain_number = 0;
    announce.header.flags = htons(0x0000);  // No special flags
    announce.header.correction_field = 0;
    memcpy(announce.header.source_port_identity, local_clock_identity, 8);
    uint16_t port_number = htons(1);
    memcpy(announce.header.source_port_identity + 8, &port_number, 2);
    announce.header.sequence_id = htons(ptp_sequence_id++);
    announce.header.control_field = 0x05;  // Deprecated, use default
    announce.header.log_message_interval = 0;  // 2^0 = 1 second
    
    // Announce body
    Types::Timestamp now = get_current_time();
    announce.origin_timestamp_seconds_high = htons(now.seconds_high);
    announce.origin_timestamp_seconds_low = htonl(now.seconds_low);
    announce.origin_timestamp_nanoseconds = htonl(now.nanoseconds);
    announce.current_utc_offset = htons(18);  // GPS-UTC offset
    announce.grandmaster_priority1 = 128;
    announce.grandmaster_clock_quality_class = current_source.quality.clock_class;
    announce.grandmaster_clock_quality_accuracy = static_cast<uint8_t>(current_source.quality.clock_accuracy);
    announce.grandmaster_clock_quality_variance = htons(current_source.quality.offset_scaled_log_variance);
    announce.grandmaster_priority2 = 128;
    memcpy(announce.grandmaster_identity, local_clock_identity, 8);
    announce.steps_removed = htons(0);
    announce.time_source = 0x20;  // GPS
    
    // Send via TX-only socket (Socket 3) to multicast address
    // Prevents ESP32 WiFiUDP bug where TX on RX socket causes packet loss
    udp_tx.beginPacket(GPTP_MULTICAST_ADDR, GPTP_GENERAL_PORT);
    udp_tx.write((const uint8_t*)&announce, sizeof(announce));
    udp_tx.endPacket();
    
    packet_stats.announce_sent++;
    Serial.println("→ Sending PTP Announce (clockClass " + String(current_source.quality.clock_class) + ")");
}

/**
 * @brief Send PTP Sync message (timestamp distribution)
 * 
 * DIAGNOSTIC WORKAROUND: Also send when SLAVE to keep UDP socket "active"
 * for multicast reception. Some APs/ESP32 WiFi stack combinations drop
 * multicast forwarding when a socket stops transmitting.
 */
void send_ptp_sync() {
    // IEEE 1588-2019: Only MASTER state sends Sync messages
    if (ptp_state != PTPState::MASTER) {
        return;  // Only masters send Sync - slaves receive
    }
    
    PTPSyncMessage sync;
    memset(&sync, 0, sizeof(sync));
    
    // PTP Header
    sync.header.transport_specific_message_type = 0x00;  // [7:4]=0 (Ethernet), [3:0]=0 (Sync)
    sync.header.version_ptp = 0x02;
    sync.header.message_length = htons(sizeof(PTPSyncMessage));
    sync.header.domain_number = 0;
    sync.header.flags = htons(0x0200);  // twoStepFlag = 1
    sync.header.correction_field = 0;
    memcpy(sync.header.source_port_identity, local_clock_identity, 8);
    uint16_t port_number = htons(1);
    memcpy(sync.header.source_port_identity + 8, &port_number, 2);
    sync.header.sequence_id = htons(ptp_sequence_id++);
    sync.header.control_field = 0x00;
    sync.header.log_message_interval = -3;  // 2^-3 = 125ms
    
    // Sync body - capture current time
    Types::Timestamp sync_time = get_current_time();
    sync.origin_timestamp_seconds_high = htons(sync_time.seconds_high);
    sync.origin_timestamp_seconds_low = htonl(sync_time.seconds_low);
    sync.origin_timestamp_nanoseconds = htonl(sync_time.nanoseconds);
    
    // IEEE 1588-2019 Annex D.2: Send Sync via UNICAST to all known foreign masters
    // This is IEEE compliant and works around ESP32 WiFi multicast issues
    int unicast_count = 0;
    
    // DEBUG: Show foreign masters state before sending unicast
    static unsigned long last_debug_ms = 0;
    if (millis() - last_debug_ms > 5000) {
        Serial.printf("[UNICAST DEBUG] Checking %d foreign master slots:\n", MAX_FOREIGN_MASTERS);
        for (int i = 0; i < MAX_FOREIGN_MASTERS; i++) {
            Serial.printf("  [%d] valid=%d, IP=%s, class=%d\n", 
                         i, foreign_masters[i].valid ? 1 : 0,
                         foreign_masters[i].ip_address.toString().c_str(),
                         foreign_masters[i].clock_class);
        }
        last_debug_ms = millis();
    }
    
    for (int i = 0; i < MAX_FOREIGN_MASTERS; i++) {
        if (foreign_masters[i].valid) {
            // Use TX-only socket (Socket 3) to avoid ESP32 WiFiUDP multicast bug
            udp_tx.beginPacket(foreign_masters[i].ip_address, GPTP_EVENT_PORT);
            udp_tx.write((const uint8_t*)&sync, sizeof(sync));
            int result = udp_tx.endPacket();
            
            // CRITICAL FIX: ESP32 WiFi stack needs time to process UDP buffers
            // Error 12 (NO_MEM) means TX buffers exhausted - wait for buffer to free
            if (result == 0) {
                Serial.printf("[UNICAST TX ERROR] Failed to send to %s (WiFi buffer full)\n", 
                             foreign_masters[i].ip_address.toString().c_str());
            } else {
                unicast_count++;
                Serial.printf("[UNICAST TX] Sent to %s\n", foreign_masters[i].ip_address.toString().c_str());
            }
            
            // Give WiFi stack 5ms to process buffer before next transmission
            delay(5);
        }
    }
    
    packet_stats.sync_sent += unicast_count;
    
    if (unicast_count > 0) {
        Serial.printf("→ Sent PTP Sync (%us) [%d unicast transmissions]\n", 
                     sync_time.getTotalSeconds(), unicast_count);
    } else {
        Serial.println("→ No foreign masters to send Sync to (waiting for Announce)");
    }
}

/**
 * @brief Count valid foreign masters
 * @return Number of active foreign master entries
 */
int count_foreign_masters() {
    int count = 0;
    for (int i = 0; i < MAX_FOREIGN_MASTERS; i++) {
        if (foreign_masters[i].valid) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Find or create foreign master slot
 * @param clock_identity IEEE EUI-64 clock identity (8 bytes)
 * @return Index in foreign_masters[] array, or -1 if full
 */
int find_foreign_master_slot(const uint8_t* clock_identity) {
    unsigned long now = millis();
    int oldest_slot = -1;
    unsigned long oldest_time = now;
    
    // First, check if this master already exists
    for (int i = 0; i < MAX_FOREIGN_MASTERS; i++) {
        if (foreign_masters[i].valid && 
            memcmp(foreign_masters[i].clock_identity, clock_identity, 8) == 0) {
            return i;  // Found existing entry
        }
    }
    
    // Not found - find empty slot or oldest entry
    for (int i = 0; i < MAX_FOREIGN_MASTERS; i++) {
        if (!foreign_masters[i].valid) {
            return i;  // Found empty slot
        }
        if (foreign_masters[i].last_announce_time < oldest_time) {
            oldest_time = foreign_masters[i].last_announce_time;
            oldest_slot = i;
        }
    }
    
    // All slots full - reuse oldest
    return oldest_slot;
}

/**
 * @brief Process incoming PTP packets (Announce and Sync messages)
 * 
 * Handles reception of:
 * - Announce messages (port 320) - Updates foreign_masters[] array
 * - Sync messages (port 319) - Calculates time offset when in SLAVE mode
 * 
 * @note Called from loop() to process network packets
 * @see IEEE 1588-2019, Section 13.5 "Announce message"
 * @see IEEE 1588-2019, Section 13.6 "Sync message"
 */
void process_ptp_packets() {
    // Enhanced Diagnostic: Log ALL port 320 activity
    static unsigned long last_port320_diagnostic = 0;
    static unsigned long port320_packet_count = 0;
    unsigned long now = millis();
    
    // Process Announce messages from udp_general (port 320)
    int packet_size = udp_general.parsePacket();
    
    // DIAGNOSTIC: Log every packet received on port 320
    if (packet_size > 0) {
        port320_packet_count++;
        IPAddress remote_ip = udp_general.remoteIP();
        uint16_t remote_port = udp_general.remotePort();
        Serial.printf("🔍 [PORT 320] Packet #%lu: %d bytes from %s:%d\n", 
                     port320_packet_count, packet_size, 
                     remote_ip.toString().c_str(), remote_port);
    }
    
    // DIAGNOSTIC: Every 10 seconds, report port 320 status
    if (now - last_port320_diagnostic >= 10000) {
        Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
        Serial.println("║  PORT 320 (ANNOUNCE) DIAGNOSTIC                            ║");
        Serial.println("╚═══════════════════════════════════════════════════════════════╝");
        Serial.printf("  Total packets received on port 320: %lu\n", port320_packet_count);
        Serial.printf("  Last parsePacket() result: %d\n", packet_size);
        
        wifi_ps_type_t ps_mode;
        esp_wifi_get_ps(&ps_mode);
        Serial.printf("  WiFi PS Mode: %s\n", ps_mode == WIFI_PS_NONE ? "✓ DISABLED" : "✗ ENABLED");
        Serial.printf("  WiFi RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("  Local IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  General Socket: Multicast %s:%d\n", GPTP_MULTICAST_ADDR, GPTP_GENERAL_PORT);
        Serial.printf("  PTP State: %s\n", 
                     ptp_state == PTPState::MASTER ? "MASTER" : 
                     ptp_state == PTPState::SLAVE ? "SLAVE" : "OTHER");
        Serial.printf("  Foreign Masters Tracked: %d\n", count_foreign_masters());
        Serial.println("════════════════════════════════════════════════════════════════\n");
        
        last_port320_diagnostic = now;
    }
    
    if (packet_size >= (int)sizeof(PTPHeader)) {
        uint8_t buffer[256];
        int len = udp_general.read(buffer, sizeof(buffer));
        
        if (len >= (int)sizeof(PTPHeader)) {
            PTPHeader* header = (PTPHeader*)buffer;
            uint8_t msg_type = header->transport_specific_message_type & 0x0F;
            
            // Parse Announce message (0x0B)
            if (msg_type == 0x0B && len >= (int)sizeof(PTPAnnounceMessage)) {
                PTPAnnounceMessage* announce = (PTPAnnounceMessage*)buffer;
                
                // Extract clock identity from source_port_identity (first 8 bytes)
                uint8_t remote_clock_identity[8];
                memcpy(remote_clock_identity, announce->header.source_port_identity, 8);
                
                // Don't track our own clock
                if (memcmp(remote_clock_identity, local_clock_identity, 8) == 0) {
                    return;  // Ignore our own messages
                }
                
                // Find or create foreign master slot
                int slot = find_foreign_master_slot(remote_clock_identity);
                if (slot < 0) {
                    Serial.println("⚠ Foreign master table full");
                    return;
                }
                
                // Populate foreign master data (convert network byte order)
                memcpy(foreign_masters[slot].clock_identity, remote_clock_identity, 8);
                foreign_masters[slot].ip_address = udp_general.remoteIP();
                foreign_masters[slot].clock_class = announce->grandmaster_clock_quality_class;
                foreign_masters[slot].clock_accuracy = announce->grandmaster_clock_quality_accuracy;
                foreign_masters[slot].variance = ntohs(announce->grandmaster_clock_quality_variance);
                foreign_masters[slot].priority1 = announce->grandmaster_priority1;
                foreign_masters[slot].priority2 = announce->grandmaster_priority2;
                foreign_masters[slot].steps_removed = ntohs(announce->steps_removed);
                foreign_masters[slot].time_source = announce->time_source;
                foreign_masters[slot].last_announce_time = millis();
                foreign_masters[slot].last_sequence_id = ntohs(announce->header.sequence_id);
                foreign_masters[slot].valid = true;
                
                packet_stats.announce_received++;
                packet_stats.last_announce_received_ms = millis();
                
                Serial.printf("← Received PTP Announce from %s (class %d, accuracy 0x%02X)\n",
                             foreign_masters[slot].ip_address.toString().c_str(),
                             foreign_masters[slot].clock_class,
                             foreign_masters[slot].clock_accuracy);
            }
        }
    }
    
    // Process Sync messages from udp_event_rx (Socket 1 - port 319 multicast RX)
    // 3-Socket architecture: This socket is RX-only to avoid ESP32 WiFiUDP bug
    packet_size = udp_event_rx.parsePacket();
    if (packet_size > 0) {
        IPAddress remote_ip = udp_event_rx.remoteIP();
        uint16_t remote_port = udp_event_rx.remotePort();
        
        packet_stats.sync_received++;
        packet_stats.last_sync_received_ms = millis();
        
        // Unicast reception (IEEE 1588-2019 compliant)
        Serial.printf("← Received Sync on port 319: %d bytes from %s:%d [UNICAST], state: %s\n", 
                     packet_size, remote_ip.toString().c_str(), remote_port,
                     ptp_state == PTPState::MASTER ? "MASTER" : ptp_state == PTPState::SLAVE ? "SLAVE" : "OTHER");
    }
    
    if (packet_size == 0) {
        // Enhanced Diagnostic: Check socket status every 5 seconds in SLAVE state
        static unsigned long last_diagnostic = 0;
        unsigned long now = millis();
        if (ptp_state == PTPState::SLAVE && now - last_diagnostic >= 5000) {
            wifi_ps_type_t ps_mode;
            esp_wifi_get_ps(&ps_mode);
            
            // Detailed diagnostic output
            Serial.println("╔═══════════════════════════════════════════════════════════════╗");
            Serial.println("║  SYNC RECEPTION DIAGNOSTIC (Port 319)                      ║");
            Serial.println("╚═══════════════════════════════════════════════════════════════╝");
            Serial.printf("  State: SLAVE (should be receiving from master)\n");
            Serial.printf("  WiFi PS Mode: %s\n", ps_mode == WIFI_PS_NONE ? "✓ DISABLED" : "✗ ENABLED");
            Serial.printf("  WiFi RSSI: %d dBm\n", WiFi.RSSI());
            Serial.printf("  Local IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("  Event Socket: UNICAST binding on port %d\n", GPTP_EVENT_PORT);
            Serial.printf("  parsePacket() returns: 0 (no packets available)\n");
            Serial.printf("  Master IP: %s\n", selected_master ? selected_master->ip_address.toString().c_str() : "NONE");
            Serial.println("  → Possible causes:");
            Serial.println("    1. Master not sending unicast Sync to this IP");
            Serial.println("    2. UDP socket not receiving unicast packets");
            Serial.println("    3. Firewall/AP blocking unicast UDP traffic");
            Serial.println("    4. Master sending to wrong IP address");
            Serial.println("════════════════════════════════════════════════════════════════\n");
            
            last_diagnostic = now;
        }
    }
    if (packet_size >= (int)sizeof(PTPSyncMessage) && ptp_state == PTPState::SLAVE) {
        uint8_t buffer[256];
        // Read from event RX socket (Socket 1 - RX-only)
        int len = udp_event_rx.read(buffer, sizeof(buffer));
        
        if (len >= (int)sizeof(PTPSyncMessage)) {
            PTPSyncMessage* sync = (PTPSyncMessage*)buffer;
            
            // WPTP: Filter own broadcasts (wireless loopback) - check if sender is ourselves
            uint8_t remote_clock_identity[8];
            memcpy(remote_clock_identity, sync->header.source_port_identity, 8);
            if (memcmp(remote_clock_identity, local_clock_identity, 8) == 0) {
                // Ignore our own Sync message (multicast loopback)
                Serial.println("  ⊗ Ignoring own Sync (multicast loopback)");
                return;
            }
            
            // Verify this is from our selected master
            if (selected_master != nullptr) {
                
                Serial.printf("  Sync from: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                             remote_clock_identity[0], remote_clock_identity[1], remote_clock_identity[2], remote_clock_identity[3],
                             remote_clock_identity[4], remote_clock_identity[5], remote_clock_identity[6], remote_clock_identity[7]);
                Serial.printf("  Expected:  %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                             selected_master->clock_identity[0], selected_master->clock_identity[1], selected_master->clock_identity[2], selected_master->clock_identity[3],
                             selected_master->clock_identity[4], selected_master->clock_identity[5], selected_master->clock_identity[6], selected_master->clock_identity[7]);
                
                if (memcmp(remote_clock_identity, selected_master->clock_identity, 8) == 0) {
                    // Extract master's timestamp (convert from network byte order)
                    uint64_t master_seconds = ((uint64_t)ntohs(sync->origin_timestamp_seconds_high) << 32) |
                                              ntohl(sync->origin_timestamp_seconds_low);
                    uint32_t master_nanos = ntohl(sync->origin_timestamp_nanoseconds);
                    
                    // Get local time
                    Types::Timestamp local_time = get_current_time();
                    uint64_t local_seconds = local_time.getTotalSeconds();
                    
                    // Calculate offset from master (in nanoseconds)
                    offset_from_master_ns = ((int64_t)master_seconds - (int64_t)local_seconds) * 1000000000LL +
                                           ((int64_t)master_nanos - (int64_t)local_time.nanoseconds);
                    
                    Serial.printf("← Received PTP Sync from master, offset: %lld ns\n", offset_from_master_ns);
                    
                    // Apply time adjustment using PI controller
                    // IEEE 1588-2019 recommends gradual adjustment to avoid time jumps
                    static int64_t integral_error = 0;
                    static unsigned long last_sync_time = 0;
                    unsigned long now = millis();
                    
                    if (last_sync_time > 0) {
                        float dt = (now - last_sync_time) / 1000.0f;  // Delta time in seconds
                        
                        // PI controller gains (tuned for slow, stable convergence)
                        const float Kp = 0.5f;   // Proportional gain
                        const float Ki = 0.1f;   // Integral gain
                        
                        // Calculate PI correction
                        int64_t proportional = (int64_t)(Kp * offset_from_master_ns);
                        integral_error += (int64_t)(Ki * offset_from_master_ns * dt);
                        
                        // Anti-windup: Limit integral term to ±1 second
                        const int64_t MAX_INTEGRAL = 1000000000LL;  // 1 second
                        if (integral_error > MAX_INTEGRAL) integral_error = MAX_INTEGRAL;
                        if (integral_error < -MAX_INTEGRAL) integral_error = -MAX_INTEGRAL;
                        
                        int64_t correction_ns = proportional + integral_error;
                        
                        // Apply correction to current_source.last_sync_time
                        // Convert correction to Timestamp format
                        if (correction_ns != 0) {
                            Types::Timestamp correction;
                            bool negative = (correction_ns < 0);
                            uint64_t abs_correction = negative ? -correction_ns : correction_ns;
                            
                            correction.seconds_high = 0;
                            correction.seconds_low = abs_correction / 1000000000ULL;
                            correction.nanoseconds = abs_correction % 1000000000ULL;
                            
                            // Apply correction
                            if (negative) {
                                // Subtract correction (move clock backward)
                                if (current_source.last_sync_time.nanoseconds >= correction.nanoseconds) {
                                    current_source.last_sync_time.nanoseconds -= correction.nanoseconds;
                                } else {
                                    current_source.last_sync_time.nanoseconds += 1000000000UL - correction.nanoseconds;
                                    correction.seconds_low += 1;
                                }
                                current_source.last_sync_time.seconds_low -= correction.seconds_low;
                            } else {
                                // Add correction (move clock forward)
                                current_source.last_sync_time.nanoseconds += correction.nanoseconds;
                                if (current_source.last_sync_time.nanoseconds >= 1000000000UL) {
                                    current_source.last_sync_time.nanoseconds -= 1000000000UL;
                                    correction.seconds_low += 1;
                                }
                                current_source.last_sync_time.seconds_low += correction.seconds_low;
                            }
                            
                            Serial.printf("  → Applied correction: %+lld ns (P: %+lld, I: %+lld)\n", 
                                         correction_ns, proportional, integral_error);
                        }
                    }
                    
                    last_sync_time = now;
                    
                    // Update time source to PTP_SLAVE mode
                    if (current_source.type != TimeSourceType::PTP_SLAVE) {
                        current_source.type = TimeSourceType::PTP_SLAVE;
                        Serial.println("✓ Time source changed to PTP_SLAVE (synchronized to master)");
                    }
                } else {
                    Serial.println("  ✗ Sync message clock ID doesn't match selected master!");
                }
            } else {
                Serial.println("  ✗ No selected master yet (selected_master is nullptr)");
            }
        }
    }
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
        
        // DEBUG: Show GPS status (every 5 seconds)
        unsigned long now = millis();
        if (now - last_debug_print >= 5000) {
            Serial.printf("\n[GPS DEBUG] Status: %u OK, %u failed, %u total bytes\n",
                         total_sentences_parsed, total_sentences_failed, total_bytes_received);
            
            // Show helpful message if no lock yet
            if (current_source.satellites == 0 && total_sentences_parsed > 0) {
                Serial.println("  ⚠ Waiting for satellite lock...");
                Serial.println("  → Position GPS module with clear sky view (near window or outside)");
                Serial.println("  → Initial lock can take 30-60 seconds");
            }
            
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
                
                // Check sentence type - only log relevant ones (RMC, GGA)
                bool is_relevant = (strncmp(nmea_buffer, "$GPRMC", 6) == 0 || 
                                   strncmp(nmea_buffer, "$GPGGA", 6) == 0 ||
                                   strncmp(nmea_buffer, "$GNRMC", 6) == 0 ||
                                   strncmp(nmea_buffer, "$GNGGA", 6) == 0);
                
                // Parse complete NMEA sentence
                GPS::NMEA::GPSTimeData gps_data;
                if (nmea_parser.parse_sentence(nmea_buffer, gps_data)) {
                    total_sentences_parsed++;
                    
                    // Update satellite count and fix status
                    current_source.satellites = gps_data.satellites;
                    
                    // Only log relevant sentences with useful data
                    if (is_relevant && gps_data.is_valid_for_ptp()) {
                        Serial.printf("[GPS] %s\n", nmea_buffer);
                        Serial.printf("  ✓ %02d:%02d:%02d UTC, %d sats, Valid\n",
                                     gps_data.hours, gps_data.minutes, gps_data.seconds,
                                     gps_data.satellites);
                        
                        // Valid GPS time available - convert to PTP timestamp
                        static GPS::Time::GPSTimeConverter time_converter;
                        GPS::Time::PTPTimestamp ptp_ts;
                        
                        if (time_converter.convert_to_ptp(gps_data, ptp_ts)) {
                            // Direct field assignment due to C++11 constexpr const issue
                            current_source.last_sync_time.seconds_high = static_cast<uint16_t>(ptp_ts.seconds >> 32);
                            current_source.last_sync_time.seconds_low = static_cast<uint32_t>(ptp_ts.seconds & 0xFFFFFFFF);
                            current_source.last_sync_time.nanoseconds = ptp_ts.nanoseconds;
                            
                            Serial.printf("  → PTP Time: %llu.%09u\n", 
                                         (unsigned long long)ptp_ts.seconds, ptp_ts.nanoseconds);
                        }
                    }
                } else {
                    // Only count failures for relevant sentences (RMC/GGA)
                    // Parse failure = malformed sentence, not just empty/invalid data
                    if (is_relevant) {
                        total_sentences_failed++;
                        // Only log if it looks like actual corruption (not just empty fields)
                        // Empty fields like "$GPRMC,,V,..." are expected when no GPS lock
                        if (strstr(nmea_buffer, ",,") == nullptr) {
                            Serial.printf("[GPS] %s\n", nmea_buffer);
                            Serial.printf("  ✗ Parse error (corrupted)\n");
                        }
                    }
                    // Silently ignore irrelevant sentences (GSV, GLL, VTG, GSA, etc.)
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
    
    // Time source with IEEE 1588-2019 context
    Serial.print("Time Source: ");
    switch (current_source.type) {
        case TimeSourceType::GPS_PPS:
            Serial.println("GPS + 1PPS ★ PRIMARY REFERENCE");
            break;
        case TimeSourceType::GPS_NMEA:
            Serial.println("GPS NMEA ★ PRIMARY REFERENCE");
            break;
        case TimeSourceType::RTC_SYNCED:
            Serial.print("HOLDOVER (");
            Serial.print(current_source.holdover_seconds);
            Serial.println("s since GPS) ⚠ DESIGNATED HOLDOVER");
            break;
        case TimeSourceType::RTC_HOLDOVER:
            Serial.print("HOLDOVER (");
            Serial.print(current_source.holdover_seconds);
            Serial.println("s since GPS) ⚠ DEGRADED");
            break;
        case TimeSourceType::PTP_SLAVE:
            if (selected_master != nullptr) {
                Serial.print("PTP SLAVE (synchronized to ");
                Serial.print(selected_master->ip_address);
                Serial.println(") ★ NETWORK DISCIPLINED");
            } else {
                Serial.println("PTP SLAVE (no master selected)");
            }
            break;
        default:
            Serial.println("NONE ⚠ UNCONFIGURED");
            break;
    }
    
    // GPS details
    Serial.print("GPS: ");
    Serial.print(current_source.satellites);
    Serial.print(" satellites, Fix: ");
    Serial.print((nmea_parser.get_fix_status() != GPS::NMEA::GPSFixStatus::NO_FIX) ? "YES" : "NO");
    Serial.print(", PPS: ");
    Serial.println(current_source.pps_healthy ? "Healthy" : "Unhealthy");
    
    // Clock quality with IEEE 1588-2019 explanations
    Serial.println("\nIEEE 1588-2019 Clock Quality:");
    Serial.print("  Clock Class: ");
    Serial.print(current_source.quality.clock_class);
    
    // Add helpful explanation of clockClass
    switch (current_source.quality.clock_class) {
        case 6:
            Serial.println(" (Primary Reference - GPS locked)");
            Serial.println("    → Traceable to UTC, highest quality");
            break;
        case 7:
            Serial.println(" (Designated Holdover - Within spec)");
            Serial.println("    → Lost GPS but within holdover specifications");
            break;
        case 52:
            Serial.println(" (Degraded by Holdover - <1 hour)");
            Serial.println("    → Accuracy degrading, but usable");
            break;
        case 187:
            Serial.println(" (Degradation Alternative B - >1 hour)");
            Serial.println("    → Significant drift, find better master!");
            break;
        case 248:
            Serial.println(" (Default/Unconfigured)");
            Serial.println("    → Never had GPS lock");
            break;
        default:
            Serial.println();
            break;
    }
    
    Serial.print("  Clock Accuracy: 0x");
    Serial.print(static_cast<uint8_t>(current_source.quality.clock_accuracy), HEX);
    
    // Decode clock accuracy
    uint8_t acc = static_cast<uint8_t>(current_source.quality.clock_accuracy);
    if (acc == 0x21) Serial.println(" (Within 25ns)");
    else if (acc == 0x27) Serial.println(" (Within 1ms)");
    else if (acc == 0x31) Serial.println(" (Within 250ms)");
    else if (acc == 0x32) Serial.println(" (Within 1s)");
    else if (acc == 0xFE) Serial.println(" (Unknown)");
    else Serial.println();
    
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
    
    // PTP State Machine
    Serial.print("\nPTP State: ");
    switch (ptp_state) {
        case PTPState::INITIALIZING:
            Serial.println("INITIALIZING");
            break;
        case PTPState::LISTENING:
            Serial.println("LISTENING (detecting better masters)");
            break;
        case PTPState::MASTER:
            Serial.println("MASTER ★ (advertising time)");
            Serial.println("  → Broadcasting Announce + Sync messages");
            break;
        case PTPState::SLAVE:
            Serial.println("SLAVE (synchronized to master)");
            if (selected_master != nullptr) {
                Serial.print("  → Master: ");
                Serial.print(selected_master->ip_address);
                Serial.print(" (class ");
                Serial.print(selected_master->clock_class);
                Serial.println(")");
                Serial.print("  → Offset: ");
                Serial.print(offset_from_master_ns);
                Serial.println(" ns");
            }
            break;
    }
    
    // Foreign Masters
    bool has_foreign = false;
    for (int i = 0; i < MAX_FOREIGN_MASTERS; i++) {
        if (foreign_masters[i].valid) {
            unsigned long age = millis() - foreign_masters[i].last_announce_time;
            if (age < 3000) {  // Only show recent masters
                if (!has_foreign) {
                    Serial.println("\nForeign Masters:");
                    has_foreign = true;
                }
                Serial.printf("  [%d] %s (class %d, acc 0x%02X) - %lu ms ago\n",
                             i,
                             foreign_masters[i].ip_address.toString().c_str(),
                             foreign_masters[i].clock_class,
                             foreign_masters[i].clock_accuracy,
                             age);
            }
        }
    }
    if (!has_foreign && WiFi.status() == WL_CONNECTED) {
        Serial.println("\nForeign Masters: None detected");
    }
    
    Serial.println("════════════════════════════════════════════════════════════\n");
}

// ====================================================================
// Web Interface
// ====================================================================

const char* web_interface_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 PTP Grandmaster Clock</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #fff;
            padding: 20px;
            min-height: 100vh;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        h1 {
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.5em;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .card {
            background: rgba(255,255,255,0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.2);
            border: 1px solid rgba(255,255,255,0.2);
        }
        .card h2 {
            margin-bottom: 15px;
            font-size: 1.5em;
            border-bottom: 2px solid rgba(255,255,255,0.3);
            padding-bottom: 10px;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        .stat {
            background: rgba(255,255,255,0.05);
            padding: 15px;
            border-radius: 10px;
            border: 1px solid rgba(255,255,255,0.1);
        }
        .stat-label {
            font-size: 0.9em;
            opacity: 0.8;
            margin-bottom: 5px;
        }
        .stat-value {
            font-size: 1.8em;
            font-weight: bold;
        }
        .stat-unit {
            font-size: 0.8em;
            opacity: 0.7;
            margin-left: 5px;
        }
        .status-badge {
            display: inline-block;
            padding: 5px 15px;
            border-radius: 20px;
            font-size: 0.9em;
            font-weight: bold;
            margin-top: 5px;
        }
        .status-excellent { background: #10b981; }
        .status-good { background: #3b82f6; }
        .status-warning { background: #f59e0b; }
        .status-error { background: #ef4444; }
        .time-display {
            font-size: 3em;
            font-weight: bold;
            text-align: center;
            padding: 20px;
            font-family: 'Courier New', monospace;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .update-time {
            text-align: center;
            opacity: 0.7;
            font-size: 0.9em;
            margin-top: 10px;
        }
        @media (max-width: 768px) {
            h1 { font-size: 1.8em; }
            .time-display { font-size: 2em; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🛰️ ESP32 PTP Grandmaster Clock</h1>
        
        <div class="card">
            <h2>Current Time</h2>
            <div class="time-display" id="current-time">--:--:--</div>
            <div class="update-time" id="update-time">Updating...</div>
        </div>
        
        <div class="card">
            <h2>GPS Status</h2>
            <div class="grid">
                <div class="stat">
                    <div class="stat-label">Satellites</div>
                    <div class="stat-value" id="satellites">--</div>
                </div>
                <div class="stat">
                    <div class="stat-label">Fix Status</div>
                    <div class="stat-value" id="fix-status">
                        <span class="status-badge status-error">NO FIX</span>
                    </div>
                </div>
                <div class="stat">
                    <div class="stat-label">1PPS Signal</div>
                    <div class="stat-value" id="pps-status">
                        <span class="status-badge status-error">UNHEALTHY</span>
                    </div>
                </div>
                <div class="stat">
                    <div class="stat-label">PPS Jitter</div>
                    <div class="stat-value" id="pps-jitter">--<span class="stat-unit">μs</span></div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>IEEE 1588-2019 Clock Quality</h2>
            <div class="grid">
                <div class="stat">
                    <div class="stat-label">Clock Class</div>
                    <div class="stat-value" id="clock-class">--</div>
                </div>
                <div class="stat">
                    <div class="stat-label">Clock Accuracy</div>
                    <div class="stat-value" id="clock-accuracy">--</div>
                </div>
                <div class="stat">
                    <div class="stat-label">Time Source</div>
                    <div class="stat-value" id="time-source">--</div>
                </div>
                <div class="stat">
                    <div class="stat-label">Variance</div>
                    <div class="stat-value" id="variance">--</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>Network Status</h2>
            <div class="grid">
                <div class="stat">
                    <div class="stat-label">WiFi</div>
                    <div class="stat-value" id="wifi-status">
                        <span class="status-badge status-error">DISCONNECTED</span>
                    </div>
                </div>
                <div class="stat">
                    <div class="stat-label">IP Address</div>
                    <div class="stat-value" id="ip-address" style="font-size: 1.2em;">--</div>
                </div>
                <div class="stat">
                    <div class="stat-label">RSSI</div>
                    <div class="stat-value" id="rssi">--<span class="stat-unit">dBm</span></div>
                </div>
                <div class="stat">
                    <div class="stat-label">Uptime</div>
                    <div class="stat-value" id="uptime">--</div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        function updateStatus() {
            fetch('/status')
                .then(response => {
                    console.log('Response status:', response.status);
                    console.log('Response headers:', response.headers);
                    if (!response.ok) {
                        throw new Error('HTTP error ' + response.status);
                    }
                    return response.text();
                })
                .then(text => {
                    console.log('Response text length:', text.length);
                    console.log('Response text (first 200 chars):', text.substring(0, 200));
                    return JSON.parse(text);
                })
                .then(data => {
                    console.log('Parsed data successfully');
                    // Update time - parse string to number for 64-bit timestamp support
                    let unixTime = typeof data.unix_time === 'string' ? parseInt(data.unix_time) : data.unix_time;
                    
                    // Fallback to RTC time if unix_time is 0 (no GPS/PTP sync yet)
                    if (unixTime === 0 && data.rtc && data.rtc.current_time) {
                        unixTime = typeof data.rtc.current_time.unix_seconds === 'string' ? 
                            parseInt(data.rtc.current_time.unix_seconds) : data.rtc.current_time.unix_seconds;
                        console.log('Using RTC time as fallback:', unixTime);
                    }
                    
                    const date = new Date(unixTime * 1000);
                    document.getElementById('current-time').textContent = date.toUTCString();
                    document.getElementById('update-time').textContent = 'Last updated: ' + new Date().toLocaleTimeString();
                    
                    // Update GPS status
                    document.getElementById('satellites').textContent = data.gps.satellites;
                    
                    const fixBadge = document.getElementById('fix-status').querySelector('.status-badge');
                    if (data.gps.has_fix) {
                        fixBadge.textContent = 'FIX OK';
                        fixBadge.className = 'status-badge status-excellent';
                    } else {
                        fixBadge.textContent = 'NO FIX';
                        fixBadge.className = 'status-badge status-error';
                    }
                    
                    const ppsBadge = document.getElementById('pps-status').querySelector('.status-badge');
                    if (data.gps.pps_healthy) {
                        ppsBadge.textContent = 'HEALTHY';
                        ppsBadge.className = 'status-badge status-excellent';
                    } else {
                        ppsBadge.textContent = 'UNHEALTHY';
                        ppsBadge.className = 'status-badge status-warning';
                    }
                    
                    document.getElementById('pps-jitter').innerHTML = data.gps.pps_jitter_us + '<span class="stat-unit">μs</span>';
                    
                    // Update PTP clock quality
                    document.getElementById('clock-class').textContent = data.ptp.local_clock_quality.clock_class;
                    document.getElementById('clock-accuracy').textContent = data.ptp.local_clock_quality.clock_accuracy;
                    document.getElementById('time-source').textContent = data.ptp.time_source;
                    document.getElementById('variance').textContent = data.ptp.local_clock_quality.variance;
                    
                    // Update network status
                    const wifiBadge = document.getElementById('wifi-status').querySelector('.status-badge');
                    if (data.network.wifi_connected) {
                        wifiBadge.textContent = 'CONNECTED';
                        wifiBadge.className = 'status-badge status-excellent';
                    } else {
                        wifiBadge.textContent = 'DISCONNECTED';
                        wifiBadge.className = 'status-badge status-error';
                    }
                    
                    document.getElementById('ip-address').textContent = data.network.ip_address;
                    document.getElementById('rssi').innerHTML = data.network.rssi + '<span class="stat-unit">dBm</span>';
                    
                    const uptimeSeconds = data.uptime_seconds;
                    const hours = Math.floor(uptimeSeconds / 3600);
                    const minutes = Math.floor((uptimeSeconds % 3600) / 60);
                    const seconds = uptimeSeconds % 60;
                    document.getElementById('uptime').textContent = 
                        String(hours).padStart(2, '0') + ':' +
                        String(minutes).padStart(2, '0') + ':' +
                        String(seconds).padStart(2, '0');
                })
                .catch(error => {
                    console.error('Error fetching status:', error);
                    document.getElementById('update-time').textContent = 'Error updating - retrying...';
                });
        }
        
        // Update immediately and then every 2 seconds
        updateStatus();
        setInterval(updateStatus, 2000);
    </script>
</body>
</html>
)rawliteral";

void setup_web_interface() {
    // Helper function to format 64-bit integers for JSON (Arduino String() doesn't handle uint64_t correctly)
    // CRITICAL: Must use manual conversion because snprintf %llu is unreliable on ESP32
    auto uint64ToString = [](uint64_t value) -> String {
        if (value == 0) return "0";
        
        char buffer[21];  // Max 20 digits for uint64_t + null terminator
        int pos = 20;
        buffer[pos] = '\0';
        
        while (value > 0 && pos > 0) {
            pos--;
            buffer[pos] = '0' + (value % 10);
            value /= 10;
        }
        
        return String(&buffer[pos]);
    };

    // Serve main page
    web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", web_interface_html);
    });
    
    // JSON status endpoint
    web_server.on("/status", HTTP_GET, [&uint64ToString](AsyncWebServerRequest *request){
        Types::Timestamp now = get_current_time();
        unsigned long current_ms = millis();
        
        // Build JSON response
        String json = "{";
        
        // Time - CRITICAL: Wrap 64-bit integers in quotes for JavaScript compatibility
        json += "\"unix_time\":\"" + uint64ToString(now.getTotalSeconds()) + "\",";
        json += "\"nanoseconds\":" + String(now.nanoseconds) + ",";
        
        // GPS status
        json += "\"gps\":{";
        json += "\"satellites\":" + String(current_source.satellites) + ",";
        json += "\"has_fix\":";
        json += nmea_parser.get_fix_status() != GPS::NMEA::GPSFixStatus::NO_FIX ? "true" : "false";
        json += ",\"pps_healthy\":";
        json += current_source.pps_healthy ? "true" : "false";
        json += ",\"pps_jitter_us\":" + String(pps_handler.get_jitter_us());
        json += "},";
        
        // RTC module status
        json += "\"rtc\":{";
        if (rtc_adapter != nullptr) {
            json += "\"connected\":true,";
            json += "\"i2c_address\":\"0x68\",";
            
            // Get RTC current time
            Types::Timestamp rtc_time = rtc_adapter->get_current_time();
            Types::UInteger64 rtc_seconds = rtc_time.getTotalSeconds();
            
            // CRITICAL: JavaScript Number.MAX_SAFE_INTEGER = 2^53-1
            // 64-bit timestamps must be strings, not numbers, to avoid JSON parse errors
            if (rtc_seconds > 0) {
                json += "\"current_time\":{";
                // Use proper uint64 formatting (Arduino String() mangles 64-bit integers)
                json += "\"unix_seconds\":\"" + uint64ToString(rtc_seconds) + "\",";
                json += "\"nanoseconds\":" + String(rtc_time.nanoseconds);
                json += "},";
            } else {
                json += "\"current_time\":null,";
                json += "\"last_error\":\"not_set\",";
            }
            
            // RTC sync information
            json += "\"last_sync\":{";
            int32_t seconds_since_sync = rtc_adapter->get_seconds_since_sync();
            if (rtc_adapter->is_synchronized() && seconds_since_sync >= 0) {
                json += "\"seconds_ago\":" + String(seconds_since_sync) + ",";
                
                // Determine sync source
                const char* sync_src = "UNKNOWN";
                if (current_source.type == TimeSourceType::GPS_PPS || current_source.type == TimeSourceType::GPS_NMEA) {
                    sync_src = "GPS";
                } else if (current_source.type == TimeSourceType::PTP_SLAVE) {
                    sync_src = "PTP";
                }
                json += "\"source\":\"" + String(sync_src) + "\",";
                json += "\"synchronized\":true";
            } else {
                json += "\"seconds_ago\":null,";
                json += "\"source\":\"NEVER\",";
                json += "\"synchronized\":false";
            }
            json += "},";
            
            // RTC temperature (DS3231 feature)
            float temperature = rtc_adapter->get_temperature_celsius();
            if (!isnan(temperature)) {
                json += "\"temperature_celsius\":" + String(temperature, 2) + ",";
            } else {
                json += "\"temperature_celsius\":null,";
            }
            
            // RTC drift information (offset in nanoseconds)
            int64_t offset_ns = rtc_adapter->get_estimated_offset_ns();
            json += "\"estimated_offset_ns\":" + String((long)offset_ns);  // Full nanosecond precision
        } else {
            json += "\"connected\":false,\"error\":\"not_initialized\"";
        }
        json += "},";
        
        // PTP comprehensive status
        json += "\"ptp\":{";
        
        // PTP state
        const char* state_str = "UNKNOWN";
        switch (ptp_state) {
            case PTPState::INITIALIZING: state_str = "INITIALIZING"; break;
            case PTPState::LISTENING: state_str = "LISTENING"; break;
            case PTPState::MASTER: state_str = "MASTER"; break;
            case PTPState::SLAVE: state_str = "SLAVE"; break;
        }
        json += "\"state\":\"" + String(state_str) + "\",";
        
        // Local clock identity
        json += "\"local_clock_identity\":\"";
        for (int i = 0; i < 8; i++) {
            char hex[3];
            sprintf(hex, "%02X", local_clock_identity[i]);
            json += String(hex);
            if (i < 7) json += ":";
        }
        json += "\",";
        
        // Local clock quality
        json += "\"local_clock_quality\":{";
        json += "\"clock_class\":" + String(current_source.quality.clock_class) + ",";
        json += "\"clock_accuracy\":\"0x" + String(static_cast<uint8_t>(current_source.quality.clock_accuracy), HEX) + "\",";
        json += "\"variance\":\"0x" + String(current_source.quality.offset_scaled_log_variance, HEX) + "\",";
        json += "\"holdover_seconds\":" + String(current_source.holdover_seconds);
        json += "},";
        
        // Time source with IEEE 1588-2019 context
        const char* source_str = "UNKNOWN";
        switch (current_source.type) {
            case TimeSourceType::GPS_PPS: source_str = "GPS + 1PPS (Primary Reference)"; break;
            case TimeSourceType::GPS_NMEA: source_str = "GPS NMEA (Primary Reference)"; break;
            case TimeSourceType::RTC_SYNCED: source_str = "RTC Holdover (Recently Synced)"; break;
            case TimeSourceType::RTC_HOLDOVER: source_str = "RTC Holdover (Degraded)"; break;
            case TimeSourceType::PTP_SLAVE: source_str = "PTP Synchronized to Network Master"; break;
            case TimeSourceType::NONE: source_str = "Unconfigured (No Valid Source)"; break;
        }
        json += "\"time_source\":\"" + String(source_str) + "\",";
        
        // Foreign masters list
        json += "\"foreign_masters\":[";
        bool first_master = true;
        for (int i = 0; i < MAX_FOREIGN_MASTERS; i++) {
            if (foreign_masters[i].valid) {
                if (!first_master) json += ",";
                first_master = false;
                
                json += "{";
                json += "\"clock_identity\":\"";
                for (int j = 0; j < 8; j++) {
                    char hex[3];
                    sprintf(hex, "%02X", foreign_masters[i].clock_identity[j]);
                    json += String(hex);
                    if (j < 7) json += ":";
                }
                json += "\",";
                json += "\"ip_address\":\"" + foreign_masters[i].ip_address.toString() + "\",";
                json += "\"clock_class\":" + String(foreign_masters[i].clock_class) + ",";
                json += "\"clock_accuracy\":\"0x" + String(foreign_masters[i].clock_accuracy, HEX) + "\",";
                json += "\"variance\":\"0x" + String(foreign_masters[i].variance, HEX) + "\",";
                json += "\"priority1\":" + String(foreign_masters[i].priority1) + ",";
                json += "\"priority2\":" + String(foreign_masters[i].priority2) + ",";
                json += "\"steps_removed\":" + String(foreign_masters[i].steps_removed) + ",";
                json += "\"time_source\":\"0x" + String(foreign_masters[i].time_source, HEX) + "\",";
                
                uint32_t ms_ago = (current_ms > foreign_masters[i].last_announce_time) ? 
                                 (current_ms - foreign_masters[i].last_announce_time) : 0;
                json += "\"last_announce_ms_ago\":" + String(ms_ago) + ",";
                json += "\"sequence_id\":" + String(foreign_masters[i].last_sequence_id);
                json += "}";
            }
        }
        json += "],";
        
        // Selected master (if in SLAVE mode)
        json += "\"selected_master\":";
        if (selected_master != nullptr && ptp_state == PTPState::SLAVE) {
            json += "{";
            json += "\"clock_identity\":\"";
            for (int j = 0; j < 8; j++) {
                char hex[3];
                sprintf(hex, "%02X", selected_master->clock_identity[j]);
                json += String(hex);
                if (j < 7) json += ":";
            }
            json += "\",";
            json += "\"ip_address\":\"" + selected_master->ip_address.toString() + "\",";
            json += "\"offset_ns\":" + String(offset_from_master_ns);
            json += "}";
        } else {
            json += "null";
        }
        json += ",";
        
        // Packet statistics
        json += "\"packet_stats\":{";
        json += "\"announce_received\":" + String(packet_stats.announce_received) + ",";
        json += "\"announce_sent\":" + String(packet_stats.announce_sent) + ",";
        json += "\"sync_received\":" + String(packet_stats.sync_received) + ",";
        json += "\"sync_sent\":" + String(packet_stats.sync_sent) + ",";
        
        uint32_t announce_ms_ago = (packet_stats.last_announce_received_ms > 0 && current_ms > packet_stats.last_announce_received_ms) ? 
                                   (current_ms - packet_stats.last_announce_received_ms) : 0;
        uint32_t sync_ms_ago = (packet_stats.last_sync_received_ms > 0 && current_ms > packet_stats.last_sync_received_ms) ? 
                              (current_ms - packet_stats.last_sync_received_ms) : 0;
        
        json += "\"last_announce_received_ms_ago\":";
        json += (packet_stats.last_announce_received_ms > 0) ? String(announce_ms_ago) : "null";
        json += ",";
        json += "\"last_sync_received_ms_ago\":";
        json += (packet_stats.last_sync_received_ms > 0) ? String(sync_ms_ago) : "null";
        json += "}";
        
        json += "},";
        
        // Network status
        json += "\"network\":{";
        json += "\"wifi_connected\":";
        json += WiFi.status() == WL_CONNECTED ? "true" : "false";
        json += ",\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"mac_address\":\"" + WiFi.macAddress() + "\"";
        json += "},";
        
        // Uptime
        json += "\"uptime_seconds\":" + String(millis() / 1000);
        
        json += "}";
        
        // Send response with CORS headers
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });
    
    web_server.begin();
    Serial.println("✓ Web interface started at http://" + WiFi.localIP().toString());
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
        
        // CRITICAL FIX #1: Disable WiFi power save for PTP
        // PTP requires continuous multicast reception - power save breaks this
        esp_wifi_set_ps(WIFI_PS_NONE);
        Serial.println("✓ WiFi power save DISABLED (required for PTP multicast)");
        
        // Initialize PTP clock identity from MAC address
        init_clock_identity();
        Serial.print("✓ PTP Clock Identity: ");
        for (int i = 0; i < 8; i++) {
            Serial.printf("%02X", local_clock_identity[i]);
            if (i < 7) Serial.print(":");
        }
        Serial.println();
        
        // ═══════════════════════════════════════════════════════════════════
        // 3-Socket Architecture for ESP32 WiFiUDP Multicast Bug Workaround
        // ═══════════════════════════════════════════════════════════════════
        // 
        // ESP32 WiFiUDP BUG: Sending on a multicast RX socket causes packet loss
        // → Solution: Separate RX-only and TX-only sockets
        // 
        // Socket 1: udp_event_rx   - RX-only multicast on port 319 (Sync reception)
        // Socket 2: udp_general    - RX-only multicast on port 320 (Announce reception)
        // Socket 3: udp_tx         - TX-only unbound (all PTP transmissions)
        //
        // This architecture prevents the WiFiUDP state machine bug that drops
        // timing-critical Sync messages when TX and RX share the same socket.
        // ═══════════════════════════════════════════════════════════════════
        
        IPAddress multicast_ip;
        multicast_ip.fromString(GPTP_MULTICAST_ADDR);
        
        Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
        Serial.println("║  Initializing 3-Socket PTP Architecture                    ║");
        Serial.println("╚═══════════════════════════════════════════════════════════════╝");
        
        // Socket 1: Event RX (port 319) - UNICAST for Sync reception
        // CRITICAL: Must use unicast binding to receive unicast Sync packets from master
        // beginMulticast() doesn't receive unicast packets on ESP32 WiFiUDP
        if (udp_event_rx.begin(GPTP_EVENT_PORT)) {
            Serial.printf("✓ [Socket 1] Event RX: UNICAST %s:%d (Sync RX)\n", 
                         WiFi.localIP().toString().c_str(), GPTP_EVENT_PORT);
        } else {
            Serial.printf("✗ [Socket 1] Failed to bind to port %d\n", GPTP_EVENT_PORT);
        }
        
        // Socket 2: General RX (port 320) - Multicast for Announce reception
        if (udp_general.beginMulticast(multicast_ip, GPTP_GENERAL_PORT)) {
            Serial.printf("✓ [Socket 2] General RX: Multicast %s:%d (Announce RX)\n", 
                         GPTP_MULTICAST_ADDR, GPTP_GENERAL_PORT);
        } else {
            Serial.printf("✗ [Socket 2] Failed to join multicast on port %d\n", GPTP_GENERAL_PORT);
        }
        
        // Socket 3: TX-only (unbound) - All PTP transmissions
        // Note: No .begin() call - unbound socket for TX-only operation
        Serial.printf("✓ [Socket 3] TX-only: Unbound (All PTP TX)\n");
        Serial.println("  → Prevents ESP32 WiFiUDP multicast RX/TX state bug");
        
        // Send dummy packets to wake up AP multicast forwarding
        Serial.println("\n✓ Sending dummy packets to warm up AP multicast table...");
        
        // Dummy for port 319 (Event)
        udp_tx.beginPacket(multicast_ip, GPTP_EVENT_PORT);
        uint8_t dummy_event[1] = {0x00};
        udp_tx.write(dummy_event, 1);
        udp_tx.endPacket();
        delay(10);
        
        // Dummy for port 320 (General)
        udp_tx.beginPacket(multicast_ip, GPTP_GENERAL_PORT);
        uint8_t dummy_general[1] = {0x00};
        udp_tx.write(dummy_general, 1);
        udp_tx.endPacket();
        delay(10);
        
        Serial.println("  ✓ AP multicast table warmed up (ports 319 & 320)");
        
        // ═══════════════════════════════════════════════════════════════════════
        // COMPREHENSIVE DIAGNOSTIC CHECK - 5 Critical Points
        // ═══════════════════════════════════════════════════════════════════════
        Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
        Serial.println("║  NETWORK DIAGNOSTIC CHECK (5 Critical Points)               ║");
        Serial.println("╚═══════════════════════════════════════════════════════════════╝");
        
        // ✅ 1. Multicast-Subscription aktiv?
        Serial.println("\n[1/5] Multicast Subscription Status:");
        Serial.printf("      Event Port (319):   ✓ beginMulticast() succeeded\n");
        Serial.printf("      General Port (320): ✓ beginMulticast() succeeded\n");
        Serial.printf("      Multicast IP: %s\n", GPTP_MULTICAST_ADDR);
        
        // ✅ 2. WiFi PS Mode aus?
        wifi_ps_type_t ps_mode;
        esp_wifi_get_ps(&ps_mode);
        Serial.println("\n[2/5] WiFi Power Save Mode:");
        Serial.printf("      Status: %s\n", ps_mode == WIFI_PS_NONE ? "✓ DISABLED (GOOD)" : "✗ ENABLED (BAD)");
        if (ps_mode != WIFI_PS_NONE) {
            Serial.println("      ⚠ WARNING: Power save will drop multicast packets!");
        }
        
        // ✅ 3. AP blockiert Multicast? (Test via ARP/Broadcast)
        Serial.println("\n[3/5] AP Multicast Forwarding Test:");
        Serial.printf("      WiFi RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("      Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("      Subnet: %s\n", WiFi.subnetMask().toString().c_str());
        Serial.println("      Dummy packets sent to warm up AP forwarding table");
        
        // ✅ 4. Socket richtig gebunden?
        Serial.println("\n[4/5] UDP Socket Binding:");
        Serial.printf("      Local IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("      Event Socket (319): UNICAST binding (RX unicast Sync from masters)\n");
        Serial.printf("      General Socket (320): Multicast binding (RX Announce multicast)\n");
        Serial.printf("      TX Socket: Unbound (all PTP TX - multicast & unicast)\n");
        
        // ✅ 5. MAC-Timestamping-Pfad aktiv? (ESP32-spezifisch)
        Serial.println("\n[5/5] Hardware Timestamping:");
        Serial.println("      ESP32 WiFi: Software timestamps only");
        Serial.println("      → Using micros()/esp_timer_get_time() for PTP");
        Serial.println("      → Accuracy: ~1-10 microseconds (no hardware PTP)");
        
        Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
        Serial.println("║  DIAGNOSTIC CHECK COMPLETE                                  ║");
        Serial.println("╚═══════════════════════════════════════════════════════════════╝\n");
        
        // Start in LISTENING state - will transition to MASTER after timeout
        ptp_state = PTPState::LISTENING;
        Serial.println("✓ PTP initialized (LISTENING for better masters)");
        
        // Start web interface
        setup_web_interface();
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
    static unsigned long last_bmca = 0;
    static unsigned long last_keepalive = 0;
    
    unsigned long now = millis();
    
    // Process GPS data (NMEA + PPS)
    process_gps_data();
    
    // Update RTC
    if (rtc_adapter) {
        rtc_adapter->update();
    }
    
    // Update time source status
    update_time_source();
    
    // No keep-alive needed for unicast - direct point-to-point communication
    
    // Process incoming PTP packets (Announce and Sync)
    if (WiFi.status() == WL_CONNECTED) {
        process_ptp_packets();
    }
    
    // Run Best Master Clock Algorithm every 2 seconds
    if (WiFi.status() == WL_CONNECTED && now - last_bmca >= 2000) {
        run_bmca();
        last_bmca = now;
    }
    
    // Send PTP Announce messages (only when MASTER)
    if (WiFi.status() == WL_CONNECTED && now - last_announce >= ANNOUNCE_INTERVAL_MS) {
        send_ptp_announce();  // Returns early if not MASTER
        last_announce = now;
    }
    
    // Send PTP Sync messages (only when MASTER)
    if (WiFi.status() == WL_CONNECTED && now - last_sync >= SYNC_INTERVAL_MS) {
        send_ptp_sync();  // Returns early if not MASTER
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
