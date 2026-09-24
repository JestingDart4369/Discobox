#ifndef SETTINGS_ITEMS_H
#define SETTINGS_ITEMS_H
/*
  Settings_Items.h — what the Settings mode (Modes.h) actually adjusts,
  object-oriented, same pattern as ModeBase/ModeManager in Modes.h.

  HOW IT WORKS
    SettingItem  — the "contract" every setting follows. A setting is a
                   class that inherits from SettingItem and overrides:
                     name()      — shown in the log
                     Enter()     — called the moment it becomes selected
                     bump()      — called on SP: step the value, wrap at max
                   No setting stores its own value in RAM — bump() and
                   Enter()/logValue() always read/write the real thing
                   directly (RingOFLeds, Buzzer, ...) instead of keeping a
                   cached copy. Memory-sparing on purpose: nothing to
                   fetch or fall out of sync except right when it's asked
                   for.

    SettingsMenu — owns the list of settings and which one is selected.
                   DP/TP move the selection (see Modes.h's Settings::DP/TP);
                   SP bumps the selected one's value.

  GESTURES (wired in Modes.h's Settings mode)
    DP — next setting in the list
    TP — previous setting in the list
    SP — bump the CURRENTLY SELECTED setting's value up one notch,
         wrapping back to the start past its max (so SP alone cycles
         all the way through — no separate "down" gesture needed)

  ADD A NEW SETTING
    1. Copy one of the classes below, rename it, fill in bump()/logValue().
    2. Create an instance of it next to the others at the bottom.
    3. Add the instance to the SETTINGS_LIST array. Done.
    If it should survive a power loss too, also wire it into
    Storage/Settings_Store.h (PersistedSettings + load()/save()).

  IMPORTANT: this file uses the global objects from main.cpp (Buzzer,
  RingOFLeds, Persist), so it must be #included in main.cpp AFTER those
  are defined, and BEFORE Modes.h (which uses SettingsList).
*/


/* ================================================================
   SettingItem — every setting inherits from this
   ================================================================ */
/// @brief The base class for all settings in the Discobox project. Each setting inherits from this class and overrides its virtual methods to define specific behavior for that setting.
   class SettingItem {
public:
  /// @brief Virtual destructor for the SettingItem class. Ensures proper cleanup of derived classes.
  virtual ~SettingItem() {}

  /// @brief Returns the name of the setting, which is shown in the log. This method must be overridden by derived classes to provide the specific name of the setting.
  /// @return The name of the setting as a C-style string. 
  virtual const char* name() const = 0;   // shown in the log

  // Called the instant this becomes the selected setting (SP/TP just
  // landed on it). Default just logs the live value — override if a
  // setting needs to do more on selection.
  /// @brief Called when the setting becomes the selected setting. Override this method to define behavior that should occur when the setting is selected.
  virtual void Enter() { logValue(); }

  // SP — step the value up one notch, wrapping back to the start once
  // it passes its max. The only value-changing gesture there is.
  /// @brief Called when the setting's value should be incremented (e.g., on a single press). Override this method to define how the setting's value should change.
  virtual void bump() {}

  // Sound cue played after bump() (see Modes.h's Settings::cueForSP() —
  // it asks the CURRENTLY SELECTED setting, so bump() has already run
  // and any theme/pack change already applies to it). Default matches
  // the generic double-click every other mode's DP uses; override for
  // something more demonstrative (e.g. cue themes want to be heard).
  /// @brief Returns the sound cue to be played after the setting's value has been changed. This method can be overridden by derived classes to provide a specific sound cue that reflects the change in the setting's value.
  /// @return The UiSfxCueId representing the sound cue to be played after the setting's value has been changed. The default implementation returns UISFX_DOUBLE_CLICK, but derived classes can override this to provide a different sound cue that reflects the change in the setting's value.
  virtual UiSfxCueId cueForBump() const { return UISFX_DOUBLE_CLICK; }

protected:
  // One-line "name = current value" straight from wherever the value
  // actually lives — never a cached copy.
  virtual void logValue() const {}
};


/* ================================================================
   Ring brightness
   ================================================================ */
/**
 * @brief Setting that steps the WS2812 ring brightness in ~15 increments.
 *
 * Each SP bump increases brightness by 17 (≈ 255/15 steps), wrapping back
 * to 0 past maximum. Value is persisted immediately via Persist.save().
 */
class BrightnessSetting : public SettingItem {
public:
  const char* name() const override { return "brightness"; }

  void bump() override {
    uint16_t v = (uint16_t)RingOFLeds.getBrightness() + STEP;
    if (v > 255) v = 0;             // wrap past max back to off
    RingOFLeds.setBrightness((uint8_t)v);
    RingOFLeds.show();
    Persist.save();
    logValue();
  }

protected:
  void logValue() const override {
    Logger::log("Settings: brightness = %d", RingOFLeds.getBrightness());
  }

private:
  static const uint8_t STEP = 17;   // ~15 steps end to end
};


/* ================================================================
   Cue mute (button/gesture cues only — see Buzzer_Player.h)
   ================================================================ */
/**
 * @brief Setting that toggles the UI cue mute flag.
 *
 * When muted, playCue() does nothing and songs keep playing uninterrupted.
 * Value is persisted immediately via Persist.save().
 */
class CueMuteSetting : public SettingItem {
public:
  const char* name() const override { return "mute"; }

  void bump() override {
    Buzzer.toggleMute();
    Persist.save();
    logValue();
  }

protected:
  void logValue() const override {
    Logger::log("Settings: mute = %s", Buzzer.isMuted() ? "on" : "off");
  }
};


/* ================================================================
   Cue theme (pitch/speed pack — see UiSfx_Cues.h)
   ================================================================ */
/**
 * @brief Setting that cycles through the available UI cue themes (pitch/speed packs).
 *
 * Each SP bump advances to the next pack and immediately plays a demo cue
 * so the user can hear the change. Value is persisted via Persist.save().
 */
class CueThemeSetting : public SettingItem {
public:
  const char* name() const override { return "theme"; }

  void bump() override {
    uint8_t next = (uint8_t)((Buzzer.cuePackIndex() + 1) % UISFX_PACK_COUNT);
    Buzzer.setCuePack(next);   // takes effect immediately — cueForBump()
                                // below plays right after, in this pack
    Persist.save();
    logValue();
  }

  // A little 3-note phrase instead of the plain double-click, so you can
  // actually hear the new theme's pitch/speed each time you cycle it.
  UiSfxCueId cueForBump() const override { return UISFX_SUCCESS; }

protected:
  void logValue() const override {
    Logger::log("Settings: theme = %s", Buzzer.cuePackName());
  }
};


/* ================================================================
   DFPlayer volume
   ================================================================ */
/**
 * @brief Setting that steps the DFPlayer volume in increments of 2 (0–30).
 *
 * Each SP bump increases volume by 2, wrapping back to 0 past 30.
 * Value is persisted immediately via Persist.save().
 */
class VolumeSetting : public SettingItem {
public:
  const char* name() const override { return "volume"; }

  void bump() override {
    uint8_t v = DFPlayer.volume() + STEP;
    if (v > 30) v = 0;
    DFPlayer.setVolume(v);
    Persist.save();
    logValue();
  }

protected:
  void logValue() const override {
    Logger::log("Settings: volume = %d", DFPlayer.volume());
  }

private:
  static const uint8_t STEP = 2;  // 16 steps: 0, 2, 4, … 30
};


/* ================================================================
   SettingsMenu — selection + forwarding, owned by Modes.h's Settings mode
   ================================================================ */
/**
 * @brief Manages the ordered list of settings and which one is currently selected.
 *
 * Mirrors the ModeManager pattern: owns a pointer array of SettingItem*,
 * tracks the selected index, and forwards navigation/bump calls.
 * A single global instance `SettingsList` is created at the bottom of this file.
 */
class SettingsMenu {
public:
  /** @brief Constructs the menu from an array of setting pointers. @param list Array of SettingItem pointers. @param count Number of settings. */
  SettingsMenu(SettingItem** list, uint8_t count) : _list(list), _count(count) {}

  /** @brief Reset selection to the first setting and call its Enter(). Call from Settings::Enter(). */
  void begin() { _index = 0; current().Enter(); }

  /** @brief Select the next setting (wraps) and call its Enter(). Called on DP. */
  void next() {
    _index = (uint8_t)((_index + 1) % _count);
    current().Enter();
  }

  /** @brief Select the previous setting (wraps) and call its Enter(). Called on TP. */
  void previous() {
    _index = (uint8_t)((_index + _count - 1) % _count);
    current().Enter();
  }

  /** @brief Bump the currently selected setting's value. Called on SP. */
  void bump() { current().bump(); }

  /** @brief Returns a reference to the currently selected setting. */
  SettingItem& current()          { return *_list[_index]; }
  /** @brief Returns a reference to setting at position @p i (for listing without changing selection). @param i 0-based index. */
  SettingItem& at(uint8_t i)      { return *_list[i]; }
  /** @brief Returns the 0-based index of the currently selected setting. */
  uint8_t      index()   const { return _index; }
  /** @brief Returns the total number of registered settings. */
  uint8_t      count()   const { return _count; }

private:
  SettingItem** _list;
  uint8_t       _count;
  uint8_t       _index = 0;
};


/* ================================================================
   The actual setting instances + the menu.
   To add a setting: create an instance, add it to SETTINGS_LIST.
   ================================================================ */
BrightnessSetting settingBrightness;
VolumeSetting      settingVolume;
CueMuteSetting     settingMute;
CueThemeSetting    settingTheme;

SettingItem* SETTINGS_LIST[] = { &settingBrightness, &settingVolume, &settingMute, &settingTheme };
SettingsMenu SettingsList(SETTINGS_LIST, sizeof(SETTINGS_LIST) / sizeof(SETTINGS_LIST[0]));

#endif // SETTINGS_ITEMS_H
