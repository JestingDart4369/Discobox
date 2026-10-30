/*
  test_shell_dispatch.cpp — the whole Shell with dummy commands (lib/Shell). Laptop only.
  Run: pio test -e native -f test_shell_dispatch

  Covers: lookup (groups, case), help / -h / --help, man, version, dry-run,
  quiet, time, unknown commands + suggestions, fallback to the old handler,
  and ShellCheck (spec validation).
*/
#include <unity.h>
#include <Shell.h>
#include <FakeShellIO.h>
#include <string>
#include <string.h>

// ── dummy commands: "strip led <index> <color> [-s|--speed]", "strip clear", "ping" ──
static int        runs;
static ParsedArgs last;
static bool       lastDebug;

static const ArgSpec LED_ARGS[] = {
  { "index", ARG_INT,   0, 59, false, 0, nullptr, "IC number (0-based)" },
  { "color", ARG_COLOR, 0, 0,  false, 0, nullptr, "r g b, r,g,b or #hex" },
};
static const FlagSpec LED_FLAGS[] = {
  { "speed", 's', ARG_INT, 1, 200, 15, "ms per step" },
};

class LedCmd : public Command {
public:
  const char* name()    const override { return "led"; }
  const char* summary() const override { return "set ONE IC"; }
  SHELL_ARGS(LED_ARGS)
  SHELL_FLAGS(LED_FLAGS)
  const char* example() const override { return "strip led 5 255 0 0"; }
  const char* manText() const override { return "Sets a single IC.\nOthers stay as they are."; }
  void run(const ParsedArgs& a, CmdContext& ctx) override {
    runs++; last = a; lastDebug = ctx.debug();
    ctx.log("led ran");
    ctx.dbg("debug detail");
  }
};
class ClearCmd : public Command {
public:
  const char* name()    const override { return "clear"; }
  const char* summary() const override { return "all off"; }
  void run(const ParsedArgs&, CmdContext& ctx) override { runs++; ctx.log("cleared"); }
};
class PingCmd : public Command {
public:
  const char* name()    const override { return "ping"; }
  const char* summary() const override { return "answers pong"; }
  void run(const ParsedArgs&, CmdContext& ctx) override { runs++; ctx.out("pong"); }
};

static LedCmd   led;
static ClearCmd clearCmd;
static PingCmd  ping;
static Command* const STRIP_KIDS[] = { &led, &clearCmd };
static CommandGroup strip("strip", "WS2805 strip", STRIP_KIDS, 2, "The 3 m strip.");
static Command* const COMMANDS[] = { &strip, &ping };

static const ManTopic TOPICS[] = {
  { "colors", "colour formats", "255 0 0\n255,0,0\n#ff0000" },
};

// fallback spy
static std::string fallbackLine;
static int         fallbackCalls;
static void oldHandler(char* line) { fallbackLine = line; fallbackCalls++; }

static FakeShellIO* io;
static Shell*       shell;

void setUp() {
  io = new FakeShellIO();
  shell = new Shell(*io, COMMANDS, 2, "Discobox test 1.0");
  shell->setTopics(TOPICS, 1);
  runs = 0; lastDebug = false; fallbackCalls = 0; fallbackLine.clear();
}
void tearDown() { delete shell; delete io; }

static void run(const char* line) {
  char buf[128];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  shell->execute(buf);
}

// ── lookup + run ────────────────────────────────────────────────────────────
void test_runs_leaf_in_group() {
  run("strip led 5 255 0 0 --speed 30");
  TEST_ASSERT_EQUAL_INT(1, runs);
  TEST_ASSERT_EQUAL_INT(5, last.num(0));
  TEST_ASSERT_EQUAL_INT(30, last.flagNum(0));
}

void test_lookup_is_case_insensitive() {
  run("STRIP Led 1 #ff0000");
  TEST_ASSERT_EQUAL_INT(1, runs);
}

void test_top_level_command() {
  run("ping");
  TEST_ASSERT_TRUE(io->has("pong"));
}

void test_update_reads_from_io() {
  io->feed("ping\r\n");
  shell->update();
  TEST_ASSERT_TRUE(io->has("pong"));
}

void test_update_reports_too_long_line() {
  std::string longLine(SHELL_MAX_LINE + 5, 'x');
  io->feed(longLine.c_str());
  io->feed("\n");
  shell->update();
  TEST_ASSERT_TRUE(io->has("line too long"));
  TEST_ASSERT_EQUAL_INT(0, runs);
}

void test_bad_args_do_not_run() {
  run("strip led 99 #ff0000");
  TEST_ASSERT_EQUAL_INT(0, runs);
  TEST_ASSERT_TRUE(io->has("<index> must be 0..59"));
  TEST_ASSERT_TRUE(io->has("usage:   strip led <index> <color> [options]"));   // concise help
  TEST_ASSERT_TRUE(io->has("example: strip led 5 255 0 0"));
  TEST_ASSERT_TRUE(io->has("more:    strip led --help"));
}

// ── unknown ─────────────────────────────────────────────────────────────────
void test_unknown_command_suggestion() {
  run("strp led 1 #ff0000");
  TEST_ASSERT_TRUE(io->has("unknown command 'strp' (did you mean 'strip'?)"));
}

void test_unknown_command_no_suggestion() {
  run("banana");
  TEST_ASSERT_TRUE(io->has("unknown command 'banana' - type 'help'"));
}

void test_unknown_subcommand_suggestion() {
  run("strip lde 1 #ff0000");
  TEST_ASSERT_TRUE(io->has("strip: unknown subcommand 'lde' (did you mean 'led'?)"));
}

void test_group_alone_shows_help() {
  run("strip");
  TEST_ASSERT_TRUE(io->has("usage: strip <subcommand>"));
  TEST_ASSERT_TRUE(io->has("led"));
  TEST_ASSERT_TRUE(io->has("clear"));
}

// ── help ────────────────────────────────────────────────────────────────────
void test_help_overview() {
  run("help");
  TEST_ASSERT_TRUE(io->has("Commands:"));
  TEST_ASSERT_TRUE(io->has("WS2805 strip"));
  TEST_ASSERT_TRUE(io->has("answers pong"));
  TEST_ASSERT_TRUE(io->has("-h   --help"));
  TEST_ASSERT_TRUE(io->has("-dry --dry-run"));
  TEST_ASSERT_TRUE(io->has("--version"));
}

void test_dash_h_and_double_dash_help_are_equal() {
  run("strip led -h");
  std::vector<std::string> a = io->lines;
  io->clear();
  run("strip led --help");
  TEST_ASSERT_TRUE(a == io->lines);
  TEST_ASSERT_TRUE(io->has("usage: strip led <index> <color> [options]"));
  TEST_ASSERT_TRUE(io->has("-s, --speed <1..200>"));
  TEST_ASSERT_TRUE(io->has("example: strip led 5 255 0 0"));
  TEST_ASSERT_EQUAL_INT(0, runs);
}

void test_help_command_path() {
  run("help strip led");
  TEST_ASSERT_TRUE(io->has("usage: strip led"));
}

void test_help_without_args_is_not_an_error() {
  run("strip led --help");                      // missing args must NOT be reported
  TEST_ASSERT_FALSE(io->has("missing"));
}

// ── man / version ───────────────────────────────────────────────────────────
void test_man_command() {
  run("man strip led");
  TEST_ASSERT_TRUE(io->has("NAME"));
  TEST_ASSERT_TRUE(io->has("strip led - set ONE IC"));
  TEST_ASSERT_TRUE(io->has("SYNOPSIS"));
  TEST_ASSERT_TRUE(io->has("Others stay as they are."));   // second manText line
  TEST_ASSERT_TRUE(io->has("GLOBAL OPTIONS"));
}

void test_man_index_flags_topic() {
  run("man");
  TEST_ASSERT_TRUE(io->has("colors"));
  io->clear();
  run("man flags");
  TEST_ASSERT_TRUE(io->has("-dry, --dry-run"));
  io->clear();
  run("man colors");
  TEST_ASSERT_TRUE(io->has("#ff0000"));
  io->clear();
  run("man nothing");
  TEST_ASSERT_TRUE(io->has("no manual entry"));
}

void test_version_two_ways() {
  run("version");   TEST_ASSERT_TRUE(io->has("Discobox test 1.0")); io->clear();
  run("--version"); TEST_ASSERT_TRUE(io->has("Discobox test 1.0")); io->clear();
  run("-v");                                     // clig.dev: no -v (ambiguous)
  TEST_ASSERT_FALSE(io->has("Discobox test 1.0"));
}

void test_missing_args_show_concise_help() {
  run("strip led");
  TEST_ASSERT_TRUE(io->has("missing <index>"));
  TEST_ASSERT_TRUE(io->has("example: strip led 5 255 0 0"));
  TEST_ASSERT_EQUAL_INT(0, runs);
}

void test_docs_url_in_help() {
  shell->setDocsUrl("https://example.org/discobox");
  run("help");
  TEST_ASSERT_TRUE(io->has("Docs: https://example.org/discobox"));
}

void test_dash_h_wins_over_bad_args() {
  run("strip led 99 nonsense -h");               // clig.dev: -h works appended to anything
  TEST_ASSERT_TRUE(io->has("usage: strip led"));
  TEST_ASSERT_FALSE(io->has("[ERROR]"));
}

// ── global flags around run() ───────────────────────────────────────────────
void test_debug_reaches_command() {
  run("strip led 1 #ff0000 -d");
  TEST_ASSERT_TRUE(lastDebug);
  TEST_ASSERT_TRUE(io->has("[DEBUG] debug detail"));
}

void test_no_debug_by_default() {
  run("strip led 1 #ff0000");
  TEST_ASSERT_FALSE(io->has("[DEBUG]"));
}

void test_quiet() {
  run("strip led 1 #ff0000 -q");
  TEST_ASSERT_EQUAL_INT(1, runs);
  TEST_ASSERT_FALSE(io->has("led ran"));        // ctx.log silent
  TEST_ASSERT_EQUAL_INT(2, io->quietCalls);     // on + off (Logger hook)
  TEST_ASSERT_FALSE(io->quietOn);               // switched back afterwards
}

void test_time() {
  run("ping -t");
  TEST_ASSERT_TRUE(io->has("[TIME]  ping took 100 us"));
}

void test_dry_run_does_not_run() {
  run("strip led 7 255 0 0 -dry");
  TEST_ASSERT_EQUAL_INT(0, runs);
  TEST_ASSERT_TRUE(io->has("<index> = 7"));
  TEST_ASSERT_TRUE(io->has("<color> = 255 0 0"));
  TEST_ASSERT_TRUE(io->has("--speed = 15 (default)"));
  TEST_ASSERT_TRUE(io->has("nothing was changed"));
}

// ── fallback (old handler during migration) ─────────────────────────────────
void test_fallback_gets_unknown_line_without_global_flags() {
  shell->setFallback(oldHandler);
  run("buzzer -play 2 -d");
  TEST_ASSERT_EQUAL_INT(1, fallbackCalls);
  TEST_ASSERT_EQUAL_STRING("buzzer -play 2", fallbackLine.c_str());
  TEST_ASSERT_TRUE(io->has("[DEBUG] old handler: 'buzzer -play 2'"));
}

void test_fallback_for_unmigrated_subcommand() {
  shell->setFallback(oldHandler);
  run("strip walk");
  TEST_ASSERT_EQUAL_STRING("strip walk", fallbackLine.c_str());
}

void test_fallback_not_used_for_known_commands() {
  shell->setFallback(oldHandler);
  run("strip led 1 #ff0000");
  TEST_ASSERT_EQUAL_INT(0, fallbackCalls);
  TEST_ASSERT_EQUAL_INT(1, runs);
}

void test_fallback_help_shows_old_help_too() {
  shell->setFallback(oldHandler);
  run("help");
  TEST_ASSERT_TRUE(io->has("Not migrated yet"));
  TEST_ASSERT_EQUAL_STRING("help", fallbackLine.c_str());
}

void test_fallback_dry_run_does_not_call_old_handler() {
  shell->setFallback(oldHandler);
  run("buzzer -play 2 -dry");
  TEST_ASSERT_EQUAL_INT(0, fallbackCalls);
  TEST_ASSERT_TRUE(io->has("would pass to the old handler: 'buzzer -play 2'"));
  TEST_ASSERT_TRUE(io->has("nothing was changed"));
}

void test_fallback_old_help_topic() {
  shell->setFallback(oldHandler);
  run("help lights");
  TEST_ASSERT_EQUAL_STRING("help lights", fallbackLine.c_str());
}

void test_fallback_dash_h_maps_to_old_help() {
  shell->setFallback(oldHandler);
  run("buzzer -h");
  TEST_ASSERT_EQUAL_STRING("help buzzer", fallbackLine.c_str());
}

// ── ShellCheck ──────────────────────────────────────────────────────────────
void test_selfcheck_clean() {
  TEST_ASSERT_EQUAL_INT(0, shell->selfCheck());
}

static const FlagSpec BAD_FLAGS[] = {
  { "speed", 'd', ARG_INT, 10, 1,  50, "" },     // global letter, min>max, default out, no help
  { "speed", 's', ARG_NONE, 0, 0,  0,  "dup" },  // duplicate long name
  { "verbose", 'v', ARG_NONE, 0, 0, 0,  "ambiguous -v" },
};
static const ArgSpec BAD_ARGS[] = {
  { "a", ARG_INT, 0, 5, true,  9, nullptr, "default out of range" },
  { "b", ARG_INT, 0, 5, false, 0, nullptr, "required after optional" },
};
class BadCmd : public Command {
public:
  const char* name()    const override { return "Bad Name"; }
  const char* summary() const override { return ""; }
  SHELL_ARGS(BAD_ARGS)
  SHELL_FLAGS(BAD_FLAGS)
  void run(const ParsedArgs&, CmdContext&) override {}
};
class HelpClash : public Command {
public:
  const char* name()    const override { return "help"; }
  const char* summary() const override { return "clash"; }
  void run(const ParsedArgs&, CmdContext&) override {}
};

void test_selfcheck_finds_problems() {
  static BadCmd bad;
  static HelpClash clash;
  Command* const list[] = { &bad, &clash };
  CmdContext ctx(io);
  uint16_t problems = ShellCheck::validate(list, 2, ctx);
  TEST_ASSERT_TRUE(problems >= 8);
  TEST_ASSERT_TRUE(io->has("lowercase"));
  TEST_ASSERT_TRUE(io->has("missing summary"));
  TEST_ASSERT_TRUE(io->has("letter is taken by a global flag"));
  TEST_ASSERT_TRUE(io->has("min > max"));
  TEST_ASSERT_TRUE(io->has("defined twice"));
  TEST_ASSERT_TRUE(io->has("required arg after an optional one"));
  TEST_ASSERT_TRUE(io->has("clashes with a built-in"));
  TEST_ASSERT_TRUE(io->has("-v: ambiguous letter"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_runs_leaf_in_group);
  RUN_TEST(test_lookup_is_case_insensitive);
  RUN_TEST(test_top_level_command);
  RUN_TEST(test_update_reads_from_io);
  RUN_TEST(test_update_reports_too_long_line);
  RUN_TEST(test_bad_args_do_not_run);
  RUN_TEST(test_unknown_command_suggestion);
  RUN_TEST(test_unknown_command_no_suggestion);
  RUN_TEST(test_unknown_subcommand_suggestion);
  RUN_TEST(test_group_alone_shows_help);
  RUN_TEST(test_help_overview);
  RUN_TEST(test_dash_h_and_double_dash_help_are_equal);
  RUN_TEST(test_help_command_path);
  RUN_TEST(test_help_without_args_is_not_an_error);
  RUN_TEST(test_man_command);
  RUN_TEST(test_man_index_flags_topic);
  RUN_TEST(test_version_two_ways);
  RUN_TEST(test_missing_args_show_concise_help);
  RUN_TEST(test_docs_url_in_help);
  RUN_TEST(test_dash_h_wins_over_bad_args);
  RUN_TEST(test_debug_reaches_command);
  RUN_TEST(test_no_debug_by_default);
  RUN_TEST(test_quiet);
  RUN_TEST(test_time);
  RUN_TEST(test_dry_run_does_not_run);
  RUN_TEST(test_fallback_gets_unknown_line_without_global_flags);
  RUN_TEST(test_fallback_for_unmigrated_subcommand);
  RUN_TEST(test_fallback_not_used_for_known_commands);
  RUN_TEST(test_fallback_help_shows_old_help_too);
  RUN_TEST(test_fallback_dry_run_does_not_call_old_handler);
  RUN_TEST(test_fallback_old_help_topic);
  RUN_TEST(test_fallback_dash_h_maps_to_old_help);
  RUN_TEST(test_selfcheck_clean);
  RUN_TEST(test_selfcheck_finds_problems);
  return UNITY_END();
}
