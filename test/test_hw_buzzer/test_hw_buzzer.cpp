/*
  test_hw_buzzer.cpp — on-device test for the piezo buzzer: songs + UI cues (runs ON the Uno R4).
  Run: pio test -e uno_r4_wifi_test -f test_hw_buzzer -v
       (-v shows the INFO lines telling you what you should hear)

  Uses the real speakerController (D9) + BuzzerPlayer + SONGLIST, exactly as
  main.cpp wires them. Song 0 is always "Example Scale" (C major scale up and
  down, last note starts at 3000 ms and lasts 360 ms -> song is 3360 ms long).
*/

#include <Arduino.h>
#include <unity.h>
#include <Logger.h>
#include <SongPlayer.h>

speakerController speaker(D9);           // same pin as main.cpp
#include "Buzzer/Buzzer_Player.h"        // creates the global `Buzzer`, uses `speaker`

static const uint32_t EXAMPLE_LEN_MS = 3000 + 360;   // last note time + its duration

// ── helpers ─────────────────────────────────────────────────────────────────
// Run Buzzer.step() for `ms` milliseconds (what loop() does in main.cpp)
static void pump(uint32_t ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) Buzzer.step();
}

// Run Buzzer.step() until nothing plays any more. Returns how long that took,
// or timeout_ms + 1 if it was still playing at the timeout.
static uint32_t pumpUntilSilent(uint32_t timeout_ms) {
  unsigned long t0 = millis();
  while (millis() - t0 <= timeout_ms) {
    Buzzer.step();
    if (!Buzzer.isPlaying()) return millis() - t0;
  }
  return timeout_ms + 1;
}

void setUp() {
  Buzzer.stop();
  Buzzer.autoNext(false);
  Buzzer.setCueGap(0, 0);
}
void tearDown() { Buzzer.stop(); }

// ── speaker basics ──────────────────────────────────────────────────────────
void test_midi_to_frequency() {
  TEST_ASSERT_EQUAL_UINT16(440, speakerController::midiToFreq(69));   // A4
  TEST_ASSERT_EQUAL_UINT16(262, speakerController::midiToFreq(60));   // middle C
  TEST_ASSERT_EQUAL_UINT16(880, speakerController::midiToFreq(81));   // A5
}

void test_tone_440_audible() {
  TEST_MESSAGE("You should hear a 440 Hz beep (A4) for 0.5 s");
  speaker.playTone(440, 500);
  // With BUZZER_PWM the tone only stops while step() runs (PwmOut has no
  // built-in duration) — so step() while waiting instead of delay().
  unsigned long t0 = millis();
  while (millis() - t0 < 700) speaker.step();
  TEST_ASSERT_FALSE_MESSAGE(speaker.isSounding(), "tone still sounding after its 500 ms");
}

// ── song list ───────────────────────────────────────────────────────────────
void test_songlist_has_example_first() {
  TEST_ASSERT_TRUE(Buzzer.songCount() >= 1);
  TEST_ASSERT_EQUAL_STRING("Example Scale", SONGLIST[0].name);
  char msg[64];
  for (uint8_t i = 0; i < Buzzer.songCount(); i++) {
    snprintf(msg, sizeof(msg), "song %u: %s (%u notes)", i, SONGLIST[i].name, (unsigned)SONGLIST[i].count);
    TEST_MESSAGE(msg);
    TEST_ASSERT_TRUE_MESSAGE(SONGLIST[i].count > 0, "song has no notes");
  }
}

// ── song playback ───────────────────────────────────────────────────────────
void test_play_starts_song() {
  Buzzer.play(0);
  TEST_ASSERT_TRUE(Buzzer.isPlaying());
  TEST_ASSERT_EQUAL_UINT8(0, Buzzer.songIndex());
}

void test_song_plays_full_length() {
  TEST_MESSAGE("You should hear the C major scale up and down (~3.4 s)");
  Buzzer.play(0);
  uint32_t t = pumpUntilSilent(6000);
  char msg[80];
  snprintf(msg, sizeof(msg), "song stopped after %lu ms, expected ~%lu ms (is the last note cut off?)",
           (unsigned long)t, (unsigned long)EXAMPLE_LEN_MS);
  TEST_ASSERT_TRUE_MESSAGE(t <= 6000, "song never stopped");
  TEST_ASSERT_UINT32_WITHIN_MESSAGE(100, EXAMPLE_LEN_MS, t, msg);
}

void test_stop_silences() {
  Buzzer.play(0);
  pump(300);
  Buzzer.stop();
  TEST_ASSERT_FALSE(Buzzer.isPlaying());
  pump(300);
  TEST_ASSERT_FALSE_MESSAGE(Buzzer.isPlaying(), "song came back after stop()");
}

void test_select_next_previous_wrap() {
  Buzzer.select(0);
  TEST_ASSERT_FALSE_MESSAGE(Buzzer.isPlaying(), "select() must not start music when stopped");
  Buzzer.previous();
  TEST_ASSERT_EQUAL_UINT8(Buzzer.songCount() - 1, Buzzer.songIndex());   // wraps to last
  Buzzer.next();
  TEST_ASSERT_EQUAL_UINT8(0, Buzzer.songIndex());                        // wraps back to 0
  TEST_ASSERT_FALSE(Buzzer.isPlaying());
}

void test_select_out_of_range_ignored() {
  Buzzer.select(0);
  Buzzer.select(Buzzer.songCount());   // one past the end
  TEST_ASSERT_EQUAL_UINT8(0, Buzzer.songIndex());
}

void test_auto_next_goes_to_next_song() {
  if (Buzzer.songCount() < 2) TEST_IGNORE_MESSAGE("only 1 song on this build");
  Buzzer.autoNext(true);
  Buzzer.play(0);
  unsigned long t0 = millis();
  while (millis() - t0 < 6000 && Buzzer.songIndex() == 0) Buzzer.step();
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, Buzzer.songIndex(), "auto-next did not move to song 1");
  TEST_ASSERT_TRUE(Buzzer.isPlaying());
}

// ── UI cues ─────────────────────────────────────────────────────────────────
void test_cue_plays_and_ends() {
  TEST_MESSAGE("You should hear the 'press' cue");
  Buzzer.playCue("press");
  TEST_ASSERT_TRUE(Buzzer.isPlaying());
  TEST_ASSERT_TRUE_MESSAGE(pumpUntilSilent(2000) <= 2000, "cue did not finish within 2 s");
}

void test_unknown_cue_is_silent() {
  Buzzer.playCue("does-not-exist");
  TEST_ASSERT_FALSE(Buzzer.isPlaying());
}

void test_mute_blocks_cues() {
  Buzzer.setMuted(true);
  Buzzer.playCue("press");
  bool playing = Buzzer.isPlaying();
  Buzzer.setMuted(false);
  TEST_ASSERT_FALSE_MESSAGE(playing, "cue played while muted");
}

void test_mute_does_not_stop_songs() {
  Buzzer.setMuted(true);
  Buzzer.play(0);
  bool playing = Buzzer.isPlaying();
  Buzzer.setMuted(false);
  TEST_ASSERT_TRUE_MESSAGE(playing, "mute must only affect cues, not songs");
}

void test_cue_ducks_and_song_resumes() {
  TEST_MESSAGE("Scale starts, gets interrupted by a cue, then continues");
  Buzzer.play(0);
  pump(800);                        // ~4 notes into the scale
  Buzzer.playCue("press");          // song paused, cue plays
  pump(1200);                       // cue is over, song resumed
  TEST_ASSERT_TRUE_MESSAGE(Buzzer.isPlaying(), "song did not resume after the cue");
  TEST_ASSERT_EQUAL_UINT8(0, Buzzer.songIndex());
  pumpUntilSilent(4000);
}

void test_cue_gap_delays_cue() {
  // With a 300 ms "before" gap the cue must start ~300 ms after playCue(), not at once.
  Buzzer.setCueGap(300, 0);
  unsigned long t0 = millis();
  Buzzer.playCue("press");
  TEST_ASSERT_FALSE_MESSAGE(Buzzer.isPlaying(), "cue started immediately, gap ignored");
  while (millis() - t0 < 1000 && !Buzzer.isPlaying()) Buzzer.step();
  uint32_t started = millis() - t0;
  char msg[64];
  snprintf(msg, sizeof(msg), "cue started after %lu ms, expected ~300 ms", (unsigned long)started);
  TEST_ASSERT_TRUE_MESSAGE(Buzzer.isPlaying(), "cue never started");
  TEST_ASSERT_UINT32_WITHIN_MESSAGE(30, 300, started, msg);
  pumpUntilSilent(2000);
}

// ── volume (needs -DBUZZER_PWM, set in platformio.ini) ──────────────────────
void test_music_volume_levels_audible() {
  TEST_MESSAGE("Scale at music volume 8 (full), then 5, then 2 - should get quieter");
  static const uint8_t LEVELS[] = { 8, 5, 2 };
  for (uint8_t i = 0; i < 3; i++) {
    Buzzer.setMusicVolume(LEVELS[i]);
    Buzzer.play(0);
    TEST_ASSERT_EQUAL_UINT8(LEVELS[i], speaker.volume());
    pumpUntilSilent(4000);
    delay(500);
  }
  Buzzer.setMusicVolume(SPEAKER_MAX_VOLUME);
}

void test_cue_volume_separate_from_music() {
  TEST_MESSAGE("Scale at full volume, interrupted by a QUIET cue (level 2), then full again");
  Buzzer.setMusicVolume(SPEAKER_MAX_VOLUME);
  Buzzer.setCueVolume(2);
  Buzzer.play(0);
  pump(800);
  Buzzer.playCue("press");
  TEST_ASSERT_EQUAL_UINT8(2, speaker.volume());                  // cue plays at cue volume
  pump(1200);
  TEST_ASSERT_EQUAL_UINT8(SPEAKER_MAX_VOLUME, speaker.volume()); // song back at music volume
  pumpUntilSilent(4000);
  Buzzer.setCueVolume(SPEAKER_MAX_VOLUME);
}

void test_cue_volume_zero_is_silent() {
  Buzzer.setCueVolume(0);
  Buzzer.playCue("press");
  TEST_ASSERT_FALSE_MESSAGE(Buzzer.isPlaying(), "cue played at volume 0");
  Buzzer.setCueVolume(SPEAKER_MAX_VOLUME);
}

void setup() {
  delay(2000);                      // R4 needs a moment before USB serial is ready
  UNITY_BEGIN();                    // starts Serial at test_speed (115200)
  speaker.setup();

  RUN_TEST(test_midi_to_frequency);
  RUN_TEST(test_tone_440_audible);
  RUN_TEST(test_songlist_has_example_first);
  RUN_TEST(test_play_starts_song);
  RUN_TEST(test_song_plays_full_length);
  RUN_TEST(test_stop_silences);
  RUN_TEST(test_select_next_previous_wrap);
  RUN_TEST(test_select_out_of_range_ignored);
  RUN_TEST(test_auto_next_goes_to_next_song);
  RUN_TEST(test_cue_plays_and_ends);
  RUN_TEST(test_unknown_cue_is_silent);
  RUN_TEST(test_mute_blocks_cues);
  RUN_TEST(test_mute_does_not_stop_songs);
  RUN_TEST(test_cue_ducks_and_song_resumes);
  RUN_TEST(test_cue_gap_delays_cue);
  RUN_TEST(test_music_volume_levels_audible);
  RUN_TEST(test_cue_volume_separate_from_music);
  RUN_TEST(test_cue_volume_zero_is_silent);

  Buzzer.stop();
  UNITY_END();
}

void loop() {}
