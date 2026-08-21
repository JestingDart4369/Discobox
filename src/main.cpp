// Discobox main.cpp
// Version 0.5
//
// main.cpp only does: hardware setup + wiring the pieces together.
//   include/Modes.h          — all modes + their gesture actions (OOP)
//   include/Button_Handler.h — button gesture detection (SP/DP/TP/hold)
//   include/Lights/...       — LED control classes
//   include/Buzzer/...       — speaker + songs + UI sound cues


// Include necessary libraries
#include <Arduino.h>
#include "Lights/led_Controller_h.h"
#include "Logger_Handler.h"
#include "Button_Handler.h"
//Audio
#include "Buzzer/SongsPlayer/SongPlayer.h"

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

    //brightness level 255/15 = 17 steps for a full circle on the ring
    uint8_t brightness = 15;

  // Power-resistant settings (cue theme/gap/mute, ring brightness) — must
  // come AFTER Buzzer + RingOFLeds above, it reads/writes those directly.
    #include "Storage/Settings_Store.h"

  // What the Settings mode adjusts (brightness/mute/theme/...) — must
  // come AFTER Buzzer + RingOFLeds + Persist above, and BEFORE Modes.h.
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

// Serial commands over the USB-C cable (must come AFTER Btn_SP etc. above —
// typing sp/dp/tp/hold in the Serial Monitor reuses the same functions,
// so serial-triggered gestures get the same sound cues as the button).
// Type 'help' in the Serial Monitor to see the commands.
#include "Serial_Handler.h"
SerialCommander SerialCmd;


//setup function
void setup() {

  //logger
    Serial.begin(9600);
    Logger::log("Booting %s", "Discobox");

  //Pins
    pinMode(LED_BUILTIN, OUTPUT);

  // Initialization of peripherals

    //button:
      Button.Setup();
      RGBButton.Setup();
      RGBButton.ledSet(255, 0, 0); // Red for button

    //ring:
      RingOFLeds.Setup();
      RingOFLeds.All_Set(255,0,0);
      RingOFLeds.show();

    //speaker:
      speaker.Setup();

    //settings saved from last time (cue theme/gap/mute, brightness) —
    //loaded before the boot cues below, so they already sound right.
      Persist.load();

  delay(500);
  //rest of setup here
  Buzzer.playCue("loading");           // ~1.06s — see bootWait() below
  for (uint8_t i = 0; i < 16; i++) {
    RingOFLeds.Set_Single(i,0, 255, 0); // Green for ring during boot
    RingOFLeds.show();
    bootWait(100);                      // was delay(100) — that silently ate the cue
  }
  RingOFLeds.clear();
  RingOFLeds.show();

//button Goes Green, to show startup ok, then start in the first mode (Party).
  RGBButton.ledSet(0, 255, 0);
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

  unsigned long now = millis();
  if (now - _last_anim_frame >= milis_fps) { // 40 fps max for animations, button handling, etc.
    _last_anim_frame = now;

    // Button gestures: SP / DP / TP + 3s hold (= next mode)
    Button.update(now);

    // Commands from the USB-C cable (Serial Monitor)
    SerialCmd.update();

    // Run the active mode's animation frame
    Modes.Step(now);
  }
}
