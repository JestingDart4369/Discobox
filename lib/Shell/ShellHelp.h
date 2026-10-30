#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/*
  ShellHelp.h — everything that EXPLAINS commands, built from their specs.

    HelpPrinter   help / -h / --help / man / man flags / man <topic>
    ShellCheck    validate all command descriptions (use it in a laptop test)
*/
#include "ShellCore.h"

// ════════════════════════════════════════════════════════════════════════════
// HelpPrinter
// ════════════════════════════════════════════════════════════════════════════
/*
  HelpPrinter.h — build every help text from the command specs.

    help              overview()      all commands + global options
    <cmd> -h/--help   commandHelp()   usage, arguments, options, example
    man <cmd>         man()           full page: NAME SYNOPSIS DESCRIPTION ...
    man               manIndex()      list of all pages
    man flags         manFlags()      the global flags explained
    man <topic>       topic()         a ManTopic (e.g. colors)

  Nothing here is written by hand per command — it all comes from the
  ArgSpec / FlagSpec tables, summary(), example() and manText().
*/

class HelpPrinter {
public:
  // ── building blocks ───────────────────────────────────────────────────────

  /** @brief "strip wipe <color> [options]" */
  static void usage(const Command& c, const char* path, char* buf, size_t len) {
    if (len == 0) return;
    buf[0] = '\0';
    append(buf, len, "%s", path);
    if (c.isGroup()) append(buf, len, " <subcommand>");
    uint8_t na;
    const ArgSpec* as = c.args(na);
    for (uint8_t k = 0; k < na; k++)
      append(buf, len, as[k].optional ? " [%s]" : " <%s>", as[k].name);
    uint8_t nf;
    c.flags(nf);
    if (nf) append(buf, len, " [options]");
  }

  /** @brief Print `text` line by line ('\n' separated), each line indented. */
  static void printText(CmdContext& ctx, const char* text, const char* indent) {
    if (!text) return;
    const char* p = text;
    while (*p) {
      const char* nl = strchr(p, '\n');
      int len = nl ? (int)(nl - p) : (int)strlen(p);
      ctx.out("%s%.*s", indent, len, p);
      if (!nl) break;
      p = nl + 1;
    }
  }

  // ── pages ─────────────────────────────────────────────────────────────────

  static void overview(CmdContext& ctx, Command* const* cmds, uint8_t n) {
    ctx.out("Commands:");
    if (n == 0) ctx.out("  (none registered yet)");
    for (uint8_t i = 0; i < n; i++)
      if (cmds[i]) ctx.out("  %-12s %s", cmds[i]->name(), cmds[i]->summary());
    ctx.out("  %-12s %s", "help [cmd]", "this list, or help for one command");
    ctx.out("  %-12s %s", "man [page]", "full manual page (try 'man flags')");
    ctx.out("  %-12s %s", "version", "firmware version");
    ctx.out("Global options (work with every command):");
    globalCompact(ctx, "  ");
    ctx.out("Type '<command> -h' for details.");
  }

  static void commandHelp(CmdContext& ctx, const Command& c) {
    char buf[SHELL_OUT_BUF];
    usage(c, ctx.path, buf, sizeof(buf));
    ctx.out("usage: %s", buf);
    if (c.aliases() && *c.aliases()) ctx.out("   or: %s", aliasUsage(c, ctx.path, buf, sizeof(buf)));
    ctx.out("  %s", c.summary());
    if (c.defaultCommand()) ctx.out("  without a subcommand: %s", c.defaultCommand()->summary());
    childLines(ctx, c, "subcommands:", "  ");
    argLines(ctx, c, "arguments:", "  ");
    flagLines(ctx, c, "options:", "  ");
    ctx.out("global options: -h -d -q -t -dry --version  (see 'man flags')");
    if (c.example()) ctx.out("example: %s", c.example());
    ctx.out("see also: man %s", ctx.path);
  }

  static void man(CmdContext& ctx, const Command& c) {
    char buf[SHELL_OUT_BUF];
    ctx.out("NAME");
    ctx.out("    %s - %s", ctx.path, c.summary());
    ctx.out("SYNOPSIS");
    usage(c, ctx.path, buf, sizeof(buf));
    ctx.out("    %s", buf);
    if (c.aliases() && *c.aliases()) ctx.out("    %s", aliasUsage(c, ctx.path, buf, sizeof(buf)));
    if (c.manText()) {
      ctx.out("DESCRIPTION");
      printText(ctx, c.manText(), "    ");
    }
    childLines(ctx, c, "SUBCOMMANDS", "    ");
    argLines(ctx, c, "ARGUMENTS", "    ");
    flagLines(ctx, c, "OPTIONS", "    ");
    ctx.out("GLOBAL OPTIONS");
    globalFull(ctx, "    ");
    if (c.example()) {
      ctx.out("EXAMPLE");
      ctx.out("    %s", c.example());
    }
    ctx.out("SEE ALSO");
    if (c.isGroup()) ctx.out("    man %s <subcommand>, man flags", ctx.path);
    else             ctx.out("    help, man flags");
  }

  static void manIndex(CmdContext& ctx, Command* const* cmds, uint8_t n,
                       const ManTopic* topics, uint8_t nt) {
    ctx.out("Manual pages - type 'man <page>':");
    for (uint8_t i = 0; i < n; i++)
      if (cmds[i]) ctx.out("  %-12s %s", cmds[i]->name(), cmds[i]->summary());
    ctx.out("  %-12s %s", "flags", "the global options -h -d -q -t -dry --version");
    for (uint8_t i = 0; i < nt; i++)
      ctx.out("  %-12s %s", topics[i].name, topics[i].summary);
  }

  static void manFlags(CmdContext& ctx) {
    ctx.out("NAME");
    ctx.out("    flags - global options, accepted by every command");
    ctx.out("DESCRIPTION");
    ctx.out("    Can stand anywhere in the line and be combined: 'strip red -dq'.");
    ctx.out("    Short and long form do the same: -d = --debug, -dry = --dry-run.");
    ctx.out("    -dry is one word: it is NOT -d -r -y.");
    ctx.out("    '--' ends option parsing: everything after it is an argument.");
    ctx.out("OPTIONS");
    globalFull(ctx, "    ");
  }

  static void topic(CmdContext& ctx, const ManTopic& t) {
    ctx.out("NAME");
    ctx.out("    %s - %s", t.name, t.summary);
    ctx.out("DESCRIPTION");
    printText(ctx, t.text, "    ");
  }

private:
  SHELL_PRINTF(3, 4) static void append(char* buf, size_t len, const char* fmt, ...) {
    size_t n = strlen(buf);
    if (n + 1 >= len) return;
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf + n, len - n, fmt, a);
    va_end(a);
  }

  static void childLines(CmdContext& ctx, const Command& c, const char* title, const char* indent) {
    uint8_t nk;
    Command* const* kids = c.children(nk);
    if (!nk) return;
    ctx.out("%s", title);
    for (uint8_t k = 0; k < nk; k++) {
      if (!kids[k]) continue;
      const char* al = kids[k]->aliases();
      ctx.out("%s%-10s %-16s %s", indent, kids[k]->name(), al ? al : "", kids[k]->summary());
    }
  }

  /** @brief "lights ring -c <color>" — the usage line with the first alias instead of the name. */
  static const char* aliasUsage(const Command& c, const char* path, char* buf, size_t len) {
    usage(c, path, buf, len);
    const char* al = c.aliases();
    size_t nameLen = strlen(c.name());
    size_t pathLen = strlen(path);
    if (!al || pathLen < nameLen) return buf;
    char first[16];
    size_t k = 0;
    while (al[k] && al[k] != ' ' && k < sizeof(first) - 1) { first[k] = al[k]; k++; }
    first[k] = '\0';
    char tail[SHELL_OUT_BUF];
    snprintf(tail, sizeof(tail), "%s", buf + pathLen);           // part after the path
    char head[SHELL_MAX_PATH];
    snprintf(head, sizeof(head), "%.*s", (int)(pathLen - nameLen), path);   // path without name
    snprintf(buf, len, "%s%s%s", head, first, tail);
    return buf;
  }

  static void argLines(CmdContext& ctx, const Command& c, const char* title, const char* indent) {
    uint8_t na;
    const ArgSpec* as = c.args(na);
    if (!na) return;
    ctx.out("%s", title);
    for (uint8_t k = 0; k < na; k++) {
      const ArgSpec& a = as[k];
      char left[24], extra[72];
      snprintf(left, sizeof(left), a.optional ? "[%s]" : "<%s>", a.name);
      extra[0] = '\0';
      if (a.type == ARG_INT) {
        if (a.optional) snprintf(extra, sizeof(extra), " (%d..%d, default %d)", a.min, a.max, a.def);
        else            snprintf(extra, sizeof(extra), " (%d..%d)", a.min, a.max);
      } else if (a.type == ARG_CHOICE) {
        char list[56];
        ArgParser::joinChoices(a.choices, list, sizeof(list));
        snprintf(extra, sizeof(extra), " (%s)", list);
      }
      ctx.out("%s%-14s %s%s", indent, left, a.help, extra);
    }
  }

  static void flagLines(CmdContext& ctx, const Command& c, const char* title, const char* indent) {
    uint8_t nf;
    const FlagSpec* fs = c.flags(nf);
    if (!nf) return;
    ctx.out("%s", title);
    for (uint8_t k = 0; k < nf; k++) {
      const FlagSpec& f = fs[k];
      char left[40], extra[32];
      flagLeft(f, left, sizeof(left));
      extra[0] = '\0';
      if (f.type == ARG_INT) snprintf(extra, sizeof(extra), " (default %d)", f.def);
      ctx.out("%s%-24s %s%s", indent, left, f.help, extra);
    }
  }

  /** @brief "-s, --speed <1..200>" / "    --save" */
  static void flagLeft(const FlagSpec& f, char* buf, size_t len) {
    buf[0] = '\0';
    if (f.shortName) append(buf, len, "-%c, ", f.shortName);
    else             append(buf, len, "    ");
    append(buf, len, "--%s", f.longName);
    if (f.type == ARG_INT)   append(buf, len, " <%d..%d>", f.min, f.max);
    if (f.type == ARG_WORD)  append(buf, len, " <text>");
    if (f.type == ARG_COLOR) append(buf, len, " <color>");
  }

  /** @brief "-h --help   -d --debug   -q --quiet" (3 per line) */
  static void globalCompact(CmdContext& ctx, const char* indent) {
    char line[SHELL_OUT_BUF];
    line[0] = '\0';
    for (uint8_t i = 0; i < GF_COUNT; i++) {
      const FlagSpec& f = SHELL_GLOBAL_FLAGS[i];
      char shortForm[8];
      globalShort(i, shortForm, sizeof(shortForm));
      append(line, sizeof(line), "%-4s --%-9s", shortForm, f.longName);
      if (i % 3 == 2 || i == GF_COUNT - 1) {
        ctx.out("%s%s", indent, line);
        line[0] = '\0';
      } else {
        append(line, sizeof(line), " ");
      }
    }
  }

  /** @brief "-h", "-dry" or "" — the short way to type a global flag. */
  static void globalShort(uint8_t i, char* buf, size_t len) {
    const char* alias = globalAliasOf(i);
    if (alias)                              snprintf(buf, len, "-%s", alias);
    else if (SHELL_GLOBAL_FLAGS[i].shortName) snprintf(buf, len, "-%c", SHELL_GLOBAL_FLAGS[i].shortName);
    else                                    buf[0] = '\0';
  }

  static void globalFull(CmdContext& ctx, const char* indent) {
    for (uint8_t i = 0; i < GF_COUNT; i++) {
      char left[40], shortForm[8];
      globalShort(i, shortForm, sizeof(shortForm));
      if (shortForm[0]) snprintf(left, sizeof(left), "%s, --%s", shortForm, SHELL_GLOBAL_FLAGS[i].longName);
      else              snprintf(left, sizeof(left), "    --%s", SHELL_GLOBAL_FLAGS[i].longName);
      ctx.out("%s%-24s %s", indent, left, SHELL_GLOBAL_FLAGS[i].help);
    }
  }
};

// ════════════════════════════════════════════════════════════════════════════
// ShellCheck
// ════════════════════════════════════════════════════════════════════════════
/*
  ShellCheck.h — check that all command DESCRIPTIONS are consistent.

  Runs over the whole command tree and reports mistakes such as a range with
  min > max, a default outside its range, a missing help text, two flags
  with the same letter, or a command flag that steals a global letter (-d).
  Call it from a laptop test so mistakes are caught before they reach the board:

    CmdContext ctx(&io);
    TEST_ASSERT_EQUAL(0, ShellCheck::validate(ALL_COMMANDS, COUNT, ctx));
*/

class ShellCheck {
public:
  /** @brief Returns the number of problems found (each one printed via ctx.err). */
  static uint16_t validate(Command* const* cmds, uint8_t n, CmdContext& ctx) {
    uint16_t problems = 0;
    for (uint8_t i = 0; i < n; i++) {
      if (!cmds[i]) { ctx.err("command #%u is nullptr", (unsigned)i); problems++; continue; }
      const char* nm = cmds[i]->name();
      if (isBuiltin(nm)) { ctx.err("'%s' clashes with a built-in command", nm); problems++; }
      for (uint8_t j = 0; j < i; j++)
        if (cmds[j] && ShellUtil::eqNoCase(cmds[j]->name(), nm)) {
          ctx.err("'%s' is registered twice", nm); problems++;
        }
      problems += checkCommand(*cmds[i], nm, ctx);
    }
    return problems;
  }

  /** @brief true if any name/alias of `a` also matches `b`. */
  static bool aliasClash(const Command& a, const Command& b) {
    if (b.matches(a.name()) || a.matches(b.name())) return true;
    const char* al = a.aliases();
    if (!al) return false;
    char word[24];
    while (*al) {
      while (*al == ' ') al++;
      uint8_t k = 0;
      while (*al && *al != ' ') { if (k < sizeof(word) - 1) word[k++] = *al; al++; }
      word[k] = '\0';
      if (k && b.matches(word)) return true;
    }
    return false;
  }

  static bool isBuiltin(const char* name) {
    return ShellUtil::eqNoCase(name, "help") || ShellUtil::eqNoCase(name, "man")
        || ShellUtil::eqNoCase(name, "version");
  }

private:
  static uint16_t checkCommand(const Command& c, const char* path, CmdContext& ctx) {
    uint16_t p = 0;
    const char* nm = c.name();

    // name + summary
    if (!nm || !*nm) { ctx.err("%s: empty name", path); return 1; }
    for (const char* s = nm; *s; s++) {
      if (*s == ' ' || (*s >= 'A' && *s <= 'Z')) {
        ctx.err("%s: name must be lowercase without spaces", path); p++; break;
      }
    }
    if (nm[0] == '-') { ctx.err("%s: name must not start with '-'", path); p++; }
    if (!c.summary() || !*c.summary()) { ctx.err("%s: missing summary", path); p++; }

    // positional args
    uint8_t na;
    const ArgSpec* as = c.args(na);
    if (na > SHELL_MAX_ARGS) { ctx.err("%s: %u args > SHELL_MAX_ARGS", path, (unsigned)na); p++; }
    if (na && c.isGroup())   { ctx.err("%s: a group cannot have arguments", path); p++; }
    bool sawOptional = false;
    for (uint8_t k = 0; k < na; k++) {
      const ArgSpec& a = as[k];
      if (!a.name || !*a.name) { ctx.err("%s: arg #%u has no name", path, (unsigned)k); p++; continue; }
      if (!a.help || !*a.help) { ctx.err("%s <%s>: missing help", path, a.name); p++; }
      if (a.type == ARG_NONE)  { ctx.err("%s <%s>: ARG_NONE is only for flags", path, a.name); p++; }
      if (a.type == ARG_INT) {
        if (a.min > a.max) { ctx.err("%s <%s>: min > max", path, a.name); p++; }
        if (a.optional && (a.def < a.min || a.def > a.max)) {
          ctx.err("%s <%s>: default %d outside %d..%d", path, a.name, a.def, a.min, a.max); p++;
        }
      }
      if (a.type == ARG_CHOICE && (!a.choices || !a.choices[0])) {
        ctx.err("%s <%s>: ARG_CHOICE without choices", path, a.name); p++;
      }
      if (a.optional) sawOptional = true;
      else if (sawOptional) { ctx.err("%s <%s>: required arg after an optional one", path, a.name); p++; }
    }

    // flags
    uint8_t nf;
    const FlagSpec* fs = c.flags(nf);
    if (nf > SHELL_MAX_FLAGS) { ctx.err("%s: %u flags > SHELL_MAX_FLAGS", path, (unsigned)nf); p++; }
    for (uint8_t k = 0; k < nf; k++) {
      const FlagSpec& f = fs[k];
      if (!f.longName || !*f.longName) { ctx.err("%s: flag #%u has no long name", path, (unsigned)k); p++; continue; }
      if (!f.help || !*f.help) { ctx.err("%s --%s: missing help", path, f.longName); p++; }
      if (globalFlagByLong(f.longName) >= 0) { ctx.err("%s --%s: clashes with a global flag", path, f.longName); p++; }
      if (f.shortName && globalFlagByShort(f.shortName) >= 0) {
        ctx.err("%s -%c: letter is taken by a global flag", path, f.shortName); p++;
      }
      if (f.shortName == 'v') {   // clig.dev: -v is ambiguous (verbose? version?) — use -d / --version
        ctx.err("%s -v: ambiguous letter, use another one", path); p++;
      }
      if (f.type == ARG_CHOICE) { ctx.err("%s --%s: ARG_CHOICE is not supported for flags", path, f.longName); p++; }
      if (f.type == ARG_INT && f.min > f.max) { ctx.err("%s --%s: min > max", path, f.longName); p++; }
      if (f.type == ARG_INT && (f.def < f.min || f.def > f.max)) {
        ctx.err("%s --%s: default %d outside %d..%d", path, f.longName, f.def, f.min, f.max); p++;
      }
      for (uint8_t j = 0; j < k; j++) {
        if (ShellUtil::eqNoCase(fs[j].longName, f.longName)) { ctx.err("%s --%s: defined twice", path, f.longName); p++; }
        if (f.shortName && fs[j].shortName == f.shortName)   { ctx.err("%s -%c: defined twice", path, f.shortName); p++; }
      }
    }

    // aliases: never -h / --help / --version (those must always mean help/version)
    if (c.aliases()) {
      static const char* const RESERVED[] = { "-h", "--help", "--version", "-dry", "--dry-run" };
      for (uint8_t r = 0; r < 5; r++)
        if (c.matches(RESERVED[r]) && !ShellUtil::eqNoCase(c.name(), RESERVED[r])) {
          ctx.err("%s: alias '%s' is reserved", path, RESERVED[r]); p++;
        }
    }

    // default command of a group
    if (c.defaultCommand() && !c.isGroup()) { ctx.err("%s: only a group can have a default command", path); p++; }

    // sub-commands
    uint8_t nk;
    Command* const* kids = c.children(nk);
    for (uint8_t k = 0; k < nk; k++) {
      if (!kids[k]) { ctx.err("%s: sub-command #%u is nullptr", path, (unsigned)k); p++; continue; }
      for (uint8_t j = 0; j < k; j++) {
        if (!kids[j]) continue;
        if (ShellUtil::eqNoCase(kids[j]->name(), kids[k]->name())) {
          ctx.err("%s %s: defined twice", path, kids[k]->name()); p++;
        }
        if (aliasClash(*kids[j], *kids[k])) {
          ctx.err("%s: '%s' and '%s' share an alias", path, kids[j]->name(), kids[k]->name()); p++;
        }
      }
      char sub[SHELL_MAX_PATH];
      snprintf(sub, sizeof(sub), "%s %s", path, kids[k]->name());
      p += checkCommand(*kids[k], sub, ctx);
    }
    return p;
  }
};
