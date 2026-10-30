#ifndef SONG_PLAYER_H
#define SONG_PLAYER_H
/*
  SongPlayer — non-blocking single-voice MIDI-style playback (tone() or PwmOut, with volume).

  Usage:
    #include "SongPlayer.h"
    #include "VisiPiano_song.h"          // an array generated from a JSON song

    SongPlayer player(D9);                // speaker on D9

    void setup() {
      player.begin();
      player.play(VisiPiano_notes, VisiPiano_notes_count);
    }

    void loop() {
      player.step();                      // call every iteration
      // ... rest of your loop runs in parallel ...
    }
*/

#include <Arduino.h>

/*
  VOLUME (0..8, 0 = silent)
  tone() always sends a 50 % square wave = one fixed loudness. With the build
  flag BUZZER_PWM (set for the board in platformio.ini) the note is produced by
  the R4's PwmOut instead, which can make the "on" part of each wave shorter:
  shorter pulse -> less energy into the piezo -> quieter.

  The levels come from the listening test test/test_hw_buzzer_volume:
    30 % sounded the same as 50 %  -> left out
    quieter from 20 %, a bit thin from 3 %, cues clear at every level
  Without BUZZER_PWM (laptop tests) tone() is used and volume only means
  0 = silent / anything else = normal.
*/
#if defined(BUZZER_PWM)
  #include "pwm.h"
#endif

/// Highest volume level of the buzzer (level 0 = silent).
static const uint8_t SPEAKER_MAX_VOLUME = 8;

/// Duty cycle in % for each level 0..8 (index = level).
static const float SPEAKER_DUTY[SPEAKER_MAX_VOLUME + 1] = {
  0.0f,    // 0  silent
  1.0f,    // 1  very quiet (thin)
  2.0f,    // 2
  3.0f,    // 3  a bit thin from here down
  5.0f,    // 4
  8.0f,    // 5
  12.0f,   // 6
  20.0f,   // 7  first audible step below full
  50.0f,   // 8  full = what tone() does
};

/**
 * @brief One note in a song.
 *
 * Times are in milliseconds. MIDI note numbers follow the standard scale:
 * 60 = middle C, 69 = A4 (440 Hz). Range is 0..127.
 */
struct SongNote {
  uint8_t  midi;         ///< MIDI note number (0..127, 60 = middle C)
  uint16_t duration_ms;  ///< How long to hold the note (ms)
  uint32_t time_ms;      ///< When (relative to song start) to play it (ms)
};

/**
 * @brief Non-blocking single-voice MIDI-style playback using Arduino tone().
 *
 * Call step() every loop() iteration. All other calls (play, stop, etc.)
 * are instantaneous — they don't block.
 */
class speakerController {
public:
  /** @brief Constructs a speakerController for the given Arduino pin. @param pin Output pin connected to the speaker/buzzer. */
#if defined(BUZZER_PWM)
  speakerController(uint8_t pin) : _pin(pin), _pwm(pin) {}
#else
  speakerController(uint8_t pin) : _pin(pin) {}
#endif

  /** @brief Hardware init — configures the speaker pin. Call once from setup(). */
  void setup() { pinMode(_pin, OUTPUT); }

  /**
   * @brief Start (or restart) playback of a note array from the beginning.
   *
   * Non-blocking — call step() from your main loop() to actually advance the song.
   * @param notes Pointer to a SongNote array.
   * @param count Number of notes in the array.
   */
  void play(const SongNote* notes, size_t count) {
    _notes    = notes;
    _count    = count;
    _index    = 0;
    _start_ms = millis();
    _playing  = true;
    // the song ends when the LAST-ENDING note is over (time + duration),
    // not when the last note starts — otherwise stop() cuts it off
    _end_ms = 0;
    for (size_t i = 0; i < count; i++) {
      uint32_t e = notes[i].time_ms + notes[i].duration_ms;
      if (e > _end_ms) _end_ms = e;
    }
  }

  /**
   * @brief Resume playback of a note array from a given offset.
   *
   * Used to resume a song after it was interrupted (e.g. by a UI cue).
   * Skips notes that would have already played before @p offset_ms.
   * @param notes    Pointer to a SongNote array.
   * @param count    Number of notes in the array.
   * @param offset_ms Position in the song to resume from (ms from start).
   */
  void playFrom(const SongNote* notes, size_t count, uint32_t offset_ms) {
    play(notes, count);
    _start_ms = millis() - offset_ms;          // pretend it started earlier
    while (_index < _count && _notes[_index].time_ms < offset_ms)
      _index++;                              // skip notes already played
  }

  /** @brief Stop playback immediately and silence the speaker. */
  void stop() {
    noteOff();
    _playing = false;
  }

  /**
   * @brief Set the loudness 0..SPEAKER_MAX_VOLUME (0 = silent, timing still runs).
   * Takes effect from the NEXT note.
   */
  void setVolume(uint8_t level) { _volume = (level > SPEAKER_MAX_VOLUME) ? SPEAKER_MAX_VOLUME : level; }

  /** @brief Current loudness 0..SPEAKER_MAX_VOLUME. */
  uint8_t volume() const { return _volume; }

  /** @brief Returns true while a song is in progress. */
  bool isPlaying() const { return _playing; }

  /** @brief Returns how far into the current song we are (ms since play() was called). Returns 0 if not playing. */
  uint32_t elapsed() const { return _playing ? (millis() - _start_ms) : 0; }

  /**
   * @brief Advance playback. Call every loop() iteration.
   *
   * Triggers any notes whose start time has been reached this frame.
   * Stops automatically once the last note has finished (time_ms + duration_ms).
   */
  void step() {
    // a note's time is up -> silence (PwmOut has no built-in duration like tone())
    if (_noteOn && (long)(millis() - _noteOffAt) >= 0) noteOff();

    if (!_playing || _notes == nullptr) return;
    uint32_t elapsed = millis() - _start_ms;

    // Fire every note whose time has come. A new note simply replaces
    // the one still sounding.
    while (_index < _count && _notes[_index].time_ms <= elapsed) {
      const SongNote& n = _notes[_index];
      noteOn(midiToFreq(n.midi), n.duration_ms);
      _index++;
    }

    // All notes fired AND the last one has finished sounding -> done.
    if (_index >= _count && elapsed >= _end_ms) stop();
  }

  /**
   * @brief Convert a MIDI note number to frequency in Hz (rounded).
   *
   * Formula: 440 * 2^((midi - 69) / 12). Reference: MIDI 69 = A4 = 440 Hz.
   * @param midi MIDI note number (0..127).
   * @return Frequency in Hz.
   */
  static uint16_t midiToFreq(uint8_t midi) {
    return (uint16_t)(440.0f * pow(2.0f, (midi - 69) / 12.0f) + 0.5f);
  }

  /**
   * @brief Play a raw frequency tone directly (bypasses the song player state).
   * @param frequency Frequency in Hz.
   * @param duration  Duration in ms (0 = plays until stopTone() is called).
   */
  void playTone(uint16_t frequency, uint32_t duration = 0) {
    noteOn(frequency, duration ? duration : 0x7FFFFFFFUL);   // 0 = until stopTone()
  }

  /** @brief Stop any tone currently produced by playTone(). */
  void stopTone() {
    noteOff();
  }

  /**
   * @brief Convert a MIDI note number to frequency in Hz.
   * @param note MIDI note number (0..127).
   * @return Frequency in Hz.
   */
  uint16_t pitchToFrequency(uint8_t note) {
    // Convert MIDI note number to frequency
    return 440 * pow(2, (note - 69) / 12.0);
  }

  /**
   * @brief Play a MIDI note directly (bypasses the song player state).
   * @param note     MIDI note number (0..127).
   * @param duration Duration in ms (0 = plays until stopTone() is called).
   */
  void playNote(uint8_t note, uint32_t duration = 0) {
    playTone(pitchToFrequency(note), duration);
  }

private:
  uint8_t _pin;
#if defined(BUZZER_PWM)
  PwmOut _pwm;
#endif
  uint8_t  _volume    = SPEAKER_MAX_VOLUME;
  bool     _noteOn    = false;
  uint32_t _noteOffAt = 0;

  /** @brief Start one note at the current volume; it ends after `ms` (checked in step()). */
  void noteOn(uint16_t freq, uint32_t ms) {
    if (_volume == 0 || freq == 0) {
      noteOff();
      return;
    }
#if defined(BUZZER_PWM)
    _pwm.end();                                   // a running note is replaced
    _pwm.begin((float)freq, SPEAKER_DUTY[_volume]);
#else
    tone(_pin, freq);
#endif
    _noteOn    = true;
    _noteOffAt = millis() + ms;
  }

  /** @brief Silence the speaker. Safe to call when nothing is sounding. */
  void noteOff() {
#if defined(BUZZER_PWM)
    _pwm.end();
    digitalWrite(_pin, LOW);
#else
    noTone(_pin);
#endif
    _noteOn = false;
  }

  const SongNote* _notes    = nullptr;
  size_t          _count    = 0;
  size_t          _index    = 0;
  uint32_t        _start_ms = 0;
  uint32_t        _end_ms   = 0;   // song length: max(time_ms + duration_ms)
  bool            _playing  = false;
};

#endif // SONG_PLAYER_H
