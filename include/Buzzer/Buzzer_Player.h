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

  ADD A NEW SONG: put the header in Buzzer/SongsPlayer/Songs/, include
  it below, add one line to SONGLIST. Done.

  IMPORTANT: include this in main.cpp AFTER the `speaker` object is
  defined — the global Buzzer instance uses it.
*/

#include "Buzzer/SongsPlayer/SongPlayer.h"
#include "Buzzer/UiSfx/UiSfx_Cues.h"

//songs
  #include "Buzzer/SongsPlayer/Songs/BackInTime_song.h"  // BackInTime_Notes / BackInTime_notes_count
  #include "Buzzer/SongsPlayer/Songs/Solas_Melody.h"     // Solas_Melody / Solas_Melody_count


/* ----------------------------------------------------------------
   The song list
---------------------------------------------------------------- */
struct Song {
  const SongNote* notes;
  size_t          count;
  const char*     name;
};

const Song SONGLIST[] = {
  { BackInTime_Notes, BackInTime_notes_count, "Back In Time" },
  { Solas_Melody,     Solas_Melody_count,     "Solas"        },
};
const uint8_t SONGLIST_COUNT = sizeof(SONGLIST) / sizeof(SONGLIST[0]);


/* ----------------------------------------------------------------
   The player
---------------------------------------------------------------- */
class BuzzerPlayer {
public:
  BuzzerPlayer(speakerController& spk) : _spk(spk) {}

  // Call every loop iteration. Advances notes, waits out cue gaps,
  // resumes a song after a cue finishes, and — when auto-next is on —
  // starts the next song as soon as the current one finishes.
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

  // Play the current song (from the start).
  void play() {
    _started = true;
    _ducking = false;
    Logger::log("Buzzer: playing %s", songName());
    _spk.play(SONGLIST[_index].notes, SONGLIST[_index].count);
  }

  // Play a specific song-list song right now (0-based index).
  void play(uint8_t index) {
    if (index >= SONGLIST_COUNT) { Logger::warn("No song %d", index + 1); return; }
    _index = index;
    play();
  }

  // Next song. Plays it if music is currently running, otherwise just selects it.
  void next() {
    _index = (uint8_t)((_index + 1) % SONGLIST_COUNT);
    if (_spk.isPlaying() || (_auto_next && _started)) play();
    else Logger::log("Buzzer: selected %s", songName());
  }

  // Previous song. Same behavior as next(), just one step back.
  void previous() {
    _index = (uint8_t)((_index + SONGLIST_COUNT - 1) % SONGLIST_COUNT);
    if (_spk.isPlaying() || (_auto_next && _started)) play();
    else Logger::log("Buzzer: selected %s", songName());
  }

  // Pick a song without forcing playback (plays only if already playing).
  void select(uint8_t index) {
    if (index >= SONGLIST_COUNT) { Logger::warn("No song %d", index + 1); return; }
    _index = index;
    if (_spk.isPlaying()) play();
    else Logger::log("Buzzer: selected %s", songName());
  }

  // Stop the music (and the auto-next chain).
  void stop() {
    _started     = false;
    _ducking     = false;
    _song_paused = false;
    _spk.stop();
  }

  // Keep the song list going by itself (true while the party runs).
  void autoNext(bool on) { _auto_next = on; }

  // ---- UI cues ----

  // Play a UI cue by id. Pauses a running song, plays the cue (after an
  // optional quiet gap — see setCueGap()), and resumes the song
  // afterwards (also after an optional gap). All handled in step().
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

  // Silence around a cue: `before` waits quietly, then plays the cue,
  // then `after` waits quietly before resuming any paused song.
  // Both default to 0 (no gap). Non-blocking either way.
  void setCueGap(uint16_t before_ms, uint16_t after_ms) {
    _gap_before_ms = before_ms;
    _gap_after_ms  = after_ms;
  }
  void     setCueGapBefore(uint16_t ms) { _gap_before_ms = ms; }
  void     setCueGapAfter(uint16_t ms)  { _gap_after_ms  = ms; }
  uint16_t cueGapBefore() const { return _gap_before_ms; }
  uint16_t cueGapAfter()  const { return _gap_after_ms; }

  // Same, but look the cue up by name (e.g. "press", "success").
  void playCue(const char* name) {
    for (uint8_t i = 0; i < UISFX_CUE_COUNT; i++) {
      if (strcmp(UISFX_CUES[i].name, name) == 0) { playCue(i); return; }
    }
    Logger::warn("Buzzer: unknown cue '%s'", name);
  }

  // Choose the cue theme (transpose + speed applied to every cue).
  // Doesn't affect songs — only playCue().
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

  // Same, but by index (e.g. restoring a saved setting) — silently
  // ignores an out-of-range index instead of warning, since this is
  // meant for loading trusted data, not typed-in commands.
  void setCuePack(uint8_t index) {
    if (index < UISFX_PACK_COUNT) _pack_index = index;
  }

  const char* cuePackName()  const { return UISFX_PACKS[_pack_index].name; }
  uint8_t     cuePackIndex() const { return _pack_index; }

  // ---- Mute (button/gesture cues only — songs are unaffected) ----
  void setMuted(bool m) {
    _cue_muted = m;
    Logger::log("Buzzer: cues %s", m ? "muted" : "unmuted");
  }
  bool isMuted() const { return _cue_muted; }
  void toggleMute()    { setMuted(!_cue_muted); }

  // ---- Info ----
  bool        isPlaying() const { return _spk.isPlaying(); }
  uint8_t     songIndex() const { return _index; }
  const char* songName()  const { return SONGLIST[_index].name; }
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

  // True once millis() has reached deadline — handles the ~49-day rollover safely.
  static bool dueBy(unsigned long deadline) {
    return (long)(millis() - deadline) >= 0;
  }

  // Actually start playing a cue's (transposed) notes right now.
  void startCueNow(uint8_t id) {
    const UiSfxCue& cue = UISFX_CUES[id];
    buildTransposed(cue);
    _spk.play(_cue_buf, cue.count);
  }

  // Cue (and its after-gap, if any) are done — resume a paused song, if any.
  void resumeAfterCue() {
    _ducking = false;
    if (_song_paused) {
      _song_paused = false;
      _spk.playFrom(SONGLIST[_index].notes, SONGLIST[_index].count, _song_offset_ms);
    }
  }

  // Apply the current pack's transpose + speed to a cue into _cue_buf.
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
