/*
  test_hw_strip.cpp — on-device test for the WS2805 strip (runs ON the Uno R4).
  Run: pio test -e uno_r4_wifi_test   (board plugged in, serial monitor closed)

  Checks the same path as the serial command `strip 255 0 0`:
    ColorParse::parseColor("255 0 0") -> Strip.setToColor(r, g, b) -> pixel buffer
  The buffer is exactly what show() sends over SPI. Whether the LEDs really
  light up (wiring, 12 V, level shifter) you check by eye: every colour test
  holds the colour for HOLD_MS.
*/

#include <Arduino.h>
#include <unity.h>
#include <LedController.h>
#define WS2805_N_LEDS 60        // same values as main.cpp
#define WS2805_INDEX_SHIFT 0
#define WS2805_CHUNK_BYTES 0
#include <WS2805.h>
#include <ColorParse.h>

static WS2805<WS2805_N_LEDS> Strip;
static const uint16_t HOLD_MS = 1500;   // time to look at the strip

// Does what `strip <color>` does in Serial_Handler.h
static void stripCommand(const char* color) {
  uint8_t r, g, b;
  TEST_ASSERT_TRUE_MESSAGE(ColorParse::parseColor(color, r, g, b), color);
  Strip.setToColor(r, g, b);            // sets buffer + show()
}

// Every IC must hold exactly this value
static void assertAll(uint8_t r, uint8_t g, uint8_t b, uint8_t cw = 0, uint8_t ww = 0) {
  char msg[16];
  for (uint16_t i = 0; i < WS2805_N_LEDS; i++) {
    RGBCCT p = Strip.pixel(i);
    snprintf(msg, sizeof(msg), "IC %u", i);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(r,  p.r,  msg);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(g,  p.g,  msg);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(b,  p.b,  msg);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(cw, p.cw, msg);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ww, p.ww, msg);
  }
}

void setUp() {                          // every test starts dark, full brightness
  Strip.setBrightness(255);
  Strip.clear();
  Strip.show();
}
void tearDown() {}

// ── colours ─────────────────────────────────────────────────────────────────
void test_strip_255_0_0_is_red()     { stripCommand("255 0 0");     assertAll(255, 0, 0);     delay(HOLD_MS); }
void test_strip_0_255_0_is_green()   { stripCommand("0 255 0");     assertAll(0, 255, 0);     delay(HOLD_MS); }
void test_strip_0_0_255_is_blue()    { stripCommand("0 0 255");     assertAll(0, 0, 255);     delay(HOLD_MS); }
void test_strip_white_rgb()          { stripCommand("255 255 255"); assertAll(255, 255, 255); delay(HOLD_MS); }
void test_strip_commas_is_red()      { stripCommand("255,0,0");     assertAll(255, 0, 0); }
void test_strip_hex_is_red()         { stripCommand("#ff0000");     assertAll(255, 0, 0); }

// ── clear / brightness ──────────────────────────────────────────────────────
void test_strip_clear_is_off() {
  stripCommand("255 0 0");
  Strip.clear();
  Strip.show();
  assertAll(0, 0, 0);
}

void test_brightness_128_halves_red() {
  Strip.setBrightness(128);
  stripCommand("255 0 0");
  assertAll(255, 0, 0);                 // buffer keeps the full colour ...
  for (uint16_t i = 0; i < WS2805_N_LEDS; i++)
    TEST_ASSERT_EQUAL_UINT8(128, Strip.shown(i).r);   // ... sent at 255*128/255 = 128
  delay(HOLD_MS);
}

void test_brightness_change_without_new_colour() {
  stripCommand("255 0 0");
  Strip.setBrightness(64);              // no new colour command
  Strip.show();
  TEST_ASSERT_EQUAL_UINT8(64, Strip.shown(0).r);
  delay(HOLD_MS);                       // dim red
  Strip.setBrightness(255);             // back up: nothing was lost
  Strip.show();
  TEST_ASSERT_EQUAL_UINT8(255, Strip.shown(0).r);
  delay(HOLD_MS);
}

// ── single IC (strip ic N) ─────────────────────────────────────────────────
void test_only_first_ic_red() {
  Strip.setToColorSingle(0, 255, 0, 0);
  Strip.show();
  RGBCCT first = Strip.pixel(0);
  RGBCCT second = Strip.pixel(1);
  TEST_ASSERT_EQUAL_UINT8(255, first.r);
  TEST_ASSERT_EQUAL_UINT8(0, second.r);
  delay(HOLD_MS);
}

void test_only_last_ic_red() {
  Strip.setToColorSingle(WS2805_N_LEDS - 1, 255, 0, 0);
  Strip.show();
  TEST_ASSERT_EQUAL_UINT8(255, Strip.pixel(WS2805_N_LEDS - 1).r);
  TEST_ASSERT_EQUAL_UINT8(0,   Strip.pixel(WS2805_N_LEDS - 2).r);
  delay(HOLD_MS);
}

void setup() {
  delay(2000);                          // R4 needs a moment before USB serial is ready
  UNITY_BEGIN();                        // starts Serial at test_speed (115200)
  Strip.setup();

  RUN_TEST(test_strip_255_0_0_is_red);
  RUN_TEST(test_strip_0_255_0_is_green);
  RUN_TEST(test_strip_0_0_255_is_blue);
  RUN_TEST(test_strip_white_rgb);
  RUN_TEST(test_strip_commas_is_red);
  RUN_TEST(test_strip_hex_is_red);
  RUN_TEST(test_strip_clear_is_off);
  RUN_TEST(test_brightness_128_halves_red);
  RUN_TEST(test_brightness_change_without_new_colour);
  RUN_TEST(test_only_first_ic_red);
  RUN_TEST(test_only_last_ic_red);

  Strip.clear();
  Strip.show();
  UNITY_END();
}

void loop() {}
