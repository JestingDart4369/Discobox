#ifndef BRIGHTNESS_MANAGER_H
#define BRIGHTNESS_MANAGER_H
/*
  Brightness_Manager.h — ONE place for all brightness.

  Every light has its own LEVEL (0..255): ring, strip, button.
  On top of that a MASTER (0..255) scales all of them together:

      shown brightness = level * master / 255
      e.g. ring level 200, master 128  ->  ring shows at 100

  WHO CHANGES WHAT
    Settings mode   ring + strip level   (Settings_Items.h, +17 per SP, wraps to 0)
    serial          `brightness N` = ring level, `strip bright N` = strip level
    master          NOT in the Settings menu — only from code, meant for your
                    CLI command:  Brightness.setMaster(128); Persist.save();

  This class never writes flash itself. Call Persist.save() after a change
  that should survive a power loss (Settings_Store.h saves all four values).

  USAGE
    Brightness.setLevel(BrightnessManager::STRIP, 200);   // applied at once
    Brightness.setMaster(128);                             // all lights at half
    uint8_t lvl  = Brightness.level(BrightnessManager::RING);
    uint8_t seen = Brightness.effective(BrightnessManager::RING);

  IMPORTANT: include in main.cpp AFTER RingOFLeds, Strip and RGBButton exist,
  and BEFORE Storage/Settings_Store.h (which saves/loads these values).
  The class itself only needs LEDBase (included below); only the global
  `Brightness` at the bottom uses the light objects from main.cpp.
*/
#include <LedController.h>     // LEDBase

class BrightnessManager {
public:
  enum Light : uint8_t { RING, STRIP, BUTTON, LIGHT_COUNT };

  static const uint8_t SETTINGS_STEP = 17;   // 255 / 15 -> 15 steps end to end

  BrightnessManager(LEDBase& ring, LEDBase& strip, LEDBase& button)
    : _lights{ &ring, &strip, &button } {}

  // ── per light ──
  /** @brief The light's own level (0..255), before the master is applied. */
  uint8_t level(Light l) const { return l < LIGHT_COUNT ? _level[l] : 0; }

  /** @brief Set the light's own level and show it at once. */
  void setLevel(Light l, uint8_t v) {
    if (l >= LIGHT_COUNT) return;
    _level[l] = v;
    apply(l);
  }

  /** @brief Settings-mode step: +SETTINGS_STEP, wrapping back to 0 past 255. */
  uint8_t stepLevel(Light l) {
    uint16_t v = (uint16_t)level(l) + SETTINGS_STEP;
    if (v > 255) v = 0;
    setLevel(l, (uint8_t)v);
    return (uint8_t)v;
  }

  /** @brief What the light really shows: level * master / 255. */
  uint8_t effective(Light l) const { return (uint8_t)((uint16_t)level(l) * _master / 255); }

  // ── master (not in the Settings menu — CLI / code only) ──
  uint8_t master() const { return _master; }

  /** @brief Scale ALL lights together (255 = no change). Applied at once. */
  void setMaster(uint8_t v) {
    _master = v;
    applyAll();
  }

  /** @brief Set everything at once (used when loading saved settings). */
  void setAll(uint8_t ring, uint8_t strip, uint8_t button, uint8_t master) {
    _level[RING] = ring;  _level[STRIP] = strip;  _level[BUTTON] = button;
    _master = master;
    applyAll();
  }

  void applyAll() { for (uint8_t i = 0; i < LIGHT_COUNT; i++) apply((Light)i); }

  static const char* name(Light l) {
    switch (l) {
      case RING:   return "ring";
      case STRIP:  return "strip";
      case BUTTON: return "button";
      default:     return "?";
    }
  }

private:
  LEDBase* _lights[LIGHT_COUNT];
  uint8_t  _level[LIGHT_COUNT] = { 255, 255, 255 };
  uint8_t  _master = 255;

  void apply(Light l) {
    _lights[l]->setBrightness(effective(l));
    _lights[l]->show();               // re-send so the change is visible right away
  }
};

// The one global manager, wired to the lights from main.cpp.
BrightnessManager Brightness(RingOFLeds, Strip, RGBButton);

#endif // BRIGHTNESS_MANAGER_H
