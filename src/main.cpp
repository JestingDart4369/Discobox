// Discobox main.cpp
// Version 0.5
//
// main.cpp only does: hardware setup + wiring the pieces together.
//   include/Modes.h          — all modes + their gesture actions (OOP)
//   lib/GestureButton/GestureButton.h — button gesture detection (SP/DP/TP/hold)
//   include/Lights/...       — LED control classes
//   include/Buzzer/...       — speaker + songs + UI sound cues
///Documentation Layout Pins
///P2=yellow, P3=Red, P4=white, P5=green, P6=blue, P9=speaker


// Include necessary libraries
#include <Arduino.h>
#include <LedController.h>
#include <Logger.h>
#include <GestureButton.h>
#define WS2805_N_LEDS 60   // pixel (IC) count — must match WS2805<60> below
#define WS2805_INDEX_SHIFT 0 // off-by-one fix: try +1 or -1 if the strip looks shifted by one IC (see WS2805.h)
#define WS2805_CHUNK_BYTES 0   // 0 = whole frame in one SPI transfer (20 = one pixel per call, breaks the frame)
#include <WS2805.h>
// Buzzer (piezo songs)
#include <SongPlayer.h>
// DFPlayer Mini (MP3 over Serial1, D0=RX D1=TX)
#include <DFPlayerController.h>
// Configuration

  // Button config
    #define button_output  D2

  // Setup Speaker
    speakerController speaker(D9);
  // Song list player on top of the speaker (must come AFTER `speaker`)
    #include "Buzzer/Buzzer_Player.h"
  //  Setup Led
    ARGB<D4, 16> RingOFLeds;
    RGBLeds<D3, D5, D6> RGBButton;
    WS2805<60> Strip;  // 20 ICs/m × 3 m = 60 pixels (each IC drives 3 LEDs, 180 LEDs total)

  // One place for all brightness: own level per light + a master on top
  // (must come AFTER RingOFLeds/RGBButton/Strip, BEFORE Settings_Store.h)
    #include "Lights/Brightness_Manager.h"

  // One place for all loudness: speaker / buzzer music / buzzer cues + master
  // (must come AFTER Buzzer + DFPlayer, BEFORE Settings_Store.h)
    #include "Audio/Volume_Manager.h"

  // Power-resistant settings (cue theme/gap/mute, brightness levels + master,
  // volume) — must come AFTER Buzzer + Brightness above, it reads/writes those.
    #include "Storage/Settings_Store.h"

  // What the Settings mode adjusts (brightness/mute/theme/...) — must
  // come AFTER Buzzer + Brightness + Persist above, and BEFORE Modes.h.
    #include "Settings_Items.h"

// Modes (must come AFTER the objects above — Modes.h uses them)
#include "Modes.h"

// Button: wire the 4 gestures to the mode system + a UI sound cue each.
// SP/DP/TP go to the active mode; HOLD switches to the next mode.
//
// The mode action runs FIRST, cue SECOND — on purpose: SP/DP often call
// Buzzer.play()/stop() themselves (e.g. Party starting/stopping music),
// and that would instantly wipe out a cue queued beforehand. Doing the
// action first means playCue() sees whatever the action just started
// and ducks it properly instead of getting overwritten by it.
//
// The cue itself is picked by the mode (cueForSP/DP/TP — default is the
// generic press/double-click/select, Party overrides SP/DP for start/
// stop/skip-next). HOLD's cue is the same everywhere, since it always
// just means "switch mode".
  void Btn_SP()   { Modes.SP();       Buzzer.playCue(Modes.current().cueForSP()); }
  void Btn_DP()   { Modes.DP();       Buzzer.playCue(Modes.current().cueForDP()); }
  void Btn_TP()   { Modes.TP();       Buzzer.playCue(Modes.current().cueForTP()); }
  void Btn_Hold() { Modes.nextMode(); Buzzer.playCue(UISFX_LONG_PRESS); }

  GestureButton Button(button_output, Btn_SP, Btn_DP, Btn_TP, Btn_Hold);

// Like delay(), but keeps the buzzer alive while it waits. setup() runs
// once, before loop() ever starts, so nothing normally calls Buzzer.step()
// during it — any Buzzer.playCue()/play() called in setup() would just
// sit queued, silent, until something later (even Buzzer.stop() itself)
// wipes it out unheard. Use this instead of delay() anywhere in setup()
// that follows a playCue()/play() call you actually want to hear.
  void bootWait(uint16_t ms) {
    unsigned long t0 = millis();
    while (millis() - t0 < ms) Buzzer.step();
  }

// Serial command shell over the USB-C cable (lib/Shell, doc/SHELL.md).
// Type 'help' in the Serial Monitor. Commands live in include/Commands/ and
// are registered in Commands.h. Must come AFTER every object a command uses
// (Strip, Buzzer, Modes, ...) and AFTER Btn_SP etc. — typing sp/dp/tp/hold
// reuses the same functions, so you get the same sound cues as the button.
#include <Shell.h>
#include "Commands/Commands.h"
#define DISCOBOX_VERSION "Discobox 0.5"
ShellSerialIO ShellIo(Serial, Logger::setQuiet);   // -q also silences Logger
Shell SerialShell(ShellIo, SHELL_COMMANDS, SHELL_COMMAND_COUNT, DISCOBOX_VERSION);


//setup function
void setup() {
  //logger
    Serial.begin(9600);
    Logger::log("Booting %s", "Discobox");

  //serial shell
    SerialShell.setTopics(SHELL_TOPICS, SHELL_TOPIC_COUNT);
  #ifdef DEBUG_ENABLED
    // checks every command description (ranges, help texts, flag letters ...)
    if (SerialShell.selfCheck() > 0) Logger::warn("shell: command specs have problems, see [ERROR] lines above");
  #endif

  //Pins
    pinMode(LED_BUILTIN, OUTPUT);

  // Initialization of peripherals
    // LED strip first — clears SPI bus noise before DFPlayer's 2s boot delay
      Strip.setup();   // takes SPI control + sends all-off immediately
      Strip.clear();
      Strip.show();
    //button:
      Button.setup();
      RGBButton.setup();
      RGBButton.setToColor(255, 0, 0); // Red for button

    //ring:
      RingOFLeds.setup();
      RingOFLeds.clear();
      RingOFLeds.setToColor(255,0,0);
      RingOFLeds.show();
    //speaker:
      speaker.setup();
    // DFPlayer Mini — Serial1 (D0=RX ← DFPlayer TX, D1=TX → DFPlayer RX via 1kΩ)
      Serial1.begin(9600);
      DFPlayer.begin(20);   // volume 0-30; begin() blocks ~2s for DFPlayer boot

    //settings saved from last time (cue theme/gap/mute, brightness) —
    //loaded before the boot cues below, so they already sound right.
      Persist.load();

  delay(500);
  //rest of setup here
  Buzzer.playCue("loading");           // ~1.06s — see bootWait() below
  for (uint8_t i = 0; i < 16; i++) {
    RingOFLeds.setToColorSingle(i,0, 255, 0); // Green for ring during boot
    RingOFLeds.show();
    bootWait(100);                      // was delay(100) — that silently ate the cue
  }
  RingOFLeds.clear();
  RingOFLeds.show();

//button Goes Green, to show startup ok, then start in the first mode (Party).
  RGBButton.setToColor(0, 255, 0);
  Buzzer.playCue("complete");          // ~0.73s
  Logger::log("Boot complete");
  bootWait(1000);                      // was delay(1000) — let "complete" actually play
  Modes.begin();                       // enters Party mode, whose Exit() calls Buzzer.stop()
}


//  Main loop

//rendering
unsigned long _last_anim_frame = 0;
uint8_t fps = 40; // max animation frame rate
uint8_t milis_fps = 1000 / fps;
void loop() {
  Buzzer.step();     // every iteration — keeps song tight (+ auto-next)
  DFPlayer.step();   // every iteration — detects track-end, auto-next

  unsigned long now = millis();
  if (now - _last_anim_frame >= milis_fps) { // 40 fps max for animations, button handling, etc.
    _last_anim_frame = now;

    // Button gestures: SP / DP / TP + 3s hold (= next mode)
    Button.update(now);

    // Commands from the USB-C cable (Serial Monitor)
    SerialShell.update();

    // Run the active mode's animation frame
    Modes.Step(now);
  }
}
