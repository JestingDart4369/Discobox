# Serial shell — how to add commands

The Discobox is controlled over the USB-C cable with a small command shell (`lib/Shell`). It follows the [Command Line Interface Guidelines](https://clig.dev/#arguments-and-flags): standard flag names, a long form for every flag, help on every level, clear errors with suggestions.

This page explains where commands live, how to write one, which kinds of arguments and flags exist, and how to test it.

## Where everything is

- `lib/Shell/` is the shell itself. You normally never change it.
  - `Shell.h` is the only file you include. It pulls in the other two.
  - `ShellCore.h` contains input, tokenizer, argument specs, parser and the `Command` base class.
  - `ShellHelp.h` builds `help`, `-h` and `man` from the specs and checks them (`ShellCheck`).
- `include/Commands/Commands.h` is the list of all commands (the registry). Every new command is added here.
- `include/Commands/Cmd_Audio.h` has `audio buzzer ...` and `audio speaker ...`.
- `include/Commands/Cmd_Lights.h` has `lights button|strip|ring ...` and `lights clear`.
- `include/Commands/Cmd_Settings.h` has `settings ...`.
- `include/Commands/Cmd_System.h` has `sp dp tp hold mode status brightness`.
- `include/Commands/_Template.h` holds copy-paste examples written as classes. It is not compiled.
- `include/Serial_Handler.h` is the old if/else handler. It is no longer used (nothing includes it) and can be deleted.
- `src/main.cpp` creates the shell (`SerialShell`) and calls `SerialShell.update()` in `loop()`.

## What happens with one line

Take `lights strip -l 5 255 0 0 -d`:

1. The LineReader collects the bytes until Enter.
2. The Tokenizer splits the line into words: `lights`, `strip`, `-l`, `5`, `255`, `0`, `0`, `-d`.
3. The built-ins `help`, `man` and `version` are checked.
4. The command is looked up, by names AND aliases: `lights` → `strip` → `-l` (= `led`).
5. The global flags are taken out of the rest: `-d` turns debug on for this one call.
6. The ArgParser checks `5 255 0 0` against the command's specs. A wrong value stops here with an error, a usage line and an example.
7. `run()` is called with values that are already valid.

If step 4 finds nothing, you get `unknown command` with a suggestion for typos.

## Actions: `-p`, `--play` or `play`

Inside a group, every action has a word name and aliases, for example `play` with `-p --play`. All three do the same:

```
audio buzzer -p 2
audio buzzer --play 2
audio buzzer play 2
```

Aliases are checked right after the group, BEFORE the global flags. That is why `lights ring -d` means disco, `audio buzzer -q 2` means queue and `audio buzzer -th` means theme, while `-d`, `-q` and `-t` anywhere later in the line are still debug, quiet and time:

```
lights ring -d        disco
lights ring -d -d     disco, with debug output
-d lights ring -d     the same (global flags may also stand in front)
```

A group can have a default action that runs when nothing follows: `settings` shows the selected setting, `audio buzzer` and `audio speaker` show their status.

`help` and `man` find commands by name anywhere in the tree: `help buzzer` shows `audio buzzer`, `man ring` shows `lights ring`.

## Writing a command the short way (`FnCommand`)

All real commands are written as data plus one function, without a class:

```cpp
static const ArgSpec BUZZER_QUEUE_ARGS[] = { { "n", ARG_INT, 0, 255, false, 0, nullptr, "song number" } };

static void buzzerQueue(const ParsedArgs& a, CmdContext& ctx) {
  Buzzer.select((uint8_t)a.num(0));
}

static const CommandDef D_BUZZER_QUEUE = {
  "queue", "select song n without playing",   // name, summary
  "-q --queue",                                // aliases (nullptr = none)
  SHELL_LIST(BUZZER_QUEUE_ARGS),               // args   (SHELL_NONE = none)
  SHELL_NONE,                                  // flags  (SHELL_LIST(...) if any)
  "audio buzzer -q 1",                         // example (or nullptr)
  nullptr,                                     // man text (or nullptr)
  buzzerQueue                                  // the function
};
FnCommand cmdBuzzerQueue(D_BUZZER_QUEUE);
```

Then put `&cmdBuzzerQueue` into the group's list (`BUZZER_CMDS[]`). A group is `CommandGroup name("name", "summary", LIST, SHELL_COUNT(LIST), "man text", &defaultCommand)`, where the last two are optional.

The class style from `_Template.h` works too. Use it when a command needs its own state.

## Adding a command in 5 steps

1. **Create the file** `include/Commands/Cmd_<group>.h` (or open the existing one) and start it with `#pragma once` and `#include <Shell.h>`.
2. **Describe the input** with an `ArgSpec[]` table for positional arguments and a `FlagSpec[]` table for options. Make both `static const`, so they stay in flash and use no RAM.
3. **Write the class** by deriving from `Command` and filling in:
   - `name()`: the word you type, lowercase, no spaces
   - `summary()`: one line, shown in `help` and at the top of `-h`
   - `SHELL_ARGS(table)` and/or `SHELL_FLAGS(table)` to connect the tables
   - `example()`: optional but recommended, shown in `-h`, `man` and after errors
   - `manText()`: optional long description for `man`, lines separated by `\n`
   - `run(const ParsedArgs& a, CmdContext& ctx)`: does the work
4. **Create the object** and pass the hardware it needs into the constructor, e.g. `StripLedCmd stripLed(Strip);`. Then either put it into a group (`Command* const STRIP_CMDS[] = { &stripLed, ... };` plus `CommandGroup stripGroup(...)`) or, for a top-level command, use it directly.
5. **Register it** in `include/Commands/Commands.h`: `#include` the file under step 1, and add the top-level object (usually the group) to `SHELL_COMMANDS` before the `nullptr` end marker.

Then upload. The run and debug builds check every spec at boot and print `[ERROR]` lines if something is wrong.

## Positional arguments (`ArgSpec`)

Positional arguments are typed in a fixed order without a name: `strip led 5 255 0 0`.

```cpp
static const ArgSpec STRIP_LED_ARGS[] = {
  // name     type       min max optional def choices  help
  { "index", ARG_INT,    0,  59, false,   0,  nullptr, "IC number (0-based)" },
  { "color", ARG_COLOR,  0,  0,  false,   0,  nullptr, "r g b, r,g,b or #hex" },
};
```

The fields:

- `name` is shown as `<name>` in help, or `[name]` if optional.
- `type` is one of the types below.
- `min` and `max` are the allowed range (`ARG_INT` only). Anything outside is rejected before `run()`.
- `optional`: `true` means the user may leave it out. Optional arguments must come after the required ones.
- `def` is the value used when an optional `ARG_INT` is left out.
- `choices` is the list for `ARG_CHOICE`, ending with `nullptr`.
- `help` is one short line for `-h` and `man`. Required.

The types:

- `ARG_INT` is a whole number within `min..max`. Read it with `a.num(i)`.
- `ARG_COLOR` is a colour: `255 0 0` (three words), `255,0,0`, `#ff0000` or `ff0000`. Read it with `a.color(i, r, g, b)`.
- `ARG_WORD` is any single word, case kept. Read it with `a.word(i)`.
- `ARG_CHOICE` is one word from a fixed list, case-insensitive. Read the index with `a.num(i)` and the word with `a.word(i)`. A wrong word lists the allowed ones.

Use `a.has(i)` to see whether an optional argument was actually typed.

## Flags (`FlagSpec`)

Flags are named options that can stand anywhere in the line. Every flag has a long form and can have a one-letter short form. All of these do the same thing: `-s 30`, `-s30`, `--speed 30`, `--speed=30`.

```cpp
static const FlagSpec STRIP_WIPE_FLAGS[] = {
  // long    short type      min max  def help
  { "speed", 's',  ARG_INT,  1,  200, 15, "ms per fade step" },
  { "only",  'o',  ARG_NONE, 0,  0,   0,  "turn every other IC off first" },
  { "save",  '\0', ARG_NONE, 0,  0,   0,  "also store in flash" },   // long form only
};
```

The types for flags:

- `ARG_NONE` is a switch without a value (`--only`). Check it with `a.flagSet(i)`. Several switches combine: `-lr` is `-l -r`.
- `ARG_INT` takes a number within `min..max`. Read it with `a.flagNum(i)`, which returns `def` if the flag was not given.
- `ARG_WORD` takes one word. Read it with `a.flagWord(i)`.
- `ARG_COLOR` takes one colour word (`#ff0000` or `255,0,0`). Read it with `a.flagColor(i, r, g, b)`.

Rules for flags, following clig.dev (the boot check enforces them):

- Every flag needs a long name and a help text.
- Only give a short letter to flags that are used often. Rare ones get `'\0'`.
- The letters `h`, `d`, `q` and `t` belong to the global flags and cannot be used. `n` is free.
- Don't use `v`: it is ambiguous (verbose or version?).
- Use the standard names where they fit: `-f/--force` to skip a confirmation, `-a/--all`, `-o/--output`.
- Never pass a secret (a future Wi-Fi password) as a flag.

## Global flags (work with every command)

These are handled by the shell. Your command only reads them from `ctx` if it needs to.

- `-h`, `--help` shows the help for that command. It works even when appended to a broken line.
- `-d`, `--debug` makes `ctx.dbg(...)` print, for this one call.
- `-q`, `--quiet` silences `ctx.log(...)` and `Logger::log(...)`. Warnings and errors still show.
- `-dry`, `--dry-run` checks and prints the arguments, but does not call `run()`. `-dry` is a whole word: it is NOT read as `-d -r -y`.
- `-t`, `--time` prints how long `run()` took, in microseconds.
- `--version` prints the firmware version (no `-v`, see above).
- `--` ends flags: everything after it is an argument, even if it starts with `-`.

## Writing `run()`

`run()` only gets valid input, so there is no `atoi`, no range check and no "usage:" message inside it. Do the work and report through `ctx`:

- `ctx.log("...")` is normal output, silenced by `-q`.
- `ctx.dbg("...")` is extra detail, only shown with `-d`.
- `ctx.warn("...")` is something unexpected but okay. Always shown.
- `ctx.err("...")` is something that failed. Always shown.
- `ctx.out("...")` is plain text without a prefix, e.g. a list or table.

Everything uses printf syntax (`%d`, `%u`, `%s`). Each call prints one line of at most 128 characters.

Pass hardware objects into the constructor instead of using the globals inside `run()`. That keeps the command testable and makes it clear what it touches.

## Groups

A group is a command that only holds sub-commands, for example `strip` holds `led`, `wipe`, `clear`. Name things noun first, then verb: `strip clear`, `buzzer play`, `audio vol`.

```cpp
Command* const STRIP_CMDS[] = { &stripLed, &stripWipe, &stripClear };
CommandGroup stripGroup("strip", "WS2805 RGBCCT strip (60 ICs)",
                        STRIP_CMDS, sizeof(STRIP_CMDS) / sizeof(STRIP_CMDS[0]),
                        "optional man text");
```

Typing just `strip` or `strip -h` shows the group's help with all sub-commands. Groups can contain groups.

## Help and man pages

You never write help text by hand. It comes from the specs, `summary()`, `example()` and `manText()`:

- `help` lists all commands.
- `help strip led`, `strip led -h` and `strip led --help` show usage, arguments, options and the example.
- `man strip led` shows the full page: NAME, SYNOPSIS, DESCRIPTION, ARGUMENTS, OPTIONS, GLOBAL OPTIONS, EXAMPLE.
- `man flags` explains the global flags. `man colors` explains the colour formats.

Extra pages that are not commands go into `SHELL_TOPICS` in `Commands.h`.

## Testing a new command

1. Upload the debug or run build. The boot check prints `[ERROR]` lines for any wrong spec (range `min > max`, missing help, taken letter, ...).
2. Type `<command> -h` and read the generated help. If it is unclear, improve `summary()`, the `help` texts or `example()`.
3. Try it with `-dry` (dry-run) to see exactly which values the parser produced, without touching the hardware.
4. Try wrong input on purpose (a number out of range, a missing argument, a typo in a flag) and check that the error is understandable.
5. Run the laptop tests for the shell itself with `pio test -e native -f "test_shell_*"`.

## Limits

All memory is static, sized in `ShellConfig` at the top of `ShellCore.h`. Each value can be raised with a build flag, e.g. `-DSHELL_MAX_ARGS=8`:

- 96 characters per line (`SHELL_MAX_LINE`)
- 12 words per line (`SHELL_MAX_TOKENS`)
- 6 positional arguments per command (`SHELL_MAX_ARGS`)
- 6 own flags per command (`SHELL_MAX_FLAGS`)
- 128 characters per output line (`SHELL_OUT_BUF`)
