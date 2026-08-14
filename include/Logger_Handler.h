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

class Logger {
private:
    static void _print(const char* prefix, const char* fmt, va_list args) {
        char buf[128];
        vsnprintf(buf, sizeof(buf), fmt, args);
        Serial.print(prefix);
        Serial.println(buf);
    }

public:
    // Always printed — general info
    static void log(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        _print("[LOG]   ", fmt, args);
        va_end(args);
    }

    // Only printed when DEBUG_ENABLED = true
    static void debugLog(const char* fmt, ...) {
        if (!DEBUG_ENABLED) return;
        va_list args;
        va_start(args, fmt);
        _print("[DEBUG] ", fmt, args);
        va_end(args);
    }

    // Always printed — something unexpected but recoverable
    static void warn(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        _print("[WARN]  ", fmt, args);
        va_end(args);
    }

    // Always printed — something went wrong
    static void error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        _print("[ERROR] ", fmt, args);
        va_end(args);
    }
};

#endif
