#ifndef MODES_H
#define MODES_H
/*
  Modes.h — all Discobox modes, object-oriented.

  4 modes. Inside each mode the gestures do mode-specific things:
    SP = single press
    DP = double press
    TP = triple press
  HOLD (3 s) is the same everywhere: switch to the next mode.
    Party -> LinkClick -> Mode3 -> Settings -> Party -> ...

  HOW IT WORKS
    ModeBase    — the "contract" every mode follows. A mode is a class that
                  inherits from ModeBase and overrides what it needs:
                    name(), color()      — identity (used for the switch flash)
                    Enter() / Exit()     — called when the mode starts / stops
                    Step(now)            — called every frame (animations)
                    SP() / DP() / TP()   — the gesture actions
                  Mode variables become member variables of the class.

    ModeManager — owns the list of modes, knows which one is active,
                  does the color flash on switching, and forwards
                  gestures + frames to the active mode.

  ADD A NEW MODE
    1. Copy one of the placeholder classes below, rename it, fill it in.
    2. Create an instance of it next to the others at the bottom.
    3. Add the instance to the MODE_LIST array. Done.

  SOUND: main.cpp plays a UI cue after each gesture (see Btn_SP etc.).
  The generic cue is press/double-click/select/long-press, but a mode
  can pick its own by overriding cueForSP()/cueForDP()/cueForTP() below
  — e.g. Party plays "start"/"stop" for SP and "skip-next" for DP.

  IMPORTANT: this file uses the global objects from main.cpp
  (RingOFLeds, RGBButton, and the Buzzer song-list player), so it must
  be #included in main.cpp AFTER those objects are defined.
  The song list itself lives in Buzzer/Buzzer_Player.h. The Settings
  mode also needs SettingsList, so Settings_Items.h must come before
  this file too.
*/


/* ================================================================
   ModeBase — every mode inherits from this
   ================================================================ */
/// @brief The base class for all modes in the Discobox project. Each mode inherits from this class and overrides its virtual methods to define specific behavior for that mode.
class ModeBase {
public:
  /// @brief Virtual destructor for the ModeBase class. Ensures proper cleanup of derived classes.
  virtual ~ModeBase() {}

  // Identity
  virtual const char* name()  const = 0;   // shown in the log
  virtual uint32_t    color() const = 0;   // flash + indicator color

  // Lifecycle
  /// @brief Called when the mode becomes active. Override this method to define behavior when entering the mode.
  virtual void Enter() {}                  
  /// @brief Called when the mode is being left. Override this method to define behavior when exiting the mode.
  virtual void Exit()  {}
  /// @brief Called every frame while the mode is active. Override this method to define behavior that should occur on each update cycle.
  virtual void Step(unsigned long now) {}  

  // Gestures — called AFTER the matching cueForXX() below has already
  // been used to pick the sound, so it's safe to change mode state here
  // (e.g. toggle a running flag) without racing the cue selection.
  virtual void SP() {}                     // single press
  virtual void DP() {}                     // double press
  virtual void TP() {}                     // triple press

  // Sound cue played for each gesture (see main.cpp's Btn_SP etc.).
  // Default is the generic tactile cue; override to be more specific
  // (e.g. "start"/"stop" instead of a plain "press").
  virtual UiSfxCueId cueForSP() const { return UISFX_PRESS; }
  virtual UiSfxCueId cueForDP() const { return UISFX_DOUBLE_CLICK; }
  virtual UiSfxCueId cueForTP() const { return UISFX_SELECT; }
};


/* ================================================================
   MODE 1 — PARTY
   SP = start/stop the party (disco lights + music)
   DP = next song
   TP = (free — add your action)
   ================================================================ */
/// @brief The PartyMode class represents the "Party" mode in the Discobox project. It inherits from ModeBase and implements specific behavior for party-related actions, including starting/stopping music and controlling disco lights.
class PartyMode : public ModeBase {
public:
  const char* name()  const override { return "Party"; }
  uint32_t    color() const override { return 0xFFFF00; }   // yellow

  void Enter() override {
    _running = false;                    // party waits for SP to start
    RingOFLeds.clear();
    RingOFLeds.setToColorSingleHex(1, color());
    RingOFLeds.show();
    RGBButton.setToColorHex(color());        // button shows the mode color while idle
  }

  void Exit() override {
    _running = false;
    Buzzer.autoNext(false);
    Buzzer.stop();
  }

  void Step(unsigned long now) override {
    if (!_running) return;

    // disco lights (the music continues by itself — Buzzer auto-next)
    if (now - _last_disco >= DISCO_STEP_MS) {
      _last_disco = now;
      RingOFLeds.stepDisco();
      RingOFLeds.show();
      RGBButton.stepDisco();
    }
  }

  // SP — start (or stop) the party
  void SP() override {
    if (_running) {
      Logger::log("Party: stop");
      _running = false;
      Buzzer.autoNext(false);
      Buzzer.stop();
      RingOFLeds.clear();
      RingOFLeds.setToColorSingleHex(1, color());
      RingOFLeds.show();
      RGBButton.setToColorHex(color());
    } else {
      Logger::log("Party: start!");
      _running = true;
      Buzzer.autoNext(true);   // keep the song list going by itself
      Buzzer.play();
    }
  }

  // DP — next song
  void DP() override { Buzzer.next(); }

  // TP — previous song
  void TP() override { Buzzer.previous(); }

  // Cues: SP plays "start" or "stop" depending on what just happened
  // (SP() above already flipped _running by the time this is read),
  // DP plays "skip-next" and TP "skip-previous" instead of the generic
  // double-click / select.
  UiSfxCueId cueForSP() const override { return _running ? UISFX_START : UISFX_STOP; }
  UiSfxCueId cueForDP() const override { return UISFX_SKIP_NEXT; }
  UiSfxCueId cueForTP() const override { return UISFX_SKIP_PREVIOUS; }

  bool isRunning() const { return _running; }

private:
  // Party variables
  bool          _running    = false;   // is the party on right now?
  unsigned long _last_disco = 0;       // timing for the disco lights
  static const uint16_t DISCO_STEP_MS = 120;  // how fast the colors change
};


/* ================================================================
   MODE 2 — (Link Click)  — template, copy me for new modes
   ================================================================ */
/// @brief Placeholder mode 2 — "LinkClick". Plays a song on enter; gestures are not yet assigned.
class LinkClick : public ModeBase {
public:
  const char* name()  const override { return "LinkClick"; }
  uint32_t    color() const override { return 0x0000FF; }   // blue

  void Enter() override {
    RingOFLeds.clear();
    for (uint8_t i = 0; i < 2; i++)    // 2 LEDs in the mode color = "this is mode 2"
      RingOFLeds.setToColorSingleHex(i, color());
    RingOFLeds.show();
    RGBButton.setToColorHex(color());
    Buzzer.play(0);
  }

  void Exit() override { Buzzer.stop(); }

  void Step(unsigned long now) override { /* TODO: animations, every frame */ }

  void SP() override { Logger::log("LinkClick: SP — not assigned yet"); /* TODO */ }
  void DP() override { Logger::log("LinkClick: DP — not assigned yet"); /* TODO */ }
  void TP() override { Logger::log("LinkClick: TP — not assigned yet"); /* TODO */ }

private:
  // LinkClick variables — e.g.:
  // bool _something = false;
};


/* ================================================================
   MODE 3 — (lorem)
   ================================================================ */
/// @brief Placeholder mode 3. Gestures are not yet assigned.
class Mode3 : public ModeBase {
public:
  const char* name()  const override { return "Mode3"; }
  uint32_t    color() const override { return 0x00FFFF; }   // cyan

  void Enter() override {
    RingOFLeds.clear();
    for (uint8_t i = 0; i < 3; i++)
      RingOFLeds.setToColorSingleHex(i, color());
    RingOFLeds.show();
    RGBButton.setToColorHex(color());
  }

  void Exit() override { Buzzer.stop(); }
  void Step(unsigned long now) override { /* TODO */ }
  void SP() override { Logger::log("Mode3: SP — not assigned yet"); /* TODO */ }
  void DP() override { Logger::log("Mode3: DP — not assigned yet"); /* TODO */ }
  void TP() override { Logger::log("Mode3: TP — not assigned yet"); /* TODO */ }

private:
  // Mode3 variables
};


/* ================================================================
   MODE 4 — Settings
   The actual settings (brightness, mute, theme, ...) live in their own
   file — see Settings_Items.h. This mode is just the gesture wiring:
     DP — next setting       TP — previous setting
     SP — bump the CURRENTLY SELECTED setting's value (wraps at its max)
   ================================================================ */
/**
 * @brief Settings mode — lets the user cycle through and adjust persisted settings.
 *
 * DP navigates to the next setting, TP to the previous.
 * SP bumps the currently selected setting's value (wraps at its max).
 * The number of lit ring LEDs indicates which setting is selected.
 */
class Settings : public ModeBase {
public:
  const char* name()  const override { return "Settings"; }
  uint32_t    color() const override { return 0xFF0000; }   // red
 
  void Enter() override {
    RGBButton.setToColorHex(color());
    for (uint8_t i = 0; i < 4; i++)
      RingOFLeds.setToColorSingleHex(i, color());
    SettingsList.begin();   // back to the first setting every time
    showSelected();
  }

  void Exit() override { Buzzer.stop(); }
  void Step(unsigned long now) override { }

  void DP() override { SettingsList.next();     showSelected(); }
  void TP() override { SettingsList.previous(); showSelected(); }
  void SP() override { SettingsList.bump(); }   // logs + saves itself

  // Which sound plays after SP depends on which setting is selected —
  // by the time this is read (see main.cpp's Btn_SP: action first, cue
  // second) bump() has already run, so a theme change already applies
  // to its own demo cue below. DP/TP get a quick "tick" so moving
  // through the list is audible too, distinct from a bump.
  UiSfxCueId cueForSP() const override { return SettingsList.current().cueForBump(); }
  UiSfxCueId cueForDP() const override { return UISFX_HOVER; }
  UiSfxCueId cueForTP() const override { return UISFX_HOVER; }

private:
  // Light (index+1) ring LEDs in the mode color, so you can see which
  // setting is selected just by counting lit LEDs — same trick the
  // other modes use to show their own mode number on Enter().
  uint32_t  color_settings=0xFF8000;   // orange
  void showSelected() {
    RingOFLeds.clear();
    for (uint8_t i = 0; i <= SettingsList.index()+4; i++)
      RingOFLeds.setToColorSingleHex(i, color_settings);
    for (uint8_t i = 0; i < 4; i++)
      RingOFLeds.setToColorSingleHex(i, color());
    RingOFLeds.show();
  }
};


/* ================================================================
   ModeManager — switching, color flash, gesture forwarding
   ================================================================ */
/**
 * @brief Owns the list of modes and manages the active one.
 *
 * Handles mode switching (exit → color flash → enter), gesture forwarding,
 * and frame stepping. A single global instance `Modes` is created at the
 * bottom of this file.
 */
class ModeManager {
public:
  /** @brief Constructs the manager from an array of mode pointers. @param list Array of ModeBase pointers. @param count Number of modes. */
  ModeManager(ModeBase** list, uint8_t count) : _list(list), _count(count) {}

  /** @brief Start in the first mode. Call once from setup(). */
  void begin() { setMode(0); }

  /**
   * @brief Switch to a specific mode.
   *
   * Calls Exit() on the current mode, flashes the ring in the new mode's
   * color, then calls Enter() on the new mode.
   * @param index 0-based mode index. Out-of-range values are ignored.
   */
  void setMode(uint8_t index) {
    if (index >= _count) return;
    current().Exit();
    _index = index;
    Logger::log("Mode -> %s", current().name());
    flashColor(current().color());
    current().Enter();
  }

  /** @brief Advance to the next mode (wraps). Called on a 3-second button hold. */
  void nextMode() { setMode((_index + 1) % _count); }

  /** @brief Forward a single-press to the active mode. */
  void SP() { current().SP(); }
  /** @brief Forward a double-press to the active mode. */
  void DP() { current().DP(); }
  /** @brief Forward a triple-press to the active mode. */
  void TP() { current().TP(); }
  /** @brief Forward an animation frame to the active mode. @param now Current millis() timestamp. */
  void Step(unsigned long now) { current().Step(now); }

  /** @brief Returns a reference to the currently active mode. */
  ModeBase& current() { return *_list[_index]; }
  /** @brief Returns the 0-based index of the currently active mode. */
  uint8_t   index() const { return _index; }
  /** @brief Returns the total number of registered modes. */
  uint8_t   count() const { return _count; }

private:
  ModeBase** _list;
  uint8_t    _count;
  uint8_t    _index = 0;

  /**
   * @brief Flash the whole ring and button in the given color (2 quick blinks).
   *
   * Blocking, but only a few hundred milliseconds — only runs during a mode switch.
   * @param c 24-bit hex color (0xRRGGBB).
   */
  void flashColor(uint32_t c) {
    for (uint8_t blink = 0; blink < 2; blink++) {
      RingOFLeds.setToColorHex(c);
      RingOFLeds.show();
      RGBButton.setToColorHex(c);
      delay(120);
      RingOFLeds.clear();
      RingOFLeds.show();
      RGBButton.clear();
      delay(80);
    }
  }
};


/* ================================================================
   The actual mode instances + the manager.
   To add a mode: create an instance, add it to MODE_LIST.
   ================================================================ */
PartyMode modeParty;
LinkClick modeLinkClick;
Mode3     mode3;
Settings  modeSettings;

ModeBase* MODE_LIST[] = { &modeParty, &modeLinkClick, &mode3, &modeSettings };
ModeManager Modes(MODE_LIST, sizeof(MODE_LIST) / sizeof(MODE_LIST[0]));

#endif // MODES_H
