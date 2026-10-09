/*
  test_song_player.cpp — Unit tests for speakerController (SongPlayer).

  Runs on the host machine (pio test -e native).
  Tests non-blocking playback state: play/stop, isPlaying(), elapsed(),
  step() advancing through notes, playFrom() resuming mid-song, and
  auto-stop when all notes have been played.

  tone() / noTone() / pinMode() are no-ops in the Arduino mock, so we test
  state transitions rather than hardware output.
*/

#include <unity.h>
#include <SongPlayer.h>

// ── Tiny melody used in all tests ────────────────────────────────────────────
// Three notes: C4 at 0 ms, D4 at 200 ms, E4 at 400 ms (each 100 ms long).
static const SongNote kMelody[] = {
  { 60, 100, 0   },   // C4
  { 62, 100, 200 },   // D4
  { 64, 200, 400 },   // E4
};
static const size_t kMelodyLen = sizeof(kMelody) / sizeof(kMelody[0]);

static const uint8_t SPK_PIN = 9;

void setUp()    { mock_reset(); }
void tearDown() {}

// ── Initial state ────────────────────────────────────────────────────────────

void test_initial_not_playing() {
  speakerController spk(SPK_PIN);
  TEST_ASSERT_FALSE(spk.isPlaying());
}

void test_initial_elapsed_is_zero() {
  speakerController spk(SPK_PIN);
  TEST_ASSERT_EQUAL(0, spk.elapsed());
}

// ── play() ───────────────────────────────────────────────────────────────────

void test_play_sets_playing() {
  speakerController spk(SPK_PIN);
  mock_set_millis(0);
  spk.play(kMelody, kMelodyLen);
  TEST_ASSERT_TRUE(spk.isPlaying());
}

void test_elapsed_increases_while_playing() {
  speakerController spk(SPK_PIN);
  mock_set_millis(1000);
  spk.play(kMelody, kMelodyLen);    // _start_ms = 1000
  mock_set_millis(1300);
  TEST_ASSERT_EQUAL(300, spk.elapsed());
}

void test_elapsed_is_zero_when_stopped() {
  speakerController spk(SPK_PIN);
  mock_set_millis(0);
  spk.play(kMelody, kMelodyLen);
  spk.stop();
  mock_set_millis(500);
  TEST_ASSERT_EQUAL(0, spk.elapsed());
}

// ── stop() ───────────────────────────────────────────────────────────────────

void test_stop_clears_playing() {
  speakerController spk(SPK_PIN);
  mock_set_millis(0);
  spk.play(kMelody, kMelodyLen);
  spk.stop();
  TEST_ASSERT_FALSE(spk.isPlaying());
}

// ── step() advances through notes and auto-stops ─────────────────────────────

void test_step_keeps_playing_mid_song() {
  speakerController spk(SPK_PIN);
  mock_set_millis(0);
  spk.play(kMelody, kMelodyLen);

  // Advance to 100 ms — first note is at 0 ms, second not until 200 ms.
  mock_set_millis(100);
  spk.step();
  TEST_ASSERT_TRUE(spk.isPlaying());
}

void test_step_auto_stops_after_last_note() {
  speakerController spk(SPK_PIN);
  mock_set_millis(0);
  spk.play(kMelody, kMelodyLen);

  // step() fires all notes whose time has come but does NOT call stop() inline —
  // stop() is triggered at the START of the next step() once _index >= _count.
  // So we need two calls: one to exhaust the note list, one to detect the end.
  mock_set_millis(700);
  spk.step();   // fires note[2] (last), _index = kMelodyLen
  spk.step();   // _index >= _count → stop()
  TEST_ASSERT_FALSE(spk.isPlaying());
}

void test_step_is_no_op_when_not_playing() {
  // Calling step() with nothing queued must not crash or change any state.
  speakerController spk(SPK_PIN);
  mock_set_millis(500);
  spk.step();   // _playing=false → returns immediately
  TEST_ASSERT_FALSE(spk.isPlaying());
}

// ── playFrom() resumes mid-song ───────────────────────────────────────────────

void test_play_from_sets_playing() {
  speakerController spk(SPK_PIN);
  mock_set_millis(0);
  spk.playFrom(kMelody, kMelodyLen, 250);  // resume from 250 ms in
  TEST_ASSERT_TRUE(spk.isPlaying());
}

void test_play_from_skips_early_notes() {
  // Resuming from 250 ms skips note[0] (at 0 ms) and note[1] (at 200 ms).
  // Only note[2] (at 400 ms) remains.  After step() advances past 400 ms the
  // song ends.  We verify it plays out — not stops immediately — at 250 ms.
  speakerController spk(SPK_PIN);
  mock_set_millis(0);
  spk.playFrom(kMelody, kMelodyLen, 250);  // skip notes 0 and 1
  // elapsed() = millis() - _start_ms; _start_ms = millis() - 250 = -250 (wraps)
  // At this moment elapsed() should equal 250.
  TEST_ASSERT_EQUAL(250, spk.elapsed());
  TEST_ASSERT_TRUE(spk.isPlaying());
}

void test_play_restarts_from_beginning() {
  // play() always resets the index — a second call starts over.
  speakerController spk(SPK_PIN);
  mock_set_millis(0);
  spk.play(kMelody, kMelodyLen);

  // Jump to after the song ends, so step() stops the player.
  mock_set_millis(700);
  spk.step();   // exhausts notes
  spk.step();   // detects _index >= _count → stop()
  TEST_ASSERT_FALSE(spk.isPlaying());

  // Now re-play from the start.
  mock_set_millis(800);
  spk.play(kMelody, kMelodyLen);
  TEST_ASSERT_TRUE(spk.isPlaying());
  TEST_ASSERT_EQUAL(0, spk.elapsed());
}

// ── Runner ───────────────────────────────────────────────────────────────────
int main() {
  UNITY_BEGIN();

  RUN_TEST(test_initial_not_playing);
  RUN_TEST(test_initial_elapsed_is_zero);

  RUN_TEST(test_play_sets_playing);
  RUN_TEST(test_elapsed_increases_while_playing);
  RUN_TEST(test_elapsed_is_zero_when_stopped);

  RUN_TEST(test_stop_clears_playing);

  RUN_TEST(test_step_keeps_playing_mid_song);
  RUN_TEST(test_step_auto_stops_after_last_note);
  RUN_TEST(test_step_is_no_op_when_not_playing);

  RUN_TEST(test_play_from_sets_playing);
  RUN_TEST(test_play_from_skips_early_notes);
  RUN_TEST(test_play_restarts_from_beginning);

  return UNITY_END();
}
