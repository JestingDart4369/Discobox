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
  The song list itself lives in Buzzer/Buzzer_Player.h.
*/


/* ================================================================
   ModeBase — every mode inherits from this
   ================================================================ */
class ModeBase {
public:
  virtual ~ModeBase() {}

  // Identity
  virtual const char* name()  const = 0;   // shown in the log
  virtual uint32_t    color() const = 0;   // flash + indicator color

  // Lifecycle
  virtual void Enter() {}                  // mode becomes active
  virtual void Exit()  {}                  // mode is being left (stop your stuff!)
  virtual void Step(unsigned long now) {}  // every frame while active

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
class PartyMode : public ModeBase {
public:
  const char* name()  const override { return "Party"; }
  uint32_t    color() const override { return 0xFFFF00; }   // yellow

  void Enter() override {
    _running = false;                    // party waits for SP to start
    RingOFLeds.clear();
    RingOFLeds.show();
    RGBButton.ledSetHex(color());        // button shows the mode color while idle
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
      RingOFLeds.Disco_step();
      RingOFLeds.show();
      RGBButton.Disco_step();
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
      RingOFLeds.show();
      RGBButton.ledSetHex(color());
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
class LinkClick : public ModeBase {
public:
  const char* name()  const override { return "LinkClick"; }
  uint32_t    color() const override { return 0x0000FF; }   // blue

  void Enter() override {
    RingOFLeds.clear();
    for (uint8_t i = 0; i < 2; i++)    // 2 LEDs in the mode color = "this is mode 2"
      RingOFLeds.Set_Single_Hex(i, color());
    RingOFLeds.show();
    RGBButton.ledSetHex(color());
    Buzzer.play(1);
  }

  void Exit() override { /* stop anything this mode started */ }

  void Step(unsigned long now) override { /* TODO: animations, every frame */ }

  void SP() override { Logger::log("LinkClick: SP — not assigned yet"); /* TODO */ }
  void DP() override { Logger::log("LinkClick: DP — not assigned yet"); /* TODO */ }
  void TP() override { Logger::log("LinkClick: TP — not assigned yet"); /* TODO */ }

private:
  // LinkClick variables — e.g.:
  // bool _something = false;
};


/* ================================================================
   MODE 3 — (name me!)
   ================================================================ */
class Mode3 : public ModeBase {
public:
  const char* name()  const override { return "Mode3"; }
  uint32_t    color() const override { return 0x00FFFF; }   // cyan

  void Enter() override {
    RingOFLeds.clear();
    for (uint8_t i = 0; i < 3; i++)
      RingOFLeds.Set_Single_Hex(i, color());
    RingOFLeds.show();
    RGBButton.ledSetHex(color());
  }

  void Exit() override { }
  void Step(unsigned long now) override { /* TODO */ }
  void SP() override { Logger::log("Mode3: SP — not assigned yet"); /* TODO */ }
  void DP() override { Logger::log("Mode3: DP — not assigned yet"); /* TODO */ }
  void TP() override { Logger::log("Mode3: TP — not assigned yet"); /* TODO */ }

private:
  // Mode3 variables
};


/* ================================================================
   MODE 4 — (Settings)
   Settings:
   ================================================================ */
class Settings : public ModeBase {
public:
  const char* name()  const override { return "Settings"; }
  uint32_t    color() const override { return 0xFF0000; }   // red

  void Enter() override {
    RingOFLeds.clear();
    for (uint8_t i = 0; i < 4; i++)
      RingOFLeds.Set_Single_Hex(i, color());
    RingOFLeds.show();
    RGBButton.ledSetHex(color());
    _settingSelected = 0;   // first setting is selected by default
  }

  void Exit() override { }
  void Step(unsigned long now) override {  }
  void SP() override { Logger::log("Settings: SP — not assigned yet"); /* UP */ }
  void DP() override { Logger::log("Settings: DP — not assigned yet"); /* PLUS */ }
  void TP() override { Logger::log("Settings: TP — not assigned yet"); /* DOWN */ }

private:
  // Settings variables
  uint8_t _settingSelected = 0;
};


/* ================================================================
   ModeManager — switching, color flash, gesture forwarding
   ================================================================ */
class ModeManager {
public:
  ModeManager(ModeBase** list, uint8_t count) : _list(list), _count(count) {}

  // Call once from setup() to start in the first mode.
  void begin() { setMode(0); }

  // Switch to a specific mode (with exit of the old + color flash + enter).
  void setMode(uint8_t index) {
    if (index >= _count) return;
    current().Exit();
    _index = index;
    Logger::log("Mode -> %s", current().name());
    flashColor(current().color());
    current().Enter();
  }

  // HOLD — same in every mode: go to the next mode
  void nextMode() { setMode((_index + 1) % _count); }

  // Forward gestures + frames to the active mode
  void SP() { current().SP(); }
  void DP() { current().DP(); }
  void TP() { current().TP(); }
  void Step(unsigned long now) { current().Step(now); }

  ModeBase& current() { return *_list[_index]; }
  uint8_t   index() const { return _index; }
  uint8_t   count() const { return _count; }

private:
  ModeBase** _list;
  uint8_t    _count;
  uint8_t    _index = 0;

  // Quick flash of the whole ring + button in the mode's color (2 blinks).
  // Blocking, but very short — only runs at the moment of a mode switch.
  void flashColor(uint32_t c) {
    for (uint8_t blink = 0; blink < 2; blink++) {
      RingOFLeds.All_Set_Hex(c);
      RingOFLeds.show();
      RGBButton.ledSetHex(c);
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
