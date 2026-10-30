#pragma once
/*
  _Template.h — COPY-PASTE EXAMPLES for new shell commands. Not compiled
  (nothing includes it). Full explanation: doc/SHELL.md

  Shows every building block, using the real strip:
    strip led <index> <color> [-o|--only]          INT + COLOR args, switch flag
    strip wipe <color> [-s|--speed N] [-S|--steps N] COLOR arg, two INT flags
    strip flicker [seconds]                          OPTIONAL INT arg with default
    strip white <tone> [level]                       CHOICE arg + optional INT
    strip clear                                      no args at all
    strip                                            the group holding them

  Recipe for one command:
    1. ArgSpec[] / FlagSpec[]  — describe the input (static const → stays in flash)
    2. class XyzCmd : public Command
         name() summary()  SHELL_ARGS() SHELL_FLAGS()  example() manText()
         run(args, ctx)    — gets ONLY valid values, does the work
    3. hardware objects come in through the constructor (testable, no globals)
    4. create the object, put it into a group or into SHELL_COMMANDS
*/
#include <Shell.h>

// ════════════════════════════════════════════════════════════════════════════
// strip led <index> <color> [-o|--only]
// ════════════════════════════════════════════════════════════════════════════
static const ArgSpec STRIP_LED_ARGS[] = {
  // name     type       min  max                optional def choices  help
  { "index", ARG_INT,    0,   WS2805_N_LEDS - 1, false,   0,  nullptr, "IC number (0-based)" },
  { "color", ARG_COLOR,  0,   0,                 false,   0,  nullptr, "r g b, r,g,b or #hex" },
};
static const FlagSpec STRIP_LED_FLAGS[] = {
  // long    short type      min max def help
  { "only",  'o',  ARG_NONE, 0,  0,  0,  "turn every other IC off first" },
};

class StripLedCmd : public Command {
public:
  explicit StripLedCmd(WS2805<WS2805_N_LEDS>& strip) : _strip(strip) {}

  const char* name()    const override { return "led"; }
  const char* summary() const override { return "set ONE IC, others unchanged"; }
  SHELL_ARGS(STRIP_LED_ARGS)
  SHELL_FLAGS(STRIP_LED_FLAGS)
  const char* example() const override { return "strip led 5 255 0 0"; }
  const char* manText() const override {
    return "Sets a single IC (3 LEDs) to a colour and shows it at once.\n"
           "1 IC = 3 LEDs, so the 3 m strip has ICs 0..59.\n"
           "With --only every other IC is switched off first.";
  }

  void run(const ParsedArgs& a, CmdContext& ctx) override {
    uint8_t n = (uint8_t)a.num(0);            // <index>, already checked 0..59
    uint8_t r, g, b;
    a.color(1, r, g, b);                      // <color>
    if (a.flagSet(0)) _strip.clear();         // --only
    ctx.dbg("brightness %u", _strip.getBrightness());   // only with -d
    _strip.setToColorSingle(n, r, g, b);
    _strip.show();
    ctx.log("strip IC %u -> %u %u %u", n, r, g, b);    // silent with -q
  }

private:
  WS2805<WS2805_N_LEDS>& _strip;
};

// ════════════════════════════════════════════════════════════════════════════
// strip wipe <color> [-s|--speed 1..200] [-S|--steps 1..100]
// ════════════════════════════════════════════════════════════════════════════
static const ArgSpec STRIP_WIPE_ARGS[] = {
  { "color", ARG_COLOR, 0, 0, false, 0, nullptr, "target colour" },
};
static const FlagSpec STRIP_WIPE_FLAGS[] = {
  { "speed", 's', ARG_INT, 1, 200, 15, "ms per fade step" },
  { "steps", 'S', ARG_INT, 1, 100, 15, "fade steps per IC" },
};

class StripWipeCmd : public Command {
public:
  explicit StripWipeCmd(WS2805<WS2805_N_LEDS>& strip) : _strip(strip) {}
  const char* name()    const override { return "wipe"; }
  const char* summary() const override { return "fill IC by IC (blocks several seconds)"; }
  SHELL_ARGS(STRIP_WIPE_ARGS)
  SHELL_FLAGS(STRIP_WIPE_FLAGS)
  const char* example() const override { return "strip wipe #00ffff --speed 5"; }

  void run(const ParsedArgs& a, CmdContext& ctx) override {
    uint8_t r, g, b;
    a.color(0, r, g, b);
    uint16_t speed = (uint16_t)a.flagNum(0);  // default 15 if not given
    uint16_t steps = (uint16_t)a.flagNum(1);
    ctx.log("wiping strip...");
    _strip.wipeToColor(r, g, b, steps, speed);
  }
private:
  WS2805<WS2805_N_LEDS>& _strip;
};

// ════════════════════════════════════════════════════════════════════════════
// strip flicker [seconds]       — optional argument with a default
// ════════════════════════════════════════════════════════════════════════════
static const ArgSpec STRIP_FLICKER_ARGS[] = {
  { "seconds", ARG_INT, 1, 30, true, 4, nullptr, "how long" },   // optional, default 4
};

class StripFlickerCmd : public Command {
public:
  explicit StripFlickerCmd(WS2805<WS2805_N_LEDS>& strip) : _strip(strip) {}
  const char* name()    const override { return "flicker"; }
  const char* summary() const override { return "warm-white candle effect (blocks)"; }
  SHELL_ARGS(STRIP_FLICKER_ARGS)
  const char* example() const override { return "strip flicker 10"; }

  void run(const ParsedArgs& a, CmdContext& ctx) override {
    uint32_t sec = (uint32_t)a.num(0);        // 4 if not typed, a.has(0) tells you
    ctx.log("candle flicker for %lu s...", (unsigned long)sec);
    _strip.flickerBegin();
    unsigned long t0 = millis();
    while (millis() - t0 < sec * 1000UL) { _strip.flickerUpdate(); delay(40); }
    _strip.clear();
    _strip.show();
  }
private:
  WS2805<WS2805_N_LEDS>& _strip;
};

// ════════════════════════════════════════════════════════════════════════════
// strip white <warm|cool> [level]   — CHOICE: one word out of a fixed list
// ════════════════════════════════════════════════════════════════════════════
static const char* const WHITE_TONES[] = { "warm", "cool", nullptr };   // nullptr = end
static const ArgSpec STRIP_WHITE_ARGS[] = {
  { "tone",  ARG_CHOICE, 0, 0,   false, 0,   WHITE_TONES, "which white channel" },
  { "level", ARG_INT,    0, 255, true,  255, nullptr,     "brightness of that channel" },
};

class StripWhiteCmd : public Command {
public:
  explicit StripWhiteCmd(WS2805<WS2805_N_LEDS>& strip) : _strip(strip) {}
  const char* name()    const override { return "white"; }
  const char* summary() const override { return "warm or cool white channel"; }
  SHELL_ARGS(STRIP_WHITE_ARGS)
  const char* example() const override { return "strip white warm 128"; }

  void run(const ParsedArgs& a, CmdContext& ctx) override {
    uint8_t level = (uint8_t)a.num(1);
    if (a.num(0) == 0) _strip.setAllWW(level);   // index in WHITE_TONES: 0 = warm
    else               _strip.setAllCW(level);   //                       1 = cool
    ctx.log("strip %s white %u", a.word(0), level);
  }
private:
  WS2805<WS2805_N_LEDS>& _strip;
};

// ════════════════════════════════════════════════════════════════════════════
// strip clear                    — no arguments, no flags
// ════════════════════════════════════════════════════════════════════════════
class StripClearCmd : public Command {
public:
  explicit StripClearCmd(WS2805<WS2805_N_LEDS>& strip) : _strip(strip) {}
  const char* name()    const override { return "clear"; }
  const char* summary() const override { return "all ICs off"; }
  void run(const ParsedArgs&, CmdContext& ctx) override {
    _strip.clear();
    _strip.show();
    ctx.log("strip cleared");
  }
private:
  WS2805<WS2805_N_LEDS>& _strip;
};

// ════════════════════════════════════════════════════════════════════════════
// The group: "strip" holds all of the above
// ════════════════════════════════════════════════════════════════════════════
StripLedCmd     stripLed(Strip);       // Strip = the global from main.cpp
StripWipeCmd    stripWipe(Strip);
StripFlickerCmd stripFlicker(Strip);
StripWhiteCmd   stripWhite(Strip);
StripClearCmd   stripClear(Strip);

Command* const STRIP_CMDS[] = { &stripLed, &stripWipe, &stripFlicker, &stripWhite, &stripClear };
CommandGroup stripGroup("strip", "WS2805 RGBCCT strip (60 ICs)",
                        STRIP_CMDS, sizeof(STRIP_CMDS) / sizeof(STRIP_CMDS[0]),
                        "The 3 m 12 V WS2805 strip on D11.\n"
                        "Type 'strip <subcommand> -h' for one command.");

// → then in Commands.h:   #include "Commands/Cmd_Strip.h"   and   &stripGroup,
