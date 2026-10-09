#ifndef BUZZER_PLAYER_H
#define BUZZER_PLAYER_H
/*
  Buzzer_Player.h — the one thing main.cpp / Modes.h / Serial_Handler.h
  talk to for all buzzer sound. Combines two independent pieces:

    Buzzer/SongsPlayer/  — the song list (Back In Time, Solas, ...)
    Buzzer/UiSfx/         — short UI feedback cues (press, success, ...)

  Usage (a global instance `Buzzer` is created at the bottom):
    Buzzer.play();          // play the current song
    Buzzer.play(1);         // play song-list song with index 1
    Buzzer.next();          // next song (plays it if music is running)
    Buzzer.previous();      // previous song (same, one step back)
    Buzzer.select(0);       // just pick a song (plays only if already playing)
    Buzzer.stop();          // silence
    Buzzer.autoNext(true)   // keep the song list going by itself

    Buzzer.playCue(UISFX_PRESS);     // play a UI cue by id
    Buzzer.playCue("success");       // ...or by name
    Buzzer.setCuePack("arcade");     // choose the cue theme (pitch/speed)
    Buzzer.setCueGap(30, 80);        // 30ms silence before a cue, 80ms after

    Buzzer.setMuted(true);   // silence button/gesture cues only
    Buzzer.toggleMute();     // flip it

    Buzzer.step();          // call every loop iteration

  A cue briefly pauses whatever song is playing and resumes it
  afterwards, so button feedback never derails the music for long.
  setCueGap() adds a short quiet gap before and/or after the cue itself
  (default 0/0 = no gap), so the cue doesn't crash straight into the
  song's last note or its resumed note. Non-blocking — implemented as
  a tiny state machine advanced from step(), no delay() involved.

  Mute is button-cue-only: when muted, playCue() does nothing at all —
  no sound, no pausing the song. Songs (play/next/etc.) are unaffected
  either way, so the party keeps going while you mute the clicks.

  ADD A NEW SONG: put the header in Buzzer/Songs/, include it below
  (guarded with __has_include if it should stay private), add one line to
  SONGLIST. Done.

  IMPORTANT: include this in main.cpp AFTER the `speaker` object is
  defined — the global Buzzer instance uses it.
*/

#include <SongPlayer.h>
#include <UiSfx_Cues.h>

// songs
// ExampleSong_song.h is tracked in git and always present. Every other song
// header in Buzzer/Songs/ is gitignored (copyrighted melodies) and optional:
// __has_include skips whichever ones are missing, so a fresh clone still builds.
#include "Buzzer/Songs/ExampleSong_song.h"   // Example_Scale / Example_Scale_notes_count
#if __has_include("Buzzer/Songs/BackInTime_song.h")
  #include "Buzzer/Songs/BackInTime_song.h"
  #define HAS_SONG_BACKINTIME
#endif
#if __has_include("Buzzer/Songs/Solas_Melody.h")
  #include "Buzzer/Songs/Solas_Melody.h"
  #define HAS_SONG_SOLAS
#endif
#if __has_include("Buzzer/Songs/megaovenia_song.h")
  #include "Buzzer/Songs/megaovenia_song.h"
  #define HAS_SONG_MEGALOVENIA
#endif
#if __has_include("Buzzer/Songs/supermario_song.h")
  #include "Buzzer/Songs/supermario_song.h"
  #define HAS_SONG_SUPERMARIO
#endif
#if __has_include("Buzzer/Songs/sims2_song.h")
  #include "Buzzer/Songs/sims2_song.h"
  #define HAS_SONG_SIMS2
#endif
#if __has_include("Buzzer/Songs/minecraft_sweden_song.h")
  #include "Buzzer/Songs/minecraft_sweden_song.h"
  #define HAS_SONG_MINECRAFT
#endif
#if __has_include("Buzzer/Songs/Pokemon_Red_Opening_song.h")
  #include "Buzzer/Songs/Pokemon_Red_Opening_song.h"
  #define HAS_SONG_POKEMON
#endif
#if __has_include("Buzzer/Songs/DancingQueen_song.h")
  #include "Buzzer/Songs/DancingQueen_song.h"
  #define HAS_SONG_DANCINGQUEEN
#endif

/* ----------------------------------------------------------------
   The song list
---------------------------------------------------------------- */
/**
 * @brief A named song: a pointer to its note array, note count, and display name.
 */
struct Song {
  const SongNote* notes; ///< Pointer to the note array
  size_t          count; ///< Number of notes in the array
  const char*     name;  ///< Human-readable name (shown in logs and serial status)
};

const Song SONGLIST[] = {
  { Example_Scale, Example_Scale_notes_count, "Example Scale" },
#ifdef HAS_SONG_BACKINTIME
  { BackInTime_Notes, BackInTime_notes_count, "Back In Time" },
#endif
#ifdef HAS_SONG_SOLAS
  { Solas_Melody,     Solas_Melody_count,     "Solas"        },
#endif
#ifdef HAS_SONG_DANCINGQUEEN
  { DancingQueen,     DancingQueen_notes_count, "Dancing Queen" },
#endif
#ifdef HAS_SONG_MEGALOVENIA
  { Megalovenia,      Megalovenia_notes_count,  "Megalovenia"  },
#endif
#ifdef HAS_SONG_SUPERMARIO
  { Supermario,       Supermario_notes_count,   "Super Mario"  },
#endif
#ifdef HAS_SONG_SIMS2
  { Sims2,            Sims2_notes_count,        "Sims 2"       },
#endif
#ifdef HAS_SONG_MINECRAFT
  { Minecraft_sweden, Minecraft_sweden_notes_count, "Minecraft Sweden" },
#endif
#ifdef HAS_SONG_POKEMON
  { Pokemon_Red_Opening, Pokemon_Red_Opening_notes_count, "Pokemon Red Opening" },
#endif
};
const uint8_t SONGLIST_COUNT = sizeof(SONGLIST) / sizeof(SONGLIST[0]);


/* ----------------------------------------------------------------
   The player
---------------------------------------------------------------- */
/**
 * @brief High-level audio controller for the Discobox.
 *
 * Combines song-list playback with UI sound cues (short feedback blips).
 * Cues briefly "duck" (pause) any playing song and resume it afterwards.
 * All operations are non-blocking — the only thing that must run every
 * loop() iteration is step().
 *
 * A single global instance `Buzzer` is created at the bottom of this file,
 * wired to the `speaker` object defined in main.cpp.
 */
class BuzzerPlayer {
public:
  /** @brief Constructs a BuzzerPlayer backed by an existing speakerController. @param spk The low-level speaker controller to drive. */
  BuzzerPlayer(speakerController& spk) : _spk(spk) {}

  /**
   * @brief Advance the audio state machine. Call every loop() iteration.
   *
   * Advances note playback, waits out pre/post-cue gaps, resumes a paused
   * song once a cue finishes, and — when autoNext is on — starts the next
   * song as soon as the current one ends.
   */
  void step() {
    _spk.step();

    // waiting out the quiet gap BEFORE a cue?
    if (_pending_cue_id != NO_PENDING_CUE) {
      if (dueBy(_gap_deadline_ms)) {
        uint8_t id = _pending_cue_id;
        _pending_cue_id = NO_PENDING_CUE;
        startCueNow(id);
      }
      return;   // nothing else to do while waiting
    }

    // waiting out the quiet gap AFTER a cue?
    if (_in_post_gap) {
      if (dueBy(_gap_deadline_ms)) {
        _in_post_gap = false;
        resumeAfterCue();
      }
      return;
    }

    if (_ducking && !_spk.isPlaying()) {
      // the cue itself finished
      if (_gap_after_ms > 0) {
        _in_post_gap     = true;
        _gap_deadline_ms = millis() + _gap_after_ms;
      } else {
        resumeAfterCue();
      }
    }

    if (!_ducking && _auto_next && _started && !_spk.isPlaying()) next();
  }

  // ---- Songs ----

  /** @brief Play the currently selected song from the beginning. */
  void play() {
    _started = true;
    _ducking = false;
    Logger::log("Buzzer: playing %s", songName());
    _spk.play(SONGLIST[_index].notes, SONGLIST[_index].count);
  }

  /**
   * @brief Play a specific song from the list right now.
   * @param index 0-based song index (see SONGLIST). Out-of-range values are ignored.
   */
  void play(uint8_t index) {
    if (index >= SONGLIST_COUNT) { Logger::warn("No song %d", index + 1); return; }
    _index = index;
    play();
  }

  /** @brief Advance to the next song (wraps). Plays it immediately if music is already running. */
  void next() {
    _index = (uint8_t)((_index + 1) % SONGLIST_COUNT);
    if (_spk.isPlaying() || (_auto_next && _started)) play();
    else Logger::log("Buzzer: selected %s", songName());
  }

  /** @brief Go back to the previous song (wraps). Plays it immediately if music is already running. */
  void previous() {
    _index = (uint8_t)((_index + SONGLIST_COUNT - 1) % SONGLIST_COUNT);
    if (_spk.isPlaying() || (_auto_next && _started)) play();
    else Logger::log("Buzzer: selected %s", songName());
  }

  /**
   * @brief Select a song without forcing playback.
   *
   * If music is currently playing the new song starts immediately;
   * otherwise the selection is remembered for the next play() call.
   * @param index 0-based song index. Out-of-range values are ignored.
   */
  void select(uint8_t index) {
    if (index >= SONGLIST_COUNT) { Logger::warn("No song %d", index + 1); return; }
    _index = index;
    if (_spk.isPlaying()) play();
    else Logger::log("Buzzer: selected %s", songName());
  }

  /** @brief Stop playback immediately and cancel the auto-next chain. */
  void stop() {
    _started     = false;
    _ducking     = false;
    _song_paused = false;
    _spk.stop();
  }

  /**
   * @brief Enable or disable automatic song advancement.
   *
   * When on, step() starts the next song as soon as the current one finishes.
   * @param on true to keep songs playing in sequence; false to stop after the current song.
   */
  void autoNext(bool on) { _auto_next = on; }

  // ---- UI cues ----

  /**
   * @brief Play a UI sound cue by its enum ID.
   *
   * Pauses a running song, plays the cue (after an optional pre-gap),
   * then resumes the song (after an optional post-gap). All timing is
   * handled non-blockingly in step(). If muted, does nothing.
   * @param id Cue ID from the UiSfxCueId enum.
   */
  void playCue(UiSfxCueId id) { playCue((uint8_t)id); }

  void playCue(uint8_t id) {
    if (_cue_muted) return;              // muted: no sound, song (if any) untouched
    if (id >= UISFX_CUE_COUNT) return;

    if (_spk.isPlaying() && !_ducking) {
      // a song is running — pause it and remember where we were
      _song_offset_ms = _spk.elapsed();
      _song_paused    = true;
    }
    _ducking     = true;
    _in_post_gap = false;    // a new cue supersedes any gap we were waiting out
    _spk.stop();             // silence whatever was sounding (song or a prior cue)

    if (_gap_before_ms > 0) {
      _pending_cue_id  = id;
      _gap_deadline_ms = millis() + _gap_before_ms;
    } else {
      startCueNow(id);
    }
  }

  /**
   * @brief Set silence gaps around UI cues.
   *
   * Waits @p before_ms of silence before playing a cue, then @p after_ms
   * of silence before resuming any paused song. Both default to 0. Non-blocking.
   * @param before_ms Silence (ms) before the cue plays.
   * @param after_ms  Silence (ms) after the cue, before the song resumes.
   */
  void setCueGap(uint16_t before_ms, uint16_t after_ms) {
    _gap_before_ms = before_ms;
    _gap_after_ms  = after_ms;
  }
  /** @brief Set the pre-cue silence gap only. @param ms Gap duration in ms. */
  void     setCueGapBefore(uint16_t ms) { _gap_before_ms = ms; }
  /** @brief Set the post-cue silence gap only. @param ms Gap duration in ms. */
  void     setCueGapAfter(uint16_t ms)  { _gap_after_ms  = ms; }
  /** @brief Returns the current pre-cue silence gap in ms. */
  uint16_t cueGapBefore() const { return _gap_before_ms; }
  /** @brief Returns the current post-cue silence gap in ms. */
  uint16_t cueGapAfter()  const { return _gap_after_ms; }

  /**
   * @brief Play a UI cue looked up by name (e.g. "press", "success").
   * @param name Name of the cue as defined in UiSfx_Cues.h.
   */
  void playCue(const char* name) {
    for (uint8_t i = 0; i < UISFX_CUE_COUNT; i++) {
      if (strcmp(UISFX_CUES[i].name, name) == 0) { playCue(i); return; }
    }
    Logger::warn("Buzzer: unknown cue '%s'", name);
  }

  /**
   * @brief Select the cue theme (pitch/speed pack) by name.
   *
   * The chosen pack's transpose and speed are applied to every subsequent
   * playCue() call. Does not affect song playback.
   * @param name Name of the pack as defined in UiSfx_Cues.h.
   */
  void setCuePack(const char* name) {
    for (uint8_t i = 0; i < UISFX_PACK_COUNT; i++) {
      if (strcmp(UISFX_PACKS[i].name, name) == 0) {
        _pack_index = i;
        Logger::log("Buzzer: cue pack -> %s", name);
        return;
      }
    }
    Logger::warn("Buzzer: unknown pack '%s'", name);
  }

  /**
   * @brief Select the cue theme by index (e.g. when restoring a saved setting).
   *
   * Silently ignores out-of-range indices — use this for loading trusted
   * persisted data, not typed-in commands.
   * @param index 0-based pack index.
   */
  void setCuePack(uint8_t index) {
    if (index < UISFX_PACK_COUNT) _pack_index = index;
  }

  /** @brief Returns the name of the currently active cue pack. */
  const char* cuePackName()  const { return UISFX_PACKS[_pack_index].name; }
  /** @brief Returns the 0-based index of the currently active cue pack. */
  uint8_t     cuePackIndex() const { return _pack_index; }

  // ---- Mute (button/gesture cues only — songs are unaffected) ----
  /**
   * @brief Mute or unmute UI cues (songs are unaffected).
   *
   * When muted, playCue() does nothing at all — no sound, no song pausing.
   * @param m true to mute, false to unmute.
   */
  void setMuted(bool m) {
    _cue_muted = m;
    Logger::log("Buzzer: cues %s", m ? "muted" : "unmuted");
  }
  /** @brief Returns true if UI cues are currently muted. */
  bool isMuted() const { return _cue_muted; }
  /** @brief Toggle the mute state of UI cues. */
  void toggleMute()    { setMuted(!_cue_muted); }

  // ---- Info ----
  /** @brief Returns true while a song or cue is playing. */
  bool        isPlaying() const { return _spk.isPlaying(); }
  /** @brief Returns the 0-based index of the currently selected song. */
  uint8_t     songIndex() const { return _index; }
  /** @brief Returns the display name of the currently selected song. */
  const char* songName()  const { return SONGLIST[_index].name; }
  /** @brief Returns the total number of songs in the SONGLIST. */
  uint8_t     songCount() const { return SONGLIST_COUNT; }

private:
  speakerController& _spk;

  // song list state
  uint8_t  _index          = 0;      // current song-list song
  bool     _auto_next      = false;  // continue with the next song automatically?
  bool     _started        = false;  // has play() been called since the last stop()?

  // cue-ducking state
  bool     _ducking        = false;  // a cue is playing (or its gap is) right now
  bool     _song_paused    = false;  // a song was interrupted and needs resuming
  uint32_t _song_offset_ms = 0;      // how far into the song we were

  // cue pack (theme) state
  uint8_t    _pack_index = 0;
  SongNote   _cue_buf[UISFX_CUE_MAX_NOTES];   // transposed copy of the cue being played

  // mute state (cues only — see setMuted() above)
  bool _cue_muted = false;

  // quiet-gap state (see setCueGap())
  static const uint8_t NO_PENDING_CUE = 0xFF;
  uint16_t      _gap_before_ms   = 0;      // silence before a cue starts
  uint16_t      _gap_after_ms    = 0;      // silence after a cue, before resuming
  uint8_t       _pending_cue_id  = NO_PENDING_CUE;  // cue waiting out its "before" gap
  bool          _in_post_gap     = false;  // currently waiting out the "after" gap
  unsigned long _gap_deadline_ms = 0;      // millis() time the current gap ends

  /**
   * @brief Returns true once millis() has reached or passed @p deadline.
   *
   * Uses signed arithmetic to handle the ~49-day millis() rollover safely.
   * @param deadline Target timestamp from millis().
   * @return true if the deadline has been reached.
   */
  static bool dueBy(unsigned long deadline) {
    return (long)(millis() - deadline) >= 0;
  }

  /**
   * @brief Immediately start playing a cue (after any pre-gap has elapsed).
   * @param id 0-based cue index into UISFX_CUES.
   */
  void startCueNow(uint8_t id) {
    const UiSfxCue& cue = UISFX_CUES[id];
    buildTransposed(cue);
    _spk.play(_cue_buf, cue.count);
  }

  /** @brief Called when a cue (and its post-gap) have finished. Resumes any song that was paused for the cue. */
  void resumeAfterCue() {
    _ducking = false;
    if (_song_paused) {
      _song_paused = false;
      _spk.playFrom(SONGLIST[_index].notes, SONGLIST[_index].count, _song_offset_ms);
    }
  }

  /**
   * @brief Transpose and speed-adjust a cue into the internal _cue_buf.
   *
   * Applies the current pack's semitone offset and speed percentage to
   * every note and stores the result in _cue_buf, ready for playback.
   * @param cue The source cue from UISFX_CUES.
   */
  void buildTransposed(const UiSfxCue& cue) {
    const UiSfxPack& pack = UISFX_PACKS[_pack_index];
    for (uint8_t i = 0; i < cue.count; i++) {
      _cue_buf[i].midi        = (uint8_t)(cue.notes[i].midi + pack.semitones);
      _cue_buf[i].duration_ms = (uint16_t)((uint32_t)cue.notes[i].duration_ms * pack.speed_pct / 100);
      _cue_buf[i].time_ms     = (uint32_t)cue.notes[i].time_ms * pack.speed_pct / 100;
    }
  }
};


// The one global player, wired to the speaker from main.cpp.
BuzzerPlayer Buzzer(speaker);

#endif // BUZZER_PLAYER_H
