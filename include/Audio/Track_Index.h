#ifndef TRACK_INDEX_H
#define TRACK_INDEX_H
/*
  Track_Index.h — WHAT is on the SD card / playing right now.

  The DFPlayer only ever reports numbers (folder 2, track 5). The names come
  from the index.txt files the microssd downloader writes next to the MP3s;
  scripts/gen_track_index.py turns them into Track_Index_Data.h on every
  `pio run` (folder: `custom_sd_root` in platformio.ini). No index found ->
  empty table, everything still works, only without names.

    const TrackInfo* t = TrackIndex::find(2, 5);   // nullptr if unknown
    t->title, t->artist, t->seconds

  Gaps: the card may miss tracks (e.g. 010-012). TrackIndex::step() gives the
  next track that really EXISTS, and DFPlayerController uses it for next /
  back / auto-next (setTrackIndex), so playback never stalls on a gap.
  Folders without an index fall back to plain +1 / -1.

  IMPORTANT: include in main.cpp AFTER DFPlayerController.h (for the watcher).
*/
#include <stdint.h>
#include <stdio.h>

/// One track from an SD folder's index.txt (all strings in flash).
struct TrackInfo {
  uint8_t     folder;     ///< SD folder 01..99
  uint8_t     track;      ///< file number 001..255
  uint16_t    seconds;    ///< length (0 = unknown)
  const char* title;      ///< cleaned title, UTF-8, max 60 bytes
  const char* artist;     ///< uploader, max 32 bytes
};

#if __has_include("Track_Index_Data.h")
  #include "Track_Index_Data.h"       // generated: TRACKS[], TRACK_COUNT
#else
  const TrackInfo TRACKS[] = { { 0, 0, 0, "", "" } };
  const uint16_t  TRACK_COUNT = 0;
#endif

// ── CARD ORDER FIX ──────────────────────────────────────────────────────────
// This DFPlayer clone plays the Nth file in CARD order (FAT order), not the file
// with the number "00N". So the firmware sends POSITIONS: the rank of a file in
// the index, rotated by the value below.
//
//   rotate = 1   current card: 044 Shout lies first, then 001..043
//                (file 001 -> position 2, 013 -> position 11, 044 -> position 1)
//   rotate = 0   after sorting the card (fatsort -n / copy in order)
//
// TEMPORARY until the card is sorted — then set it to 0 (keep the entry: with
// gaps like 010-012, even a sorted card needs positions, not numbers).
// Try other values live:  audio speaker --shift N   (not saved)
struct CardOrderFix { uint8_t folder; uint8_t rotate; };
static const CardOrderFix CARD_ORDER_FIX[] = {
  { 2, 1 },          // folder 02, Shout first   <- set to 0 once the card is sorted
};

namespace TrackIndex {

  /** @brief The index entry for folder/track, or nullptr if it isn't in the index. */
  inline const TrackInfo* find(uint8_t folder, uint8_t track) {
    for (uint16_t i = 0; i < TRACK_COUNT; i++)
      if (TRACKS[i].folder == folder && TRACKS[i].track == track) return &TRACKS[i];
    return nullptr;
  }

  /** @brief How many indexed tracks a folder has (0 = folder not indexed). */
  inline uint16_t countIn(uint8_t folder) {
    uint16_t n = 0;
    for (uint16_t i = 0; i < TRACK_COUNT; i++)
      if (TRACKS[i].folder == folder) n++;
    return n;
  }

  /**
   * @brief Next track that really exists after `track`, going `dir` (+1 / -1).
   * @return the track number, or -1 if there is none in that direction.
   * Folder without an index: plain track + dir (no knowledge about gaps).
   * Matches DFPlayerController::TrackStepFn.
   */
  inline int16_t step(uint8_t folder, uint8_t track, int8_t dir) {
    if (countIn(folder) == 0) {
      int16_t t = (int16_t)track + dir;
      return (t >= 1 && t <= 255) ? t : -1;
    }
    int16_t best = -1;
    for (uint16_t i = 0; i < TRACK_COUNT; i++) {
      if (TRACKS[i].folder != folder) continue;
      int16_t t = TRACKS[i].track;
      if (dir > 0 && t > track && (best < 0 || t < best)) best = t;
      if (dir < 0 && t < track && (best < 0 || t > best)) best = t;
    }
    return best;
  }

  // runtime value per folder (starts from CARD_ORDER_FIX, changed by --shift)
  inline int16_t& _rotate(uint8_t folder) {
    static int16_t r[100];
    static bool init = false;
    if (!init) {
      for (uint8_t i = 0; i < 100; i++) r[i] = -1;               // -1 = no fix: send numbers
      for (const CardOrderFix& f : CARD_ORDER_FIX) if (f.folder < 100) r[f.folder] = f.rotate;
      init = true;
    }
    return r[folder < 100 ? folder : 0];
  }
  /** @brief Card-order rotation of a folder, -1 = no fix (numbers are sent as they are). */
  inline int16_t rotation(uint8_t folder) { return _rotate(folder); }
  /** @brief Change it live (not saved): -1 = off, 0 = sorted card, 1 = one file in front, ... */
  inline void setRotation(uint8_t folder, int16_t r) { _rotate(folder) = r; }

  /**
   * @brief File number -> position on the card (what this DFPlayer clone needs).
   * Folders without a fix or without an index, and tracks not in the index,
   * are returned unchanged. Matches DFPlayerController::TrackMapFn.
   */
  inline uint8_t cardPosition(uint8_t folder, uint8_t track) {
    int16_t rot = _rotate(folder);
    if (rot < 0) return track;
    uint16_t n = 0, rank = 0;
    bool found = false;
    for (uint16_t i = 0; i < TRACK_COUNT; i++) {                 // TRACKS is sorted by number
      if (TRACKS[i].folder != folder) continue;
      if (TRACKS[i].track == track) { rank = n; found = true; }
      n++;
    }
    if (!found || n == 0) return track;
    return (uint8_t)(((rank + rot) % n) + 1);
  }

  /** @brief "3:53" (or "-:--" for unknown). */
  inline void formatTime(uint32_t secs, char* buf, size_t len) {
    if (secs == 0) { snprintf(buf, len, "-:--"); return; }
    snprintf(buf, len, "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  }

  /** @brief "02/005 Wham! - Wake Me Up Before You Go-Go (3:53)" — one line for logs. */
  inline void describe(uint8_t folder, uint8_t track, char* buf, size_t len) {
    const TrackInfo* info = find(folder, track);
    if (!info) { snprintf(buf, len, "%02u/%03u (not in the index)", folder, track); return; }
    char t[8];
    formatTime(info->seconds, t, sizeof(t));
    snprintf(buf, len, "%02u/%03u %s (%s)", folder, track, info->title, t);
  }

  // what watch() announced last — commands that print the title themselves
  // call markAnnounced() so the same track isn't announced twice
  inline uint8_t& _lastFolder() { static uint8_t v = 0; return v; }
  inline uint8_t& _lastTrack()  { static uint8_t v = 0; return v; }
  inline void markAnnounced(uint8_t folder, uint8_t track) { _lastFolder() = folder; _lastTrack() = track; }

#ifdef DFPLAYERCONTROLLER_H
  /**
   * @brief Call every loop(): logs "Now playing ..." whenever the DFPlayer
   * starts a different track (Party mode, auto-next, ... — whatever caused it).
   */
  inline void watch() {
    if (!DFPlayer.isOK() || !DFPlayer.isPlaying()) return;
    uint8_t f = DFPlayer.currentFolder(), t = DFPlayer.currentTrack();
    if (f == _lastFolder() && t == _lastTrack()) return;
    markAnnounced(f, t);
    char line[96];
    describe(f, t, line, sizeof(line));
    Logger::log("Now playing %s", line);
  }
#endif

} // namespace TrackIndex

#endif // TRACK_INDEX_H
