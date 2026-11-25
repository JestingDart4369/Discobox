#ifndef RELAY_H
#define RELAY_H
/*
  Relay.h — one relay channel on one pin (header-only library, doc/RELAY.md).

  Usage:
    Relay relay1(D7);                 // active-low module (most 5 V modules)
    Relay relay2(D8, false);          // active-high module

    void setup() { relay1.begin(); }  // pin is set to "off" BEFORE it becomes an output
    relay1.on();  relay1.off();  relay1.toggle();  relay1.set(true);
    relay1.isOn();                    // what we last asked for (the library never reads the pin)
    relay1.setEnabled(false);         // forbid "on" (and switch off now); the Settings menu uses this

  Active-low = the module switches ON when the input pin is LOW (opto-coupler
  modules). If your relay does the opposite of what you ask, flip the second
  constructor argument.

  Safe start: the relay is OFF after begin(). Before begin() nothing is written.
  The library only drives the pin; it does not know what is behind the relay.
  For a 230 V load read the safety notes in doc/RELAY.md first.
*/

#include <Arduino.h>

/// @brief One relay channel. Non-blocking, no timers.
class Relay {
public:
  /// @param pin        Arduino pin wired to the relay module's IN.
  /// @param activeLow  true (default): pin LOW = relay on. false: pin HIGH = relay on.
  explicit Relay(uint8_t pin, bool activeLow = true) : _pin(pin), _activeLow(activeLow) {}

  /// @brief Call once in setup(). Writes "off" first, then makes the pin an output (no click at boot).
  void begin() {
    _on = false;
    digitalWrite(_pin, offLevel());
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, offLevel());
    _started = true;
  }

  /// @brief Switch the relay on or off. While disabled (setEnabled(false)) "on" is ignored.
  void set(bool on) {
    if (on && !_enabled) on = false;
    _on = on;
    if (_started) digitalWrite(_pin, on ? onLevel() : offLevel());
  }

  /// @brief Allow or forbid switching on (default: allowed). Disabling switches the relay off at once.
  void setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) off();
  }
  bool isEnabled() const { return _enabled; }
  void on()  { set(true); }
  void off() { set(false); }
  void toggle() { set(!_on); }

  /// @brief true if the relay was last switched on.
  bool isOn() const { return _on; }
  /// @brief true after begin() was called.
  bool isStarted() const { return _started; }
  uint8_t pin() const { return _pin; }
  bool activeLow() const { return _activeLow; }

private:
  uint8_t _pin;
  bool    _activeLow;
  bool    _on      = false;
  bool    _started = false;
  bool    _enabled = true;

  uint8_t onLevel()  const { return _activeLow ? LOW : HIGH; }
  uint8_t offLevel() const { return _activeLow ? HIGH : LOW; }
};

#endif // RELAY_H
