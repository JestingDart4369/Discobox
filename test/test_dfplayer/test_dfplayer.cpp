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
// ── Track index: skip tracks that are missing on the card ───────────────────
// Fake index for folder 2: tracks 1, 2, 3, 9, 13 exist (4..8 and 10..12 missing).
static int16_t fakeIndex(uint8_t folder, uint8_t track, int8_t dir) {
  static const uint8_t EXIST[] = { 1, 2, 3, 9, 13 };
  if (folder != 2) { int16_t t = track + dir; return (t >= 1) ? t : -1; }
  int16_t best = -1;
  for (uint8_t e : EXIST) {
    if (dir > 0 && e > track && (best < 0 || e < best)) best = e;
    if (dir < 0 && e < track && (best < 0 || e > best)) best = e;
  }
  return best;
}

static void startIndexed(DFPlayerController& ctrl, uint8_t track) {
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.setTrackIndex(fakeIndex);
  ctrl.playFolder(2, track);
}

void test_index_next_skips_gap() {
  DFPlayerController ctrl(ts);
  startIndexed(ctrl, 3);
  ctrl.next();
  TEST_ASSERT_EQUAL(9, ctrl.currentTrack());     // 4..8 don't exist
}

void test_index_previous_skips_gap() {
  DFPlayerController ctrl(ts);
  startIndexed(ctrl, 13);
  ctrl.previous();
  TEST_ASSERT_EQUAL(9, ctrl.currentTrack());     // 10..12 don't exist
}

void test_index_previous_stays_at_first() {
  DFPlayerController ctrl(ts);
  startIndexed(ctrl, 1);
  ctrl.previous();
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());
}

void test_index_next_wraps_after_last() {
  DFPlayerController ctrl(ts);
  startIndexed(ctrl, 13);
  ctrl.next();
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());     // last -> first, no missing file asked for
}

void test_index_auto_next_skips_gap() {
  DFPlayerController ctrl(ts);
  startIndexed(ctrl, 9);
  ctrl.setAutoNext(true);
  mock_dfplayer_send_event(DFPlayerPlayFinished);
  mock_set_millis(600);
  ctrl.step();
  TEST_ASSERT_EQUAL(13, ctrl.currentTrack());    // 9 finished -> 13 (10..12 missing)
  TEST_ASSERT_TRUE(ctrl.trackChanged());
}

void test_index_unindexed_folder_plain_steps() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.setTrackIndex(fakeIndex);
  ctrl.playFolder(5, 4);                         // folder 5 has no index
  ctrl.next();
  TEST_ASSERT_EQUAL(5, ctrl.currentTrack());
}

// ── Folder loop: the DFPlayer moves on by itself, we follow its "finished" events ──
static void startLoop(DFPlayerController& ctrl) {
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.setTrackIndex(fakeIndex);
  mock_set_millis(1000);
  ctrl.playFolderLoop(2);
}
static void finishAt(DFPlayerController& ctrl, unsigned long ms) {
  mock_dfplayer_send_event(DFPlayerPlayFinished);
  mock_set_millis(ms);
  ctrl.step();
}

void test_loop_starts_at_first_track() {
  DFPlayerController ctrl(ts);
  startLoop(ctrl);
  TEST_ASSERT_TRUE(ctrl.isLooping());
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());
}

void test_loop_finished_means_next_song() {
  DFPlayerController ctrl(ts);
  startLoop(ctrl);
  finishAt(ctrl, 200000);
  TEST_ASSERT_EQUAL(2, ctrl.currentTrack());
  TEST_ASSERT_TRUE(ctrl.isPlaying());              // NOT stopped
  TEST_ASSERT_TRUE(ctrl.trackChanged());
  TEST_ASSERT_EQUAL(0, ctrl.playingForMs());       // clock restarted for the new song
}

void test_loop_skips_gap_and_wraps() {
  DFPlayerController ctrl(ts);
  startLoop(ctrl);
  finishAt(ctrl, 200000);   // 1 -> 2
  finishAt(ctrl, 400000);   // 2 -> 3
  finishAt(ctrl, 600000);   // 3 -> 9  (4..8 missing)
  TEST_ASSERT_EQUAL(9, ctrl.currentTrack());
  finishAt(ctrl, 800000);   // 9 -> 13
  finishAt(ctrl, 1000000);  // 13 -> 1 (wrap, like the DFPlayer)
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());
  TEST_ASSERT_TRUE(ctrl.isLooping());
}

void test_loop_duplicate_finished_ignored() {
  DFPlayerController ctrl(ts);
  startLoop(ctrl);
  finishAt(ctrl, 200000);   // 1 -> 2
  finishAt(ctrl, 200600);   // same event again 0.6 s later (clone quirk)
  TEST_ASSERT_EQUAL(2, ctrl.currentTrack());
}

void test_loop_survives_next() {
  DFPlayerController ctrl(ts);
  startLoop(ctrl);
  ctrl.next();                                     // manual skip inside the loop
  TEST_ASSERT_EQUAL(2, ctrl.currentTrack());
  TEST_ASSERT_TRUE(ctrl.isLooping());
  finishAt(ctrl, 200000);                          // the firmware continues the loop
  TEST_ASSERT_EQUAL(3, ctrl.currentTrack());
  TEST_ASSERT_TRUE(ctrl.isPlaying());
}

void test_loop_is_firmware_driven() {
  // The DFPlayer's own loop (loopFolder) plays in card order, not number order
  // (on the Discobox card it started with 044). So the firmware must start
  // EVERY song itself, by its number.
  DFPlayerController ctrl(ts);
  startLoop(ctrl);
  TEST_ASSERT_EQUAL(0, mock_dfplayer_loop_folder());        // loopFolder() never used
  TEST_ASSERT_EQUAL(2, mock_dfplayer_last_folder());
  TEST_ASSERT_EQUAL(1, mock_dfplayer_last_track());         // started with 001 by number
  finishAt(ctrl, 200000);
  TEST_ASSERT_EQUAL(2, mock_dfplayer_last_track());         // next song requested by number
  finishAt(ctrl, 400000);
  finishAt(ctrl, 600000);
  TEST_ASSERT_EQUAL(9, mock_dfplayer_last_track());         // gap skipped: 3 -> 9
  TEST_ASSERT_EQUAL(ctrl.currentTrack(), mock_dfplayer_last_track());   // shown == played
}

void test_play_folder_ends_loop() {
  DFPlayerController ctrl(ts);
  startLoop(ctrl);
  ctrl.playFolder(2, 3);
  TEST_ASSERT_FALSE(ctrl.isLooping());
  finishAt(ctrl, 200000);
  TEST_ASSERT_FALSE(ctrl.isPlaying());             // single track: finished = stopped
  TEST_ASSERT_EQUAL(3, ctrl.currentTrack());
}

// ── Card-order map: file number -> what is SENT to the module ───────────────
static uint8_t rotateMap(uint8_t folder, uint8_t track) {      // like the real fix, files 1..13 of fakeIndex
  static const uint8_t EXIST[] = { 1, 2, 3, 9, 13 };          // card order: 13 first, then 1, 2, 3, 9
  if (folder != 2) return track;
  for (uint8_t r = 0; r < 5; r++) if (EXIST[r] == track) return (uint8_t)(((r + 1) % 5) + 1);
  return track;
}

void test_map_translates_every_play() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.setTrackIndex(fakeIndex);
  ctrl.setTrackMap(rotateMap);
  ctrl.playFolder(2, 1);
  TEST_ASSERT_EQUAL(1, ctrl.currentTrack());                  // logic keeps the FILE number
  TEST_ASSERT_EQUAL(2, mock_dfplayer_last_track());           // module gets the CARD position
  ctrl.next();                                                // 1 -> 2
  TEST_ASSERT_EQUAL(3, mock_dfplayer_last_track());
  ctrl.playFolder(2, 13);
  TEST_ASSERT_EQUAL(1, mock_dfplayer_last_track());           // 13 lies first on the card
  mock_set_millis(1000);
  ctrl.playFolderLoop(2);                                     // loop also goes through the map
  TEST_ASSERT_EQUAL(2, mock_dfplayer_last_track());
  finishAt(ctrl, 200000);
  TEST_ASSERT_EQUAL(2, ctrl.currentTrack());
  TEST_ASSERT_EQUAL(3, mock_dfplayer_last_track());
}

void test_no_map_sends_numbers() {
  DFPlayerController ctrl(ts);
  mock_dfplayer_set_begin_ok(true);
  ctrl.begin(20);
  ctrl.playFolder(2, 13);
  TEST_ASSERT_EQUAL(13, mock_dfplayer_last_track());
}

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

  RUN_TEST(test_index_next_skips_gap);
  RUN_TEST(test_index_previous_skips_gap);
  RUN_TEST(test_index_previous_stays_at_first);
  RUN_TEST(test_index_next_wraps_after_last);
  RUN_TEST(test_index_auto_next_skips_gap);
  RUN_TEST(test_index_unindexed_folder_plain_steps);
  RUN_TEST(test_loop_starts_at_first_track);
  RUN_TEST(test_loop_finished_means_next_song);
  RUN_TEST(test_loop_skips_gap_and_wraps);
  RUN_TEST(test_loop_duplicate_finished_ignored);
  RUN_TEST(test_loop_survives_next);
  RUN_TEST(test_loop_is_firmware_driven);
  RUN_TEST(test_play_folder_ends_loop);
  RUN_TEST(test_map_translates_every_play);
  RUN_TEST(test_no_map_sends_numbers);

  return UNITY_END();
}
