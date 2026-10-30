/*
  test_shell_input.cpp — LineReader + Tokenizer (lib/Shell). Laptop only.
  Run: pio test -e native -f test_shell_input
*/
#include <unity.h>
#include <Shell.h>
#include <FakeShellIO.h>
#include <string.h>
#include <string>       // std::string (long test lines) — don't rely on FakeShellIO.h

static FakeShellIO* io;
static LineReader*  reader;

void setUp()    { io = new FakeShellIO(); reader = new LineReader(*io); }
void tearDown() { delete reader; delete io; }

// ── LineReader ──────────────────────────────────────────────────────────────
void test_line_lf() {
  io->feed("sp\n");
  TEST_ASSERT_EQUAL_STRING("sp", reader->poll());
}

void test_line_crlf() {
  io->feed("sp\r\n");
  TEST_ASSERT_EQUAL_STRING("sp", reader->poll());
}

void test_line_in_two_chunks() {
  io->feed("str");
  TEST_ASSERT_NULL(reader->poll());              // no Enter yet
  io->feed("ip red\n");
  TEST_ASSERT_EQUAL_STRING("strip red", reader->poll());
}

void test_empty_line_ignored() {
  io->feed("\n\n");
  TEST_ASSERT_NULL(reader->poll());
}

void test_backspace() {
  io->feed("spx\b\n");
  TEST_ASSERT_EQUAL_STRING("sp", reader->poll());
}

void test_tab_becomes_space() {
  io->feed("strip\tred\n");
  TEST_ASSERT_EQUAL_STRING("strip red", reader->poll());
}

void test_too_long_line_dropped_and_reported_once() {
  std::string longLine(LineReader::MAX_LEN + 10, 'a');
  io->feed(longLine.c_str());
  io->feed("\n");
  TEST_ASSERT_NULL(reader->poll());
  TEST_ASSERT_TRUE(reader->takeOverflow());
  TEST_ASSERT_FALSE(reader->takeOverflow());     // only once
}

void test_line_after_overflow_works() {
  std::string longLine(LineReader::MAX_LEN + 10, 'a');
  io->feed(longLine.c_str());
  io->feed("\nsp\n");
  TEST_ASSERT_EQUAL_STRING("sp", reader->poll());
}

void test_two_lines_one_at_a_time() {
  io->feed("sp\ndp\n");
  TEST_ASSERT_EQUAL_STRING("sp", reader->poll());
  TEST_ASSERT_EQUAL_STRING("dp", reader->poll());
  TEST_ASSERT_NULL(reader->poll());
}

void test_exactly_max_len_fits() {
  std::string line(LineReader::MAX_LEN, 'b');
  io->feed(line.c_str());
  io->feed("\n");
  char* got = reader->poll();
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_EQUAL_INT((int)LineReader::MAX_LEN, (int)strlen(got));
}

// ── Tokenizer ───────────────────────────────────────────────────────────────
static char*   argv[SHELL_MAX_TOKENS];
static uint8_t argc;

void test_split_basic() {
  char line[] = "strip  led 5 -d";
  TEST_ASSERT_EQUAL_INT(Tokenizer::OK, Tokenizer::split(line, argv, argc));
  TEST_ASSERT_EQUAL_UINT8(4, argc);
  TEST_ASSERT_EQUAL_STRING("strip", argv[0]);
  TEST_ASSERT_EQUAL_STRING("led",   argv[1]);
  TEST_ASSERT_EQUAL_STRING("5",     argv[2]);
  TEST_ASSERT_EQUAL_STRING("-d",    argv[3]);
}

void test_split_leading_trailing_spaces() {
  char line[] = "   strip   red   ";
  Tokenizer::split(line, argv, argc);
  TEST_ASSERT_EQUAL_UINT8(2, argc);
  TEST_ASSERT_EQUAL_STRING("red", argv[1]);
}

void test_split_only_spaces() {
  char line[] = "    ";
  TEST_ASSERT_EQUAL_INT(Tokenizer::OK, Tokenizer::split(line, argv, argc));
  TEST_ASSERT_EQUAL_UINT8(0, argc);
}

void test_split_quotes() {
  char line[] = "wifi set \"My Net\" now";
  TEST_ASSERT_EQUAL_INT(Tokenizer::OK, Tokenizer::split(line, argv, argc));
  TEST_ASSERT_EQUAL_UINT8(4, argc);
  TEST_ASSERT_EQUAL_STRING("My Net", argv[2]);
  TEST_ASSERT_EQUAL_STRING("now", argv[3]);
}

void test_split_unclosed_quote() {
  char line[] = "say \"hi";
  TEST_ASSERT_EQUAL_INT(Tokenizer::UNCLOSED_QUOTE, Tokenizer::split(line, argv, argc));
}

void test_split_too_many() {
  char line[] = "a b c d e f g h i j k l m";      // 13 words, max 12
  TEST_ASSERT_EQUAL_INT(Tokenizer::TOO_MANY, Tokenizer::split(line, argv, argc));
}

void test_split_exactly_max() {
  char line[] = "a b c d e f g h i j k l";        // 12 words
  TEST_ASSERT_EQUAL_INT(Tokenizer::OK, Tokenizer::split(line, argv, argc));
  TEST_ASSERT_EQUAL_UINT8(12, argc);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_line_lf);
  RUN_TEST(test_line_crlf);
  RUN_TEST(test_line_in_two_chunks);
  RUN_TEST(test_empty_line_ignored);
  RUN_TEST(test_backspace);
  RUN_TEST(test_tab_becomes_space);
  RUN_TEST(test_too_long_line_dropped_and_reported_once);
  RUN_TEST(test_line_after_overflow_works);
  RUN_TEST(test_two_lines_one_at_a_time);
  RUN_TEST(test_exactly_max_len_fits);
  RUN_TEST(test_split_basic);
  RUN_TEST(test_split_leading_trailing_spaces);
  RUN_TEST(test_split_only_spaces);
  RUN_TEST(test_split_quotes);
  RUN_TEST(test_split_unclosed_quote);
  RUN_TEST(test_split_too_many);
  RUN_TEST(test_split_exactly_max);
  return UNITY_END();
}
