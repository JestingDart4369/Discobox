#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <stdarg.h>

// Usage:
//   Logger::log("Connected to %s", ssid);        // always printed
//   Logger::debugLog("RSSI: %d dBm", rssi);      // only when -DDEBUG_ENABLED in platformio.ini
//   Logger::warn("Failed to connect to %s", ssid);
//   Logger::error("WiFi module not found!");

/**
 * @brief Static serial logging utility.
 *
 * All methods are static — no instance needed. Output always goes to Serial.
 * debugLog() is silenced unless DEBUG_ENABLED is defined as a build flag
 * in platformio.ini:  build_flags = -DDEBUG_ENABLED
 *
 * Usage:
 * @code
 *   Logger::log("Connected to %s", ssid);
 *   Logger::debugLog("RSSI: %d dBm", rssi);  // only when -DDEBUG_ENABLED
 *   Logger::warn("Unexpected value: %d", v);
 *   Logger::error("Module not found!");
 * @endcode
 */
class Logger {
private:
  /// true while a shell command runs with -q / --quiet (see setQuiet)
  static bool& _quiet() {
    static bool q = false;
    return q;
  }

  static void _print(const char* prefix, const char* fmt, va_list args) {
    char buf[128];
    vsnprintf(buf, sizeof(buf), fmt, args);
    Serial.print(prefix);
    Serial.println(buf);
  }

public:
  /**
   * @brief Silence log() and debugLog() (warn/error still print).
   * Used by the serial shell for -q / --quiet: on before the command, off after.
   */
  static void setQuiet(bool q) { _quiet() = q; }
  /** @brief true while output is silenced by setQuiet(true). */
  static bool isQuiet() { return _quiet(); }

  /** @brief General info — printed to Serial (silent while setQuiet(true)). */
  static void log(const char* fmt, ...) {
    if (_quiet()) return;
    va_list args;
    va_start(args, fmt);
    _print("[LOG]   ", fmt, args);
    va_end(args);
  }

  /** @brief Debug detail — only printed when -DDEBUG_ENABLED is set in platformio.ini. */
  static void debugLog(const char* fmt, ...) {
#ifndef DEBUG_ENABLED
    return;
#endif
    if (_quiet()) return;
    va_list args;
    va_start(args, fmt);
    _print("[DEBUG] ", fmt, args);
    va_end(args);
  }

  /** @brief Warning — always printed. Use for something unexpected but recoverable. */
  static void warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    _print("[WARN]  ", fmt, args);
    va_end(args);
  }

  /** @brief Error — always printed. Use when something went wrong. */
  static void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    _print("[ERROR] ", fmt, args);
    va_end(args);
  }
};

#endif
