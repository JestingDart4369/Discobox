/*
  test_shell_args.cpp — GlobalFlags + ArgParser (lib/Shell). Laptop only.
  Run: pio test -e native -f test_shell_args
*/
#include <unity.h>
#include <Shell.h>
#include <FakeShellIO.h>
#include <string.h>

// ── a test spec, like "strip wipe <index> <color> [count] -s/--speed -l/--loop -m/--mode" ──
static const char* const MODES[] = { "fast", "slow", nullptr };
static const ArgSpec ARGS[] = {
  { "index", ARG_INT,   0, 59, false, 0, nullptr, "IC number" },
  { "color", ARG_COLOR, 0, 0,  false, 0, nullptr, "colour" },
  { "count", ARG_INT,   1, 9,  true,  3, nullptr, "how often" },
};
static const FlagSpec FLAGS[] = {
  { "speed", 's', ARG_INT,  1, 200, 15, "ms per step" },
  { "loop",  'l', ARG_NONE, 0, 0,   0,  "repeat" },
  { "rev",   'r', ARG_NONE, 0, 0,   0,  "reverse" },
  { "name",  'N', ARG_WORD, 0, 0,   0,  "a name" },
};
static const ArgSpec CHOICE_ARGS[] = {
  { "mode", ARG_CHOICE, 0, 0, false, 0, MODES, "speed mode" },
};

static FakeShellIO* io;
static CmdContext*  ctx;
static ParsedArgs   pa;
static char         buf[128];
static char*        argv[SHELL_MAX_TOKENS];
static uint8_t      argc;

void setUp()    { io = new FakeShellIO(); ctx = new CmdContext(io); ctx->appendPath("test"); }
void tearDown() { delete ctx; delete io; }

// split a line into argv (helper)
static void tok(const char* line) {
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  Tokenizer::split(buf, argv, argc);
}
static bool parse(const char* line) {
  tok(line);
  return ArgParser::parse(argv, argc, ARGS, 3, FLAGS, 4, pa, *ctx);
}

// ── GlobalFlags ─────────────────────────────────────────────────────────────
void test_global_long() {
  tok("strip --debug red");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_TRUE(ctx->debug());
  TEST_ASSERT_EQUAL_UINT8(2, argc);
  TEST_ASSERT_EQUAL_STRING("red", argv[1]);
}

void test_global_short_combined() {
  tok("strip red -dq");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_TRUE(ctx->debug());
  TEST_ASSERT_TRUE(ctx->quiet());
  TEST_ASSERT_EQUAL_UINT8(2, argc);
}

void test_global_anywhere() {
  tok("-t strip red");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_TRUE(ctx->timed());
  TEST_ASSERT_EQUAL_STRING("strip", argv[0]);
}

void test_dry_word_is_dry_run() {
  tok("strip red -dry");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_TRUE(ctx->dryRun());
  TEST_ASSERT_FALSE(ctx->debug());               // NOT -d -r -y
  TEST_ASSERT_EQUAL_UINT8(2, argc);
}

void test_n_is_free_for_commands() {
  tok("strip red -n");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_FALSE(ctx->dryRun());
  TEST_ASSERT_EQUAL_UINT8(3, argc);              // -n stays for the command
}

void test_no_short_version() {
  tok("x -v");                                    // -v is NOT global (clig.dev)
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_FALSE(ctx->version());
  TEST_ASSERT_EQUAL_UINT8(2, argc);
}

void test_global_all_six() {
  tok("x --help --debug --quiet --dry-run --time --version");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_TRUE(ctx->help() && ctx->debug() && ctx->quiet());
  TEST_ASSERT_TRUE(ctx->dryRun() && ctx->timed() && ctx->version());
  TEST_ASSERT_EQUAL_UINT8(1, argc);
}

void test_global_leaves_command_flags() {
  tok("strip wipe red -s 30 --speed 30");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_EQUAL_UINT8(7, argc);
  TEST_ASSERT_FALSE(ctx->debug());
}

void test_global_leaves_mixed_group() {
  tok("strip -ds");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_EQUAL_UINT8(2, argc);              // -ds stays for the ArgParser
  TEST_ASSERT_FALSE(ctx->debug());
}

void test_global_leaves_negative_number() {
  tok("strip cct -5");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_EQUAL_UINT8(3, argc);
}

void test_global_stops_at_double_dash() {
  tok("say -- -d");
  GlobalFlags::extract(argv, argc, *ctx);
  TEST_ASSERT_FALSE(ctx->debug());
  TEST_ASSERT_EQUAL_UINT8(3, argc);
}

// ── ArgParser: positional ───────────────────────────────────────────────────
void test_args_int_and_color_3_words() {
  TEST_ASSERT_TRUE(parse("5 255 0 0"));
  TEST_ASSERT_EQUAL_INT(5, pa.num(0));
  uint8_t r, g, b; pa.color(1, r, g, b);
  TEST_ASSERT_EQUAL_UINT8(255, r); TEST_ASSERT_EQUAL_UINT8(0, g); TEST_ASSERT_EQUAL_UINT8(0, b);
}

void test_args_color_commas_and_hex() {
  uint8_t r, g, b;
  TEST_ASSERT_TRUE(parse("5 0,255,0"));
  pa.color(1, r, g, b); TEST_ASSERT_EQUAL_UINT8(255, g);
  TEST_ASSERT_TRUE(parse("5 #0000ff"));
  pa.color(1, r, g, b); TEST_ASSERT_EQUAL_UINT8(255, b);
}

void test_args_optional_default() {
  TEST_ASSERT_TRUE(parse("5 red_is_not_valid") == false);   // sanity: bad colour fails
  TEST_ASSERT_TRUE(parse("5 #ff0000"));
  TEST_ASSERT_FALSE(pa.has(2));
  TEST_ASSERT_EQUAL_INT(3, pa.num(2));                      // default
  TEST_ASSERT_TRUE(parse("5 #ff0000 7"));
  TEST_ASSERT_TRUE(pa.has(2));
  TEST_ASSERT_EQUAL_INT(7, pa.num(2));
}

void test_args_out_of_range() {
  TEST_ASSERT_FALSE(parse("99 #ff0000"));
  TEST_ASSERT_TRUE(io->has("<index> must be 0..59, got 99"));
}

void test_args_not_a_number() {
  TEST_ASSERT_FALSE(parse("five #ff0000"));
  TEST_ASSERT_TRUE(io->has("must be a number"));
}

void test_args_missing() {
  TEST_ASSERT_FALSE(parse("5"));
  TEST_ASSERT_TRUE(io->has("missing <color>"));
}

void test_args_too_many() {
  TEST_ASSERT_FALSE(parse("5 #ff0000 3 extra"));
  TEST_ASSERT_TRUE(io->has("unexpected argument 'extra'"));
}

void test_args_choice() {
  tok("SLOW");
  TEST_ASSERT_TRUE(ArgParser::parse(argv, argc, CHOICE_ARGS, 1, nullptr, 0, pa, *ctx));
  TEST_ASSERT_EQUAL_INT(1, pa.num(0));
  TEST_ASSERT_EQUAL_STRING("slow", pa.word(0));
  tok("medium");
  TEST_ASSERT_FALSE(ArgParser::parse(argv, argc, CHOICE_ARGS, 1, nullptr, 0, pa, *ctx));
  TEST_ASSERT_TRUE(io->has("must be one of: fast, slow"));
}

// ── ArgParser: flags ────────────────────────────────────────────────────────
void test_flag_forms_all_equal() {
  const char* forms[] = { "5 #ff0000 -s 30", "5 #ff0000 -s30", "5 #ff0000 --speed 30", "5 #ff0000 --speed=30" };
  for (const char* f : forms) {
    TEST_ASSERT_TRUE_MESSAGE(parse(f), f);
    TEST_ASSERT_TRUE_MESSAGE(pa.flagSet(0), f);
    TEST_ASSERT_EQUAL_INT_MESSAGE(30, pa.flagNum(0), f);
  }
}

void test_flag_default_when_missing() {
  TEST_ASSERT_TRUE(parse("5 #ff0000"));
  TEST_ASSERT_FALSE(pa.flagSet(0));
  TEST_ASSERT_EQUAL_INT(15, pa.flagNum(0));
}

void test_flag_before_args() {
  TEST_ASSERT_TRUE(parse("-l 5 #ff0000"));
  TEST_ASSERT_TRUE(pa.flagSet(1));
  TEST_ASSERT_EQUAL_INT(5, pa.num(0));
}

void test_flag_combined_switches() {
  TEST_ASSERT_TRUE(parse("5 #ff0000 -lr"));
  TEST_ASSERT_TRUE(pa.flagSet(1));
  TEST_ASSERT_TRUE(pa.flagSet(2));
}

void test_flag_global_letter_in_group() {
  TEST_ASSERT_TRUE(parse("5 #ff0000 -ds 40"));
  TEST_ASSERT_TRUE(ctx->debug());
  TEST_ASSERT_EQUAL_INT(40, pa.flagNum(0));
}

void test_flag_word_value_keeps_case() {
  TEST_ASSERT_TRUE(parse("5 #ff0000 -N Eric"));
  TEST_ASSERT_EQUAL_STRING("Eric", pa.flagWord(3));
}

void test_flag_missing_value() {
  TEST_ASSERT_FALSE(parse("5 #ff0000 --speed"));
  TEST_ASSERT_TRUE(io->has("--speed needs a value (1..200)"));
}

void test_flag_out_of_range() {
  TEST_ASSERT_FALSE(parse("5 #ff0000 -s 999"));
  TEST_ASSERT_TRUE(io->has("--speed must be 1..200"));
}

void test_flag_switch_with_value_rejected() {
  TEST_ASSERT_FALSE(parse("5 #ff0000 --loop=3"));
  TEST_ASSERT_TRUE(io->has("--loop takes no value"));
}

void test_flag_unknown_with_suggestion() {
  TEST_ASSERT_FALSE(parse("5 #ff0000 --sped 30"));
  TEST_ASSERT_TRUE(io->has("did you mean --speed?"));
}

void test_flag_unknown_short() {
  TEST_ASSERT_FALSE(parse("5 #ff0000 -x"));
  TEST_ASSERT_TRUE(io->has("unknown option -x"));
}

void test_negative_number_is_not_a_flag() {
  TEST_ASSERT_TRUE(ArgParser::isFlag("-s"));
  TEST_ASSERT_TRUE(ArgParser::isFlag("--speed"));
  TEST_ASSERT_FALSE(ArgParser::isFlag("-5"));
  TEST_ASSERT_FALSE(ArgParser::isFlag("-"));
  TEST_ASSERT_FALSE(ArgParser::isFlag("--"));
}

void test_double_dash_ends_flags() {
  static const ArgSpec WORD_ARG[] = { { "text", ARG_WORD, 0, 0, false, 0, nullptr, "text" } };
  tok("-- -l");
  TEST_ASSERT_TRUE(ArgParser::parse(argv, argc, WORD_ARG, 1, FLAGS, 4, pa, *ctx));
  TEST_ASSERT_EQUAL_STRING("-l", pa.word(0));
  TEST_ASSERT_FALSE(pa.flagSet(1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_global_long);
  RUN_TEST(test_global_short_combined);
  RUN_TEST(test_global_anywhere);
  RUN_TEST(test_global_all_six);
  RUN_TEST(test_no_short_version);
  RUN_TEST(test_dry_word_is_dry_run);
  RUN_TEST(test_n_is_free_for_commands);
  RUN_TEST(test_global_leaves_command_flags);
  RUN_TEST(test_global_leaves_mixed_group);
  RUN_TEST(test_global_leaves_negative_number);
  RUN_TEST(test_global_stops_at_double_dash);
  RUN_TEST(test_args_int_and_color_3_words);
  RUN_TEST(test_args_color_commas_and_hex);
  RUN_TEST(test_args_optional_default);
  RUN_TEST(test_args_out_of_range);
  RUN_TEST(test_args_not_a_number);
  RUN_TEST(test_args_missing);
  RUN_TEST(test_args_too_many);
  RUN_TEST(test_args_choice);
  RUN_TEST(test_flag_forms_all_equal);
  RUN_TEST(test_flag_default_when_missing);
  RUN_TEST(test_flag_before_args);
  RUN_TEST(test_flag_combined_switches);
  RUN_TEST(test_flag_global_letter_in_group);
  RUN_TEST(test_flag_word_value_keeps_case);
  RUN_TEST(test_flag_missing_value);
  RUN_TEST(test_flag_out_of_range);
  RUN_TEST(test_flag_switch_with_value_rejected);
  RUN_TEST(test_flag_unknown_with_suggestion);
  RUN_TEST(test_flag_unknown_short);
  RUN_TEST(test_negative_number_is_not_a_flag);
  RUN_TEST(test_double_dash_ends_flags);
  return UNITY_END();
}
