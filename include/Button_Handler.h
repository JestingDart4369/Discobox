#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H
/*
  Button_Handler.h — gesture detection for one push button (non-blocking).

  Detects 4 gestures:
    SP   = single press
    DP   = double press
    TP   = triple press (or more)
    HOLD = pressed for HOLD_MS

  How it works:
    - debounced press/release edges
    - counts clicks; a pause of CLICK_GAP_MS after the last release
      finalizes the gesture as SP / DP / TP
    - holding for HOLD_MS fires the hold gesture instead (not a click)

  Usage:
    void mySP()   { ... }
    void myDP()   { ... }
    void myTP()   { ... }
    void myHold() { ... }

    GestureButton Button(D2, mySP, myDP, myTP, myHold);

    void setup() { Button.Setup(); }               // sets pinMode INPUT_PULLUP
    void loop()  { Button.update(millis()); }      // call every frame

  Timing can be tuned per button, e.g.:  Button.CLICK_GAP_MS = 300;
*/

#include <Arduino.h>
/// @brief A class that handles button gestures (single press, double press, triple press, and hold) for a single push button. It debounces the button input and counts clicks to determine the gesture performed.
class GestureButton {
public:

  /// @brief A type definition for a callback function that takes no arguments and returns void. This is used for the gesture actions (SP, DP, TP, HOLD).
  typedef void (*Callback)();

  /// @brief Constructs a GestureButton object with the specified pin and gesture callbacks.
  /// @param pin The pin number where the button is connected.
  /// @param onSP Callback function for single press gesture.
  /// @param onDP Callback function for double press gesture.
  /// @param onTP Callback function for triple press gesture.
  /// @param onHold Callback function for hold gesture.
  GestureButton(uint8_t pin,
                Callback onSP, Callback onDP,
                Callback onTP, Callback onHold)
    : _pin(pin), _onSP(onSP), _onDP(onDP), _onTP(onTP), _onHold(onHold) {}

  /// @brief Initializes the button by setting the pin mode to INPUT_PULLUP. This should be called in the setup() function.
  /// @param pin The pin number where the button is connected.
  void setup() { pinMode(_pin, INPUT_PULLUP); }

  // Tunable timing (public on purpose — adjust after construction if needed)
  uint16_t DEBOUNCE_MS  = 25;    // ignore contact bounce shorter than this
  uint16_t CLICK_GAP_MS = 400;   // max pause between clicks of a multi-click
  uint16_t HOLD_MS      = 3000;  // press this long to trigger the hold gesture
  
  /// @brief Updates the button state based on the current time.
  /// @param now The current time in milliseconds.
  void update(unsigned long now) {
    // 1) Debounce the raw reading (INPUT_PULLUP: pressed = LOW)
    bool reading = (digitalRead(_pin) == LOW);
    if (reading != _reading_last) {
      _reading_last   = reading;
      _last_change_ms = now;
    }

    if ((now - _last_change_ms) >= DEBOUNCE_MS && reading != _pressed) {
      _pressed = reading;

      if (_pressed) {
        // press edge
        _press_start_ms = now;
        _hold_fired     = false;
      } else {
        // release edge — count it as a click unless this press was a hold
        if (!_hold_fired) {
          _click_count++;
          _last_release_ms = now;
        }
      }
    }

    // 2) Hold detection (fires once, while still pressed)
    if (_pressed && !_hold_fired && (now - _press_start_ms) >= HOLD_MS) {
      _hold_fired  = true;
      _click_count = 0;              // a hold cancels any pending clicks
      if (_onHold) _onHold();
    }

    // 3) Finalize the click count once the multi-click window has passed
    if (!_pressed && _click_count > 0 &&
        (now - _last_release_ms) >= CLICK_GAP_MS) {
      uint8_t clicks = _click_count;
      _click_count   = 0;

      if      (clicks == 1) { if (_onSP) _onSP(); }
      else if (clicks == 2) { if (_onDP) _onDP(); }
      else                  { if (_onTP) _onTP(); }   // 3 or more
    }
  }

  // Is the button physically held down right now? (debounced)
  bool isPressed() const { return _pressed; }

private:
  uint8_t  _pin;
  Callback _onSP, _onDP, _onTP, _onHold;

  bool          _reading_last    = false;  // raw reading from last loop
  bool          _pressed         = false;  // debounced state
  unsigned long _last_change_ms  = 0;      // last time the raw reading changed
  unsigned long _press_start_ms  = 0;      // when the current press began
  unsigned long _last_release_ms = 0;      // when the last release happened
  uint8_t       _click_count     = 0;      // clicks collected so far
  bool          _hold_fired      = false;  // hold already triggered for this press
};

#endif // BUTTON_HANDLER_H
