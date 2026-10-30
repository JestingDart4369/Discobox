#pragma once
/*
  Cmd_Settings.h — `settings ...`: the same menu Settings mode's SP/DP/TP use
  (Settings_Items.h). Works from any mode.

    settings                     show the selected setting
    settings -p / --plus         bump the selected value (wraps at its max), saved
    settings -n / --next         select the next setting
    settings -b / --back         select the previous setting
    settings -l / --list         list all settings, marking the selected one

  Uses the globals from main.cpp: SettingsList, Buzzer, UISFX_HOVER.
*/
#include <Shell.h>

static void settingsShow(const ParsedArgs&, CmdContext& ctx) {
  ctx.out("setting %u/%u: %s selected   (-p plus, -n next, -b back, -l list)",
          (unsigned)(SettingsList.index() + 1), (unsigned)SettingsList.count(), SettingsList.current().name());
  SettingsList.current().Enter();               // logs the current value
}
static void settingsPlus(const ParsedArgs&, CmdContext&) {
  SettingsList.bump();                           // logs + saves itself
  Buzzer.playCue(SettingsList.current().cueForBump());
}
static void settingsNext(const ParsedArgs&, CmdContext&) {
  SettingsList.next();
  Buzzer.playCue(UISFX_HOVER);
}
static void settingsBack(const ParsedArgs&, CmdContext&) {
  SettingsList.previous();
  Buzzer.playCue(UISFX_HOVER);
}
static void settingsList(const ParsedArgs&, CmdContext& ctx) {
  for (uint8_t i = 0; i < SettingsList.count(); i++)
    ctx.out("  %u: %s%s", (unsigned)i, SettingsList.at(i).name(), (i == SettingsList.index()) ? "  <- selected" : "");
}

static const CommandDef D_SETTINGS_SHOW = { "show", "show the selected setting", nullptr,
  SHELL_NONE, SHELL_NONE, "settings", nullptr, settingsShow };
static const CommandDef D_SETTINGS_PLUS = { "plus", "bump the selected value (wraps), saved", "-p --plus",
  SHELL_NONE, SHELL_NONE, "settings -p", nullptr, settingsPlus };
static const CommandDef D_SETTINGS_NEXT = { "next", "select the next setting", "-n --next",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, settingsNext };
static const CommandDef D_SETTINGS_BACK = { "back", "select the previous setting", "-b --back",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, settingsBack };
static const CommandDef D_SETTINGS_LIST = { "list", "list all settings", "-l --list",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, settingsList };

FnCommand cmdSettingsShow(D_SETTINGS_SHOW), cmdSettingsPlus(D_SETTINGS_PLUS);
FnCommand cmdSettingsNext(D_SETTINGS_NEXT), cmdSettingsBack(D_SETTINGS_BACK), cmdSettingsList(D_SETTINGS_LIST);

Command* const SETTINGS_CMDS[] = { &cmdSettingsPlus, &cmdSettingsNext, &cmdSettingsBack, &cmdSettingsList };
CommandGroup settingsGroup("settings", "the Settings-mode menu, from any mode", SETTINGS_CMDS, SHELL_COUNT(SETTINGS_CMDS),
  "The same list Settings mode's SP/DP/TP use (ring/strip brightness,\n"
  "volume, mute, theme). Every change is saved to flash at once.",
  &cmdSettingsShow);
