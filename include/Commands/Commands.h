#pragma once
/*
  Commands.h — the list of ALL shell commands (the registry).

  Included once in main.cpp, AFTER all hardware objects and Btn_SP & co.
  Full guide: doc/SHELL.md.   Copy-paste examples: include/Commands/_Template.h

    audio      Cmd_Audio.h     audio buzzer ... / audio speaker ...
    lights     Cmd_Lights.h    lights button|strip|ring ... / lights clear
    settings   Cmd_Settings.h  settings -p|-n|-b|-l
    sp dp tp hold mode status brightness    Cmd_System.h
    help / man / version       built into the shell

  To add a command: write it in one of these files (or a new Cmd_*.h),
  #include the file below and put the TOP-LEVEL object into SHELL_COMMANDS.
  With DEBUG_ENABLED the shell checks all specs at boot and prints [ERROR]
  lines if something is wrong.
*/
#include <Shell.h>

// ── step 1: command files ───────────────────────────────────────────────────
#include "Commands/Cmd_Audio.h"
#include "Commands/Cmd_Lights.h"
#include "Commands/Cmd_Settings.h"
#include "Commands/Cmd_System.h"

// ── step 2: top-level commands / groups (order = order in 'help') ───────────
Command* const SHELL_COMMANDS[] = {
  &audioGroup,
  &lightsGroup,
  &settingsGroup,
  &cmdBrightness,
  &cmdSP, &cmdDP, &cmdTP, &cmdHold,
  &cmdMode,
  &cmdStatus,
  nullptr          // end marker — keep it LAST (not counted)
};
const uint8_t SHELL_COMMAND_COUNT = sizeof(SHELL_COMMANDS) / sizeof(SHELL_COMMANDS[0]) - 1;

// ── pages that are not commands:  help <topic> / man <topic> ────────────────
const ManTopic SHELL_TOPICS[] = {
  { "gestures", "button gestures and how to simulate them",
    "The button knows four gestures. Each one does whatever the CURRENT\n"
    "mode assigned to it (see 'status' for the mode):\n"
    "  sp     single press\n"
    "  dp     double press  (two presses within 400 ms)\n"
    "  tp     triple press\n"
    "  hold   hold 3 s: always switches to the next mode (also: next)\n"
    "Type them in the console to simulate the button, with the same\n"
    "sound cue. 'mode N' jumps straight to mode N." },
  { "colors", "every way to write a colour",
    "Wherever a command takes <color> you can write:\n"
    "  255 0 0      three numbers (red green blue, 0..255)\n"
    "  255,0,0      the same with commas (spaces after commas are ok)\n"
    "  #ff0000      hex with #\n"
    "  ff0000       hex without #\n"
    "Numbers outside 0..255 are clamped. Upper/lower case doesn't matter." },
};
const uint8_t SHELL_TOPIC_COUNT = sizeof(SHELL_TOPICS) / sizeof(SHELL_TOPICS[0]);
