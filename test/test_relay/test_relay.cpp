/*
  test_relay.cpp — Unit tests for Relay (lib/Relay).
  Runs on the laptop (pio test -e native). digitalWrite() in the Arduino mock
  stores the level, so digitalRead() shows what the pin was driven to.
*/
#include <unity.h>
#include <Relay.h>

static const uint8_t PIN = 7;

void setUp()    { mock_reset(); }
void tearDown() {}

void test_nothing_written_before_begin() {
  Relay r(PIN);
  r.on();
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN));   // untouched mock default
  TEST_ASSERT_FALSE(r.isStarted());
}

void test_active_low_off_after_begin() {
  Relay r(PIN);
  mock_set_pin(PIN, LOW);
  r.begin();
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN));   // active-low: HIGH = off
  TEST_ASSERT_FALSE(r.isOn());
}

void test_active_low_on_off() {
  Relay r(PIN);
  r.begin();
  r.on();
  TEST_ASSERT_EQUAL(LOW, digitalRead(PIN));
  TEST_ASSERT_TRUE(r.isOn());
  r.off();
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN));
  TEST_ASSERT_FALSE(r.isOn());
}

void test_active_high() {
  Relay r(PIN, false);
  mock_set_pin(PIN, HIGH);
  r.begin();
  TEST_ASSERT_EQUAL(LOW, digitalRead(PIN));    // active-high: LOW = off
  r.on();
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN));
}

void test_toggle() {
  Relay r(PIN);
  r.begin();
  r.toggle();
  TEST_ASSERT_TRUE(r.isOn());
  TEST_ASSERT_EQUAL(LOW, digitalRead(PIN));
  r.toggle();
  TEST_ASSERT_FALSE(r.isOn());
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN));
}

void test_set_bool() {
  Relay r(PIN);
  r.begin();
  r.set(true);
  TEST_ASSERT_TRUE(r.isOn());
  r.set(false);
  TEST_ASSERT_FALSE(r.isOn());
}

void test_begin_again_switches_off() {
  Relay r(PIN);
  r.begin();
  r.on();
  r.begin();
  TEST_ASSERT_FALSE(r.isOn());
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN));
}

void test_disabled_ignores_on() {
  Relay r(PIN);
  r.begin();
  r.setEnabled(false);
  r.on();
  TEST_ASSERT_FALSE(r.isOn());
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN));
  r.setEnabled(true);
  r.on();
  TEST_ASSERT_TRUE(r.isOn());
}

void test_disabling_switches_off() {
  Relay r(PIN);
  r.begin();
  r.on();
  r.setEnabled(false);
  TEST_ASSERT_FALSE(r.isOn());
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN));
  TEST_ASSERT_FALSE(r.isEnabled());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_written_before_begin);
  RUN_TEST(test_active_low_off_after_begin);
  RUN_TEST(test_active_low_on_off);
  RUN_TEST(test_active_high);
  RUN_TEST(test_toggle);
  RUN_TEST(test_set_bool);
  RUN_TEST(test_begin_again_switches_off);
  RUN_TEST(test_disabled_ignores_on);
  RUN_TEST(test_disabling_switches_off);
  return UNITY_END();
}
