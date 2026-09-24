#ifndef SONG_PLAYER_H
#define SONG_PLAYER_H
/*
  SongPlayer — non-blocking single-voice MIDI-style playback for Arduino tone().

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
class speakerController{
public:
    /** @brief Constructs a speakerController for the given Arduino pin. @param pin Output pin connected to the speaker/buzzer. */
    speakerController(uint8_t pin) : _pin(pin) {}

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
        noTone(_pin);
        _playing = false;
    }

    /** @brief Returns true while a song is in progress. */
    bool isPlaying() const { return _playing; }

    /** @brief Returns how far into the current song we are (ms since play() was called). Returns 0 if not playing. */
    uint32_t elapsed() const { return _playing ? (millis() - _start_ms) : 0; }

    /**
     * @brief Advance playback. Call every loop() iteration.
     *
     * Triggers any notes whose start time has been reached this frame.
     * Stops automatically when all notes have fired.
     */
    void step() {
        if (!_playing || _notes == nullptr) return;
        if (_index >= _count) { stop(); return; }

        uint32_t elapsed = millis() - _start_ms;

        // Fire every note whose time has come. tone() with a duration runs
        // in the background, so consecutive notes overlap or replace cleanly.
        while (_index < _count && _notes[_index].time_ms <= elapsed) {
            const SongNote& n = _notes[_index];
            tone(_pin, midiToFreq(n.midi), n.duration_ms);
            _index++;
        }
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
        tone(_pin, frequency, duration);
    }

    /** @brief Stop any tone currently produced by playTone(). */
    void stopTone() {
        noTone(_pin);
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
    uint8_t  _pin;
    const SongNote* _notes = nullptr;
    size_t   _count    = 0;
    size_t   _index    = 0;
    uint32_t _start_ms = 0;
    bool     _playing  = false;
};

#endif // SONG_PLAYER_H
