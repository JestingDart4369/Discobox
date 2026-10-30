/*
  test_hw_button.cpp — on-device test for the button + its RGB LED (runs ON the Uno R4).
  Run: pio test -e uno_r4_wifi_test   (board plugged in, serial monitor closed)
  Only this test:  pio test -e uno_r4_wifi_test -f test_hw_button

  Part 1 — button LED (automatic): red / green / blue / off, each held so you can look.
  Part 2 — physical button (INTERACTIVE): the test tells you what to do in the
           output (INFO lines). The button LED shows the state:
             yellow = waiting for you   green = correct   red = wrong / timeout
           You have WAIT_MS per step.

  Pins and timings are the same as main.cpp:
    button D2 (INPUT_PULLUP, pressed = LOW), LED R=D3 G=D5 B=D6
    GestureButton defaults: debounce 25 ms, click gap 400 ms, hold 3000 ms
*/

#include <Arduino.h>
#include <unity.h>
#include <LedController.h>
#include <GestureButton.h>

static const unsigned long WAIT_MS = 20000;   // time you get for each step
static const uint16_t      HOLD_MS = 1000;    // time to look at an LED colour

// ── gesture spy ─────────────────────────────────────────────────────────────
static volatile int nSP, nDP, nTP, nHold;
static void onSP()   { nSP++;   }
static void onDP()   { nDP++;   }
static void onTP()   { nTP++;   }
static void onHold() { nHold++; }

static GestureButton Button(D2, onSP, onDP, onTP, onHold);
static RGBLeds<D3, D5, D6> ButtonLed;

static void resetCounts() { nSP = nDP = nTP = nHold = 0; }
static int  total()       { return nSP + nDP + nTP + nHold; }

// Calls Button.update() until any gesture fires or WAIT_MS runs out.
// Returns true if a gesture was detected.
static bool waitForGesture() {
  resetCounts();
  ButtonLed.setToColor(255, 160, 0);          // yellow: your turn
  unsigned long t0 = millis();
  while (millis() - t0 < WAIT_MS) {
    Button.update(millis());
    if (total() > 0) {
      // let a late extra click finish so a wrong gesture is caught too
      unsigned long t1 = millis();
      while (millis() - t1 < 600) Button.update(millis());
      return true;
    }
  }
  return false;
}

// Checks that exactly the expected gesture fired once, and shows the result on the LED.
static void expectGesture(const char* name, volatile int& counter) {
  char msg[96];
  snprintf(msg, sizeof(msg), "got SP=%d DP=%d TP=%d HOLD=%d, expected one %s", nSP, nDP, nTP, nHold, name);
  bool ok = (counter == 1 && total() == 1);
  ButtonLed.setToColor(ok ? 0 : 255, ok ? 255 : 0, 0);   // green / red
  delay(500);
  ButtonLed.clear();
  TEST_ASSERT_TRUE_MESSAGE(ok, msg);
}

static void waitUntilReleased() {
  while (digitalRead(D2) == LOW) delay(5);
  unsigned long t0 = millis();
  while (millis() - t0 < 600) Button.update(millis());   // settle debounce + click gap
  resetCounts();
}

void setUp()    { ButtonLed.setBrightness(255); ButtonLed.clear(); }
void tearDown() { ButtonLed.clear(); }

// ── Part 1: button LED ──────────────────────────────────────────────────────
static void assertLed(uint8_t r, uint8_t g, uint8_t b) {
  CRGB c = ButtonLed.color();
  TEST_ASSERT_EQUAL_UINT8(r, c.r);
  TEST_ASSERT_EQUAL_UINT8(g, c.g);
  TEST_ASSERT_EQUAL_UINT8(b, c.b);
}

void test_led_red()   { ButtonLed.setToColor(255, 0, 0); assertLed(255, 0, 0); delay(HOLD_MS); }
void test_led_green() { ButtonLed.setToColor(0, 255, 0); assertLed(0, 255, 0); delay(HOLD_MS); }
void test_led_blue()  { ButtonLed.setToColor(0, 0, 255); assertLed(0, 0, 255); delay(HOLD_MS); }

void test_led_clear_is_off() {
  ButtonLed.setToColor(255, 0, 0);
  ButtonLed.clear();
  assertLed(0, 0, 0);
}

void test_led_brightness_keeps_colour() {
  // brightness is applied in show() (PWM), the stored colour stays the same
  ButtonLed.setBrightness(40);
  ButtonLed.setToColor(255, 0, 0);
  assertLed(255, 0, 0);
  TEST_ASSERT_EQUAL_UINT8(40, ButtonLed.getBrightness());
  delay(HOLD_MS);                                         // dim red
}

// ── Part 2: physical button (interactive) ───────────────────────────────────
void test_button_not_pressed_at_rest() {
  TEST_MESSAGE("Do NOT touch the button for 1 second...");
  unsigned long t0 = millis();
  while (millis() - t0 < 1000) Button.update(millis());
  TEST_ASSERT_FALSE_MESSAGE(Button.isPressed(), "button reads pressed without touching -> wiring / pull-up?");
}

void test_button_is_pressed_while_held() {
  TEST_MESSAGE(">>> Press and KEEP HOLDING the button (LED yellow)...");
  ButtonLed.setToColor(255, 160, 0);
  unsigned long t0 = millis();
  bool seen = false;
  while (millis() - t0 < WAIT_MS && !seen) {
    Button.update(millis());
    seen = Button.isPressed();
  }
  ButtonLed.setToColor(seen ? 0 : 255, seen ? 255 : 0, 0);
  TEST_ASSERT_TRUE_MESSAGE(seen, "no press within 20 s");
  TEST_MESSAGE("OK - now let go.");
  waitUntilReleased();
}

void test_single_press() {
  TEST_MESSAGE(">>> Press the button ONCE");
  TEST_ASSERT_TRUE_MESSAGE(waitForGesture(), "no gesture within 20 s");
  expectGesture("SP", nSP);
}

void test_double_press() {
  TEST_MESSAGE(">>> Press the button TWICE quickly");
  TEST_ASSERT_TRUE_MESSAGE(waitForGesture(), "no gesture within 20 s");
  expectGesture("DP", nDP);
}

void test_triple_press() {
  TEST_MESSAGE(">>> Press the button THREE times quickly");
  TEST_ASSERT_TRUE_MESSAGE(waitForGesture(), "no gesture within 20 s");
  expectGesture("TP", nTP);
}

void test_hold() {
  TEST_MESSAGE(">>> HOLD the button for 3+ seconds, then let go");
  TEST_ASSERT_TRUE_MESSAGE(waitForGesture(), "no gesture within 20 s");
  expectGesture("HOLD", nHold);
  waitUntilReleased();
}

void setup() {
  delay(2000);                         // R4 needs a moment before USB serial is ready
  UNITY_BEGIN();                       // starts Serial at test_speed (115200)
  Button.setup();
  ButtonLed.setup();

  // Part 1 — LED
  RUN_TEST(test_led_red);
  RUN_TEST(test_led_green);
  RUN_TEST(test_led_blue);
  RUN_TEST(test_led_clear_is_off);
  RUN_TEST(test_led_brightness_keeps_colour);

  // Part 2 — button (you press)
  RUN_TEST(test_button_not_pressed_at_rest);
  RUN_TEST(test_button_is_pressed_while_held);
  RUN_TEST(test_single_press);
  RUN_TEST(test_double_press);
  RUN_TEST(test_triple_press);
  RUN_TEST(test_hold);

  ButtonLed.clear();
  UNITY_END();
}

void loop() {}
