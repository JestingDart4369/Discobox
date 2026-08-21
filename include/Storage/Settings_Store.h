#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H
/*
  Settings_Store.h — power-resistant settings: values that survive a
  power loss / reboot, not just staying set until the party box is
  unplugged.

  HOW IT SURVIVES POWER LOSS
  The Uno R4 has no real EEPROM chip — "EEPROM.h" here is a wrapper
  around ~8KB of the Renesas chip's own flash, reserved for exactly this.
  It behaves like classic EEPROM (byte-addressed, read/write survive a
  power cycle and even re-flashing the sketch), with one real caveat:
  flash wears out, roughly 1,000-10,000 write cycles per byte. That's
  plenty for "save when a setting changes" but would die fast if called
  every loop() — so nothing in here is ever called from step()/loop(),
  only from the exact spots that change a setting on purpose. EEPROM.put()
  also only rewrites bytes that actually differ from what's already
  there, so re-saving the same value again costs nothing.

  WHAT'S SAVED
  Cue theme, cue gap (before/after), cue mute, ring brightness — the
  things Settings mode / `buzzer -theme|-gap|-mute` / `bright` change.
  NOT saved: which mode is active, party running/stopped, which song is
  queued — Discobox is meant to always wake up fresh in Party, silent,
  at song 0, regardless of how it was left.

  USAGE
    Persist.load();   // call once in setup(), AFTER Buzzer.Setup()-type
                       // objects exist — applies the saved values right
                       // away (first boot ever: seeds flash with the
                       // current defaults instead, so there's something
                       // to load next time)
    Persist.save();   // call right after changing a setting you want to
                       // survive a power cycle (see main.cpp / Serial_Handler.h
                       // for where -theme/-gap/-mute/bright already do this)

  ADD A NEW SETTING: add a field to PersistedSettings, bump VERSION (so
  old flash data — now the wrong shape — is recognized as stale and
  replaced with defaults instead of misread), fill the field in save(),
  apply it in load().

  IMPORTANT: include this in main.cpp AFTER Buzzer_Player.h and
  led_Controller_h.h's objects (Buzzer, RingOFLeds) are defined — it
  reads and writes those globals directly.
*/

#include <EEPROM.h>

struct PersistedSettings {
  uint16_t magic;          // "is this actually our data?" check
  uint8_t  version;        // bump when the struct shape changes
  uint8_t  cuePackIndex;
  uint16_t gapBeforeMs;
  uint16_t gapAfterMs;
  uint8_t  muted;           // 0/1 — bool's stored size isn't guaranteed
  uint8_t  brightness;
};

class SettingsStore {
public:
  static const uint16_t MAGIC   = 0xD15C;   // arbitrary, just "not garbage"
  static const uint8_t  VERSION = 1;

  // Read flash and apply it to Buzzer / RingOFLeds. If nothing valid is
  // there yet (first boot ever, or the struct shape changed since the
  // last save), seeds flash with the current (default) settings instead,
  // so there's something sane to load next time.
  void load() {
    PersistedSettings d;
    EEPROM.get(0, d);

    if (d.magic != MAGIC || d.version != VERSION) {
      Logger::log("Settings: nothing saved yet, using + storing defaults");
      save();
      return;
    }
    
    Buzzer.setCuePack(d.cuePackIndex);
    Buzzer.setCueGap(d.gapBeforeMs, d.gapAfterMs);
    Buzzer.setMuted(d.muted != 0);
    RingOFLeds.setBrightness(d.brightness);

    Logger::log("Settings: loaded (pack %s, gap %d/%dms, cues %s, brightness %d)",
                Buzzer.cuePackName(), d.gapBeforeMs, d.gapAfterMs,
                d.muted ? "muted" : "on", d.brightness);
  }

  // Snapshot the current settings from Buzzer / RingOFLeds and write them
  // to flash. Call this right after changing something you want to stick
  // — not from loop()/step(), see the file header on why.
  void save() {
    PersistedSettings d;
    d.magic        = MAGIC;
    d.version      = VERSION;
    d.cuePackIndex = Buzzer.cuePackIndex();
    d.gapBeforeMs  = Buzzer.cueGapBefore();
    d.gapAfterMs   = Buzzer.cueGapAfter();
    d.muted        = Buzzer.isMuted() ? 1 : 0;
    d.brightness   = RingOFLeds.getBrightness();

    EEPROM.put(0, d);   // only actually rewrites bytes that changed
    Logger::log("Settings: saved");
  }
};

// The one global store, wired to the global Buzzer / RingOFLeds objects.
SettingsStore Persist;

#endif // SETTINGS_STORE_H
