#pragma once
/*
  test/mocks/DFRobotDFPlayerMini.h — Mock for the DFRobot DFPlayer Mini library.

  Provides a controllable stub of DFRobotDFPlayerMini so that DFPlayerController
  can be unit-tested on a laptop without real hardware or the actual library.

  Test helpers:
    mock_dfplayer_set_begin_ok(true/false)    — control begin() return value
    mock_dfplayer_send_event(type, data)      — queue a player event (consumed on next available())
    mock_dfplayer_reset()                     — clear all mock state between tests
*/

#include "Arduino.h"

// ── DFPlayer event constants (mirrors the real library) ──────────────────────
static const uint8_t DFPlayerPlayFinished = 3;
static const uint8_t DFPlayerError        = 8;
static const uint8_t DFPlayerCardInserted = 1;

// ── Controllable mock state ──────────────────────────────────────────────────
inline bool&    _mock_dfp_begin_ok()     { static bool    v = true;  return v; }
inline bool&    _mock_dfp_has_event()    { static bool    v = false; return v; }
inline uint8_t& _mock_dfp_event_type()  { static uint8_t v = 0;     return v; }
inline uint8_t& _mock_dfp_event_data()  { static uint8_t v = 0;     return v; }

// ── Test helpers ─────────────────────────────────────────────────────────────
/// Make the next begin() call return ok (true) or not-found (false).
inline void mock_dfplayer_set_begin_ok(bool ok) { _mock_dfp_begin_ok() = ok; }

/// Queue one player event that available() + readType()/read() will return.
inline void mock_dfplayer_send_event(uint8_t type, uint8_t data = 0) {
  _mock_dfp_has_event()   = true;
  _mock_dfp_event_type()  = type;
  _mock_dfp_event_data()  = data;
}

/// Reset all mock state — call from setUp() between tests.
inline void mock_dfplayer_reset() {
  _mock_dfp_begin_ok()    = true;
  _mock_dfp_has_event()   = false;
  _mock_dfp_event_type()  = 0;
  _mock_dfp_event_data()  = 0;
}

// ── Stub class ───────────────────────────────────────────────────────────────
class DFRobotDFPlayerMini {
public:
  /// Returns the mocked result — no hardware touched.
  bool begin(Stream&, bool) { return _mock_dfp_begin_ok(); }

  /// Sets the player volume — no-op in stub.
  void volume(uint8_t) {}

  /// Returns true once per queued event, then false.
  bool available() {
    if (_mock_dfp_has_event()) { _mock_dfp_has_event() = false; return true; }
    return false;
  }

  /// Returns the type of the last event queued via mock_dfplayer_send_event().
  uint8_t readType() { return _mock_dfp_event_type(); }

  /// Returns the data byte of the last queued event.
  uint8_t read() { return _mock_dfp_event_data(); }

  void pause()                           {}
  void start()                           {}
  void playFolder(uint8_t, uint8_t)     {}
  void loopFolder(uint8_t)              {}
};
