#pragma once
/*
  Cmd_Relay.h — `relay ...`: switch the relay from the console.

    relay                 show the state
    relay on              switch on
    relay off             switch off
    relay toggle          flip it

  Works from any mode. Party mode also switches the relay (on with the party,
  off when it stops). Uses the global from main.cpp: relay1.
*/
#include <Shell.h>

static const char* const RELAY_ACTIONS[] = { "on", "off", "toggle", nullptr };
static const ArgSpec RELAY_ARGS[] = {
  { "action", ARG_CHOICE, 0, 0, true, 0, RELAY_ACTIONS, "on, off or toggle (empty = show state)" },
};

static void relayCmd(const ParsedArgs& a, CmdContext& ctx) {
  if (a.has(0)) {
    switch (a.num(0)) {
      case 0: relay1.on();     break;
      case 1: relay1.off();    break;
      default: relay1.toggle(); break;
    }
  }
  if (!relay1.isEnabled()) ctx.out("relay is switched off in Settings (relay setting), it stays off");
  ctx.out("relay: %s (pin %u, %s)", relay1.isOn() ? "ON" : "off",
          (unsigned)relay1.pin(), relay1.activeLow() ? "active-low" : "active-high");
}

static const CommandDef D_RELAY = { "relay", "switch the relay: on | off | toggle", nullptr,
  SHELL_LIST(RELAY_ARGS), SHELL_NONE, "relay on",
  "One relay channel on D7 (e.g. the spinning disco light).\n"
  "Party mode switches it on with the party and off when it stops.\n"
  "No argument shows the state. If the relay does the opposite of what you\n"
  "type, flip the active-low flag in main.cpp (see doc/RELAY.md).",
  relayCmd };
FnCommand cmdRelay(D_RELAY);
