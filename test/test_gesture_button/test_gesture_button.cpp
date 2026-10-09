/*
  test_gesture_button.cpp — Unit tests for GestureButton gesture detection.

  Runs on the host machine (pio test -e native).
  Tests single press, double press, triple press, hold, debounce, and
  the isPressed() state accessor.

  Timing recap (GestureButton defaults):
    DEBOUNCE_MS  = 25   — raw signal must be stable for 25 ms before registering
    CLICK_GAP_MS = 400  — silence this long after the last release → finalise gesture
    HOLD_MS      = 3000 — press held 3 s → hold gesture (cancels click count)
*/

#include <unity.h>
#include <GestureButton.h>

// ── Callback spy counters ────────────────────────────────────────────────────
static int sp_count, dp_count, tp_count, hold_count;
static void onSP()   { sp_count++;   }
static void onDP()   { dp_count++;   }
static void onTP()   { tp_count++;   }
static void onHold() { hold_count++; }

static const uint8_t BTN = 2;   // pin used in all tests

// ── Helpers ──────────────────────────────────────────────────────────────────
// GestureButton::update(now) takes the current time as a parameter (not millis()),
// so we just pass carefully chosen `t` values — no millis mocking needed.
//
// Each helper sets the pin AND calls update() twice:
//   first call  — raw edge registered, debounce timer starts
//   second call — 30 ms later, debounce done, state committed

static void press(GestureButton& b, unsigned long t) {
  mock_set_pin(BTN, LOW);
  b.update(t);
  b.update(t + 30);    // settle past DEBOUNCE_MS (25)
}

static void release(GestureButton& b, unsigned long t) {
  mock_set_pin(BTN, HIGH);
  b.update(t);
  b.update(t + 30);    // settle → click_count++ / _last_release_ms = t+30
}

static void idle(GestureButton& b, unsigned long t) { b.update(t); }

// ── setUp / tearDown ─────────────────────────────────────────────────────────
void setUp() {
  mock_reset();
  sp_count = dp_count = tp_count = hold_count = 0;
}
void tearDown() {}

// ── Tests ────────────────────────────────────────────────────────────────────

void test_sp_fires_after_single_click_and_gap() {
  GestureButton btn(BTN, onSP, onDP, onTP, onHold);

  press(btn, 0);          // press settled at t=30
  release(btn, 100);      // release settled at t=130, click_count=1, _last_release=130
  idle(btn, 600);         // (600-130)=470 >= CLICK_GAP_MS(400) → SP fires

  TEST_ASSERT_EQUAL(1, sp_count);
  TEST_ASSERT_EQUAL(0, dp_count);
  TEST_ASSERT_EQUAL(0, tp_count);
  TEST_ASSERT_EQUAL(0, hold_count);
}

void test_dp_fires_after_two_clicks_and_gap() {
  GestureButton btn(BTN, onSP, onDP, onTP, onHold);

  press(btn, 0);
  release(btn, 100);      // click_count=1, _last_release=130
  press(btn, 200);        // within 400 ms of last release
  release(btn, 300);      // click_count=2, _last_release=330
  idle(btn, 800);         // (800-330)=470 >= 400 → DP fires

  TEST_ASSERT_EQUAL(0, sp_count);
  TEST_ASSERT_EQUAL(1, dp_count);
}

void test_tp_fires_after_three_clicks() {
  GestureButton btn(BTN, onSP, onDP, onTP, onHold);

  press(btn, 0);
  release(btn, 100);
  press(btn, 200);
  release(btn, 300);
  press(btn, 400);
  release(btn, 500);      // click_count=3, _last_release=530
  idle(btn, 1100);        // (1100-530)=570 >= 400 → TP fires

  TEST_ASSERT_EQUAL(0, sp_count);
  TEST_ASSERT_EQUAL(0, dp_count);
  TEST_ASSERT_EQUAL(1, tp_count);
}

void test_hold_fires_and_suppresses_click() {
  GestureButton btn(BTN, onSP, onDP, onTP, onHold);

  press(btn, 0);          // _press_start_ms = 30 (settled)
  idle(btn, 3100);        // (3100-30)=3070 >= HOLD_MS(3000) → hold fires, click_count=0
  release(btn, 3200);     // hold_fired=true → no click counted
  idle(btn, 4000);        // click_count still 0 → nothing fires

  TEST_ASSERT_EQUAL(1, hold_count);
  TEST_ASSERT_EQUAL(0, sp_count);
}

void test_hold_resets_accumulated_clicks() {
  // One click lands, then the second press becomes a hold.
  // The pending click_count should be reset by the hold.
  GestureButton btn(BTN, onSP, onDP, onTP, onHold);

  press(btn, 0);
  release(btn, 100);      // click_count=1

  // Second press — still within CLICK_GAP_MS
  mock_set_pin(BTN, LOW);
  btn.update(200);
  btn.update(230);        // settled, _press_start_ms=230

  // Hold fires mid-press
  idle(btn, 3300);        // (3300-230)=3070 >= 3000 → hold fires, click_count=0

  release(btn, 3400);
  idle(btn, 4000);

  TEST_ASSERT_EQUAL(1, hold_count);
  TEST_ASSERT_EQUAL(0, dp_count);   // the pending click was discarded
  TEST_ASSERT_EQUAL(0, sp_count);
}

void test_debounce_ignores_short_bounce() {
  // Pin glitches LOW for only 10 ms — never reaches DEBOUNCE_MS — no click.
  GestureButton btn(BTN, onSP, onDP, onTP, onHold);

  mock_set_pin(BTN, LOW);
  btn.update(0);          // raw edge at t=0 → _last_change_ms=0

  mock_set_pin(BTN, HIGH);
  btn.update(10);         // raw edge at t=10 → _last_change_ms=10
                          // (10-10)=0 < 25 → debounce not done, no state change

  // After settling the HIGH (non-pressed) state
  btn.update(50);         // (50-10)=40 >= 25, reading=HIGH, _pressed=false → no change
  idle(btn, 600);         // nothing in the pipeline

  TEST_ASSERT_EQUAL(0, sp_count);
  TEST_ASSERT_EQUAL(0, dp_count);
}

void test_is_pressed_reflects_debounced_state() {
  GestureButton btn(BTN, onSP, onDP, onTP, onHold);

  TEST_ASSERT_FALSE(btn.isPressed());

  // Raw press — before debounce
  mock_set_pin(BTN, LOW);
  btn.update(0);
  TEST_ASSERT_FALSE(btn.isPressed());  // debounce not done yet

  // After debounce
  btn.update(30);
  TEST_ASSERT_TRUE(btn.isPressed());

  // Release and settle
  mock_set_pin(BTN, HIGH);
  btn.update(100);
  btn.update(130);
  TEST_ASSERT_FALSE(btn.isPressed());
}

void test_click_gap_defaults() {
  // Verifies the public tuning constants have their documented defaults.
  GestureButton btn(BTN, onSP, onDP, onTP, onHold);
  TEST_ASSERT_EQUAL(25,   btn.DEBOUNCE_MS);
  TEST_ASSERT_EQUAL(400,  btn.CLICK_GAP_MS);
  TEST_ASSERT_EQUAL(3000, btn.HOLD_MS);
}

// ── Runner ───────────────────────────────────────────────────────────────────
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sp_fires_after_single_click_and_gap);
  RUN_TEST(test_dp_fires_after_two_clicks_and_gap);
  RUN_TEST(test_tp_fires_after_three_clicks);
  RUN_TEST(test_hold_fires_and_suppresses_click);
  RUN_TEST(test_hold_resets_accumulated_clicks);
  RUN_TEST(test_debounce_ignores_short_bounce);
  RUN_TEST(test_is_pressed_reflects_debounced_state);
  RUN_TEST(test_click_gap_defaults);
  return UNITY_END();
}
