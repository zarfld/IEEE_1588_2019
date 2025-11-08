/*
Module: include/Common/utils/logger.hpp
Phase: 05-implementation
Traceability:
  Design: DES-C-020  # Reliability logging interface
  Requirements: REQ-NF-REL-001  # Reliability evidence logging (Phase 05 hooks)
  Tests: TEST-UNIT-OFFSET-CALCULATION, TEST-UNIT-BMCA-BASIC (indirect usage)
Notes: Header-only, hardware/OS agnostic structured logging stub with optional sink.
*/

#pragma once

#include <cstdint>

namespace Common {
namespace utils {
namespace logging {

enum class Level : std::uint8_t { Debug = 0, Info = 1, Warn = 2, Error = 3 };

// Sink callback: level, component (static string), code (operation or event code), message (static string)
// Avoid noexcept on function pointer typedef for MSVC compatibility
using LogSink = void(*)(Level level, const char* component, std::uint32_t code, const char* message);

// Internal accessor to a function-local static sink pointer to avoid global init order issues
inline LogSink& sink_ref() noexcept {
    static LogSink s = nullptr; // no-op by default
    return s;
}

inline void set_sink(LogSink sink) noexcept { sink_ref() = sink; }
inline LogSink get_sink() noexcept { return sink_ref(); }

inline void log(Level level, const char* component, std::uint32_t code, const char* message) noexcept {
    auto s = sink_ref();
    if (s) { s(level, component, code, message); }
}

inline void debug(const char* component, std::uint32_t code, const char* message) noexcept {
    log(Level::Debug, component, code, message);
}
inline void info(const char* component, std::uint32_t code, const char* message) noexcept {
    log(Level::Info, component, code, message);
}
inline void warn(const char* component, std::uint32_t code, const char* message) noexcept {
    log(Level::Warn, component, code, message);
}
inline void error(const char* component, std::uint32_t code, const char* message) noexcept {
    log(Level::Error, component, code, message);
}

} // namespace logging
} // namespace utils
} // namespace Common
