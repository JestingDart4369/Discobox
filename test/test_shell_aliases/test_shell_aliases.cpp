/*
  test_shell_aliases.cpp — action aliases, group defaults, deep help (lib/Shell). Laptop only.
  Run: pio test -e native -f test_shell_aliases

  Uses a small tree like the real one:
    lights ring  color(-c --color) <color> | disco(-d --disco) | led(-l --led) <n> <color>
    audio buzzer queue(-q --queue) <n> | theme(-th --theme) [pack]
    settings     plus(-p --plus) | default: show
*/
#include <unity.h>
#include <Shell.h>
#include <FakeShellIO.h>
#include <string.h>

static int  lastAction;          // which function ran
static long lastNum;
static bool lastDebug;
enum { NONE, COLOR, DISCO, LED, QUEUE, THEME, PLUS, SHOW };

static void fColor(const ParsedArgs&, CmdContext& c)   { lastAction = COLOR; lastDebug = c.debug(); }
static void fDisco(const ParsedArgs&, CmdContext& c)   { lastAction = DISCO; lastDebug = c.debug(); }
static void fLed(const ParsedArgs& a, CmdContext&)     { lastAction = LED;   lastNum = a.num(0); }
static void fQueue(const ParsedArgs& a, CmdContext& c) { lastAction = QUEUE; lastNum = a.num(0); lastDebug = c.debug(); }
static void fTheme(const ParsedArgs& a, CmdContext&)   { lastAction = THEME; lastNum = a.has(0); }
static void fPlus(const ParsedArgs&, CmdContext&)      { lastAction = PLUS; }
static void fShow(const ParsedArgs&, CmdContext& c)    { lastAction = SHOW; c.out("selected: x"); }

static const ArgSpec COLOR_ARGS[] = { { "color", ARG_COLOR, 0, 0, false, 0, nullptr, "colour" } };
static const ArgSpec LED_ARGS[]   = { { "n", ARG_INT, 1, 16, false, 0, nullptr, "LED" },
                                      { "color", ARG_COLOR, 0, 0, false, 0, nullptr, "colour" } };
static const ArgSpec QUEUE_ARGS[] = { { "n", ARG_INT, 0, 20, false, 0, nullptr, "song" } };
static const ArgSpec THEME_ARGS[] = { { "pack", ARG_WORD, 0, 0, true, 0, nullptr, "pack" } };

static const CommandDef D_COLOR = { "color", "whole ring", "-c --color", SHELL_LIST(COLOR_ARGS), SHELL_NONE, "lights ring -c red", nullptr, fColor };
static const CommandDef D_DISCO = { "disco", "random colours", "-d --disco", SHELL_NONE, SHELL_NONE, nullptr, nullptr, fDisco };
static const CommandDef D_LED   = { "led", "one LED", "-l --led", SHELL_LIST(LED_ARGS), SHELL_NONE, nullptr, nullptr, fLed };
static const CommandDef D_QUEUE = { "queue", "select a song", "-q --queue", SHELL_LIST(QUEUE_ARGS), SHELL_NONE, nullptr, nullptr, fQueue };
static const CommandDef D_THEME = { "theme", "cue theme", "-th --theme", SHELL_LIST(THEME_ARGS), SHELL_NONE, nullptr, nullptr, fTheme };
static const CommandDef D_PLUS  = { "plus", "bump value", "-p --plus", SHELL_NONE, SHELL_NONE, nullptr, nullptr, fPlus };
static const CommandDef D_SHOW  = { "show", "show the selected setting", nullptr, SHELL_NONE, SHELL_NONE, nullptr, nullptr, fShow };

static FnCommand cColor(D_COLOR), cDisco(D_DISCO), cLed(D_LED), cQueue(D_QUEUE), cTheme(D_THEME), cPlus(D_PLUS), cShow(D_SHOW);
static Command* const RING_K[]   = { &cColor, &cDisco, &cLed };
static Command* const BUZZER_K[] = { &cQueue, &cTheme };
static Command* const SET_K[]    = { &cPlus };
static CommandGroup ring("ring", "16-LED ring", RING_K, 3);
static CommandGroup buzzer("buzzer", "piezo", BUZZER_K, 2);
static Command* const LIGHTS_K[] = { &ring };
static Command* const AUDIO_K[]  = { &buzzer };
static CommandGroup lights("lights", "all lights", LIGHTS_K, 1);
static CommandGroup audio("audio", "sound", AUDIO_K, 1);
static CommandGroup settings("settings", "settings menu", SET_K, 1, nullptr, &cShow);
static Command* const CMDS[] = { &lights, &audio, &settings };
static const ManTopic TOPICS[] = { { "gestures", "button gestures", "sp dp tp hold" } };

static FakeShellIO* io;
static Shell*       shell;

void setUp() {
  io = new FakeShellIO();
  shell = new Shell(*io, CMDS, 3, "test 1");
  shell->setTopics(TOPICS, 1);
  lastAction = NONE; lastNum = -1; lastDebug = false;
}
void tearDown() { delete shell; delete io; }

static void run(const char* line) {
  char buf[128];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  shell->execute(buf);
}

// ── aliases ─────────────────────────────────────────────────────────────────
void test_short_long_and_word_are_equal() {
  run("lights ring -c #ff0000");     TEST_ASSERT_EQUAL_INT(COLOR, lastAction); lastAction = NONE;
  run("lights ring --color #ff0000"); TEST_ASSERT_EQUAL_INT(COLOR, lastAction); lastAction = NONE;
  run("lights ring color #ff0000");   TEST_ASSERT_EQUAL_INT(COLOR, lastAction);
}

void test_d_after_group_is_disco_not_debug() {
  run("lights ring -d");
  TEST_ASSERT_EQUAL_INT(DISCO, lastAction);
  TEST_ASSERT_FALSE(lastDebug);
}

void test_d_later_is_debug() {
  run("lights ring -c red -d");      // 'red' is not a colour → error, but -d must be debug
  run("lights ring -c #ff0000 -d");
  TEST_ASSERT_EQUAL_INT(COLOR, lastAction);
  TEST_ASSERT_TRUE(lastDebug);
}

void test_d_twice_disco_and_debug() {
  run("lights ring -d -d");
  TEST_ASSERT_EQUAL_INT(DISCO, lastAction);
  TEST_ASSERT_TRUE(lastDebug);
}

void test_debug_in_front() {
  run("-d lights ring -d");
  TEST_ASSERT_EQUAL_INT(DISCO, lastAction);
  TEST_ASSERT_TRUE(lastDebug);
}

void test_q_after_group_is_queue() {
  run("audio buzzer -q 3");
  TEST_ASSERT_EQUAL_INT(QUEUE, lastAction);
  TEST_ASSERT_EQUAL_INT(3, lastNum);
  TEST_ASSERT_EQUAL_INT(0, io->quietCalls);   // NOT quiet
}

void test_th_is_theme_not_time_help() {
  run("audio buzzer -th arcade");
  TEST_ASSERT_EQUAL_INT(THEME, lastAction);
  TEST_ASSERT_EQUAL_INT(1, lastNum);
  TEST_ASSERT_FALSE(io->has("usage:"));         // no help printed
}

void test_alias_case_insensitive_group_names() {
  run("Lights RING -L 4 #00ff00");
  TEST_ASSERT_EQUAL_INT(LED, lastAction);
  TEST_ASSERT_EQUAL_INT(4, lastNum);
}

void test_h_after_group_is_help() {
  run("lights ring -h");
  TEST_ASSERT_TRUE(io->has("usage: lights ring <subcommand>"));
  TEST_ASSERT_TRUE(io->has("-d --disco"));
  TEST_ASSERT_EQUAL_INT(NONE, lastAction);
}

void test_leaf_help_shows_alias_form() {
  run("lights ring -l -h");
  TEST_ASSERT_TRUE(io->has("usage: lights ring led <n> <color>"));
  TEST_ASSERT_TRUE(io->has("or: lights ring -l <n> <color>"));
}

void test_unknown_action() {
  run("lights ring -zz");
  TEST_ASSERT_TRUE(io->has("unknown subcommand '-zz'"));
}

// ── group default ───────────────────────────────────────────────────────────
void test_group_default_runs_without_sub() {
  run("settings");
  TEST_ASSERT_EQUAL_INT(SHOW, lastAction);
  TEST_ASSERT_TRUE(io->has("selected: x"));
}

void test_group_default_sub_still_works() {
  run("settings -p");
  TEST_ASSERT_EQUAL_INT(PLUS, lastAction);
}

void test_group_default_help() {
  run("settings -h");
  TEST_ASSERT_EQUAL_INT(NONE, lastAction);
  TEST_ASSERT_TRUE(io->has("without a subcommand: show the selected setting"));
}

void test_group_without_default_shows_help() {
  run("lights ring");
  TEST_ASSERT_TRUE(io->has("usage: lights ring <subcommand>"));
}

// ── help / man find nested commands and topics ──────────────────────────────
void test_help_finds_nested_by_name() {
  run("help buzzer");
  TEST_ASSERT_TRUE(io->has("usage: audio buzzer <subcommand>"));
}

void test_help_nested_then_sub() {
  run("help buzzer queue");
  TEST_ASSERT_TRUE(io->has("usage: audio buzzer queue <n>"));
}

void test_man_nested() {
  run("man ring");
  TEST_ASSERT_TRUE(io->has("lights ring - 16-LED ring"));
}

void test_help_topic() {
  run("help gestures");
  TEST_ASSERT_TRUE(io->has("sp dp tp hold"));
}

void test_overview_lists_topics() {
  run("help");
  TEST_ASSERT_TRUE(io->has("gestures"));
}

// ── ShellCheck ──────────────────────────────────────────────────────────────
void test_selfcheck_clean_tree() {
  TEST_ASSERT_EQUAL_INT(0, shell->selfCheck());
}

static const CommandDef D_BAD1 = { "one", "x", "-x --help", SHELL_NONE, SHELL_NONE, nullptr, nullptr, fPlus };
static const CommandDef D_BAD2 = { "two", "y", "-x", SHELL_NONE, SHELL_NONE, nullptr, nullptr, fPlus };
void test_selfcheck_alias_problems() {
  static FnCommand b1(D_BAD1), b2(D_BAD2);
  static Command* const K[] = { &b1, &b2 };
  static CommandGroup g("grp", "group", K, 2);
  Command* const list[] = { &g };
  CmdContext ctx(io);
  TEST_ASSERT_TRUE(ShellCheck::validate(list, 1, ctx) >= 2);
  TEST_ASSERT_TRUE(io->has("alias '--help' is reserved"));
  TEST_ASSERT_TRUE(io->has("share an alias"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_short_long_and_word_are_equal);
  RUN_TEST(test_d_after_group_is_disco_not_debug);
  RUN_TEST(test_d_later_is_debug);
  RUN_TEST(test_d_twice_disco_and_debug);
  RUN_TEST(test_debug_in_front);
  RUN_TEST(test_q_after_group_is_queue);
  RUN_TEST(test_th_is_theme_not_time_help);
  RUN_TEST(test_alias_case_insensitive_group_names);
  RUN_TEST(test_h_after_group_is_help);
  RUN_TEST(test_leaf_help_shows_alias_form);
  RUN_TEST(test_unknown_action);
  RUN_TEST(test_group_default_runs_without_sub);
  RUN_TEST(test_group_default_sub_still_works);
  RUN_TEST(test_group_default_help);
  RUN_TEST(test_group_without_default_shows_help);
  RUN_TEST(test_help_finds_nested_by_name);
  RUN_TEST(test_help_nested_then_sub);
  RUN_TEST(test_man_nested);
  RUN_TEST(test_help_topic);
  RUN_TEST(test_overview_lists_topics);
  RUN_TEST(test_selfcheck_clean_tree);
  RUN_TEST(test_selfcheck_alias_problems);
  return UNITY_END();
}
