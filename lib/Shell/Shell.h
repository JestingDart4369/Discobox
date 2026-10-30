#pragma once
/*
  Shell.h — the serial command shell. Include this ONE file to get everything.

  lib/Shell has three files:
    ShellCore.h   building blocks (input, tokenizer, specs, parser, Command)
    ShellHelp.h   help / man / spec check
    Shell.h       this file: the Shell class + ShellSerialIO for the board

  Follows https://clig.dev/#arguments-and-flags (help, errors, flags, subcommands).

  Flow for one line, e.g. "lights strip -l 5 255 0 0 -d":
    1. LineReader    bytes → "lights strip -l 5 255 0 0 -d"
    2. Tokenizer     → {lights, strip, -l, 5, 255, 0, 0, -d}
    3. built-ins     help / man / version
    4. lookup        "lights" → "strip" → "-l" (= led, an ALIAS) — any depth.
                     Aliases are matched BEFORE global flags, so right after
                     a group "-d" can be an action (ring disco) ...
    5. GlobalFlags   ... while in the rest "-d" is --debug again
    6. ArgParser     {5, 255, 0, 0} → ParsedArgs (checked against the specs)
    7. run()         with -q / -t / -dry handled around it

  Migration: setFallback() gets every line the shell does not know yet
  (global flags already removed), so the old if/else handler keeps working
  while commands move over one group at a time.

  Usage (main.cpp):
    ShellSerialIO ShellIo(Serial, Logger::setQuiet);
    Command* const COMMANDS[] = { &stripGroup, &buzzerGroup };
    Shell SerialShell(ShellIo, COMMANDS, 2, "Discobox 0.5");
    SerialShell.setDocsUrl("https://github.com/<you>/Discobox");   // optional
    void loop() { SerialShell.update(); }
*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ShellCore.h"
#include "ShellHelp.h"

class Shell {
public:
  /// Receives a line the shell doesn't know (lowercase not guaranteed).
  typedef void (*Fallback)(char* line);

  Shell(ShellIO& io, Command* const* commands, uint8_t count, const char* version)
    : _io(io), _reader(io), _cmds(commands), _count(count), _version(version) {}

  /** @brief Extra `man` / `help` pages that are not commands (e.g. "colors"). */
  void setTopics(const ManTopic* topics, uint8_t count) { _topics = topics; _topicCount = count; }

  /** @brief Link shown at the end of `help` (clig.dev: point to the docs). nullptr = none. */
  void setDocsUrl(const char* url) { _docsUrl = url; }

  /** @brief Old handler for commands not migrated yet (nullptr = none). */
  void setFallback(Fallback f) { _fallback = f; }

  /** @brief Call every loop(): reads input and runs complete lines. Non-blocking. */
  void update() {
    char* line = _reader.poll();
    if (_reader.takeOverflow()) {
      CmdContext ctx(&_io);
      ctx.warn("line too long (max %u characters) - ignored", (unsigned)LineReader::MAX_LEN);
    }
    if (line) execute(line);
  }

  /**
   * @brief Run one line directly (modifies `line`). Used by update() and tests.
   *
   * Order matters:
   *   1. global flags IN FRONT of the command   "-d lights ring -c red"
   *   2. the command path, names AND aliases    "lights ring -d"  (-d = disco here)
   *   3. global flags in the REST of the line   "lights ring -c red -d" (-d = debug)
   */
  void execute(char* line) {
    CmdContext ctx(&_io);
    char*   argv[SHELL_MAX_TOKENS];
    uint8_t argc = 0;

    switch (Tokenizer::split(line, argv, argc)) {
      case Tokenizer::TOO_MANY:
        ctx.warn("too many words (max %u)", (unsigned)SHELL_MAX_TOKENS);
        return;
      case Tokenizer::UNCLOSED_QUOTE:
        ctx.warn("missing closing \"");
        return;
      case Tokenizer::OK:
        break;
    }

    // 1. global flags in front
    uint8_t start = 0;
    while (start < argc && GlobalFlags::applyToken(argv[start], ctx)) start++;
    char**  av = argv + start;
    uint8_t ac = (uint8_t)(argc - start);

    if (ac == 0) {
      if (ctx.version())   ctx.out("%s", _version);
      else if (ctx.help()) printOverview(ctx);
      return;
    }

    // built-ins (their rest is plain words)
    if (ShellUtil::eqNoCase(av[0], "help") || ShellUtil::eqNoCase(av[0], "man")
        || ShellUtil::eqNoCase(av[0], "version")) {
      uint8_t rest = (uint8_t)(ac - 1);
      GlobalFlags::extract(av + 1, rest, ctx);
      ac = (uint8_t)(rest + 1);
      if (ShellUtil::eqNoCase(av[0], "help"))     builtinHelp(av, ac, ctx);
      else if (ShellUtil::eqNoCase(av[0], "man")) builtinMan(av, ac, ctx);
      else                                        ctx.out("%s", _version);
      return;
    }

    // 2. command path
    Command* cmd = findTop(av[0]);
    if (!cmd) {
      GlobalFlags::extract(av, ac, ctx);
      if (ctx.version()) { ctx.out("%s", _version); return; }
      if (ac == 0) return;
      unknownCommand(av, ac, ctx);
      return;
    }
    uint8_t idx = 1;
    cmd = walk(cmd, av, ac, idx, ctx);

    // 3. global flags in the rest
    uint8_t rest = (uint8_t)(ac - idx);
    GlobalFlags::extract(av + idx, rest, ctx);
    ac = (uint8_t)(idx + rest);
    if (ctx.version()) { ctx.out("%s", _version); return; }

    if (cmd->isGroup()) {
      if (ctx.help()) { HelpPrinter::commandHelp(ctx, *cmd); return; }
      Command* def = cmd->defaultCommand();
      bool more = idx < ac;
      if (def && (def->needsArgs() ? more : !more)) {
        cmd = def;                                      // e.g. "settings" -> show current
      } else if (more) {
        if (_fallback) { runFallback(av, ac, ctx); return; }   // sub not migrated yet
        unknownSub(*cmd, av[idx], ctx);
        return;
      } else {
        HelpPrinter::commandHelp(ctx, *cmd);
        return;
      }
    }
    if (ctx.help()) { HelpPrinter::commandHelp(ctx, *cmd); return; }

    ParsedArgs args;
    if (!ArgParser::parse(av + idx, (uint8_t)(ac - idx), *cmd, args, ctx)) {
      shortHelp(*cmd, ctx);                             // clig.dev: concise help on bad input
      return;
    }
    if (ctx.help())   { HelpPrinter::commandHelp(ctx, *cmd); return; }   // -h inside a group like -hs
    if (ctx.dryRun()) { printDryRun(*cmd, args, ctx); return; }

    uint32_t t0 = begin(ctx);
    cmd->run(args, ctx);
    end(ctx, t0);
  }

  /** @brief Check all command specs (see ShellHelp.h). Returns number of problems. */
  uint16_t selfCheck() {
    CmdContext ctx(&_io);
    return ShellCheck::validate(_cmds, _count, ctx);
  }

private:
  ShellIO&        _io;
  LineReader      _reader;
  Command* const* _cmds;
  uint8_t         _count;
  const char*     _version;
  const char*     _docsUrl    = nullptr;
  const ManTopic* _topics     = nullptr;
  uint8_t         _topicCount = 0;
  Fallback        _fallback   = nullptr;
  char            _fbBuf[SHELL_MAX_LINE + 1];

  // ── lookup ────────────────────────────────────────────────────────────────
  Command* findTop(const char* name) const {
    for (uint8_t i = 0; i < _count; i++)
      if (_cmds[i] && _cmds[i]->matches(name)) return _cmds[i];
    return nullptr;
  }

  /** @brief Follow sub-commands (names or aliases) as far as the words match. */
  Command* walk(Command* c, char* argv[], uint8_t argc, uint8_t& idx, CmdContext& ctx) const {
    ctx.appendPath(c->name());
    while (idx < argc) {
      Command* kid = c->findChild(argv[idx]);
      if (!kid) break;
      c = kid;
      ctx.appendPath(c->name());
      idx++;
    }
    return c;
  }

  /** @brief Find a command by NAME anywhere in the tree ("buzzer" -> "audio buzzer"). */
  Command* findDeep(const char* name, CmdContext& ctx) const {
    for (uint8_t i = 0; i < _count; i++) {
      if (!_cmds[i]) continue;
      ctx.path[0] = '\0';
      Command* hit = findIn(_cmds[i], name, ctx);
      if (hit) return hit;
    }
    ctx.path[0] = '\0';
    return nullptr;
  }

  Command* findIn(Command* c, const char* name, CmdContext& ctx) const {
    size_t keep = strlen(ctx.path);
    ctx.appendPath(c->name());
    if (ShellUtil::eqNoCase(c->name(), name)) return c;
    uint8_t n;
    Command* const* kids = c->children(n);
    for (uint8_t k = 0; k < n; k++) {
      if (!kids[k]) continue;
      Command* hit = findIn(kids[k], name, ctx);
      if (hit) return hit;
    }
    ctx.path[keep] = '\0';
    return nullptr;
  }

  /** @brief Command for "help x y" / "man x y": exact path first, then by name anywhere. */
  Command* resolveForHelp(char* argv[], uint8_t argc, CmdContext& ctx) const {
    Command* c = findTop(argv[1]);
    uint8_t idx = 2;
    if (c) return walk(c, argv, argc, idx, ctx);
    c = findDeep(argv[1], ctx);
    if (!c) return nullptr;
    while (idx < argc) {                                // "help buzzer play"
      Command* kid = c->findChild(argv[idx]);
      if (!kid) break;
      c = kid;
      ctx.appendPath(c->name());
      idx++;
    }
    return c;
  }

  const ManTopic* findTopic(const char* name) const {
    for (uint8_t i = 0; i < _topicCount; i++)
      if (ShellUtil::eqNoCase(_topics[i].name, name)) return &_topics[i];
    return nullptr;
  }

  // ── built-ins ─────────────────────────────────────────────────────────────
  void printOverview(CmdContext& ctx) {
    HelpPrinter::overview(ctx, _cmds, _count);
    if (_topicCount) {
      ctx.out("Topics (help <topic> / man <topic>):");
      for (uint8_t i = 0; i < _topicCount; i++)
        ctx.out("  %-12s %s", _topics[i].name, _topics[i].summary);
    }
    if (_fallback) {
      ctx.out("Not migrated yet (old handler):");
      strcpy(_fbBuf, "help");
      _fallback(_fbBuf);
    }
    if (_docsUrl) ctx.out("Docs: %s", _docsUrl);
  }

  void builtinHelp(char* argv[], uint8_t argc, CmdContext& ctx) {
    if (argc < 2) { printOverview(ctx); return; }
    Command* c = resolveForHelp(argv, argc, ctx);
    if (c) { HelpPrinter::commandHelp(ctx, *c); return; }
    const ManTopic* t = findTopic(argv[1]);
    if (t) { HelpPrinter::topic(ctx, *t); return; }
    if (_fallback) { runFallback(argv, argc, ctx); return; }   // e.g. old "help lights"
    ctx.err("no help for '%s' - type 'help'", argv[1]);
  }

  void builtinMan(char* argv[], uint8_t argc, CmdContext& ctx) {
    if (argc < 2) { HelpPrinter::manIndex(ctx, _cmds, _count, _topics, _topicCount); return; }
    if (ShellUtil::eqNoCase(argv[1], "flags")) { HelpPrinter::manFlags(ctx); return; }
    const ManTopic* t = findTopic(argv[1]);
    if (t) { HelpPrinter::topic(ctx, *t); return; }
    Command* c = resolveForHelp(argv, argc, ctx);
    if (!c) { ctx.err("no manual entry for '%s' - type 'man' for the list", argv[1]); return; }
    HelpPrinter::man(ctx, *c);
  }

  // ── errors ────────────────────────────────────────────────────────────────
  /** @brief After a bad call: usage + example + where to find more. */
  void shortHelp(const Command& c, CmdContext& ctx) {
    char buf[SHELL_OUT_BUF];
    HelpPrinter::usage(c, ctx.path, buf, sizeof(buf));
    ctx.out("  usage:   %s", buf);
    if (c.example()) ctx.out("  example: %s", c.example());
    ctx.out("  more:    %s --help", ctx.path);
  }

  void unknownCommand(char* argv[], uint8_t argc, CmdContext& ctx) {
    if (_fallback) { runFallback(argv, argc, ctx); return; }
    const char* best = nullptr;
    for (uint8_t i = 0; i < _count && !best; i++)
      if (_cmds[i] && ShellUtil::looksLike(argv[0], _cmds[i]->name())) best = _cmds[i]->name();
    static const char* const BUILTINS[] = { "help", "man", "version" };
    for (uint8_t i = 0; i < 3 && !best; i++)
      if (ShellUtil::looksLike(argv[0], BUILTINS[i])) best = BUILTINS[i];
    if (best) ctx.err("unknown command '%s' (did you mean '%s'?)", argv[0], best);
    else      ctx.err("unknown command '%s' - type 'help'", argv[0]);
  }

  void unknownSub(const Command& group, const char* word, CmdContext& ctx) {
    uint8_t nk;
    Command* const* kids = group.children(nk);
    const char* best = nullptr;
    for (uint8_t k = 0; k < nk && !best; k++)
      if (kids[k] && ShellUtil::looksLike(word, kids[k]->name())) best = kids[k]->name();
    if (best) ctx.err("%s: unknown subcommand '%s' (did you mean '%s'?)", ctx.path, word, best);
    else      ctx.err("%s: unknown subcommand '%s' - type '%s -h'", ctx.path, word, ctx.path);
  }

  // ── running ───────────────────────────────────────────────────────────────
  uint32_t begin(CmdContext& ctx) {
    if (ctx.quiet()) _io.setQuiet(true);
    return _io.micros();
  }

  void end(CmdContext& ctx, uint32_t t0) {
    uint32_t dt = _io.micros() - t0;
    if (ctx.quiet()) _io.setQuiet(false);
    if (ctx.timed()) ctx.out("[TIME]  %s took %lu us", ctx.path[0] ? ctx.path : "command", (unsigned long)dt);
  }

  /** @brief Hand a line to the old handler (global flags already removed). */
  void runFallback(char* argv[], uint8_t argc, CmdContext& ctx) {
    if (ctx.help()) {                                         // old handler: "help <topic>"
      snprintf(_fbBuf, sizeof(_fbBuf), "help %s", argv[0]);
      _fallback(_fbBuf);
      return;
    }
    _fbBuf[0] = '\0';
    for (uint8_t i = 0; i < argc; i++) {
      size_t n = strlen(_fbBuf);
      snprintf(_fbBuf + n, sizeof(_fbBuf) - n, i ? " %s" : "%s", argv[i]);
    }
    if (ctx.dryRun()) {                                       // show, don't run
      ctx.out("[DRY]   would pass to the old handler: '%s'", _fbBuf);
      ctx.out("[DRY]   (arguments are checked there, not here)");
      ctx.out("[DRY]   nothing was changed");
      return;
    }
    ctx.dbg("old handler: '%s'", _fbBuf);
    if (ctx.path[0] == '\0') ctx.appendPath(argv[0]);
    uint32_t t0 = begin(ctx);
    _fallback(_fbBuf);
    end(ctx, t0);
  }

  void printDryRun(const Command& c, const ParsedArgs& a, CmdContext& ctx) {
    ctx.out("[DRY]   %s", ctx.path);
    uint8_t na;
    const ArgSpec* as = c.args(na);
    for (uint8_t k = 0; k < na; k++) {
      const ArgSpec& s = as[k];
      const ArgValue& v = a.arg[k];
      const char* note = v.given ? "" : " (default)";
      switch (s.type) {
        case ARG_INT:    ctx.out("  <%s> = %ld%s", s.name, (long)v.i, note); break;
        case ARG_COLOR:  ctx.out("  <%s> = %u %u %u", s.name, v.r, v.g, v.b); break;
        case ARG_WORD:
        case ARG_CHOICE: ctx.out("  <%s> = '%s'", s.name, v.s ? v.s : "(not given)"); break;
        default: break;
      }
    }
    uint8_t nf;
    const FlagSpec* fs = c.flags(nf);
    for (uint8_t k = 0; k < nf; k++) {
      const FlagSpec& f = fs[k];
      const ArgValue& v = a.flag[k];
      switch (f.type) {
        case ARG_NONE:  ctx.out("  --%s = %s", f.longName, v.given ? "on" : "off"); break;
        case ARG_INT:   ctx.out("  --%s = %ld%s", f.longName, (long)v.i, v.given ? "" : " (default)"); break;
        case ARG_COLOR: if (v.given) ctx.out("  --%s = %u %u %u", f.longName, v.r, v.g, v.b); break;
        case ARG_WORD:  if (v.given) ctx.out("  --%s = '%s'", f.longName, v.s); break;
        default: break;
      }
    }
    ctx.out("[DRY]   nothing was changed");
  }
};

// ════════════════════════════════════════════════════════════════════════════
// ShellSerialIO — board only (ARDUINO is defined by every Arduino build,
// not by `pio test -e native`, so laptop tests never see <Arduino.h>)
// ════════════════════════════════════════════════════════════════════════════
#if defined(ARDUINO)
#include <Arduino.h>
/*
  ShellSerialIO — ShellIO for the real board.

  Wraps any Arduino Stream (normally Serial), uses micros() for --time and
  calls an optional hook for --quiet so Logger output is silenced too:

    ShellSerialIO ShellIo(Serial, Logger::setQuiet);

  Laptop tests use test/mocks/FakeShellIO.h instead.
*/

class ShellSerialIO : public ShellIO {
public:
  typedef void (*QuietHook)(bool quiet);

  explicit ShellSerialIO(Stream& stream, QuietHook quietHook = nullptr)
    : _stream(stream), _quietHook(quietHook) {}

  int      available() override                 { return _stream.available(); }
  int      read() override                      { return _stream.read(); }
  void     writeLine(const char* line) override { _stream.println(line); }
  uint32_t micros() override                    { return ::micros(); }
  void     setQuiet(bool q) override            { if (_quietHook) _quietHook(q); }

private:
  Stream&   _stream;
  QuietHook _quietHook;
};
#endif // ARDUINO
