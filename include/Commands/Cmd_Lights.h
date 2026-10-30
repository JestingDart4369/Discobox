#pragma once
/*
  Cmd_Lights.h — `lights button ...`, `lights strip ...`, `lights ring ...`, `lights clear`.

  Every action can be typed three ways:  -c  /  --color  /  color
  Right after "lights strip" / "lights ring" these are ACTIONS (so -d = disco
  there); later in the line -d / -q / -h / -t are the global flags again.

  <color> everywhere:  255 0 0   255,0,0   #ff0000   ff0000   (see 'man colors')

    lights clear                         everything off (ring, button, strip)

    lights button -c  / --color <color>  button LED colour
    lights button -cl / --clear          button LED off

    lights strip -c   / --color <color>        whole strip
    lights strip -ww  / --warmwhite [0-255]    warm white channel (default 255)
    lights strip -cw  / --coolwhite [0-255]    cool white channel (default 255)
    lights strip -cct / --cct <r> <g> <b> <cw> <ww>   all 5 channels
    lights strip -hsv / --hsv <h> <s> <v>      hue / saturation / value
    lights strip -b   / --brightness [0-255]   strip brightness level (saved), none = show
    lights strip -f   / --fade <color>         smooth fade (~0.6 s, blocks)
    lights strip -w   / --wipe <color>         IC by IC (blocks several seconds)
    lights strip -l   / --led <n> <color>      ONE IC, others unchanged (n = 0..59)
    lights strip -fi  / --fadeic <n> <color>   fade ONE IC
    lights strip -i   / --ic <n> <color>       ONLY IC n lit, rest off (debug)
    lights strip -fl  / --flicker [sec]        warm candle effect (default 4, max 30)
    lights strip -wa  / --walk                 light ICs one by one (debug)
    lights strip -be  / --bench                time one disco frame (us)
    lights strip -d   / --disco                one random-colour frame
    lights strip -cl  / --clear                strip off

    lights ring -c   / --color <color>   whole ring
    lights ring -w   / --wipe <color>    LED by LED (~4 s, blocks)
    lights ring -f   / --fade <color>    smooth fade (~0.6 s, blocks)
    lights ring -d   / --disco           one random-colour frame
    lights ring -l   / --led <n> <color> ONE LED (n = 1..16)
    lights ring -hsv / --hsv <h> <s> <v> hue / saturation / value
    lights ring -b   / --brightness [0-255]   ring brightness level (saved), none = show
    lights ring -cl  / --clear           ring off

  A running animation (Party disco) repaints the ring every frame and will
  overwrite these — stop it first (sp).
  Uses the globals from main.cpp: RingOFLeds, RGBButton, Strip, Brightness, Persist.
*/
#include <Shell.h>

// ── shared argument tables ──────────────────────────────────────────────────
static const ArgSpec LIGHT_COLOR_ARGS[] = { { "color", ARG_COLOR, 0, 0, false, 0, nullptr, "r g b, r,g,b or #hex" } };
static const ArgSpec LIGHT_HSV_ARGS[]   = {
  { "h", ARG_INT, 0, 255, false, 0, nullptr, "hue (0 red, 85 green, 170 blue)" },
  { "s", ARG_INT, 0, 255, false, 0, nullptr, "saturation (0 = white)" },
  { "v", ARG_INT, 0, 255, false, 0, nullptr, "value / brightness" },
};
static const ArgSpec LIGHT_LEVEL_ARGS[] = { { "level", ARG_INT, 0, 255, true, 255, nullptr, "0..255; none = show" } };

/** @brief Shared by `lights strip -b` / `lights ring -b`: show or set + save one level. */
static void lightLevel(BrightnessManager::Light l, const ParsedArgs& a, CmdContext& ctx) {
  if (!a.has(0)) {
    ctx.out("%s brightness %u (shown at %u, master %u)", BrightnessManager::name(l),
            (unsigned)Brightness.level(l), (unsigned)Brightness.effective(l), (unsigned)Brightness.master());
    return;
  }
  Brightness.setLevel(l, (uint8_t)a.num(0));
  Persist.save();
  ctx.log("%s brightness -> %ld (saved)", BrightnessManager::name(l), (long)a.num(0));
}

// ════════════════════════════════════════════════════════════════════════════
// lights button
// ════════════════════════════════════════════════════════════════════════════
static void buttonColor(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(0, r, g, b);
  RGBButton.setToColor(r, g, b);                 // shows at once
  ctx.log("button -> %u %u %u", r, g, b);
}
static void buttonClear(const ParsedArgs&, CmdContext& ctx) {
  RGBButton.clear();
  ctx.log("button off");
}

static const CommandDef D_BUTTON_COLOR = { "color", "button LED colour", "-c --color",
  SHELL_LIST(LIGHT_COLOR_ARGS), SHELL_NONE, "lights button -c 0 0 255", nullptr, buttonColor };
static const CommandDef D_BUTTON_CLEAR = { "clear", "button LED off", "-cl --clear",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, buttonClear };
FnCommand cmdButtonColor(D_BUTTON_COLOR), cmdButtonClear(D_BUTTON_CLEAR);
Command* const BUTTON_CMDS[] = { &cmdButtonColor, &cmdButtonClear };
CommandGroup buttonGroup("button", "the RGB LED in the button", BUTTON_CMDS, SHELL_COUNT(BUTTON_CMDS));

// ════════════════════════════════════════════════════════════════════════════
// lights strip (WS2805 RGBCCT, 60 ICs)
// ════════════════════════════════════════════════════════════════════════════
static const ArgSpec STRIP_WHITE_ARGS[] = { { "level", ARG_INT, 0, 255, true, 255, nullptr, "channel level (default 255)" } };
static const ArgSpec STRIP_CCT_ARGS[]   = {
  { "r",  ARG_INT, 0, 255, false, 0, nullptr, "red" },
  { "g",  ARG_INT, 0, 255, false, 0, nullptr, "green" },
  { "b",  ARG_INT, 0, 255, false, 0, nullptr, "blue" },
  { "cw", ARG_INT, 0, 255, false, 0, nullptr, "cool white" },
  { "ww", ARG_INT, 0, 255, false, 0, nullptr, "warm white" },
};
static const ArgSpec STRIP_IC_ARGS[] = {
  { "n",     ARG_INT,   0, WS2805_N_LEDS - 1, false, 0, nullptr, "IC number, 0-based (1 IC = 3 LEDs)" },
  { "color", ARG_COLOR, 0, 0,                 false, 0, nullptr, "r g b, r,g,b or #hex" },
};
static const ArgSpec STRIP_FLICKER_ARGS[] = { { "sec", ARG_INT, 1, 30, true, 4, nullptr, "seconds (default 4)" } };

static void stripColor(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(0, r, g, b);
  Strip.setToColor(r, g, b);                     // shows at once
  ctx.log("strip -> %u %u %u", r, g, b);
}
static void stripWarm(const ParsedArgs& a, CmdContext& ctx) {
  Strip.setAllWW((uint8_t)a.num(0));
  ctx.log("strip warm white %ld", (long)a.num(0));
}
static void stripCool(const ParsedArgs& a, CmdContext& ctx) {
  Strip.setAllCW((uint8_t)a.num(0));
  ctx.log("strip cool white %ld", (long)a.num(0));
}
static void stripCct(const ParsedArgs& a, CmdContext& ctx) {
  Strip.setToColorCCT((uint8_t)a.num(0), (uint8_t)a.num(1), (uint8_t)a.num(2), (uint8_t)a.num(3), (uint8_t)a.num(4));
  ctx.log("strip cct R%ld G%ld B%ld CW%ld WW%ld", (long)a.num(0), (long)a.num(1), (long)a.num(2), (long)a.num(3), (long)a.num(4));
}
static void stripHsv(const ParsedArgs& a, CmdContext& ctx) {
  Strip.setToColorHSV((uint8_t)a.num(0), (uint8_t)a.num(1), (uint8_t)a.num(2));
  ctx.log("strip hsv %ld %ld %ld", (long)a.num(0), (long)a.num(1), (long)a.num(2));
}
static void stripBrightness(const ParsedArgs& a, CmdContext& ctx) { lightLevel(BrightnessManager::STRIP, a, ctx); }
static void stripFade(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(0, r, g, b);
  ctx.log("fading strip...");
  Strip.fadeToColor(r, g, b);
}
static void stripWipe(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(0, r, g, b);
  ctx.log("wiping strip... (blocks several seconds)");
  Strip.wipeToColor(r, g, b);
}
static void stripLed(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(1, r, g, b);
  Strip.setToColorSingle((uint8_t)a.num(0), r, g, b);
  Strip.show();
  ctx.log("strip IC %ld -> %u %u %u", (long)a.num(0), r, g, b);
}
static void stripFadeIc(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(1, r, g, b);
  Strip.fadeToColorSingle((uint8_t)a.num(0), r, g, b);
  ctx.log("strip IC %ld faded -> %u %u %u", (long)a.num(0), r, g, b);
}
static void stripIc(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(1, r, g, b);
  Strip.clear();
  Strip.setToColorSingle((uint8_t)a.num(0), r, g, b);
  Strip.show();
  ctx.log("only strip IC %ld -> %u %u %u", (long)a.num(0), r, g, b);
}
static void stripFlicker(const ParsedArgs& a, CmdContext& ctx) {
  unsigned long sec = (unsigned long)a.num(0);
  ctx.log("candle flicker for %lu s...", sec);
  Strip.flickerBegin();
  unsigned long t0 = millis();
  while (millis() - t0 < sec * 1000UL) { Strip.flickerUpdate(); delay(40); }
  Strip.clear();
  Strip.show();
}
static void stripWalk(const ParsedArgs&, CmdContext& ctx) {
  ctx.log("strip walk...");
  for (uint8_t i = 0; i < WS2805_N_LEDS; i++) {
    Strip.clear();
    Strip.setToColorSingle(i, 255, 0, 0);
    Strip.show();
    ctx.log("IC %u", (unsigned)i);
    delay(150);
  }
  Strip.clear();
  Strip.show();
}
static void stripBench(const ParsedArgs&, CmdContext& ctx) {
  unsigned long t0 = micros();
  Strip.stepDisco();                             // generate random colours for all ICs
  unsigned long t1 = micros();
  Strip.show();                                  // encode + send over SPI
  unsigned long t2 = micros();
  ctx.out("[bench] stepDisco %lu us | show (encode+SPI) %lu us | total %lu us", t1 - t0, t2 - t1, t2 - t0);
}
static void stripDisco(const ParsedArgs&, CmdContext& ctx) {
  Strip.showDisco();
  ctx.log("strip disco");
}
static void stripClear(const ParsedArgs&, CmdContext& ctx) {
  Strip.clear();
  Strip.show();
  ctx.log("strip cleared");
}

static const CommandDef D_STRIP_COLOR = { "color", "whole strip to one colour", "-c --color",
  SHELL_LIST(LIGHT_COLOR_ARGS), SHELL_NONE, "lights strip -c 255 0 0", nullptr, stripColor };
static const CommandDef D_STRIP_WW    = { "warmwhite", "warm white channel", "-ww --warmwhite",
  SHELL_LIST(STRIP_WHITE_ARGS), SHELL_NONE, "lights strip -ww 128", nullptr, stripWarm };
static const CommandDef D_STRIP_CW    = { "coolwhite", "cool white channel", "-cw --coolwhite",
  SHELL_LIST(STRIP_WHITE_ARGS), SHELL_NONE, "lights strip -cw 128", nullptr, stripCool };
static const CommandDef D_STRIP_CCT   = { "cct", "all 5 channels: r g b cw ww", "-cct --cct",
  SHELL_LIST(STRIP_CCT_ARGS), SHELL_NONE, "lights strip -cct 255 0 0 0 128", nullptr, stripCct };
static const CommandDef D_STRIP_HSV   = { "hsv", "hue / saturation / value", "-hsv --hsv",
  SHELL_LIST(LIGHT_HSV_ARGS), SHELL_NONE, "lights strip -hsv 160 255 255", nullptr, stripHsv };
static const CommandDef D_STRIP_BRIGHT = { "brightness", "strip brightness level (saved), none = show", "-b --brightness",
  SHELL_LIST(LIGHT_LEVEL_ARGS), SHELL_NONE, "lights strip -b 128", nullptr, stripBrightness };
static const CommandDef D_STRIP_FADE  = { "fade", "smooth fade (~0.6 s, blocks)", "-f --fade",
  SHELL_LIST(LIGHT_COLOR_ARGS), SHELL_NONE, "lights strip -f #ff8800", nullptr, stripFade };
static const CommandDef D_STRIP_WIPE  = { "wipe", "fill IC by IC (blocks several seconds)", "-w --wipe",
  SHELL_LIST(LIGHT_COLOR_ARGS), SHELL_NONE, "lights strip -w 0 255 255", nullptr, stripWipe };
static const CommandDef D_STRIP_LED   = { "led", "ONE IC, others unchanged", "-l --led",
  SHELL_LIST(STRIP_IC_ARGS), SHELL_NONE, "lights strip -l 5 255 0 0", nullptr, stripLed };
static const CommandDef D_STRIP_FADEIC = { "fadeic", "fade ONE IC to a colour", "-fi --fadeic",
  SHELL_LIST(STRIP_IC_ARGS), SHELL_NONE, "lights strip -fi 5 255 0 0", nullptr, stripFadeIc };
static const CommandDef D_STRIP_IC    = { "ic", "ONLY IC n lit, rest off (debug)", "-i --ic",
  SHELL_LIST(STRIP_IC_ARGS), SHELL_NONE, "lights strip -i 0 255 0 0", nullptr, stripIc };
static const CommandDef D_STRIP_FLICKER = { "flicker", "warm-white candle effect (blocks)", "-fl --flicker",
  SHELL_LIST(STRIP_FLICKER_ARGS), SHELL_NONE, "lights strip -fl 10", nullptr, stripFlicker };
static const CommandDef D_STRIP_WALK  = { "walk", "light ICs one by one (debug)", "-wa --walk",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, stripWalk };
static const CommandDef D_STRIP_BENCH = { "bench", "time one disco frame: generate + send (us)", "-be --bench",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, stripBench };
static const CommandDef D_STRIP_DISCO = { "disco", "one random-colour frame", "-d --disco",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, stripDisco };
static const CommandDef D_STRIP_CLEAR = { "clear", "strip off", "-cl --clear",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, stripClear };

FnCommand cmdStripColor(D_STRIP_COLOR), cmdStripWW(D_STRIP_WW), cmdStripCW(D_STRIP_CW), cmdStripCct(D_STRIP_CCT);
FnCommand cmdStripHsv(D_STRIP_HSV), cmdStripBright(D_STRIP_BRIGHT), cmdStripFade(D_STRIP_FADE), cmdStripWipe(D_STRIP_WIPE);
FnCommand cmdStripLed(D_STRIP_LED), cmdStripFadeIc(D_STRIP_FADEIC), cmdStripIc(D_STRIP_IC), cmdStripFlicker(D_STRIP_FLICKER);
FnCommand cmdStripWalk(D_STRIP_WALK), cmdStripBench(D_STRIP_BENCH), cmdStripDisco(D_STRIP_DISCO), cmdStripClear(D_STRIP_CLEAR);

Command* const STRIP_CMDS[] = {
  &cmdStripColor, &cmdStripWW, &cmdStripCW, &cmdStripCct, &cmdStripHsv, &cmdStripBright,
  &cmdStripFade, &cmdStripWipe, &cmdStripLed, &cmdStripFadeIc, &cmdStripIc, &cmdStripFlicker,
  &cmdStripWalk, &cmdStripBench, &cmdStripDisco, &cmdStripClear,
};
CommandGroup stripGroup("strip", "WS2805 RGBCCT strip (60 ICs = 180 LEDs)", STRIP_CMDS, SHELL_COUNT(STRIP_CMDS),
  "The 3 m 12 V WS2805 strip on D11. 1 IC drives 3 LEDs, so IC numbers\n"
  "are 0..59. Brightness is applied when sending, so -b changes it at once.");

// ════════════════════════════════════════════════════════════════════════════
// lights ring (16 x WS2812)
// ════════════════════════════════════════════════════════════════════════════
static const ArgSpec RING_LED_ARGS[] = {
  { "n",     ARG_INT,   1, 16, false, 1, nullptr, "LED number 1..16" },
  { "color", ARG_COLOR, 0, 0,  false, 0, nullptr, "r g b, r,g,b or #hex" },
};

static void ringColor(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(0, r, g, b);
  RingOFLeds.setToColor(r, g, b);
  RingOFLeds.show();
  ctx.log("ring -> %u %u %u", r, g, b);
}
static void ringWipe(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(0, r, g, b);
  ctx.log("wiping ring...");
  RingOFLeds.wipeToColor(r, g, b);
}
static void ringFade(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(0, r, g, b);
  ctx.log("fading ring...");
  RingOFLeds.fadeToColor(r, g, b);
}
static void ringDisco(const ParsedArgs&, CmdContext& ctx) {
  RingOFLeds.showDisco();
  ctx.log("ring disco");
}
static void ringLed(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t r, g, b;
  a.color(1, r, g, b);
  RingOFLeds.setToColorSingle((uint8_t)(a.num(0) - 1), r, g, b);
  RingOFLeds.show();
  ctx.log("ring LED %ld -> %u %u %u", (long)a.num(0), r, g, b);
}
static void ringHsv(const ParsedArgs& a, CmdContext& ctx) {
  RingOFLeds.setToColorHSV((uint8_t)a.num(0), (uint8_t)a.num(1), (uint8_t)a.num(2));
  RingOFLeds.show();
  ctx.log("ring hsv %ld %ld %ld", (long)a.num(0), (long)a.num(1), (long)a.num(2));
}
static void ringBrightness(const ParsedArgs& a, CmdContext& ctx) { lightLevel(BrightnessManager::RING, a, ctx); }
static void ringClear(const ParsedArgs&, CmdContext& ctx) {
  RingOFLeds.clear();
  RingOFLeds.show();
  ctx.log("ring cleared");
}

static const CommandDef D_RING_COLOR = { "color", "whole ring to one colour", "-c --color",
  SHELL_LIST(LIGHT_COLOR_ARGS), SHELL_NONE, "lights ring -c 255 0 0", nullptr, ringColor };
static const CommandDef D_RING_WIPE  = { "wipe", "fill LED by LED (~4 s, blocks)", "-w --wipe",
  SHELL_LIST(LIGHT_COLOR_ARGS), SHELL_NONE, "lights ring -w #00ffff", nullptr, ringWipe };
static const CommandDef D_RING_FADE  = { "fade", "smooth fade (~0.6 s, blocks)", "-f --fade",
  SHELL_LIST(LIGHT_COLOR_ARGS), SHELL_NONE, "lights ring -f 255 136 0", nullptr, ringFade };
static const CommandDef D_RING_DISCO = { "disco", "one random-colour frame", "-d --disco",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, ringDisco };
static const CommandDef D_RING_LED   = { "led", "ONE LED, others unchanged", "-l --led",
  SHELL_LIST(RING_LED_ARGS), SHELL_NONE, "lights ring -l 3 255 0 0", nullptr, ringLed };
static const CommandDef D_RING_HSV   = { "hsv", "hue / saturation / value", "-hsv --hsv",
  SHELL_LIST(LIGHT_HSV_ARGS), SHELL_NONE, "lights ring -hsv 85 255 255", nullptr, ringHsv };
static const CommandDef D_RING_BRIGHT = { "brightness", "ring brightness level (saved), none = show", "-b --brightness",
  SHELL_LIST(LIGHT_LEVEL_ARGS), SHELL_NONE, "lights ring -b 128", nullptr, ringBrightness };
static const CommandDef D_RING_CLEAR = { "clear", "ring off", "-cl --clear",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, ringClear };

FnCommand cmdRingColor(D_RING_COLOR), cmdRingWipe(D_RING_WIPE), cmdRingFade(D_RING_FADE), cmdRingDisco(D_RING_DISCO);
FnCommand cmdRingLed(D_RING_LED), cmdRingHsv(D_RING_HSV), cmdRingBright(D_RING_BRIGHT), cmdRingClear(D_RING_CLEAR);

Command* const RING_CMDS[] = {
  &cmdRingColor, &cmdRingWipe, &cmdRingFade, &cmdRingDisco, &cmdRingLed, &cmdRingHsv, &cmdRingBright, &cmdRingClear,
};
CommandGroup ringGroup("ring", "16-LED WS2812 ring", RING_CMDS, SHELL_COUNT(RING_CMDS),
  "The 16-LED ring on D4. LED numbers are 1..16.\n"
  "A running animation (Party disco) repaints the ring every frame -\n"
  "stop it first (sp) or your colour is overwritten at once.");

// ════════════════════════════════════════════════════════════════════════════
// lights
// ════════════════════════════════════════════════════════════════════════════
static void lightsClear(const ParsedArgs&, CmdContext& ctx) {
  RingOFLeds.clear();
  RingOFLeds.show();
  RGBButton.clear();
  Strip.clear();
  Strip.show();
  ctx.log("all lights off");
}
static const CommandDef D_LIGHTS_CLEAR = { "clear", "everything off (ring, button, strip)", "-cl --clear",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, lightsClear };
FnCommand cmdLightsClear(D_LIGHTS_CLEAR);

Command* const LIGHTS_CMDS[] = { &buttonGroup, &stripGroup, &ringGroup, &cmdLightsClear };
CommandGroup lightsGroup("lights", "button LED, strip and ring", LIGHTS_CMDS, SHELL_COUNT(LIGHTS_CMDS));
