/**
 * @file gps_ptp_sync_example.cpp
 * @brief Complete GPS NMEA + PPS + IEEE 1588-2019 Clock Quality Integration Example
 * 
 * Demonstrates the full integration of:
 * - GPS NMEA parsing (time extraction)
 * - PPS hardware detection (sub-microsecond timestamping)
 * - Dynamic clock quality management (IEEE 1588-2019 compliant)
 * - PTP Grandmaster attribute updates
 * 
 * This example shows how a GPS-based PTP Grandmaster should dynamically
 * adjust its clock quality attributes based on GPS fix status and PPS
 * detection state, ensuring proper BMCA behavior in a PTP network.
 * 
 * Hardware Requirements:
 * - GT-U7 GPS Module (or compatible NEO-6M/7M)
 * - USB connection for NMEA data
 * - Pin 3 (TIMEPULSE) → Serial DCD (Pin 1) for PPS
 * 
 * @see IEEE 1588-2019, Section 8.6.2 "Clock quality attributes"
 * @see IEEE 1588-2019, Section 9.3 "Best master clock algorithm"
 * @see CLOCK_QUALITY_MANAGEMENT.md for detailed documentation
 */

#include "nmea_parser.hpp"
#include "gps_time_converter.hpp"
#include "pps_detector.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <ctime>

using namespace GPS;

/**
 * @brief Simulated PTP Clock Interface
 * 
 * In a real implementation, this would interact with the actual
 * IEEE 1588-2019 PTP clock implementation (clocks.hpp).
 */
class PTPClockInterface {
public:
    /**
     * @brief Update PTP clock quality attributes
     * 
     * Updates the defaultDS (Default Data Set) and timePropertiesDS
     * (Time Properties Data Set) based on GPS/PPS quality.
     * 
     * @param quality Updated clock quality attributes
     */
    void update_clock_quality(const Time::GPSTimeConverter::ClockQualityAttributes& quality) {
        bool quality_changed = false;
        
        // Check if clock quality changed
        if (current_clock_class_ != quality.clock_class) {
            std::cout << "  [PTP] clockClass changed: " 
                      << (int)current_clock_class_ << " → " << (int)quality.clock_class << "\n";
            current_clock_class_ = quality.clock_class;
            quality_changed = true;
        }
        
        if (current_clock_accuracy_ != quality.clock_accuracy) {
            std::cout << "  [PTP] clockAccuracy changed: 0x" << std::hex 
                      << (int)current_clock_accuracy_ << " → 0x" << (int)quality.clock_accuracy 
                      << std::dec << "\n";
            current_clock_accuracy_ = quality.clock_accuracy;
            quality_changed = true;
        }
        
        if (current_time_source_ != quality.time_source) {
            std::cout << "  [PTP] timeSource changed: 0x" << std::hex 
                      << (int)current_time_source_ << " → 0x" << (int)quality.time_source 
                      << std::dec;
            if (quality.time_source == 0x20) {
                std::cout << " (GPS)\n";
            } else if (quality.time_source == 0xA0) {
                std::cout << " (INTERNAL_OSCILLATOR)\n";
            } else {
                std::cout << "\n";
            }
            current_time_source_ = quality.time_source;
            quality_changed = true;
        }
        
        if (quality_changed) {
            std::cout << "  [PTP] Triggering BMCA re-evaluation...\n";
            std::cout << "  [PTP] Next Announce message will advertise updated quality\n";
            
            // In real implementation:
            // ptp_clock_.get_default_data_set().clockQuality = quality;
            // ptp_clock_.get_time_properties_data_set().timeSource = quality.time_source;
            // ptp_clock_.trigger_announce_update();
        }
        
        // Store current quality
        current_variance_ = quality.offset_scaled_log_variance;
        current_priority1_ = quality.priority1;
        current_priority2_ = quality.priority2;
    }
    
    /**
     * @brief Get current clock quality summary
     */
    void print_current_quality() const {
        std::cout << "Current PTP Clock Quality:\n";
        std::cout << "  clockClass:     " << (int)current_clock_class_;
        switch (current_clock_class_) {
            case 6:   std::cout << " (Primary reference - GPS traceable)"; break;
            case 7:   std::cout << " (Primary reference - holdover)"; break;
            case 248: std::cout << " (Default - not traceable)"; break;
        }
        std::cout << "\n";
        
        std::cout << "  clockAccuracy:  0x" << std::hex << (int)current_clock_accuracy_ << std::dec;
        switch (current_clock_accuracy_) {
            case 0x20: std::cout << " (25 nanoseconds)"; break;
            case 0x21: std::cout << " (100 nanoseconds)"; break;
            case 0x22: std::cout << " (250 nanoseconds)"; break;
            case 0x31: std::cout << " (10 milliseconds)"; break;
            case 0xFE: std::cout << " (Unknown)"; break;
        }
        std::cout << "\n";
        
        std::cout << "  timeSource:     0x" << std::hex << (int)current_time_source_ << std::dec;
        if (current_time_source_ == 0x20) {
            std::cout << " (GPS)";
        } else if (current_time_source_ == 0xA0) {
            std::cout << " (INTERNAL_OSCILLATOR)";
        }
        std::cout << "\n";
        
        std::cout << "  variance:       0x" << std::hex << current_variance_ << std::dec << "\n";
        std::cout << "  priority1:      " << (int)current_priority1_ << "\n";
        std::cout << "  priority2:      " << (int)current_priority2_ << "\n";
    }
    
private:
    uint8_t  current_clock_class_ = 248;      // Default: not traceable
    uint8_t  current_clock_accuracy_ = 0xFE;  // Unknown
    uint16_t current_variance_ = 0xFFFF;      // Maximum variance
    uint8_t  current_time_source_ = 0xA0;     // Internal oscillator
    uint8_t  current_priority1_ = 128;        // Default
    uint8_t  current_priority2_ = 128;        // Default
};

/**
 * @brief Monitor and log quality state changes
 */
class QualityMonitor {
public:
    void log_gps_fix_change(NMEA::GPSFixStatus old_fix, NMEA::GPSFixStatus new_fix) {
        std::cout << "\n*** GPS Fix Status Changed ***\n";
        std::cout << "  Previous: " << fix_status_to_string(old_fix) << "\n";
        std::cout << "  Current:  " << fix_status_to_string(new_fix) << "\n";
        
        if (new_fix == NMEA::GPSFixStatus::NO_FIX) {
            std::cout << "  ⚠️  WARNING: GPS signal lost! Clock running on internal oscillator.\n";
            std::cout << "  ⚠️  Timing accuracy degraded. Clock will drift over time.\n";
        } else if (old_fix == NMEA::GPSFixStatus::NO_FIX) {
            std::cout << "  ✓ GPS signal acquired! Clock can now synchronize to GPS time.\n";
        }
    }
    
    void log_pps_state_change(PPS::DetectionState old_state, PPS::DetectionState new_state) {
        std::cout << "\n*** PPS Detection State Changed ***\n";
        std::cout << "  Previous: " << pps_state_to_string(old_state) << "\n";
        std::cout << "  Current:  " << pps_state_to_string(new_state) << "\n";
        
        if (new_state == PPS::DetectionState::Locked) {
            std::cout << "  ✓ PPS locked! Timing accuracy improved: 10ms → 100ns\n";
            std::cout << "  ✓ Sub-microsecond timestamping now available.\n";
        } else if (old_state == PPS::DetectionState::Locked) {
            std::cout << "  ⚠️  WARNING: PPS signal lost! Falling back to NMEA-only mode.\n";
            std::cout << "  ⚠️  Timing accuracy degraded: 100ns → 10ms\n";
        }
    }
    
    void log_quality_update(const Time::GPSTimeConverter::ClockQualityAttributes& quality) {
        update_count_++;
        std::cout << "\n=== Clock Quality Update #" << update_count_ << " ===\n";
        std::cout << "Timestamp: " << get_current_time_string() << "\n";
    }
    
    void print_statistics() const {
        std::cout << "\n=== Quality Monitoring Statistics ===\n";
        std::cout << "Total quality updates: " << update_count_ << "\n";
    }
    
private:
    int update_count_ = 0;
    
    std::string fix_status_to_string(NMEA::GPSFixStatus status) const {
        switch (status) {
            case NMEA::GPSFixStatus::NO_FIX:           return "NO_FIX";
            case NMEA::GPSFixStatus::TIME_ONLY:        return "TIME_ONLY";
            case NMEA::GPSFixStatus::AUTONOMOUS_FIX:   return "AUTONOMOUS_FIX (3D)";
            case NMEA::GPSFixStatus::DGPS_FIX:         return "DGPS_FIX";
            case NMEA::GPSFixStatus::SIGNAL_LOST:      return "SIGNAL_LOST";
            default:                                    return "UNKNOWN";
        }
    }
    
    std::string pps_state_to_string(PPS::DetectionState state) const {
        switch (state) {
            case PPS::DetectionState::Idle:      return "Idle (not started)";
            case PPS::DetectionState::Detecting: return "Detecting (monitoring pins)";
            case PPS::DetectionState::Locked:    return "Locked (PPS detected)";
            case PPS::DetectionState::Failed:    return "Failed (timeout/no PPS)";
            default:                              return "UNKNOWN";
        }
    }
    
    std::string get_current_time_string() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::tm tm_buf;
        #ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
        #else
        localtime_r(&time_t, &tm_buf);
        #endif
        
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);
        return std::string(buffer);
    }
};

/**
 * @brief Main GPS + PPS + PTP Clock Quality Integration Example
 */
int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "GPS NMEA + PPS + PTP Clock Quality\n";
    std::cout << "IEEE 1588-2019 Integration Example\n";
    std::cout << "========================================\n\n";
    
    // Initialize components
    NMEA::NMEAParser nmea_parser;
    Time::GPSTimeConverter time_converter;
    PTPClockInterface ptp_clock;
    QualityMonitor monitor;
    
    // Simulate serial port (in real implementation, open actual serial port)
    void* serial_handle = nullptr;  // Replace with real handle: open_serial_port("COM3")
    
    std::cout << "Initializing GPS + PPS system...\n\n";
    
    // Track previous states
    NMEA::GPSFixStatus previous_fix = NMEA::GPSFixStatus::NO_FIX;
    PPS::DetectionState previous_pps_state = PPS::DetectionState::Idle;
    
    // Simulation mode: demonstrate different scenarios
    std::cout << "=== SIMULATION MODE ===\n";
    std::cout << "Demonstrating quality changes for different GPS/PPS states...\n\n";
    
    // Scenario 1: System startup (no GPS, no PPS)
    std::cout << "--- Scenario 1: System Startup ---\n";
    NMEA::GPSFixStatus current_fix = NMEA::GPSFixStatus::NO_FIX;
    PPS::DetectionState current_pps_state = PPS::DetectionState::Idle;
    
    monitor.log_quality_update(time_converter.get_clock_quality());
    auto quality = time_converter.update_clock_quality(
        current_fix, 
        static_cast<uint8_t>(current_pps_state)
    );
    ptp_clock.update_clock_quality(quality);
    ptp_clock.print_current_quality();
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Scenario 2: GPS acquires time-only fix
    std::cout << "\n--- Scenario 2: GPS Time-Only Fix Acquired ---\n";
    previous_fix = current_fix;
    current_fix = NMEA::GPSFixStatus::TIME_ONLY;
    monitor.log_gps_fix_change(previous_fix, current_fix);
    
    monitor.log_quality_update(time_converter.get_clock_quality());
    quality = time_converter.update_clock_quality(
        current_fix, 
        static_cast<uint8_t>(current_pps_state)
    );
    ptp_clock.update_clock_quality(quality);
    ptp_clock.print_current_quality();
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Scenario 3: GPS acquires full 3D fix
    std::cout << "\n--- Scenario 3: GPS 3D Fix Acquired ---\n";
    previous_fix = current_fix;
    current_fix = NMEA::GPSFixStatus::AUTONOMOUS_FIX;
    monitor.log_gps_fix_change(previous_fix, current_fix);
    
    monitor.log_quality_update(time_converter.get_clock_quality());
    quality = time_converter.update_clock_quality(
        current_fix, 
        static_cast<uint8_t>(current_pps_state)
    );
    ptp_clock.update_clock_quality(quality);
    ptp_clock.print_current_quality();
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Scenario 4: PPS detection starts
    std::cout << "\n--- Scenario 4: PPS Detection Started ---\n";
    previous_pps_state = current_pps_state;
    current_pps_state = PPS::DetectionState::Detecting;
    monitor.log_pps_state_change(previous_pps_state, current_pps_state);
    
    monitor.log_quality_update(time_converter.get_clock_quality());
    quality = time_converter.update_clock_quality(
        current_fix, 
        static_cast<uint8_t>(current_pps_state)
    );
    ptp_clock.update_clock_quality(quality);
    ptp_clock.print_current_quality();
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Scenario 5: PPS locked! (OPTIMAL STATE)
    std::cout << "\n--- Scenario 5: PPS Locked! (OPTIMAL) ---\n";
    previous_pps_state = current_pps_state;
    current_pps_state = PPS::DetectionState::Locked;
    monitor.log_pps_state_change(previous_pps_state, current_pps_state);
    
    monitor.log_quality_update(time_converter.get_clock_quality());
    quality = time_converter.update_clock_quality(
        current_fix, 
        static_cast<uint8_t>(current_pps_state)
    );
    ptp_clock.update_clock_quality(quality);
    ptp_clock.print_current_quality();
    
    std::cout << "\n✓✓✓ OPTIMAL STATE REACHED ✓✓✓\n";
    std::cout << "GPS: 3D Fix + PPS: Locked = 100ns accuracy\n";
    std::cout << "This clock is now a high-quality PTP Grandmaster!\n";
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Scenario 6: Simulate GPS signal loss (going indoors)
    std::cout << "\n--- Scenario 6: GPS Signal Lost (Indoor/Tunnel) ---\n";
    previous_fix = current_fix;
    current_fix = NMEA::GPSFixStatus::NO_FIX;
    monitor.log_gps_fix_change(previous_fix, current_fix);
    
    previous_pps_state = current_pps_state;
    current_pps_state = PPS::DetectionState::Failed;
    monitor.log_pps_state_change(previous_pps_state, current_pps_state);
    
    monitor.log_quality_update(time_converter.get_clock_quality());
    quality = time_converter.update_clock_quality(
        current_fix, 
        static_cast<uint8_t>(current_pps_state)
    );
    ptp_clock.update_clock_quality(quality);
    ptp_clock.print_current_quality();
    
    std::cout << "\n⚠️⚠️⚠️ DEGRADED STATE ⚠️⚠️⚠️\n";
    std::cout << "Clock is now running on internal oscillator!\n";
    std::cout << "Another PTP clock should take over as Grandmaster.\n";
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Scenario 7: GPS recovers
    std::cout << "\n--- Scenario 7: GPS Signal Recovered ---\n";
    previous_fix = current_fix;
    current_fix = NMEA::GPSFixStatus::AUTONOMOUS_FIX;
    monitor.log_gps_fix_change(previous_fix, current_fix);
    
    previous_pps_state = current_pps_state;
    current_pps_state = PPS::DetectionState::Locked;
    monitor.log_pps_state_change(previous_pps_state, current_pps_state);
    
    monitor.log_quality_update(time_converter.get_clock_quality());
    quality = time_converter.update_clock_quality(
        current_fix, 
        static_cast<uint8_t>(current_pps_state)
    );
    ptp_clock.update_clock_quality(quality);
    ptp_clock.print_current_quality();
    
    std::cout << "\n✓ System recovered! Back to optimal state.\n";
    
    // Print final statistics
    std::cout << "\n";
    monitor.print_statistics();
    
    std::cout << "\n========================================\n";
    std::cout << "Real Hardware Integration Steps:\n";
    std::cout << "========================================\n";
    std::cout << "1. Connect GT-U7 GPS module:\n";
    std::cout << "   - USB cable → PC (NMEA data + power)\n";
    std::cout << "   - Pin 3 (TIMEPULSE) → Serial DCD (Pin 1)\n";
    std::cout << "   - Pin 24 (GND) → Serial GND (Pin 5)\n\n";
    
    std::cout << "2. Modify this code to use real hardware:\n";
    std::cout << "   - Open actual serial port: serial_handle = open_serial_port(\"COM3\");\n";
    std::cout << "   - Initialize PPSDetector: PPS::PPSDetector detector(serial_handle);\n";
    std::cout << "   - detector.start_detection(10000); // 10s timeout\n\n";
    
    std::cout << "3. Main loop:\n";
    std::cout << "   - Parse NMEA sentences from serial port\n";
    std::cout << "   - Check PPS detection state periodically\n";
    std::cout << "   - Update clock quality when GPS/PPS state changes\n";
    std::cout << "   - Update PTP clock attributes\n\n";
    
    std::cout << "4. Monitor quality changes:\n";
    std::cout << "   - Watch for GPS fix changes (satellite acquisition/loss)\n";
    std::cout << "   - Watch for PPS state changes (lock/unlock)\n";
    std::cout << "   - Verify BMCA selects correct Grandmaster\n\n";
    
    std::cout << "5. Expected performance:\n";
    std::cout << "   - NMEA-only:   ±10ms accuracy\n";
    std::cout << "   - NMEA + PPS:  ±100ns accuracy (100× better!)\n";
    std::cout << "   - DGPS + PPS:  ±25ns accuracy (optimal)\n\n";
    
    std::cout << "See CLOCK_QUALITY_MANAGEMENT.md for detailed documentation.\n";
    std::cout << "See README.md for hardware wiring diagrams.\n\n";
    
    std::cout << "Simulation completed successfully!\n";
    
    return 0;
}
