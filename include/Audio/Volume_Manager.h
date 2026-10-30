#ifndef VOLUME_MANAGER_H
#define VOLUME_MANAGER_H
/*
  Volume_Manager.h — ONE place for all loudness (like Lights/Brightness_Manager.h).

  Three channels, each with its own LEVEL:
    SPEAKER   DFPlayer MP3s       0..30
    MUSIC     buzzer songs        0..8   (piezo, see SongPlayer.h)
    CUES      buzzer UI cues      0..8
  On top: an audio MASTER in percent (0..100) that scales all of them:

      heard level = level * master / 100   (rounded)
      e.g. speaker 20, master 50 %  ->  DFPlayer at 10

  The DFPlayer and the piezo are different devices, so "the same level" never
  sounds exactly equally loud — the master only scales them together.

  WHO CHANGES WHAT
    Settings mode   speaker volume + cue volume   (Settings_Items.h)
    CLI             audio speaker -v / audio buzzer -v / -cv / audio -a
  Never writes flash itself — call Persist.save() after a change that should
  survive a power loss (Settings_Store.h saves all four values).

  USAGE
    Volume.setLevel(VolumeManager::CUES, 4);
    Volume.setMaster(50);                          // everything at half
    uint8_t lvl   = Volume.level(VolumeManager::SPEAKER);
    uint8_t heard = Volume.effective(VolumeManager::SPEAKER);

  IMPORTANT: include in main.cpp AFTER Buzzer (Buzzer_Player.h) and DFPlayer
  exist, and BEFORE Storage/Settings_Store.h.
*/

class VolumeManager {
public:
  enum Channel : uint8_t { SPEAKER, MUSIC, CUES, CHANNEL_COUNT };

  static const uint8_t MASTER_MAX = 100;   // percent

  /** @brief Highest level of a channel (30 for the DFPlayer, 8 for the buzzer). */
  static uint8_t maxOf(Channel c) { return c == SPEAKER ? 30 : SPEAKER_MAX_VOLUME; }

  static const char* name(Channel c) {
    switch (c) {
      case SPEAKER: return "speaker";
      case MUSIC:   return "music";
      case CUES:    return "cues";
      default:      return "?";
    }
  }

  // ── per channel ──
  uint8_t level(Channel c) const { return c < CHANNEL_COUNT ? _level[c] : 0; }

  /** @brief Set a channel's own level (clamped to 0..maxOf) and apply at once. */
  void setLevel(Channel c, int v) {
    if (c >= CHANNEL_COUNT) return;
    _level[c] = clamp(v, maxOf(c));
    apply(c);
  }

  /** @brief Settings-mode step: +step, wrapping back to 0 past the maximum. */
  uint8_t stepLevel(Channel c, uint8_t step) {
    int v = level(c) + step;
    if (v > maxOf(c)) v = 0;
    setLevel(c, v);
    return (uint8_t)v;
  }

  /** @brief What is really played: level * master / 100, rounded. */
  uint8_t effective(Channel c) const {
    return (uint8_t)(((uint16_t)level(c) * _master + MASTER_MAX / 2) / MASTER_MAX);
  }

  // ── master (CLI: audio -a) ──
  uint8_t master() const { return _master; }
  void setMaster(int percent) {
    _master = clamp(percent, MASTER_MAX);
    applyAll();
  }

  /** @brief Set everything at once (used when loading saved settings). */
  void setAll(uint8_t speaker, uint8_t music, uint8_t cues, uint8_t master) {
    _level[SPEAKER] = clamp(speaker, maxOf(SPEAKER));
    _level[MUSIC]   = clamp(music,   maxOf(MUSIC));
    _level[CUES]    = clamp(cues,    maxOf(CUES));
    _master         = clamp(master,  MASTER_MAX);
    applyAll();
  }

  void applyAll() { for (uint8_t i = 0; i < CHANNEL_COUNT; i++) apply((Channel)i); }

private:
  uint8_t _level[CHANNEL_COUNT] = { 20, SPEAKER_MAX_VOLUME, SPEAKER_MAX_VOLUME };   // DFPlayer.begin(20) default
  uint8_t _master = MASTER_MAX;

  static uint8_t clamp(int v, uint8_t max) { return v < 0 ? 0 : (v > max ? max : (uint8_t)v); }

  void apply(Channel c) {
    switch (c) {
      case SPEAKER: DFPlayer.setVolume(effective(SPEAKER)); break;
      case MUSIC:   Buzzer.setMusicVolume(effective(MUSIC)); break;
      case CUES:    Buzzer.setCueVolume(effective(CUES));    break;
      default: break;
    }
  }
};

// The one global manager, wired to Buzzer + DFPlayer from main.cpp.
VolumeManager Volume;

#endif // VOLUME_MANAGER_H
