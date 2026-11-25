#pragma once
#define DFPLAYERCONTROLLER_H  // allows other headers to detect this via #ifdef
/*
  DFPlayerController.h — DFPlayer Mini Controller

  Verwaltet Wiedergabe, Playlist (Auto-Next) und Zustand intern.
  step() muss jeden loop() aufgerufen werden — darin wird erkannt
  wann ein Track endet und der nächste automatisch gestartet.

  Verdrahtung (R4 WiFi — Serial1 = D0/D1):
    Arduino D1 (TX) → DFPlayer RX  (1kΩ in Serie!)
    Arduino D0 (RX) ← DFPlayer TX
    DFPlayer VCC    → 5V (Buck Converter)
    DFPlayer GND    → GND (gemeinsam mit Arduino!)

  Verwendung (das Serial-Objekt kommt von main.cpp — wie speaker beim Buzzer):


    Serial1.begin(9600);          // before DFPlayer.begin()
    DFPlayer.begin(25);           // in setup()
    DFPlayer.playFolder(1, 1);   // /01/001.mp3 starten
    DFPlayer.pause();
    DFPlayer.resume();
    DFPlayer.next();
    DFPlayer.previous();
    DFPlayer.setVolume(20);
    DFPlayer.volumeUp();
    DFPlayer.volumeDown();
    DFPlayer.setAutoNext(true);  // Playlist: nächsten Track automatisch spielen
    DFPlayer.step();              // jeden loop() aufrufen!

    DFPlayer.isPlaying()          // true wenn gerade Musik läuft
    DFPlayer.trackChanged()       // true für genau einen loop()-Durchlauf nach Track-Wechsel
    DFPlayer.currentFolder()
    DFPlayer.currentTrack()
    DFPlayer.volume()
    DFPlayer.isOK()
    DFPlayer.playingForMs()       // wie lange läuft der Track schon (ms)
*/

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

/**
 * @brief DFPlayer Mini audio controller.
 *
 * Wraps DFRobotDFPlayerMini with playlist management, auto-next,
 * and playback state tracking. All operations are non-blocking —
 * the only thing that must run every loop() iteration is step().
 *
 * Takes a Stream& so it works with both HardwareSerial (R4 WiFi → Serial1)
 * and SoftwareSerial (AVR boards). Pass whichever serial port the DFPlayer
 * is wired to — do NOT call .begin() on it yourself; DFPlayerController
 * does that internally in begin().
 *
 * A single global instance `Audio` is created at the bottom of this file.
 */
class DFPlayerController {
public:
  /** @brief Constructs a DFPlayerController. @param serial Any Stream (Serial1, SoftwareSerial, …) the DFPlayer is wired to. */
  DFPlayerController(Stream& serial) : _serial(serial) {}

  // ── Initialisierung ─────────────────────────────────────

  /**
   * @brief Initialise the DFPlayer. Call once in setup().
   * @param volume Starting volume (0–30, default 25).
   * @return true if the DFPlayer responded; false if not found (audio skipped).
   */
  bool begin(uint8_t volume = 25) {
    // Serial port must already be started by main.cpp before calling begin()
    // (e.g. Serial1.begin(9600) for R4 WiFi, or dfSerial.begin(9600) for AVR).
    delay(2000);  // DFPlayer Bootzeit

    if (_player.begin(_serial, false)) {  // false = kein ACK (Clone-kompatibel)
      _ok  = true;
      _vol = volume;
      _player.volume(_vol);
      delay(200);
      Serial.println(F("[DFPlayer] OK"));
      return true;
    }
    Serial.println(F("[DFPlayer] nicht gefunden — weiter ohne Audio"));
    return false;
  }

  // ── Loop ────────────────────────────────────────────────

  /**
   * @brief Advance the audio state machine. Call every loop() iteration.
   *
   * Reads incoming DFPlayer responses, detects track-end events, and
   * starts the next track automatically when autoNext is enabled.
   * Sets trackChanged() true for exactly one loop() cycle after a track switch.
   */
  void step() {
    if (!_ok) return;

    _track_changed = false;  // nur einen Loop-Durchlauf true

    // SoftwareSerial-Polling rate-limitieren: nur alle 100ms prüfen.
    // Verhindert dass available() jeden loop()-Durchlauf blockiert.
    unsigned long now = millis();
    if (now - _last_poll_ms < 500) return;
    _last_poll_ms = now;

    if (_player.available()) {
      uint8_t type = _player.readType();

      if (type == DFPlayerPlayFinished && _looping) {
        // LOOP MODE: "finished" -> start the next song (index order, gaps skipped,
        // last -> first). Some clones send the event twice -> ignore it right after a start.
        if (millis() - _play_start_ms < LOOP_DEBOUNCE_MS) return;
        _track = nextTrack(_track, +1, true);         // the DFPlayer plays the folder in number order
        sendPlay(_folder, _track);           // the FIRMWARE starts the next song by its number
        _playing       = true;
        _track_changed = true;
        _play_start_ms = millis();
        Serial.print(F("[DFPlayer] Loop: "));
        Serial.print(_folder); Serial.print('/'); Serial.println(_track);
      }
      else if (type == DFPlayerPlayFinished) {
        _playing = false;
        Serial.print(F("[DFPlayer] Track fertig: "));
        Serial.print(_folder); Serial.print('/'); Serial.println(_track);

        if (_auto_next) {
          _track = nextTrack(_track, +1, true);       // skips gaps if an index is set
          sendPlay(_folder, _track);
          _playing       = true;
          _track_changed = true;
          _play_start_ms = millis();
          Serial.print(F("[DFPlayer] Auto-Next: "));
          Serial.print(_folder); Serial.print('/'); Serial.println(_track);
        }
      }
      else if (type == DFPlayerError) {
        uint8_t err = _player.read();
        // Track existiert nicht → Playlist von vorne
        if (_auto_next && (err == DFPlayerCardInserted || err == 0x06)) {
          _track = firstTrack();
          sendPlay(_folder, _track);
          _playing       = true;
          _track_changed = true;
          _play_start_ms = millis();
          Serial.println(F("[DFPlayer] Ende der Playlist — zurück zu Track 1"));
        }
      }
    }
  }

  // ── Wiedergabe ──────────────────────────────────────────

  /** @brief Play a specific track. @param folder SD folder number (e.g. 1 = /01/). @param track Track number (e.g. 1 = 001.mp3). */
  void playFolder(uint8_t folder, uint8_t track) {
    if (!_ok) return;
    _looping       = false;                // a single track ends any loop
    _folder        = folder;
    _track         = track;
    _playing       = true;
    _track_changed = true;
    _play_start_ms = millis();
    sendPlay(folder, track);
  }

  /**
   * @brief Loop all tracks in a folder, driven by the FIRMWARE.
   *
   * Every song is started by its number (playFolder), and step() starts the
   * next one when the DFPlayer reports "finished": index order, missing tracks
   * skipped, after the last one the first again. So currentTrack() is always
   * the song that really plays.
   *
   * Not the DFPlayer's own loop (loopFolder): that plays the files in the order
   * they lie on the card (FAT order, roughly the copy order), which the firmware
   * can't see — on the Discobox card it started with 044 while the firmware
   * assumed 001, so every title was one off.
   * @param folder SD folder number (1–99).
   */
  void playFolderLoop(uint8_t folder) {
    if (!_ok) return;
    _folder        = folder;
    _track         = firstTrack();         // the DFPlayer starts with the lowest number
    _looping       = true;                 // step(): finished = start the next song
    _playing       = true;
    _track_changed = true;
    _play_start_ms = millis();
    sendPlay(folder, _track);
  }

  /** @brief Pause playback. No-op if already paused or not initialised. */
  void pause() {
    if (!_ok || !_playing) return;
    _paused_at_ms = millis() - _play_start_ms;
    _playing = false;
    _player.pause();
  }

  /** @brief Resume a paused track. No-op if already playing or not initialised. */
  void resume() {
    if (!_ok || _playing) return;
    _playing       = true;
    _play_start_ms = millis() - _paused_at_ms;
    _player.start();
  }

  /** @brief Skip to the next track in the current folder. */
  void next() {
    if (!_ok) return;
    _track = nextTrack(_track, +1, true);
    _playing       = true;
    _track_changed = true;
    _play_start_ms = millis();
    sendPlay(_folder, _track);
  }

  /** @brief Go back to the previous track in the current folder. No-op if already at track 1. */
  void previous() {
    if (!_ok) return;
    uint8_t t = nextTrack(_track, -1, false);
    if (t == _track) return;                          // already at the first track
    _track = t;
    _playing       = true;
    _track_changed = true;
    _play_start_ms = millis();
    sendPlay(_folder, _track);
  }

  /** @brief Enable or disable automatic playlist advancement. @param on true = play next track when current one ends; false = stop after track. */
  void setAutoNext(bool on) { _auto_next = on; }

  /** @brief true while a folder loop runs (playFolderLoop), also after next/back inside it. */
  bool isLooping() const { return _looping; }

  /**
   * @brief Tells the controller which tracks really EXIST on the card.
   * fn(folder, track, dir) returns the next existing track after `track` in
   * direction dir (+1 / -1), or -1 if there is none. With it, next / back /
   * auto-next skip missing files instead of hitting them (and auto-next wraps
   * from the last track to the first). nullptr = plain +1 / -1 (default).
   * See include/Audio/Track_Index.h (TrackIndex::step).
   */
  typedef int16_t (*TrackStepFn)(uint8_t folder, uint8_t track, int8_t dir);
  void setTrackIndex(TrackStepFn fn) { _stepFn = fn; }

  /**
   * @brief Translates a FILE NUMBER into what the module must be sent.
   * Some DFPlayer clones ignore the "001" in the file name and play the Nth
   * file in CARD order instead. fn(folder, track) returns that card position.
   * currentTrack() / titles keep using the file number. nullptr = send as is.
   * See TrackIndex::cardPosition() in include/Audio/Track_Index.h.
   */
  typedef uint8_t (*TrackMapFn)(uint8_t folder, uint8_t track);
  void setTrackMap(TrackMapFn fn) { _mapFn = fn; }

  // ── Lautstärke ──────────────────────────────────────────

  /** @brief Set playback volume. @param vol 0 (silent) to 30 (max). Out-of-range values are clamped. */
  void setVolume(uint8_t vol) {
    if (vol > 30) vol = 30;
    _vol = vol;
    if (_ok) _player.volume(_vol);
  }
  /** @brief Increase volume by 1 step (max 30). */
  void volumeUp()   { if (_vol < 30) setVolume(_vol + 1); }
  /** @brief Decrease volume by 1 step (min 0). */
  void volumeDown() { if (_vol > 0)  setVolume(_vol - 1); }

  // ── Status ──────────────────────────────────────────────
  /** @brief Returns true if the DFPlayer initialised successfully. */
  bool    isOK()          const { return _ok; }
  /** @brief Returns true while a track is actively playing. */
  bool    isPlaying()     const { return _playing; }
  /** @brief Returns true for exactly one loop() cycle after the track changes. */
  bool    trackChanged()  const { return _track_changed; }
  /** @brief Returns the currently playing folder number. */
  uint8_t currentFolder() const { return _folder; }
  /** @brief Returns the currently playing track number. */
  uint8_t currentTrack()  const { return _track; }
  /** @brief Returns the current volume (0–30). */
  uint8_t volume()        const { return _vol; }

  /** @brief Returns how long the current track has been playing in milliseconds. 0 if paused or stopped. */
  uint32_t playingForMs() const {
    if (!_playing) return 0;
    return (uint32_t)(millis() - _play_start_ms);
  }

private:
  Stream&             _serial;
  DFRobotDFPlayerMini _player;

  bool    _ok            = false;
  bool    _playing       = false;
  bool    _track_changed = false;
  bool    _auto_next     = false;

  uint8_t  _folder = 1;
  uint8_t  _track  = 1;
  uint8_t  _vol    = 30;

  TrackStepFn _stepFn = nullptr;   // see setTrackIndex()
  TrackMapFn  _mapFn  = nullptr;   // see setTrackMap()

  /** @brief The ONLY place that tells the module to play: file number -> card position. */
  void sendPlay(uint8_t folder, uint8_t track) {
    _player.playFolder(folder, _mapFn ? _mapFn(folder, track) : track);
  }

  // Folder loop (playFolderLoop), driven by the firmware: on "finished" step()
  // starts the next song itself, so currentTrack() is always what really plays.
  static const uint16_t LOOP_DEBOUNCE_MS = 1000;   // ignore a 2nd "finished" right after a start
  bool _looping = false;

  /** @brief Next track number in direction dir; `wrap` = after the last comes the first. */
  uint8_t nextTrack(uint8_t from, int8_t dir, bool wrap) const {
    if (!_stepFn) {                                    // no index: the old behaviour
      if (dir > 0) return (uint8_t)(from + 1);
      return from > 1 ? (uint8_t)(from - 1) : from;
    }
    int16_t t = _stepFn(_folder, from, dir);
    if (t < 0 && wrap) t = _stepFn(_folder, 0, +1);    // past the end -> first track
    return t < 0 ? from : (uint8_t)t;
  }

  /** @brief First track of the current folder (1 without an index). */
  uint8_t firstTrack() const {
    if (!_stepFn) return 1;
    int16_t t = _stepFn(_folder, 0, +1);
    return t < 0 ? 1 : (uint8_t)t;
  }

  uint32_t _play_start_ms = 0;
  uint32_t _paused_at_ms  = 0;
  uint32_t _last_poll_ms  = 0;  // rate-limits available() polling
};

// Globale Instanz — Audio hier definiert, genau wie
// speakerController + BuzzerPlayer am Ende von Buzzer_Player.h.
//
// R4 WiFi: uses Serial1 (D0=RX, D1=TX).
//   In main.cpp call Serial1.begin(9600) BEFORE Audio.begin().
//
// AVR boards: define DF_USE_SOFTWARE_SERIAL + DF_RX_PIN/DF_TX_PIN
//   in main.cpp before including this file to use SoftwareSerial instead.
#ifdef DF_USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  #ifndef DF_RX_PIN
  #define DF_RX_PIN 6
  #endif
  #ifndef DF_TX_PIN
  #define DF_TX_PIN 7
  #endif
  SoftwareSerial     dfSerial(DF_RX_PIN, DF_TX_PIN);
  DFPlayerController DFPlayer(dfSerial);
#else
  // Default: hardware Serial1 (R4 WiFi D0/D1)
  DFPlayerController DFPlayer(Serial1);
#endif
