/*
  test_color_parse.cpp — unit tests for lib/ColorParse (runs on the laptop).
  Run: pio test -e native -f test_color_parse

  This is the parser behind every "<color>" serial command
  (strip, ring, led, btn, fade, wipe ...).
*/

#include <unity.h>
#include <ColorParse.h>

static uint8_t r, g, b;

void setUp()    { r = g = b = 7; }   // 7 = "not touched" marker
void tearDown() {}

static void assertRGB(uint8_t er, uint8_t eg, uint8_t eb) {
  TEST_ASSERT_EQUAL_UINT8(er, r);
  TEST_ASSERT_EQUAL_UINT8(eg, g);
  TEST_ASSERT_EQUAL_UINT8(eb, b);
}

// ── valid formats ───────────────────────────────────────────────────────────
void test_spaces()          { TEST_ASSERT_TRUE(ColorParse::parseColor("255 0 0", r, g, b));      assertRGB(255, 0, 0); }
void test_commas()          { TEST_ASSERT_TRUE(ColorParse::parseColor("255,0,0", r, g, b));      assertRGB(255, 0, 0); }
void test_comma_and_space() { TEST_ASSERT_TRUE(ColorParse::parseColor("255, 0, 0", r, g, b));    assertRGB(255, 0, 0); }
void test_extra_spaces()    { TEST_ASSERT_TRUE(ColorParse::parseColor("  0   255  0  ", r, g, b)); assertRGB(0, 255, 0); }
void test_hex_hash()        { TEST_ASSERT_TRUE(ColorParse::parseColor("#0000ff", r, g, b));      assertRGB(0, 0, 255); }
void test_hex_bare()        { TEST_ASSERT_TRUE(ColorParse::parseColor("ff8800", r, g, b));       assertRGB(255, 136, 0); }
void test_hex_uppercase()   { TEST_ASSERT_TRUE(ColorParse::parseColor("#FF00FF", r, g, b));      assertRGB(255, 0, 255); }

// ── clamping ────────────────────────────────────────────────────────────────
void test_clamp_high()      { TEST_ASSERT_TRUE(ColorParse::parseColor("300 0 0", r, g, b));      assertRGB(255, 0, 0); }
void test_clamp_negative()  { TEST_ASSERT_TRUE(ColorParse::parseColor("-5 10 0", r, g, b));      assertRGB(0, 10, 0); }

// ── invalid input: returns false, leaves r/g/b untouched ───────────────────
void test_empty()           { TEST_ASSERT_FALSE(ColorParse::parseColor("", r, g, b));          assertRGB(7, 7, 7); }
void test_word()            { TEST_ASSERT_FALSE(ColorParse::parseColor("red", r, g, b));       assertRGB(7, 7, 7); }
void test_two_values()      { TEST_ASSERT_FALSE(ColorParse::parseColor("255 0", r, g, b));     assertRGB(7, 7, 7); }
void test_four_values()     { TEST_ASSERT_FALSE(ColorParse::parseColor("255 0 0 0", r, g, b)); assertRGB(7, 7, 7); }
void test_semicolons()      { TEST_ASSERT_FALSE(ColorParse::parseColor("255;0;0", r, g, b));   assertRGB(7, 7, 7); }
void test_short_hex()       { TEST_ASSERT_FALSE(ColorParse::parseColor("#ff00", r, g, b));     assertRGB(7, 7, 7); }
void test_bad_hex()         { TEST_ASSERT_FALSE(ColorParse::parseColor("#gg0000", r, g, b));   assertRGB(7, 7, 7); }

// ── parseInts (used by "strip cct" and "strip hsv") ─────────────────────────
void test_parse_ints_cct() {
  int v[5];
  TEST_ASSERT_TRUE(ColorParse::parseInts("255 0 0 0 128", v, 5));
  int expected[5] = {255, 0, 0, 0, 128};
  TEST_ASSERT_EQUAL_INT_ARRAY(expected, v, 5);
  TEST_ASSERT_TRUE(ColorParse::parseInts("255,0,0,0,128", v, 5));
  TEST_ASSERT_EQUAL_INT_ARRAY(expected, v, 5);
}
void test_parse_ints_too_few() {
  int v[5];
  TEST_ASSERT_FALSE(ColorParse::parseInts("255 0 0 0", v, 5));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_spaces);
  RUN_TEST(test_commas);
  RUN_TEST(test_comma_and_space);
  RUN_TEST(test_extra_spaces);
  RUN_TEST(test_hex_hash);
  RUN_TEST(test_hex_bare);
  RUN_TEST(test_hex_uppercase);
  RUN_TEST(test_clamp_high);
  RUN_TEST(test_clamp_negative);
  RUN_TEST(test_empty);
  RUN_TEST(test_word);
  RUN_TEST(test_two_values);
  RUN_TEST(test_four_values);
  RUN_TEST(test_semicolons);
  RUN_TEST(test_short_hex);
  RUN_TEST(test_bad_hex);
  RUN_TEST(test_parse_ints_cct);
  RUN_TEST(test_parse_ints_too_few);
  return UNITY_END();
}
