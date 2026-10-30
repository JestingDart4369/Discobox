#pragma once
/*
  ShellCore.h — the building blocks of the serial shell (lib/Shell).

  Sections, in order (each one has its own explanation below):
    ShellConfig   size limits (all static, no heap)
    ShellIO       interface to the outside world (Serial / fake in tests)
    ShellUtil     string helpers
    LineReader    bytes → complete lines (non-blocking)
    Tokenizer     line → argv (in place)
    ArgSpec       ArgSpec / FlagSpec / global flags — DESCRIBE a command
    CmdContext    one call: global flags + output (log/dbg/warn/err/out)
    GlobalFlags   take -h -d -q -t -dry --version out of argv
    ParsedArgs    checked values a command receives
    Command       base class + CommandGroup + ManTopic
    ArgParser     argv + specs → ParsedArgs

  No <Arduino.h> in here → the whole file is unit-tested on the laptop.
  Normally you include <Shell.h>, which includes this.
*/
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ColorParse.h>

// ════════════════════════════════════════════════════════════════════════════
// ShellConfig
// ════════════════════════════════════════════════════════════════════════════
/*
  ShellConfig.h — size limits for the serial shell (lib/Shell).

  Everything in the shell is static (no heap / new / std::string), so these
  numbers decide how much RAM it uses. Override any of them with a build
  flag in platformio.ini, e.g.   -DSHELL_MAX_LINE=128
*/

#ifndef SHELL_MAX_LINE
#define SHELL_MAX_LINE   96   // characters per input line
#endif
#ifndef SHELL_MAX_TOKENS
#define SHELL_MAX_TOKENS 12   // words per line
#endif
#ifndef SHELL_MAX_ARGS
#define SHELL_MAX_ARGS    6   // positional arguments per command
#endif
#ifndef SHELL_MAX_FLAGS
#define SHELL_MAX_FLAGS   6   // own options per command (global flags not counted)
#endif
#ifndef SHELL_MAX_PATH
#define SHELL_MAX_PATH   32   // "strip led" — command path used in messages
#endif
#ifndef SHELL_OUT_BUF
#define SHELL_OUT_BUF   128   // one output line (longer lines are cut)
#endif

// Lets the compiler check printf-style format strings (catches %d vs %s mistakes).
#if defined(__GNUC__)
  #define SHELL_PRINTF(fmtIdx, argIdx) __attribute__((format(printf, fmtIdx, argIdx)))
#else
  #define SHELL_PRINTF(fmtIdx, argIdx)
#endif

// ════════════════════════════════════════════════════════════════════════════
// ShellIO
// ════════════════════════════════════════════════════════════════════════════
/*
  ShellIO.h — the ONLY connection between the shell and the outside world.

  On the board:   ShellSerialIO  (Serial + micros() + Logger quiet switch)
  On the laptop:  FakeShellIO    (test/mocks — prepared input, captured output)

  Because everything goes through this interface, nothing else in lib/Shell
  includes <Arduino.h>, and the whole shell is unit-tested with `pio test -e native`.
*/

class ShellIO {
public:
  virtual ~ShellIO() {}

  /** @brief Number of input bytes waiting. */
  virtual int available() = 0;

  /** @brief Read one input byte, -1 if there is none. */
  virtual int read() = 0;

  /** @brief Write one complete output line (the implementation adds the newline). */
  virtual void writeLine(const char* line) = 0;

  /** @brief Microsecond clock for --time. Default: no clock (always 0). */
  virtual uint32_t micros() { return 0; }

  /** @brief Called with true before a --quiet command runs, false afterwards. */
  virtual void setQuiet(bool) {}
};

// ════════════════════════════════════════════════════════════════════════════
// ShellUtil
// ════════════════════════════════════════════════════════════════════════════
/*
  ShellUtil.h — small string helpers used by the shell (no Arduino, no heap).
*/

namespace ShellUtil {

  /** @brief ASCII lowercase of one character. */
  inline char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

  /** @brief Case-insensitive string compare (true if equal). nullptr never matches. */
  inline bool eqNoCase(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
      if (lower(*a) != lower(*b)) return false;
      a++; b++;
    }
    return *a == *b;
  }

  /**
   * @brief Strict integer parse: the WHOLE token must be a number ("12", "-5", "+3").
   * "12abc", "" and "abc" return false.
   */
  inline bool parseInt(const char* s, int32_t& out) {
    if (!s || !*s) return false;
    char* end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;
    out = (int32_t)v;
    return true;
  }

  /** @brief true if the token is a whole number. */
  inline bool isNumber(const char* s) { int32_t d; return parseInt(s, d); }

  /**
   * @brief Edit distance (Levenshtein, case-insensitive) — how many single-letter
   * changes turn a into b. Used for "did you mean ...?". Words over 15 chars → 99.
   */
  inline uint8_t distance(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la > 15 || lb > 15) return 99;
    uint8_t prev[16], cur[16];
    for (uint8_t j = 0; j <= lb; j++) prev[j] = j;
    for (uint8_t i = 1; i <= la; i++) {
      cur[0] = i;
      for (uint8_t j = 1; j <= lb; j++) {
        uint8_t cost = (lower(a[i - 1]) == lower(b[j - 1])) ? 0 : 1;
        uint8_t d = prev[j] + 1;                       // delete
        if (cur[j - 1] + 1 < d) d = cur[j - 1] + 1;    // insert
        if (prev[j - 1] + cost < d) d = prev[j - 1] + cost;   // replace
        cur[j] = d;
      }
      memcpy(prev, cur, lb + 1);
    }
    return prev[lb];
  }

  /** @brief true if `typed` is close enough to `known` to suggest it (1-2 typos). */
  inline bool looksLike(const char* typed, const char* known) {
    uint8_t d = distance(typed, known);
    return d > 0 && d <= 2 && d < strlen(known);
  }

} // namespace ShellUtil

// ════════════════════════════════════════════════════════════════════════════
// LineReader
// ════════════════════════════════════════════════════════════════════════════
/*
  LineReader.h — collect input bytes into complete lines (non-blocking).

  poll() only handles bytes that have ALREADY arrived and returns at once,
  so music and animations keep running while you type.

  - '\r' is ignored (Windows sends "\r\n"), '\n' ends the line
  - Backspace / Delete remove the last character, tab counts as a space
  - other control characters are dropped
  - a line longer than MAX_LEN is dropped COMPLETELY (not cut off — a cut-off
    command could do something different); takeOverflow() reports it once
*/

class LineReader {
public:
  static const uint16_t MAX_LEN = SHELL_MAX_LINE;

  explicit LineReader(ShellIO& io) : _io(io) {}

  /**
   * @brief Call every loop(). Returns the finished line once Enter arrives,
   * otherwise nullptr. The line stays valid until the next poll().
   */
  char* poll() {
    while (_io.available() > 0) {
      int v = _io.read();
      if (v < 0) break;
      char c = (char)v;

      if (c == '\r') continue;
      if (c == '\n') {
        char* line = finish();
        if (line) return line;                 // empty / too long: keep reading
        continue;
      }
      if (c == '\b' || c == 0x7F) {            // Backspace / Delete
        if (_len > 0) _len--;
        continue;
      }
      if (c == '\t') c = ' ';
      if ((unsigned char)c < 0x20) continue;   // other control characters

      if (_len < MAX_LEN) _buf[_len++] = c;
      else _overflow = true;                   // too long: remember, keep reading to '\n'
    }
    return nullptr;
  }

  /** @brief true ONCE after a too-long line was dropped. */
  bool takeOverflow() {
    bool o = _overflowSeen;
    _overflowSeen = false;
    return o;
  }

private:
  ShellIO& _io;
  char     _buf[MAX_LEN + 1];                  // +1 for '\0'
  uint16_t _len = 0;
  bool     _overflow = false;                  // current line is too long
  bool     _overflowSeen = false;              // a line was dropped, not yet reported

  char* finish() {
    _buf[_len] = '\0';
    bool tooLong = _overflow;
    uint16_t len = _len;
    _len = 0;
    _overflow = false;
    if (tooLong) { _overflowSeen = true; return nullptr; }
    if (len == 0) return nullptr;
    return _buf;
  }
};

// ════════════════════════════════════════════════════════════════════════════
// Tokenizer
// ════════════════════════════════════════════════════════════════════════════
/*
  Tokenizer.h — split a line into words (argv), IN PLACE.

  No copying: the first separator after every word is overwritten with '\0'
  and argv[i] points into the original line. "quoted text" becomes ONE word
  (without the quotes), e.g. for names with spaces.

    "strip  led 5 -d"   ->  argc = 4, argv = {"strip", "led", "5", "-d"}
*/

class Tokenizer {
public:
  static const uint8_t MAX_TOKENS = SHELL_MAX_TOKENS;

  enum Result { OK, TOO_MANY, UNCLOSED_QUOTE };

  /**
   * @brief Split `line` at spaces/tabs into argv[] (modifies `line`).
   * @param argv  array with room for MAX_TOKENS pointers
   * @param argc  out: number of words
   */
  static Result split(char* line, char* argv[], uint8_t& argc) {
    argc = 0;
    char* p = line;
    while (true) {
      while (*p == ' ' || *p == '\t') p++;     // skip separators
      if (*p == '\0') return OK;
      if (argc >= MAX_TOKENS) return TOO_MANY;

      if (*p == '"') {                         // "quoted token"
        p++;
        argv[argc++] = p;
        while (*p && *p != '"') p++;
        if (*p != '"') return UNCLOSED_QUOTE;
        *p++ = '\0';
      } else {                                 // normal token
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
      }
    }
  }
};

// ════════════════════════════════════════════════════════════════════════════
// ArgSpec
// ════════════════════════════════════════════════════════════════════════════
/*
  ArgSpec.h — DESCRIBE what a command accepts (pure data, no logic).

  The ArgParser checks input against these tables, the HelpPrinter builds
  -h / --help / man from them. Declare them `const` → they stay in flash.

    // strip led <index> <color>
    const ArgSpec STRIP_LED_ARGS[] = {
      // name     type       min max optional def choices  help
      { "index", ARG_INT,    0, 59, false,   0, nullptr, "IC number (0-based)" },
      { "color", ARG_COLOR,  0,  0, false,   0, nullptr, "r g b, r,g,b or #hex" },
    };
    // [-s|--speed 1..200]
    const FlagSpec STRIP_WIPE_FLAGS[] = {
      // long     short type     min  max def  help
      { "speed", 's',  ARG_INT,  1,  200, 15, "ms per step" },
    };
*/

enum ArgType : uint8_t {
  ARG_NONE,    // flag without a value (a switch): --loop
  ARG_INT,     // whole number with a range: 0..59
  ARG_COLOR,   // colour: "255 0 0" (3 words) or "255,0,0" / "#ff0000" / "ff0000" (1 word)
  ARG_WORD,    // any single word: a cue name like "press"
  ARG_CHOICE,  // one word out of a fixed list: "arcade" | "soft" | ...
};

/// A positional argument — given in a fixed order, without a name in front.
struct ArgSpec {
  const char*        name;      // "index"  → shown as <index> (or [index] if optional)
  ArgType            type;
  int16_t            min, max;  // ARG_INT only
  bool               optional;
  int16_t            def;       // ARG_INT: value used when an optional arg is missing
  const char* const* choices;   // ARG_CHOICE only: nullptr-terminated list
  const char*        help;
};

/// A named option:  -s 30  /  -s30  /  --speed 30  /  --speed=30
struct FlagSpec {
  const char* longName;         // "speed" → --speed
  char        shortName;        // 's'     → -s   ('\0' = long form only)
  ArgType     type;             // ARG_NONE = switch, ARG_INT / ARG_WORD / ARG_COLOR = takes a value
  int16_t     min, max;         // ARG_INT only
  int16_t     def;              // ARG_INT: value when the flag is not given
  const char* help;
};

// ── Global flags: work with EVERY command, handled by the shell itself ──────
// Names follow https://clig.dev/#arguments-and-flags (standard names, a long
// form for every flag, no -v because it is ambiguous between verbose/version).
enum GlobalFlag : uint8_t { GF_HELP, GF_DEBUG, GF_QUIET, GF_DRYRUN, GF_TIME, GF_VERSION, GF_COUNT };

static const FlagSpec SHELL_GLOBAL_FLAGS[GF_COUNT] = {
  { "help",    'h', ARG_NONE, 0, 0, 0, "show help for this command" },
  { "debug",   'd', ARG_NONE, 0, 0, 0, "extra [DEBUG] output for this call" },
  { "quiet",   'q', ARG_NONE, 0, 0, 0, "no [LOG] output, only warnings and errors" },
  { "dry-run", '\0', ARG_NONE, 0, 0, 0, "check the arguments, change nothing" },   // short form: -dry (below)
  { "time",    't', ARG_NONE, 0, 0, 0, "print how long the command took" },
  { "version", '\0', ARG_NONE, 0, 0, 0, "print the firmware version" },   // no -v: ambiguous (clig.dev)
};

/// Single-dash WORD forms of global flags. Checked BEFORE letters are combined,
/// so "-dry" means dry-run and not -d -r -y. Only an EXACT match counts.
struct GlobalAlias { const char* word; GlobalFlag flag; };
static const GlobalAlias SHELL_GLOBAL_ALIASES[] = {
  { "dry", GF_DRYRUN },          // -dry  =  --dry-run
};
static const uint8_t SHELL_GLOBAL_ALIAS_COUNT = sizeof(SHELL_GLOBAL_ALIASES) / sizeof(SHELL_GLOBAL_ALIASES[0]);

/** @brief Global flag for a single-dash word like "dry" (without '-'), or -1. */
inline int8_t globalFlagByAlias(const char* word) {
  for (uint8_t i = 0; i < SHELL_GLOBAL_ALIAS_COUNT; i++)
    if (ShellUtil::eqNoCase(SHELL_GLOBAL_ALIASES[i].word, word)) return (int8_t)SHELL_GLOBAL_ALIASES[i].flag;
  return -1;
}

/** @brief The single-dash word of a global flag ("dry"), or nullptr. */
inline const char* globalAliasOf(uint8_t gf) {
  for (uint8_t i = 0; i < SHELL_GLOBAL_ALIAS_COUNT; i++)
    if (SHELL_GLOBAL_ALIASES[i].flag == gf) return SHELL_GLOBAL_ALIASES[i].word;
  return nullptr;
}

/** @brief Index of the global flag with this short letter, or -1. Case-sensitive. */
inline int8_t globalFlagByShort(char c) {
  if (c == '\0') return -1;
  for (uint8_t i = 0; i < GF_COUNT; i++)
    if (SHELL_GLOBAL_FLAGS[i].shortName == c) return (int8_t)i;
  return -1;
}

/** @brief Index of the global flag with this long name (without "--"), or -1. */
inline int8_t globalFlagByLong(const char* name) {
  for (uint8_t i = 0; i < GF_COUNT; i++)
    if (ShellUtil::eqNoCase(SHELL_GLOBAL_FLAGS[i].longName, name)) return (int8_t)i;
  return -1;
}

// ════════════════════════════════════════════════════════════════════════════
// CmdContext
// ════════════════════════════════════════════════════════════════════════════
/*
  CmdContext.h — everything that belongs to ONE call of a command.

  Created fresh for every input line, so `-d` only affects that one command.
  Commands write their output through it and never touch Serial directly:

    ctx.log("strip -> %d %d %d", r, g, b);   // [LOG]    silent with -q
    ctx.dbg("raw value %d", v);              // [DEBUG]  only with -d
    ctx.warn("blocked, stop party first");   // [WARN]   always
    ctx.err("IC %d not found", n);           // [ERROR]  always
    ctx.out("plain text");                   //          always, no prefix (help pages)
*/

struct CmdContext {
  bool     flag[GF_COUNT];        // global flags, index = GlobalFlag
  ShellIO* io;                    // where output goes
  char     path[SHELL_MAX_PATH];  // resolved command, e.g. "strip led"

  explicit CmdContext(ShellIO* out = nullptr) : io(out) {
    for (uint8_t i = 0; i < GF_COUNT; i++) flag[i] = false;
    path[0] = '\0';
  }

  // ── global flags ──
  bool help()    const { return flag[GF_HELP]; }
  bool debug()   const { return flag[GF_DEBUG]; }
  bool quiet()   const { return flag[GF_QUIET]; }
  bool dryRun()  const { return flag[GF_DRYRUN]; }
  bool timed()   const { return flag[GF_TIME]; }
  bool version() const { return flag[GF_VERSION]; }

  /** @brief Add one word to the command path ("strip" → "strip led"). */
  void appendPath(const char* name) {
    size_t n = strlen(path);
    if (n + 1 >= sizeof(path)) return;
    snprintf(path + n, sizeof(path) - n, n ? " %s" : "%s", name);
  }

  // ── output ──
  SHELL_PRINTF(2, 3) void out(const char* fmt, ...) const {
    va_list a; va_start(a, fmt); emit("", fmt, a); va_end(a);
  }
  SHELL_PRINTF(2, 3) void log(const char* fmt, ...) const {
    if (quiet()) return;
    va_list a; va_start(a, fmt); emit("[LOG]   ", fmt, a); va_end(a);
  }
  SHELL_PRINTF(2, 3) void dbg(const char* fmt, ...) const {
    if (!debug()) return;
    va_list a; va_start(a, fmt); emit("[DEBUG] ", fmt, a); va_end(a);
  }
  SHELL_PRINTF(2, 3) void warn(const char* fmt, ...) const {
    va_list a; va_start(a, fmt); emit("[WARN]  ", fmt, a); va_end(a);
  }
  SHELL_PRINTF(2, 3) void err(const char* fmt, ...) const {
    va_list a; va_start(a, fmt); emit("[ERROR] ", fmt, a); va_end(a);
  }

private:
  void emit(const char* prefix, const char* fmt, va_list a) const {
    if (!io) return;
    char buf[SHELL_OUT_BUF];
    size_t p = strlen(prefix);
    if (p >= sizeof(buf)) p = 0;
    memcpy(buf, prefix, p);
    vsnprintf(buf + p, sizeof(buf) - p, fmt, a);
    io->writeLine(buf);
  }
};

// ════════════════════════════════════════════════════════════════════════════
// GlobalFlags
// ════════════════════════════════════════════════════════════════════════════
/*
  GlobalFlags.h — take the global flags (-h -d -q -t -dry, --version and their --long
  forms) out of argv BEFORE the command is looked up, and set them in the
  CmdContext. Everything else is left in argv, in order.

    "strip wipe red -dq --speed 30"  ->  argv = {strip, wipe, red, --speed, 30}
                                          ctx: debug + quiet

  Rules:
    -d, --debug        global → removed
    -dq                every letter global → removed (both set)
    -dry               exact alias word → dry-run (NOT -d -r -y)
    -ds                's' is not global → left for the ArgParser
    -5                 a number, not a flag → left
    --speed            not a global name → left
    --                 end of flags: kept, nothing after it is touched
  The flags themselves are listed once, in SHELL_GLOBAL_FLAGS (ArgSpec.h).
*/

class GlobalFlags {
public:
  static void extract(char* argv[], uint8_t& argc, CmdContext& ctx) {
    uint8_t out = 0;
    bool stop = false;
    for (uint8_t i = 0; i < argc; i++) {
      char* t = argv[i];
      if (!stop) {
        if (strcmp(t, "--") == 0) { stop = true; argv[out++] = t; continue; }
        if (applyToken(t, ctx)) continue;
      }
      argv[out++] = t;
    }
    argc = out;
  }

  /** @brief If `t` is a global flag (any form), set it in ctx and return true. */
  static bool applyToken(const char* t, CmdContext& ctx) {
    return applyLong(t, ctx) || applyAlias(t, ctx) || applyShort(t, ctx);
  }

private:
  static bool applyLong(const char* t, CmdContext& ctx) {
    if (t[0] != '-' || t[1] != '-' || t[2] == '\0') return false;
    int8_t g = globalFlagByLong(t + 2);
    if (g < 0) return false;
    ctx.flag[g] = true;
    return true;
  }

  /** @brief "-dry": a whole single-dash word that is a global alias. */
  static bool applyAlias(const char* t, CmdContext& ctx) {
    if (t[0] != '-' || t[1] == '-' || t[1] == '\0') return false;
    int8_t g = globalFlagByAlias(t + 1);
    if (g < 0) return false;
    ctx.flag[g] = true;
    return true;
  }

  static bool applyShort(const char* t, CmdContext& ctx) {
    if (t[0] != '-') return false;
    char first = t[1];
    bool letter = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z');
    if (!letter) return false;                       // "-", "--x", "-5"
    for (const char* p = t + 1; *p; p++)
      if (globalFlagByShort(*p) < 0) return false;   // one non-global letter → not ours
    for (const char* p = t + 1; *p; p++)
      ctx.flag[globalFlagByShort(*p)] = true;
    return true;
  }
};

// ════════════════════════════════════════════════════════════════════════════
// ParsedArgs
// ════════════════════════════════════════════════════════════════════════════
/*
  ParsedArgs.h — the checked values a command receives in run().

  Index = position in the command's ArgSpec / FlagSpec table. Everything is
  already validated (types, ranges, choices), so run() never needs atoi or
  range checks:

    void run(const ParsedArgs& a, CmdContext& ctx) override {
      uint8_t n = a.num(0);                       // <index>
      uint8_t r, g, b; a.color(1, r, g, b);       // <color>
      int speed = a.flagNum(0);                   // --speed (its default if not given)
      if (a.flagSet(1)) ...                       // --loop given?
    }
*/

struct ArgValue {
  bool        given;   // typed by the user (false = default / not given)
  int32_t     i;       // ARG_INT value, ARG_CHOICE index
  uint8_t     r, g, b; // ARG_COLOR
  const char* s;       // ARG_WORD / ARG_CHOICE text (points into the input line)
};

struct ParsedArgs {
  ArgValue arg[SHELL_MAX_ARGS];
  ArgValue flag[SHELL_MAX_FLAGS];
  uint8_t  argCount;
  uint8_t  flagCount;

  ParsedArgs() { clear(); }

  void clear() {
    for (uint8_t k = 0; k < SHELL_MAX_ARGS; k++)  arg[k]  = ArgValue{false, 0, 0, 0, 0, nullptr};
    for (uint8_t k = 0; k < SHELL_MAX_FLAGS; k++) flag[k] = ArgValue{false, 0, 0, 0, 0, nullptr};
    argCount = 0;
    flagCount = 0;
  }

  // ── positional arguments ──
  bool        has(uint8_t n)  const { return n < argCount && arg[n].given; }
  int32_t     num(uint8_t n)  const { return n < argCount ? arg[n].i : 0; }
  const char* word(uint8_t n) const { return n < argCount ? arg[n].s : nullptr; }
  void color(uint8_t n, uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (n >= argCount) { r = g = b = 0; return; }
    r = arg[n].r; g = arg[n].g; b = arg[n].b;
  }

  // ── flags ──
  bool        flagSet(uint8_t n)  const { return n < flagCount && flag[n].given; }
  int32_t     flagNum(uint8_t n)  const { return n < flagCount ? flag[n].i : 0; }
  const char* flagWord(uint8_t n) const { return n < flagCount ? flag[n].s : nullptr; }
  void flagColor(uint8_t n, uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (n >= flagCount) { r = g = b = 0; return; }
    r = flag[n].r; g = flag[n].g; b = flag[n].b;
  }
};

// ════════════════════════════════════════════════════════════════════════════
// Command
// ════════════════════════════════════════════════════════════════════════════
/*
  Command.h — base class for every shell command, plus CommandGroup.

  A command DESCRIBES itself (name, summary, args, flags, example, man text)
  and DOES its job in run(). The shell finds it, checks the input against
  its specs, and only calls run() with valid values.

    class StripLedCmd : public Command {
    public:
      const char* name()    const override { return "led"; }
      const char* summary() const override { return "set ONE IC, others unchanged"; }
      SHELL_ARGS(STRIP_LED_ARGS)
      const char* example() const override { return "strip led 5 255 0 0"; }
      void run(const ParsedArgs& a, CmdContext& ctx) override { ... }
    };

  CommandGroup = a command that only holds sub-commands (Composite pattern):
    Command* const STRIP_CMDS[] = { &stripLed, &stripClear };
    CommandGroup stripGroup("strip", "WS2805 strip", STRIP_CMDS, 2);
  Groups can be nested to any depth.
*/

// Helpers so a command can point at its tables in one line.
#define SHELL_ARGS(table) \
  const ArgSpec* args(uint8_t& n) const override { n = sizeof(table) / sizeof(table[0]); return table; }
#define SHELL_FLAGS(table) \
  const FlagSpec* flags(uint8_t& n) const override { n = sizeof(table) / sizeof(table[0]); return table; }

class Command {
public:
  virtual ~Command() {}

  /** @brief Word the user types, lowercase, e.g. "led". */
  virtual const char* name() const = 0;
  /** @brief One line for `help` and the top of `-h`. */
  virtual const char* summary() const = 0;

  /** @brief Positional arguments (default: none). */
  virtual const ArgSpec* args(uint8_t& n) const { n = 0; return nullptr; }
  /** @brief Own options (default: none). Global flags are added by the shell. */
  virtual const FlagSpec* flags(uint8_t& n) const { n = 0; return nullptr; }
  /** @brief Example line shown in -h and man (optional). */
  virtual const char* example() const { return nullptr; }
  /** @brief Long DESCRIPTION for `man`, lines separated by '\n' (optional). */
  virtual const char* manText() const { return nullptr; }
  /** @brief Sub-commands (default: none — only CommandGroup has them). */
  virtual Command* const* children(uint8_t& n) const { n = 0; return nullptr; }
  /**
   * @brief Other ways to type this sub-command, space-separated, e.g. "-p --play".
   * Matched only where the command name is expected (right after its group),
   * so "-d" can mean "disco" there and still mean --debug everywhere else.
   */
  virtual const char* aliases() const { return nullptr; }
  /** @brief Group only: what runs when the group is typed without a sub-command. */
  virtual Command* defaultCommand() const { return nullptr; }

  /** @brief Do the work. Only called with input that passed all checks. */
  virtual void run(const ParsedArgs& args, CmdContext& ctx) = 0;

  bool isGroup() const { uint8_t n; children(n); return n > 0; }

  /** @brief true if `word` is this command's name or one of its aliases. */
  bool matches(const char* word) const {
    if (ShellUtil::eqNoCase(name(), word)) return true;
    const char* a = aliases();
    if (!a) return false;
    size_t wl = strlen(word);
    while (*a) {
      while (*a == ' ') a++;
      const char* e = a;
      while (*e && *e != ' ') e++;
      if ((size_t)(e - a) == wl && wl > 0) {
        bool same = true;
        for (size_t k = 0; k < wl && same; k++) same = ShellUtil::lower(a[k]) == ShellUtil::lower(word[k]);
        if (same) return true;
      }
      a = e;
    }
    return false;
  }

  /** @brief Sub-command by name or alias (case-insensitive), or nullptr. */
  Command* findChild(const char* word) const {
    uint8_t n;
    Command* const* kids = children(n);
    for (uint8_t i = 0; i < n; i++)
      if (kids[i] && kids[i]->matches(word)) return kids[i];
    return nullptr;
  }

  /** @brief true if run() needs at least one argument. */
  bool needsArgs() const {
    uint8_t na;
    const ArgSpec* as = args(na);
    for (uint8_t k = 0; k < na; k++) if (!as[k].optional) return true;
    return false;
  }
};

/// A command that only holds sub-commands: "strip" → "strip led", "strip clear", ...
/// Typed without a sub-command, the shell runs `def` (if given) or shows the help.
class CommandGroup : public Command {
public:
  CommandGroup(const char* name, const char* summary,
               Command* const* kids, uint8_t count,
               const char* man = nullptr, Command* def = nullptr)
    : _name(name), _summary(summary), _kids(kids), _count(count), _man(man), _def(def) {}

  const char* name()    const override { return _name; }
  const char* summary() const override { return _summary; }
  const char* manText() const override { return _man; }
  Command* const* children(uint8_t& n) const override { n = _count; return _kids; }
  Command* defaultCommand() const override { return _def; }
  void run(const ParsedArgs&, CmdContext&) override {}   // the shell runs _def or prints help

private:
  const char*     _name;
  const char*     _summary;
  Command* const* _kids;
  uint8_t         _count;
  const char*     _man;
  Command*        _def;
};

// ── FnCommand: a command described by DATA + one plain function ─────────────
/*
  The short way to write a command — no class needed:

    static void buzzerPlay(const ParsedArgs& a, CmdContext& ctx) { ... }
    static const CommandDef BUZZER_PLAY = {
      "play", "play a song",                  // name, summary
      "-p --play",                            // aliases ("" or nullptr = none)
      SHELL_LIST(BUZZER_PLAY_ARGS),           // positional args  (or SHELL_NONE)
      SHELL_NONE,                             // flags            (or SHELL_LIST(...))
      "audio buzzer -p 2",                    // example          (or nullptr)
      nullptr,                                // man text         (or nullptr)
      buzzerPlay                              // the function
    };
    FnCommand buzzerPlayCmd(BUZZER_PLAY);
*/
#define SHELL_LIST(arr) arr, (uint8_t)(sizeof(arr) / sizeof(arr[0]))
#define SHELL_NONE      nullptr, 0
#define SHELL_COUNT(arr) (uint8_t)(sizeof(arr) / sizeof(arr[0]))

typedef void (*CommandFn)(const ParsedArgs& args, CmdContext& ctx);

struct CommandDef {
  const char*     name;
  const char*     summary;
  const char*     aliases;
  const ArgSpec*  args;   uint8_t nArgs;
  const FlagSpec* flags;  uint8_t nFlags;
  const char*     example;
  const char*     man;
  CommandFn       fn;
};

class FnCommand : public Command {
public:
  explicit FnCommand(const CommandDef& def) : _d(def) {}
  const char* name()    const override { return _d.name; }
  const char* summary() const override { return _d.summary; }
  const char* aliases() const override { return _d.aliases; }
  const ArgSpec*  args(uint8_t& n)  const override { n = _d.nArgs;  return _d.args; }
  const FlagSpec* flags(uint8_t& n) const override { n = _d.nFlags; return _d.flags; }
  const char* example() const override { return _d.example; }
  const char* manText() const override { return _d.man; }
  void run(const ParsedArgs& a, CmdContext& ctx) override { if (_d.fn) _d.fn(a, ctx); }
private:
  const CommandDef& _d;
};

/// A `man` page that is not a command, e.g. `man colors`.
struct ManTopic {
  const char* name;
  const char* summary;
  const char* text;     // lines separated by '\n'
};

// ════════════════════════════════════════════════════════════════════════════
// ArgParser
// ════════════════════════════════════════════════════════════════════════════
/*
  ArgParser.h — check argv against a command's ArgSpec/FlagSpec and fill ParsedArgs.

  Gets argv AFTER the command path, e.g. for "strip led 5 255 0 0 -s 30":
    argv = {"5", "255", "0", "0", "-s", "30"}

  Understands:
    positional args in order          5  255 0 0
    --long value / --long=value        --speed 30   --speed=30
    -s value / -s30                    -s 30        -s30
    combined switches                  -lr   (= -l -r, only flags without a value)
    global letters inside a group      -ds 30  (= -d + -s 30)
    negative numbers are NOT flags     -5
    --                                 everything after it is an argument
  On any mistake it prints ONE clear error via ctx.err() and returns false.
*/

class ArgParser {
public:
  /** @brief Parse for a command (uses its args() and flags()). */
  static bool parse(char* argv[], uint8_t argc, const Command& cmd, ParsedArgs& out, CmdContext& ctx) {
    uint8_t na, nf;
    const ArgSpec*  as = cmd.args(na);
    const FlagSpec* fs = cmd.flags(nf);
    return parse(argv, argc, as, na, fs, nf, out, ctx);
  }

  /** @brief Parse against explicit tables. */
  static bool parse(char* argv[], uint8_t argc,
                    const ArgSpec* as, uint8_t na, const FlagSpec* fs, uint8_t nf,
                    ParsedArgs& out, CmdContext& ctx) {
    out.clear();
    if (na > SHELL_MAX_ARGS || nf > SHELL_MAX_FLAGS) {
      ctx.err("%s: declares too many args/flags (see ShellConfig.h)", ctx.path);
      return false;
    }
    out.argCount  = na;
    out.flagCount = nf;
    for (uint8_t k = 0; k < na; k++) out.arg[k].i  = as[k].def;
    for (uint8_t k = 0; k < nf; k++) out.flag[k].i = fs[k].def;

    uint8_t pos = 0;
    bool endOfFlags = false;
    for (uint8_t i = 0; i < argc; i++) {
      char* t = argv[i];
      if (!endOfFlags && strcmp(t, "--") == 0) { endOfFlags = true; continue; }
      if (!endOfFlags && isFlag(t)) {
        if (!parseFlag(argv, argc, i, fs, nf, out, ctx)) return false;
        continue;
      }
      if (pos >= na) {
        ctx.err("%s: unexpected argument '%s'", ctx.path, t);
        return false;
      }
      if (!parsePositional(argv, argc, i, as[pos], out.arg[pos], ctx)) return false;
      pos++;
    }
    for (; pos < na; pos++) {
      if (!as[pos].optional) {
        ctx.err("%s: missing <%s>", ctx.path, as[pos].name);
        return false;
      }
    }
    return true;
  }

  /** @brief "-x..." or "--x..." is a flag; "-5", "-" and "--" are not. */
  static bool isFlag(const char* t) {
    if (t[0] != '-') return false;
    if (t[1] == '-') return t[2] != '\0';
    return (t[1] >= 'a' && t[1] <= 'z') || (t[1] >= 'A' && t[1] <= 'Z');
  }

private:
  // ── flags ─────────────────────────────────────────────────────────────────
  static bool parseFlag(char* argv[], uint8_t argc, uint8_t& i,
                        const FlagSpec* fs, uint8_t nf, ParsedArgs& out, CmdContext& ctx) {
    char* t = argv[i];

    if (t[1] == '-') {                                   // --long or --long=value
      char* name  = t + 2;
      char* value = strchr(name, '=');
      if (value) *value++ = '\0';
      int8_t f = findLong(fs, nf, name);
      if (f < 0) {
        int8_t g = globalFlagByLong(name);
        if (g >= 0 && !value) { ctx.flag[g] = true; return true; }
        unknownLong(name, fs, nf, ctx);
        return false;
      }
      return takeValue(fs[f], out.flag[f], value, argv, argc, i, ctx);
    }

    for (char* p = t + 1; *p; p++) {                     // -abc / -s 30 / -s30
      int8_t f = findShort(fs, nf, *p);
      if (f < 0) {
        int8_t g = globalFlagByShort(*p);
        if (g >= 0) { ctx.flag[g] = true; continue; }
        ctx.err("%s: unknown option -%c", ctx.path, *p);
        return false;
      }
      if (fs[f].type == ARG_NONE) { out.flag[f].given = true; continue; }
      char* value = (p[1] != '\0') ? p + 1 : nullptr;   // -s30 → value "30"
      return takeValue(fs[f], out.flag[f], value, argv, argc, i, ctx);
    }
    return true;
  }

  static bool takeValue(const FlagSpec& f, ArgValue& v, char* value,
                        char* argv[], uint8_t argc, uint8_t& i, CmdContext& ctx) {
    char label[24];
    snprintf(label, sizeof(label), "--%s", f.longName);
    if (f.type == ARG_NONE) {
      if (value) { ctx.err("%s: %s takes no value", ctx.path, label); return false; }
      v.given = true;
      return true;
    }
    if (!value) {
      if (i + 1 >= argc) {
        if (f.type == ARG_INT) ctx.err("%s: %s needs a value (%d..%d)", ctx.path, label, f.min, f.max);
        else                   ctx.err("%s: %s needs a value", ctx.path, label);
        return false;
      }
      value = argv[++i];
    }
    if (!convert(label, f.type, f.min, f.max, nullptr, value, v, ctx)) return false;
    v.given = true;
    return true;
  }

  static int8_t findLong(const FlagSpec* fs, uint8_t nf, const char* name) {
    for (uint8_t k = 0; k < nf; k++)
      if (ShellUtil::eqNoCase(fs[k].longName, name)) return (int8_t)k;
    return -1;
  }
  static int8_t findShort(const FlagSpec* fs, uint8_t nf, char c) {
    if (c == '\0') return -1;
    for (uint8_t k = 0; k < nf; k++)
      if (fs[k].shortName == c) return (int8_t)k;
    return -1;
  }

  static void unknownLong(const char* name, const FlagSpec* fs, uint8_t nf, CmdContext& ctx) {
    const char* best = nullptr;
    for (uint8_t k = 0; k < nf && !best; k++)
      if (ShellUtil::looksLike(name, fs[k].longName)) best = fs[k].longName;
    for (uint8_t k = 0; k < GF_COUNT && !best; k++)
      if (ShellUtil::looksLike(name, SHELL_GLOBAL_FLAGS[k].longName)) best = SHELL_GLOBAL_FLAGS[k].longName;
    if (best) ctx.err("%s: unknown option --%s (did you mean --%s?)", ctx.path, name, best);
    else      ctx.err("%s: unknown option --%s", ctx.path, name);
  }

  // ── positional ────────────────────────────────────────────────────────────
  static bool parsePositional(char* argv[], uint8_t argc, uint8_t& i,
                              const ArgSpec& a, ArgValue& v, CmdContext& ctx) {
    char label[24];
    snprintf(label, sizeof(label), "<%s>", a.name);

    // colour as three separate numbers: 255 0 0
    if (a.type == ARG_COLOR && i + 2 < argc && ShellUtil::isNumber(argv[i])
        && ShellUtil::isNumber(argv[i + 1]) && ShellUtil::isNumber(argv[i + 2])) {
      int32_t r, g, b;
      ShellUtil::parseInt(argv[i], r);
      ShellUtil::parseInt(argv[i + 1], g);
      ShellUtil::parseInt(argv[i + 2], b);
      v.r = ColorParse::clamp8(r);
      v.g = ColorParse::clamp8(g);
      v.b = ColorParse::clamp8(b);
      v.given = true;
      i += 2;
      return true;
    }
    if (!convert(label, a.type, a.min, a.max, a.choices, argv[i], v, ctx)) return false;
    v.given = true;
    return true;
  }

  // ── one token → value ─────────────────────────────────────────────────────
  static bool convert(const char* label, ArgType type, int16_t mn, int16_t mx,
                      const char* const* choices, const char* tok, ArgValue& v, CmdContext& ctx) {
    switch (type) {
      case ARG_INT: {
        int32_t n;
        if (!ShellUtil::parseInt(tok, n)) {
          ctx.err("%s: %s must be a number, got '%s'", ctx.path, label, tok);
          return false;
        }
        if (n < mn || n > mx) {
          ctx.err("%s: %s must be %d..%d, got %ld", ctx.path, label, mn, mx, (long)n);
          return false;
        }
        v.i = n;
        return true;
      }
      case ARG_COLOR:
        if (!ColorParse::parseColor(tok, v.r, v.g, v.b)) {
          ctx.err("%s: %s '%s' is not a colour (use r g b, r,g,b or #hex)", ctx.path, label, tok);
          return false;
        }
        return true;
      case ARG_WORD:
        v.s = tok;
        return true;
      case ARG_CHOICE: {
        for (uint8_t k = 0; choices && choices[k]; k++) {
          if (ShellUtil::eqNoCase(choices[k], tok)) { v.i = k; v.s = choices[k]; return true; }
        }
        char list[96];
        joinChoices(choices, list, sizeof(list));
        ctx.err("%s: %s must be one of: %s", ctx.path, label, list);
        return false;
      }
      case ARG_NONE:
      default:
        return true;
    }
  }

public:
  /** @brief "a, b, c" from a nullptr-terminated list (also used by the HelpPrinter). */
  static void joinChoices(const char* const* choices, char* buf, size_t len) {
    if (len == 0) return;
    buf[0] = '\0';
    for (uint8_t k = 0; choices && choices[k]; k++) {
      size_t n = strlen(buf);
      if (n + 1 >= len) break;
      snprintf(buf + n, len - n, k ? ", %s" : "%s", choices[k]);
    }
  }
};
