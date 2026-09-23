#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <stdarg.h>
#include "config.h"

// Usage:
//   Logger::log("Connected to %s", ssid);        // always printed
//   Logger::debugLog("RSSI: %d dBm", rssi);      // only when DEBUG_ENABLED = true
//   Logger::warn("Failed to connect to %s", ssid);
//   Logger::error("WiFi module not found!");

/**
 * @brief Static serial logging utility.
 *
 * All methods are static — no instance needed. Output always goes to Serial.
 * debugLog() is compiled in but silenced when DEBUG_ENABLED is false.
 *
 * Usage:
 * @code
 *   Logger::log("Connected to %s", ssid);
 *   Logger::debugLog("RSSI: %d dBm", rssi);  // only when DEBUG_ENABLED = true
 *   Logger::warn("Unexpected value: %d", v);
 *   Logger::error("Module not found!");
 * @endcode
 */
class Logger {
private:
    /**
     * @brief Internal helper — formats and prints a line with a fixed prefix.
     * @param prefix Tag prepended to the output (e.g. "[LOG]   ").
     * @param fmt    printf-style format string.
     * @param args   Variadic argument list already opened by the caller.
     */
    static void _print(const char* prefix, const char* fmt, va_list args) {
        char buf[128];
        vsnprintf(buf, sizeof(buf), fmt, args);
        Serial.print(prefix);
        Serial.println(buf);
    }

public:
    /** @brief General info — always printed to Serial. @param fmt printf-style format string. */
    static void log(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        _print("[LOG]   ", fmt, args);
        va_end(args);
    }

    /** @brief Debug detail — only printed when `DEBUG_ENABLED` is true (see config.h). @param fmt printf-style format string. */
    static void debugLog(const char* fmt, ...) {
        if (!DEBUG_ENABLED) return;
        va_list args;
        va_start(args, fmt);
        _print("[DEBUG] ", fmt, args);
        va_end(args);
    }

    /** @brief Warning — always printed. Use for something unexpected but recoverable. @param fmt printf-style format string. */
    static void warn(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        _print("[WARN]  ", fmt, args);
        va_end(args);
    }

    /** @brief Error — always printed. Use when something went wrong. @param fmt printf-style format string. */
    static void error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        _print("[ERROR] ", fmt, args);
        va_end(args);
    }
};

#endif
