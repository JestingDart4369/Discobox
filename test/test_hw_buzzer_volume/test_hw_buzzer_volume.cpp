/*
  test_hw_buzzer_volume.cpp — can the piezo on D9 play QUIETER with a shorter pulse?
  (runs ON the Uno R4; this is a LISTENING test to decide on a buzzer volume)

  Run: pio test -e uno_r4_wifi_test -f test_hw_buzzer_volume -v
       (-v shows the INFO lines: which level is playing right now)

  Idea: tone() always sends a 50 % square wave (fixed loudness). PwmOut (R4 core,
  pwm.h) can send the SAME frequency with a shorter "on" part (duty cycle).
  A shorter pulse = less energy into the piezo = quieter.

  What you hear, in this order:
    1. reference      — the phrase with tone() (today's loudness)
    2. level ladder   — the same phrase at duty 50 % → 0.5 % (10 levels, loud → quiet)
    3. pitch check    — a low, middle and high note at a quiet level (does it stay clean?)
    4. cue check      — a short "press" blip at every level (cues are very short)

  Write down: (a) from which level on it gets quieter at all, (b) which levels you
  can still tell apart, (c) where it gets too thin / crackly or silent.
  Those answers decide how many volume steps the real feature gets.
*/

#include <Arduino.h>
#include <unity.h>
#include "pwm.h"                        // PwmOut — Arduino UNO R4 core

static const uint8_t BUZZER_PIN = D9;   // same pin as speakerController in main.cpp

// Loud → quiet. 50 % is what tone() does.
static const float DUTY[] = { 50.0f, 30.0f, 20.0f, 12.0f, 8.0f, 5.0f, 3.0f, 2.0f, 1.0f, 0.5f };
static const uint8_t LEVELS = sizeof(DUTY) / sizeof(DUTY[0]);

// C5 E5 G5 C6 — a short, recognisable phrase
static const float PHRASE[] = { 523.25f, 659.25f, 783.99f, 1046.50f };
static const uint8_t PHRASE_LEN = sizeof(PHRASE) / sizeof(PHRASE[0]);

static PwmOut buzzerPwm(BUZZER_PIN);

// One note with a given duty. Returns false if the PWM could not be started.
static bool pwmNote(float hz, float dutyPerc, uint16_t ms) {
  bool ok = buzzerPwm.begin(hz, dutyPerc);
  delay(ms);
  buzzerPwm.end();
  return ok;
}

static bool pwmPhrase(float dutyPerc) {
  bool ok = true;
  for (uint8_t i = 0; i < PHRASE_LEN; i++) {
    ok = pwmNote(PHRASE[i], dutyPerc, 220) && ok;
    delay(30);
  }
  return ok;
}

void setUp()    { noTone(BUZZER_PIN); }
void tearDown() { buzzerPwm.end(); digitalWrite(BUZZER_PIN, LOW); }

// 1 ─────────────────────────────────────────────────────────────────────────
void test_reference_tone() {
  TEST_MESSAGE("REFERENCE: phrase with tone() = today's loudness");
  for (uint8_t i = 0; i < PHRASE_LEN; i++) {
    tone(BUZZER_PIN, (unsigned int)PHRASE[i], 220);
    delay(250);
  }
  noTone(BUZZER_PIN);
  delay(800);
}

// 2 ─────────────────────────────────────────────────────────────────────────
void test_pwm_starts_on_d9() {
  // If this fails, D9 can't be driven by PwmOut and the software route is out.
  TEST_ASSERT_TRUE_MESSAGE(pwmNote(440.0f, 50.0f, 300), "PwmOut.begin() failed on D9");
  delay(500);
}

void test_level_ladder() {
  char msg[64];
  bool allOk = true;
  for (uint8_t l = 0; l < LEVELS; l++) {
    unsigned t = (unsigned)(DUTY[l] * 10.0f + 0.5f);   // tenths (no %f: R4 printf may lack float)
    snprintf(msg, sizeof(msg), "level %u / %u  -  duty %u.%u %%", (unsigned)(l + 1), (unsigned)LEVELS, t / 10, t % 10);
    TEST_MESSAGE(msg);
    allOk = pwmPhrase(DUTY[l]) && allOk;
    delay(900);                        // pause so you can note what you heard
  }
  TEST_ASSERT_TRUE_MESSAGE(allOk, "PwmOut.begin() failed for at least one level");
}

// 3 ─────────────────────────────────────────────────────────────────────────
void test_pitch_at_quiet_level() {
  const float quiet = 5.0f;
  static const float NOTES[] = { 262.0f, 523.0f, 1047.0f, 2093.0f };   // C4 C5 C6 C7
  char msg[64];
  for (uint8_t i = 0; i < 4; i++) {
    snprintf(msg, sizeof(msg), "pitch check: %u Hz at duty %u %%", (unsigned)NOTES[i], (unsigned)quiet);
    TEST_MESSAGE(msg);
    TEST_ASSERT_TRUE(pwmNote(NOTES[i], quiet, 500));
    delay(400);
  }
}

// 4 ─────────────────────────────────────────────────────────────────────────
void test_short_cue_blips() {
  // UI cues are 30-80 ms blips — check they're still audible at low levels
  char msg[64];
  for (uint8_t l = 0; l < LEVELS; l++) {
    unsigned t = (unsigned)(DUTY[l] * 10.0f + 0.5f);
    snprintf(msg, sizeof(msg), "cue blip at level %u (duty %u.%u %%)", (unsigned)(l + 1), t / 10, t % 10);
    TEST_MESSAGE(msg);
    TEST_ASSERT_TRUE(pwmNote(1568.0f, DUTY[l], 40));   // G6, 40 ms
    delay(60);
    TEST_ASSERT_TRUE(pwmNote(2093.0f, DUTY[l], 60));   // C7, 60 ms
    delay(700);
  }
}

void setup() {
  delay(2000);                         // R4 needs a moment before USB serial is ready
  pinMode(BUZZER_PIN, OUTPUT);
  UNITY_BEGIN();
  RUN_TEST(test_reference_tone);
  RUN_TEST(test_pwm_starts_on_d9);
  RUN_TEST(test_level_ladder);
  RUN_TEST(test_pitch_at_quiet_level);
  RUN_TEST(test_short_cue_blips);
  UNITY_END();
}

void loop() {}
