#pragma once
/*
  Cmd_System.h — gestures, modes, status and brightness.

    sp / dp / tp           simulate a single / double / triple press (same as the button)
    hold                   simulate the 3 s hold: next mode   (also: next)
    mode <n>               jump straight to mode n (1..count)
    status                 mode, setting, song, cues, speaker, brightness

    brightness                         show all levels + master
    brightness -a / --all <0-255>      MASTER: scales every light together
    brightness -r / --ring <0-255>     ring level
    brightness -s / --strip <0-255>    strip level
    brightness --button <0-255>        button LED level
    (flags combine: brightness -r 200 -s 80; every change is saved)

  Uses the globals from main.cpp: Btn_SP/DP/TP/Hold, Modes, SettingsList,
  Buzzer, DFPlayer, modeParty, Brightness, Persist.
*/
#include <Shell.h>

// ── gestures ────────────────────────────────────────────────────────────────
static void gestureSP(const ParsedArgs&, CmdContext& ctx)   { ctx.log("SP");   Btn_SP(); }
static void gestureDP(const ParsedArgs&, CmdContext& ctx)   { ctx.log("DP");   Btn_DP(); }
static void gestureTP(const ParsedArgs&, CmdContext& ctx)   { ctx.log("TP");   Btn_TP(); }
static void gestureHold(const ParsedArgs&, CmdContext& ctx) { ctx.log("HOLD"); Btn_Hold(); }

static const CommandDef D_SP   = { "sp",   "single press (whatever the current mode does)", nullptr,
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, gestureSP };
static const CommandDef D_DP   = { "dp",   "double press", nullptr, SHELL_NONE, SHELL_NONE, nullptr, nullptr, gestureDP };
static const CommandDef D_TP   = { "tp",   "triple press", nullptr, SHELL_NONE, SHELL_NONE, nullptr, nullptr, gestureTP };
static const CommandDef D_HOLD = { "hold", "3 s hold: switch to the next mode", "next",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, gestureHold };
FnCommand cmdSP(D_SP), cmdDP(D_DP), cmdTP(D_TP), cmdHold(D_HOLD);

// ── mode ────────────────────────────────────────────────────────────────────
static const ArgSpec MODE_ARGS[] = { { "n", ARG_INT, 1, 9, false, 1, nullptr, "mode number (see 'status')" } };
static void modeJump(const ParsedArgs& a, CmdContext& ctx) {
  if (a.num(0) > Modes.count()) { ctx.err("mode must be 1..%u", (unsigned)Modes.count()); return; }
  Modes.setMode((uint8_t)(a.num(0) - 1));
}
static const CommandDef D_MODE = { "mode", "jump straight to mode n", nullptr,
  SHELL_LIST(MODE_ARGS), SHELL_NONE, "mode 2", nullptr, modeJump };
FnCommand cmdMode(D_MODE);

// ── status ──────────────────────────────────────────────────────────────────
static void statusShow(const ParsedArgs&, CmdContext& ctx) {
  ctx.out("mode:     %s (%u/%u)", Modes.current().name(), (unsigned)(Modes.index() + 1), (unsigned)Modes.count());
  ctx.out("setting:  %s (%u/%u)", SettingsList.current().name(),
          (unsigned)(SettingsList.index() + 1), (unsigned)SettingsList.count());
  ctx.out("buzzer:   %s | party %s | music %s | cues %s | theme %s | gap %u/%u ms",
          Buzzer.songName(), modeParty.isRunning() ? "running" : "off",
          Buzzer.isPlaying() ? "playing" : "silent", Buzzer.isMuted() ? "muted" : "on",
          Buzzer.cuePackName(), (unsigned)Buzzer.cueGapBefore(), (unsigned)Buzzer.cueGapAfter());
  ctx.out("speaker:  %s | folder %u track %u",
          !DFPlayer.isOK() ? "not found" : DFPlayer.isPlaying() ? "playing" : "ready",
          (unsigned)DFPlayer.currentFolder(), (unsigned)DFPlayer.currentTrack());
  ctx.out("volume:   speaker %u/30, music %u/8, cues %u/8, master %u %%",
          (unsigned)Volume.level(VolumeManager::SPEAKER), (unsigned)Volume.level(VolumeManager::MUSIC),
          (unsigned)Volume.level(VolumeManager::CUES), (unsigned)Volume.master());
  ctx.out("bright:   ring %u, strip %u, button %u, master %u",
          (unsigned)Brightness.level(BrightnessManager::RING), (unsigned)Brightness.level(BrightnessManager::STRIP),
          (unsigned)Brightness.level(BrightnessManager::BUTTON), (unsigned)Brightness.master());
}
static const CommandDef D_STATUS = { "status", "mode, setting, song, cues, speaker, brightness", nullptr,
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, statusShow };
FnCommand cmdStatus(D_STATUS);

// ── brightness ──────────────────────────────────────────────────────────────
static const FlagSpec BRIGHTNESS_FLAGS[] = {
  // long      short type     min max  def  help
  { "all",    'a',  ARG_INT, 0,  255, 255, "MASTER: scales every light together" },
  { "ring",   'r',  ARG_INT, 0,  255, 255, "ring level" },
  { "strip",  's',  ARG_INT, 0,  255, 255, "strip level" },
  { "button", '\0', ARG_INT, 0,  255, 255, "button LED level" },
};
static void brightnessCmd(const ParsedArgs& a, CmdContext& ctx) {
  bool changed = false;
  if (a.flagSet(0)) { Brightness.setMaster((uint8_t)a.flagNum(0)); changed = true; }
  if (a.flagSet(1)) { Brightness.setLevel(BrightnessManager::RING,   (uint8_t)a.flagNum(1)); changed = true; }
  if (a.flagSet(2)) { Brightness.setLevel(BrightnessManager::STRIP,  (uint8_t)a.flagNum(2)); changed = true; }
  if (a.flagSet(3)) { Brightness.setLevel(BrightnessManager::BUTTON, (uint8_t)a.flagNum(3)); changed = true; }
  if (changed) Persist.save();
  for (uint8_t l = 0; l < BrightnessManager::LIGHT_COUNT; l++) {
    BrightnessManager::Light light = (BrightnessManager::Light)l;
    ctx.out("  %-7s level %3u  -> shown at %3u", BrightnessManager::name(light),
            (unsigned)Brightness.level(light), (unsigned)Brightness.effective(light));
  }
  ctx.out("  master  %3u%s", (unsigned)Brightness.master(), changed ? "   (saved)" : "");
}
static const CommandDef D_BRIGHTNESS = { "brightness", "show or set brightness (levels + master), saved", nullptr,
  SHELL_NONE, SHELL_LIST(BRIGHTNESS_FLAGS), "brightness -a 128",
  "Every light has its own level; the master (-a / --all) scales all of them:\n"
  "  shown = level * master / 255   (ring 200, master 128 -> shown at 100)\n"
  "Flags combine: brightness -r 200 -s 80 --button 40\n"
  "Settings mode changes the ring/strip levels; the master is CLI only.",
  brightnessCmd };
FnCommand cmdBrightness(D_BRIGHTNESS);
