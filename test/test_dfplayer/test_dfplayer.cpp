/*
  test_dfplayer.cpp — Unit tests for DFPlayerController.

  Runs on the host machine (pio test -e native).
  Tests volume clamping, initial state, begin() success/failure, playback state
  tracking, auto-next on track-end, and playlist wrap-around.

  The DFRobotDFPlayerMini library is replaced by test/mocks/DFRobotDFPlayerMini.h
  (a controllable stub), and Arduino.h is replaced by test/mocks/Arduino.h.

  Important: DFPlayerController.h defines a global `DFPlayer(Serial1)` at its
  bottom (same pattern as Buzzer in main.cpp). That global is constructed but
  not used in these tests — each test creates its own fresh local instance.

  Timing note: step() has a 500 ms poll rate-limit using millis().
  Call mock_set_millis(600) before the first step() and advance by 500+ for
  each subsequent call so the rate-limit doesn't swallow events.
*/

#include <unity.h>
#include <DFPlayerController.h>

// ── Test fixture helpers ─────────────────────────────────────────────────────
// Fresh stream + controller for every test.
struct TestStream : public Stream {};

static TestStream ts;

// setUp / tearDown called by Unity before/after every test function.
void setUp() {
  mock_reset();
  mock_dfplayer_reset();
}
void tearDown() {}

// ── State before begin() ─────────────────────────────────────────────────────

void test_initial_not_ok() {
  DFPlayerController ctrl(ts);
  TEST_ASSERT_FALSE(ctrl.isOK());
}

void test_initial_not_playing() {
  DFPlayerController ctrl(ts);
  TEST_ASSERT_FALSE(ctrl.isPlaying());
}

void test_initial_no_track_changed() {
  DFPlayerController ctrl(ts);
  TEST_ASSERT_FALSE(ctrl.trackChanged());
}

void test_initial_folder_and_track() {
  DFPlayerController ctrl(ts);
  TEST_ASSERT_EQUAL(1, ctrl.currentFolder());
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());
}

void test_initial_volume_is_30() {
  DFPlayerController ctrl(ts);
  TEST_ASSERT_EQUAL(30, ctrl.volume());
}

void test_playing_for_ms_is_zero_when_not_playing() {
  DFPlayerController ctrl(ts);
  TEST_ASSERT_EQUAL(0, ctrl.playingForMs());
}

// ── begin() ─────────────────────────────────────────────────────────────────

void test_begin_ok_sets_isOK() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  TEST_ASSERT_TRUE(ctrl.isOK());
}

void test_begin_failure_leaves_not_ok() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(false);
  ctrl.begin(20);
  TEST_ASSERT_FALSE(ctrl.isOK());
}

void test_begin_sets_volume() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(15);
  TEST_ASSERT_EQUAL(15, ctrl.volume());
}

// ── setVolume() clamping ─────────────────────────────────────────────────────

void test_set_volume_normal() {
  DFPlayerController ctrl(ts);
  ctrl.setVolume(20);
  TEST_ASSERT_EQUAL(20, ctrl.volume());
}

void test_set_volume_clamps_above_30() {
  DFPlayerController ctrl(ts);
  ctrl.setVolume(99);
  TEST_ASSERT_EQUAL(30, ctrl.volume());
}

void test_set_volume_zero() {
  DFPlayerController ctrl(ts);
  ctrl.setVolume(0);
  TEST_ASSERT_EQUAL(0, ctrl.volume());
}

// ── volumeUp / volumeDown ────────────────────────────────────────────────────

void test_volume_up_increments() {
  DFPlayerController ctrl(ts);
  ctrl.setVolume(10);
  ctrl.volumeUp();
  TEST_ASSERT_EQUAL(11, ctrl.volume());
}

void test_volume_up_caps_at_30() {
  DFPlayerController ctrl(ts);
  ctrl.setVolume(30);
  ctrl.volumeUp();
  TEST_ASSERT_EQUAL(30, ctrl.volume());   // no overflow
}

void test_volume_down_decrements() {
  DFPlayerController ctrl(ts);
  ctrl.setVolume(10);
  ctrl.volumeDown();
  TEST_ASSERT_EQUAL(9, ctrl.volume());
}

void test_volume_down_floors_at_0() {
  DFPlayerController ctrl(ts);
  ctrl.setVolume(0);
  ctrl.volumeDown();
  TEST_ASSERT_EQUAL(0, ctrl.volume());    // no underflow
}

// ── playFolder() ─────────────────────────────────────────────────────────────

void test_play_folder_sets_playing() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(2, 3);
  TEST_ASSERT_TRUE(ctrl.isPlaying());
}

void test_play_folder_sets_folder_and_track() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(2, 5);
  TEST_ASSERT_EQUAL(2, ctrl.currentFolder());
  TEST_ASSERT_EQUAL(5, ctrl.currentTrack());
}

void test_play_folder_sets_track_changed() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 1);
  TEST_ASSERT_TRUE(ctrl.trackChanged());
}

void test_playing_for_ms_increases_while_playing() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);

  mock_set_millis(1000);
  ctrl.playFolder(1, 1);   // _play_start_ms = millis() = 1000

  mock_set_millis(1500);
  TEST_ASSERT_EQUAL(500, ctrl.playingForMs());
}

// ── pause() / resume() ───────────────────────────────────────────────────────

void test_pause_stops_playing() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 1);
  ctrl.pause();
  TEST_ASSERT_FALSE(ctrl.isPlaying());
}

void test_resume_restarts_playing() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 1);
  ctrl.pause();
  ctrl.resume();
  TEST_ASSERT_TRUE(ctrl.isPlaying());
}

void test_playing_for_ms_is_zero_while_paused() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 1);
  ctrl.pause();
  TEST_ASSERT_EQUAL(0, ctrl.playingForMs());
}

// ── next() / previous() ──────────────────────────────────────────────────────

void test_next_advances_track() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 3);
  ctrl.next();
  TEST_ASSERT_EQUAL(4, ctrl.currentTrack());
}

void test_previous_goes_back() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 3);
  ctrl.previous();
  TEST_ASSERT_EQUAL(2, ctrl.currentTrack());
}

void test_previous_no_op_at_track_1() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 1);
  ctrl.previous();
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());   // stays at 1
}

// ── step() + auto-next ───────────────────────────────────────────────────────

void test_step_auto_next_on_track_finished() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 1);
  ctrl.setAutoNext(true);

  // Feed a DFPlayerPlayFinished event, then call step() past the poll rate-limit.
  mock_dfplayer_send_event(DFPlayerPlayFinished);
  mock_set_millis(600);    // > 500 ms poll interval
  ctrl.step();

  // Controller should have auto-advanced to track 2.
  TEST_ASSERT_EQUAL(2, ctrl.currentTrack());
  TEST_ASSERT_TRUE(ctrl.isPlaying());
  TEST_ASSERT_TRUE(ctrl.trackChanged());
}

void test_step_no_auto_next_when_disabled() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 1);
  ctrl.setAutoNext(false);

  mock_dfplayer_send_event(DFPlayerPlayFinished);
  mock_set_millis(600);
  ctrl.step();

  // Track should not advance.
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());
  TEST_ASSERT_FALSE(ctrl.isPlaying());
}

void test_step_skipped_when_not_ok() {
  // begin() not called — _ok=false; step() should do nothing, not crash.
  DFPlayerController ctrl(ts);
  mock_dfplayer_send_event(DFPlayerPlayFinished);
  mock_set_millis(600);
  ctrl.step();   // must not crash or change state

  TEST_ASSERT_FALSE(ctrl.isOK());
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());
}

void test_step_rate_limited_below_500ms() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(1, 1);
  ctrl.setAutoNext(true);

  mock_dfplayer_send_event(DFPlayerPlayFinished);
  // Poll at only 300 ms — rate-limit not yet reached
  mock_set_millis(300);
  ctrl.step();

  // Event was NOT consumed — track still 1
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());
}

// ── Runner ───────────────────────────────────────────────────────────────────
int main() {
  UNITY_BEGIN();

  RUN_TEST(test_initial_not_ok);
  RUN_TEST(test_initial_not_playing);
  RUN_TEST(test_initial_no_track_changed);
  RUN_TEST(test_initial_folder_and_track);
  RUN_TEST(test_initial_volume_is_30);
  RUN_TEST(test_playing_for_ms_is_zero_when_not_playing);

  RUN_TEST(test_begin_ok_sets_isOK);
  RUN_TEST(test_begin_failure_leaves_not_ok);
  RUN_TEST(test_begin_sets_volume);

  RUN_TEST(test_set_volume_normal);
  RUN_TEST(test_set_volume_clamps_above_30);
  RUN_TEST(test_set_volume_zero);
  RUN_TEST(test_volume_up_increments);
  RUN_TEST(test_volume_up_caps_at_30);
  RUN_TEST(test_volume_down_decrements);
  RUN_TEST(test_volume_down_floors_at_0);

  RUN_TEST(test_play_folder_sets_playing);
  RUN_TEST(test_play_folder_sets_folder_and_track);
  RUN_TEST(test_play_folder_sets_track_changed);
  RUN_TEST(test_playing_for_ms_increases_while_playing);

  RUN_TEST(test_pause_stops_playing);
  RUN_TEST(test_resume_restarts_playing);
  RUN_TEST(test_playing_for_ms_is_zero_while_paused);

  RUN_TEST(test_next_advances_track);
  RUN_TEST(test_previous_goes_back);
  RUN_TEST(test_previous_no_op_at_track_1);

  RUN_TEST(test_step_auto_next_on_track_finished);
  RUN_TEST(test_step_no_auto_next_when_disabled);
  RUN_TEST(test_step_skipped_when_not_ok);
  RUN_TEST(test_step_rate_limited_below_500ms);

  return UNITY_END();
}
