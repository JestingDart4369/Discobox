#pragma once
/*
  test/mocks/Arduino.h — Minimal Arduino stub for native PlatformIO tests.

  Provides types, constants, mock-controllable hardware state (millis, digitalRead),
  and a Stream base class so DFPlayerController / SongPlayer can be tested on a
  laptop without any real hardware.

  Include path: add  -I${PROJECT_DIR}/test/mocks  to the [env:native] build_flags
  so that #include <Arduino.h> resolves here instead of the real Arduino core.

  Usage from test code:
    mock_set_millis(500);          // set the fake clock
    mock_advance_millis(100);      // advance it
    mock_set_pin(2, LOW);          // simulate a button press on pin 2
    mock_reset();                  // reset everything between tests
*/

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cmath>     // pow(), sqrt(), etc. — included by the real Arduino core

// ── Arduino integer aliases ──────────────────────────────────────────────────
// Already fully defined by <cstdint>; just add the ones Arduino names differ.
using byte    = uint8_t;
using boolean = bool;

// ── Arduino constants ────────────────────────────────────────────────────────
#define LOW          0
#define HIGH         1
#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2
#define LED_BUILTIN  13

// Pin aliases used by Discobox main.cpp
#define D2   2
#define D3   3
#define D4   4
#define D5   5
#define D6   6
#define D7   7
#define D9   9
#define D11  11

// F() — on AVR/R4 this moves string literals to flash; on native it is a no-op.
#define F(s) (s)

// ── Mock hardware state ──────────────────────────────────────────────────────
// Function-local statics are safe in header-only code: no ODR violations when
// the same header is included by multiple translation units.

inline unsigned long& _mock_millis_ref() {
  static unsigned long v = 0;
  return v;
}

inline int* _mock_pins_ref() {
  static int pins[64] = {};
  static bool inited = false;
  if (!inited) {
    for (int i = 0; i < 64; i++) pins[i] = HIGH;  // pull-up default
    inited = true;
  }
  return pins;
}

// ── Test helper API ──────────────────────────────────────────────────────────
/// Set the fake clock to an absolute value (ms).
inline void mock_set_millis(unsigned long ms) { _mock_millis_ref() = ms; }
/// Advance the fake clock by delta ms.
inline void mock_advance_millis(unsigned long ms) { _mock_millis_ref() += ms; }
/// Set a pin's fake digital value (LOW or HIGH).
inline void mock_set_pin(uint8_t pin, int val) { _mock_pins_ref()[pin & 63] = val; }
/// Reset clock to 0 and all pins to HIGH (INPUT_PULLUP default).
inline void mock_reset() {
  _mock_millis_ref() = 0;
  int* p = _mock_pins_ref();
  for (int i = 0; i < 64; i++) p[i] = HIGH;
}

// ── Arduino hardware API stubs ───────────────────────────────────────────────
inline unsigned long millis()                         { return _mock_millis_ref(); }
inline void          delay(unsigned long)             {}
inline int           digitalRead(uint8_t pin)        { return _mock_pins_ref()[pin & 63]; }
inline void          digitalWrite(uint8_t, uint8_t)  {}
inline void          pinMode(uint8_t, uint8_t)       {}
inline void          analogWrite(uint8_t, int)        {}
inline void          tone(uint8_t, unsigned int,
                          unsigned long = 0)          {}
inline void          noTone(uint8_t)                  {}
inline void          analogReference(uint8_t)         {}

// ── Stream / Serial stubs ────────────────────────────────────────────────────

/// Minimal Stream base: DFPlayerController takes a Stream& in its constructor.
struct Stream {
  virtual int  available()              { return 0; }
  virtual int  read()                   { return -1; }
  virtual int  peek()                   { return -1; }
  void         print(const char*)       {}
  void         println(const char*)     {}
  void         println()                {}
  void         print(int)               {}
  void         println(int)             {}
  void         print(char)              {}
  void         flush()                  {}
  void         begin(unsigned long)     {}
};

// Global Serial / Serial1 instances (C++17 inline — safe in headers).
inline Stream Serial;
inline Stream Serial1;
