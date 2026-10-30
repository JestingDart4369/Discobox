/*
  test_hw_dfplayer.cpp — on-device test for the DFPlayer Mini MP3 module (runs ON the Uno R4).
  Run: pio test -e uno_r4_wifi_test -f test_hw_dfplayer -v
       (-v shows the INFO lines telling you what you should hear)

  Needs: DFPlayer on Serial1 (D0 = RX <- DFPlayer TX, D1 = TX -> DFPlayer RX via 1 kOhm),
         SD card inserted with at least /02/001.mp3 and /02/002.mp3 (see doc/SD_CARD.md).
  If the module is not found, test_module_found FAILS and every other test is IGNORED.
*/

#include <Arduino.h>
#include <unity.h>
#include <DFPlayerController.h>          // creates the global `DFPlayer` on Serial1

static const uint8_t TEST_FOLDER = 2;    // Party playlist folder (doc/SD_CARD.md)
static const uint8_t TEST_VOLUME = 15;   // quiet enough for a desk

// Run DFPlayer.step() for `ms` milliseconds (what loop() does in main.cpp)
static void pump(uint32_t ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) DFPlayer.step();
}

static void requireModule() {
  if (!DFPlayer.isOK()) TEST_IGNORE_MESSAGE("no DFPlayer found - skipped");
}

void setUp()    {}
void tearDown() { if (DFPlayer.isOK()) DFPlayer.pause(); }

void test_module_found() {
  TEST_ASSERT_TRUE_MESSAGE(DFPlayer.isOK(),
    "DFPlayer not found: check D0/D1 wiring (crossed RX/TX), 1 kOhm on D1, 5 V, GND, SD card");
}

void test_begin_sets_volume() {
  requireModule();
  TEST_ASSERT_EQUAL_UINT8(TEST_VOLUME, DFPlayer.volume());
}

void test_play_folder_track() {
  requireModule();
  TEST_MESSAGE("You should hear /02/001.mp3 for 4 s");
  DFPlayer.playFolder(TEST_FOLDER, 1);
  TEST_ASSERT_TRUE(DFPlayer.isPlaying());
  TEST_ASSERT_EQUAL_UINT8(TEST_FOLDER, DFPlayer.currentFolder());
  TEST_ASSERT_EQUAL_UINT8(1, DFPlayer.currentTrack());
  pump(4000);
  TEST_ASSERT_TRUE_MESSAGE(DFPlayer.isPlaying(), "track ended within 4 s (file missing or very short?)");
  TEST_ASSERT_UINT32_WITHIN(500, 4000, DFPlayer.playingForMs());
}

void test_pause_and_resume() {
  requireModule();
  DFPlayer.playFolder(TEST_FOLDER, 1);
  pump(2000);
  TEST_MESSAGE("Paused for 2 s - should be silent");
  DFPlayer.pause();
  TEST_ASSERT_FALSE(DFPlayer.isPlaying());
  TEST_ASSERT_EQUAL_UINT32(0, DFPlayer.playingForMs());
  pump(2000);
  TEST_MESSAGE("Resumed - should continue where it stopped");
  DFPlayer.resume();
  TEST_ASSERT_TRUE(DFPlayer.isPlaying());
  TEST_ASSERT_UINT32_WITHIN(300, 2000, DFPlayer.playingForMs());   // pause time not counted
  pump(2000);
}

void test_next_and_previous() {
  requireModule();
  DFPlayer.playFolder(TEST_FOLDER, 1);
  pump(1000);
  TEST_MESSAGE("Next track: /02/002.mp3");
  DFPlayer.next();
  TEST_ASSERT_EQUAL_UINT8(2, DFPlayer.currentTrack());
  pump(2500);
  TEST_MESSAGE("Previous track: back to /02/001.mp3");
  DFPlayer.previous();
  TEST_ASSERT_EQUAL_UINT8(1, DFPlayer.currentTrack());
  pump(2500);
  DFPlayer.previous();                                   // already at track 1
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, DFPlayer.currentTrack(), "previous() went below track 1");
}

void test_volume_quiet_then_loud() {
  requireModule();
  DFPlayer.playFolder(TEST_FOLDER, 1);
  TEST_MESSAGE("Volume 5 (quiet) for 2 s ...");
  DFPlayer.setVolume(5);
  TEST_ASSERT_EQUAL_UINT8(5, DFPlayer.volume());
  pump(2000);
  TEST_MESSAGE("... then volume 25 (loud) for 2 s");
  DFPlayer.setVolume(25);
  TEST_ASSERT_EQUAL_UINT8(25, DFPlayer.volume());
  pump(2000);
  DFPlayer.setVolume(TEST_VOLUME);
}

void test_volume_limits() {
  requireModule();
  DFPlayer.setVolume(40);
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(30, DFPlayer.volume(), "volume must clamp to 30");
  DFPlayer.volumeUp();
  TEST_ASSERT_EQUAL_UINT8(30, DFPlayer.volume());
  DFPlayer.setVolume(0);
  DFPlayer.volumeDown();
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, DFPlayer.volume(), "volumeDown went below 0");
  DFPlayer.setVolume(TEST_VOLUME);
}

void setup() {
  delay(2000);                         // R4 needs a moment before USB serial is ready
  UNITY_BEGIN();                       // starts Serial at test_speed (115200)
  Serial1.begin(9600);                 // DFPlayer UART, same as main.cpp
  DFPlayer.begin(TEST_VOLUME);         // blocks ~2 s while the module boots

  RUN_TEST(test_module_found);
  RUN_TEST(test_begin_sets_volume);
  RUN_TEST(test_play_folder_track);
  RUN_TEST(test_pause_and_resume);
  RUN_TEST(test_next_and_previous);
  RUN_TEST(test_volume_quiet_then_loud);
  RUN_TEST(test_volume_limits);

  if (DFPlayer.isOK()) DFPlayer.pause();
  UNITY_END();
}

void loop() {}
