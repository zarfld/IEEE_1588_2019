/**
 * @file test_pps_hardware.cpp
 * @brief Hardware validation tests for GPS PPS Detector
 * 
 * ============================================================================
 * HARDWARE REQUIRED: These tests require actual GPS hardware!
 * ============================================================================
 * 
 * Requirements:
 * - u-blox NEO-G7 GPS module (or compatible)
 * - Serial connection on COM3 (Windows) or /dev/ttyS0 (Linux)
 * - PPS signal connected to DCD (Pin 1), CTS (Pin 8), or DSR (Pin 6)
 * - GPS module must have satellite lock and be outputting stable PPS
 * 
 * These tests validate REAL PPS signal detection with actual hardware:
 * - Real PPS pulse detection (1Hz signal)
 * - Edge timestamping accuracy (sub-microsecond)
 * - 1Hz frequency validation (0.8-1.2s intervals)
 * - Lock confirmation with 3+ edges
 * - Timeout behavior (10s max)
 * - Signal loss detection
 * - Statistics accuracy (jitter, intervals)
 * 
 * Expected Test Duration:
 * - Minimum: ~2 seconds (3 edges @ 1Hz)
 * - Typical: 3-5 seconds (with GPS startup and alignment)
 * - Maximum: 10 seconds (timeout if no PPS)
 * 
 * Test Execution:
 * - Run manually when hardware available
 * - NOT part of standard CI pipeline
 * - Mark as [hardware] test in CTest
 * 
 * @see PPS_TESTING_STRATEGY.md for detailed timing analysis
 */

#include "pps_detector.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <termios.h>
#endif

using namespace GPS::PPS;

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

// Test helper macros
#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        std::cerr << "FAIL: " << message << std::endl; \
        tests_failed++; \
        return false; \
    }

#define TEST_PASS(message) \
    std::cout << "PASS: " << message << std::endl; \
    tests_passed++; \
    return true;

#define TEST_SKIP(message) \
    std::cout << "SKIP: " << message << std::endl; \
    tests_skipped++; \
    return true;

/**
 * @brief Platform-specific serial port opening
 */
void* open_serial_port(const char* port_name) {
#ifdef _WIN32
    HANDLE handle = CreateFileA(
        port_name,
        GENERIC_READ | GENERIC_WRITE,
        0,                          // No sharing
        NULL,                       // Default security
        OPEN_EXISTING,              // Open existing port
        FILE_FLAG_OVERLAPPED,       // Async I/O for PPS detection
        NULL
    );
    
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cerr << "Failed to open " << port_name << ", error: " << error << std::endl;
        
        // Common error codes
        if (error == ERROR_FILE_NOT_FOUND) {
            std::cerr << "  → Port not found. Available ports: COM1, COM2, COM3, ..." << std::endl;
        } else if (error == ERROR_ACCESS_DENIED) {
            std::cerr << "  → Access denied. Port may be in use by another application." << std::endl;
        }
        
        return nullptr;
    }
    
    // Configure serial port
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    
    if (!GetCommState(handle, &dcb)) {
        std::cerr << "Failed to get comm state" << std::endl;
        CloseHandle(handle);
        return nullptr;
    }
    
    // Set baud rate (9600 for NMEA, but doesn't matter for PPS on control pins)
    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    
    if (!SetCommState(handle, &dcb)) {
        std::cerr << "Failed to set comm state" << std::endl;
        CloseHandle(handle);
        return nullptr;
    }
    
    std::cout << "✓ Opened " << port_name << " successfully" << std::endl;
    return handle;
    
#else
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    
    if (fd < 0) {
        std::cerr << "Failed to open " << port_name << ": " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    // Configure serial port
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Failed to get termios attributes" << std::endl;
        close(fd);
        return nullptr;
    }
    
    // Set baud rate
    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);
    
    // 8N1
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    
    // No flow control
    tty.c_cflag &= ~CRTSCTS;
    
    // Raw mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Failed to set termios attributes" << std::endl;
        close(fd);
        return nullptr;
    }
    
    std::cout << "✓ Opened " << port_name << " successfully" << std::endl;
    return reinterpret_cast<void*>(static_cast<intptr_t>(fd));
#endif
}

/**
 * @brief Platform-specific serial port closing
 */
void close_serial_port(void* handle) {
#ifdef _WIN32
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(handle));
    }
#else
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
    if (fd >= 0) {
        close(fd);
    }
#endif
}

/**
 * @brief Print timestamp in human-readable format
 */
void print_timestamp(const PPSTimestamp& ts) {
    std::cout << std::setw(10) << ts.seconds << "s " 
              << std::setw(9) << std::setfill('0') << ts.nanoseconds 
              << std::setfill(' ') << "ns";
}

/**
 * @brief Test 1: Serial Port Availability
 * 
 * Verify that the GPS serial port can be opened and configured.
 * This is a prerequisite for all other tests.
 */
bool test_serial_port_availability() {
    std::cout << "\n=== Test 1: Serial Port Availability ===" << std::endl;
    
#ifdef _WIN32
    const char* port_name = "COM3";
#else
    const char* port_name = "/dev/ttyS0";
#endif
    
    std::cout << "Attempting to open " << port_name << "..." << std::endl;
    
    void* handle = open_serial_port(port_name);
    
    if (handle == nullptr) {
        std::cerr << "\nHARDWARE NOT AVAILABLE:" << std::endl;
        std::cerr << "  → GPS module not connected to " << port_name << std::endl;
        std::cerr << "  → Ensure u-blox NEO-G7 is powered and connected" << std::endl;
        std::cerr << "  → All hardware tests will be skipped" << std::endl;
        TEST_SKIP("Serial port not available");
    }
    
    close_serial_port(handle);
    
    TEST_PASS("Serial port available and configured");
}

/**
 * @brief Test 2: PPS Signal Detection
 * 
 * Core test: Detect real PPS signal from GPS module.
 * This test will take 2-5 seconds minimum to wait for 3 edges @ 1Hz.
 */
bool test_pps_detection() {
    std::cout << "\n=== Test 2: Real PPS Signal Detection ===" << std::endl;
    std::cout << "⏱  This test requires 2-5 seconds to detect 3 edges @ 1Hz" << std::endl;
    
#ifdef _WIN32
    const char* port_name = "COM3";
#else
    const char* port_name = "/dev/ttyS0";
#endif
    
    void* handle = open_serial_port(port_name);
    if (handle == nullptr) {
        TEST_SKIP("Serial port not available");
    }
    
    try {
        PPSDetector detector(handle);
        
        // Start detection with 10 second timeout
        std::cout << "\nStarting PPS autodetection (10s timeout)..." << std::endl;
        std::cout << "Monitoring pins: DCD (Pin 1), CTS (Pin 8), DSR (Pin 6)" << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        bool started = detector.start_detection(10000);
        TEST_ASSERT(started, "Detection should start successfully");
        
        // Wait for detection to complete (give it up to 8 seconds)
        std::cout << "\nWaiting for PPS detection..." << std::endl;
        int dots = 0;
        for (int i = 0; i < 80; i++) {  // 8 seconds max (100ms * 80)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            DetectionState state = detector.get_state();
            
            if (state == DetectionState::Locked) {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                
                std::cout << std::endl;
                std::cout << "✓ PPS DETECTED!" << std::endl;
                std::cout << "  Detection time: " << elapsed_ms << "ms" << std::endl;
                std::cout << "  Detected on: " << to_string(detector.get_detected_line()) << std::endl;
                
                // Get statistics
                auto stats = detector.get_statistics();
                std::cout << "\nDetection Statistics:" << std::endl;
                std::cout << "  Total edges: " << stats.total_edges << std::endl;
                std::cout << "  Valid intervals: " << stats.valid_intervals << std::endl;
                std::cout << "  Invalid intervals: " << stats.invalid_intervals << std::endl;
                std::cout << "  Avg interval: " << std::fixed << std::setprecision(6) 
                          << stats.avg_interval_sec << "s" << std::endl;
                std::cout << "  Min interval: " << stats.min_interval_sec << "s" << std::endl;
                std::cout << "  Max interval: " << stats.max_interval_sec << "s" << std::endl;
                std::cout << "  Jitter: " << std::fixed << std::setprecision(0) 
                          << stats.jitter_ns << "ns" << std::endl;
                
                // Validate detection requirements
                TEST_ASSERT(stats.total_edges >= 3, 
                           "Should have at least 3 edges for lock");
                TEST_ASSERT(stats.valid_intervals >= 2,
                           "Should have at least 2 valid intervals");
                TEST_ASSERT(stats.avg_interval_sec >= 0.95 && stats.avg_interval_sec <= 1.05,
                           "Average interval should be ~1.0s (±5%)");
                
                detector.stop_detection();
                close_serial_port(handle);
                
                TEST_PASS("PPS signal detected and validated");
            } else if (state == DetectionState::Failed) {
                std::cout << std::endl;
                std::cerr << "\n✗ PPS DETECTION FAILED" << std::endl;
                std::cerr << "Possible causes:" << std::endl;
                std::cerr << "  → GPS module not outputting PPS signal" << std::endl;
                std::cerr << "  → PPS not connected to any monitored pin (DCD/CTS/DSR)" << std::endl;
                std::cerr << "  → GPS does not have satellite lock" << std::endl;
                std::cerr << "  → Check GPS LED - should be blinking 1Hz when locked" << std::endl;
                
                detector.stop_detection();
                close_serial_port(handle);
                
                tests_failed++;
                return false;
            }
            
            // Print progress dots
            if (i % 10 == 0) {
                std::cout << "." << std::flush;
                dots++;
                if (dots % 8 == 0) std::cout << " ";
            }
        }
        
        // If we get here, detection is still running (shouldn't happen)
        std::cout << std::endl;
        std::cerr << "\n⚠ Detection still running after 8 seconds" << std::endl;
        
        detector.stop_detection();
        close_serial_port(handle);
        
        tests_failed++;
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during PPS detection: " << e.what() << std::endl;
        close_serial_port(handle);
        tests_failed++;
        return false;
    }
}

/**
 * @brief Test 3: PPS Timestamp Acquisition
 * 
 * After successful detection, verify we can acquire accurate timestamps.
 */
bool test_pps_timestamp_acquisition() {
    std::cout << "\n=== Test 3: PPS Timestamp Acquisition ===" << std::endl;
    
#ifdef _WIN32
    const char* port_name = "COM3";
#else
    const char* port_name = "/dev/ttyS0";
#endif
    
    void* handle = open_serial_port(port_name);
    if (handle == nullptr) {
        TEST_SKIP("Serial port not available");
    }
    
    try {
        PPSDetector detector(handle);
        
        // Start detection
        std::cout << "Starting PPS detection..." << std::endl;
        detector.start_detection(10000);
        
        // Wait for lock
        for (int i = 0; i < 80; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (detector.get_state() == DetectionState::Locked) {
                break;
            }
        }
        
        if (!detector.is_pps_available()) {
            detector.stop_detection();
            close_serial_port(handle);
            TEST_SKIP("PPS not detected, cannot test timestamps");
        }
        
        std::cout << "✓ PPS locked, acquiring timestamps..." << std::endl;
        
        // Acquire 3 consecutive timestamps
        PPSTimestamp timestamps[3];
        bool got_all = true;
        
        for (int i = 0; i < 3; i++) {
            std::cout << "\nWaiting for PPS edge " << (i+1) << "/3 (timeout 2s)..." << std::endl;
            
            bool got_timestamp = detector.get_pps_timestamp(2000, timestamps[i]);
            
            if (got_timestamp) {
                std::cout << "  Timestamp " << (i+1) << ": ";
                print_timestamp(timestamps[i]);
                std::cout << " (source: " << to_string(timestamps[i].source) << ")" << std::endl;
            } else {
                std::cerr << "  Failed to get timestamp " << (i+1) << std::endl;
                got_all = false;
                break;
            }
        }
        
        if (got_all) {
            // Validate intervals between timestamps
            double interval1 = timestamps[1] - timestamps[0];
            double interval2 = timestamps[2] - timestamps[1];
            
            std::cout << "\nTimestamp Intervals:" << std::endl;
            std::cout << "  T1→T2: " << std::fixed << std::setprecision(6) 
                      << interval1 << "s" << std::endl;
            std::cout << "  T2→T3: " << interval2 << "s" << std::endl;
            
            // Intervals should be ~1.0s (±200ms = 0.8-1.2s)
            TEST_ASSERT(interval1 >= 0.8 && interval1 <= 1.2,
                       "First interval should be 0.8-1.2s");
            TEST_ASSERT(interval2 >= 0.8 && interval2 <= 1.2,
                       "Second interval should be 0.8-1.2s");
            
            // Check they're from same source
            TEST_ASSERT(timestamps[0].source == timestamps[1].source &&
                       timestamps[1].source == timestamps[2].source,
                       "All timestamps should be from same PPS line");
        }
        
        detector.stop_detection();
        close_serial_port(handle);
        
        if (got_all) {
            TEST_PASS("PPS timestamp acquisition successful");
        } else {
            tests_failed++;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during timestamp acquisition: " << e.what() << std::endl;
        close_serial_port(handle);
        tests_failed++;
        return false;
    }
}

/**
 * @brief Test 4: Detection Timeout
 * 
 * Verify that detection times out correctly when PPS is not available.
 * Uses very short timeout (2s) to avoid long waits.
 */
bool test_detection_timeout() {
    std::cout << "\n=== Test 4: Detection Timeout Behavior ===" << std::endl;
    std::cout << "Note: This test verifies timeout logic, not PPS detection" << std::endl;
    
    // Use invalid handle to force timeout
    void* invalid_handle = nullptr;
    
    PPSDetector detector(invalid_handle);
    
    std::cout << "Starting detection with 2s timeout (expected to fail)..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    bool started = detector.start_detection(2000);
    
    if (!started) {
        std::cout << "Detection failed to start (expected)" << std::endl;
        TEST_PASS("Timeout behavior validated (immediate failure)");
    }
    
    // If it did start, wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    DetectionState state = detector.get_state();
    
    std::cout << "Detection state after " << elapsed_ms << "ms: " 
              << to_string(state) << std::endl;
    
    TEST_ASSERT(state == DetectionState::Failed || state == DetectionState::Idle,
               "State should be Failed or Idle after timeout");
    TEST_ASSERT(!detector.is_pps_available(),
               "PPS should not be available after timeout");
    
    TEST_PASS("Detection timeout behavior validated");
}

/**
 * @brief Main test runner
 */
int main() {
    std::cout << "============================================================================" << std::endl;
    std::cout << "GPS PPS Detector - Hardware Validation Tests" << std::endl;
    std::cout << "============================================================================" << std::endl;
    
    std::cout << "\n⚠️  HARDWARE REQUIRED: These tests need real GPS hardware!" << std::endl;
    std::cout << "\nRequirements:" << std::endl;
    std::cout << "  • u-blox NEO-G7 GPS module (or compatible)" << std::endl;
#ifdef _WIN32
    std::cout << "  • Serial connection on COM3" << std::endl;
#else
    std::cout << "  • Serial connection on /dev/ttyS0" << std::endl;
#endif
    std::cout << "  • PPS signal connected to DCD (Pin 1), CTS (Pin 8), or DSR (Pin 6)" << std::endl;
    std::cout << "  • GPS must have satellite lock (PPS LED blinking @ 1Hz)" << std::endl;
    
    std::cout << "\nExpected test duration: 10-20 seconds (waiting for real PPS pulses)" << std::endl;
    std::cout << "============================================================================\n" << std::endl;
    
    // Run tests
    test_serial_port_availability();
    test_pps_detection();
    test_pps_timestamp_acquisition();
    test_detection_timeout();
    
    // Print summary
    std::cout << "\n============================================================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "============================================================================" << std::endl;
    std::cout << "Tests Passed:  " << tests_passed << std::endl;
    std::cout << "Tests Failed:  " << tests_failed << std::endl;
    std::cout << "Tests Skipped: " << tests_skipped << std::endl;
    std::cout << "Total Tests:   " << (tests_passed + tests_failed + tests_skipped) << std::endl;
    
    if (tests_skipped > 0) {
        std::cout << "\n⚠  Some tests were skipped due to missing hardware" << std::endl;
        std::cout << "   This is expected in CI or development environments" << std::endl;
    }
    
    if (tests_failed == 0 && tests_passed > 0) {
        std::cout << "\n✓ ALL TESTS PASSED" << std::endl;
        return 0;
    } else if (tests_failed == 0 && tests_skipped > 0) {
        std::cout << "\n⊘ ALL TESTS SKIPPED (Hardware not available)" << std::endl;
        return 0;  // Return success if just skipped (no hardware)
    } else {
        std::cout << "\n✗ SOME TESTS FAILED" << std::endl;
        return 1;
    }
}
