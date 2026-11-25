#pragma once
/*
  Cmd_Audio.h — `audio buzzer ...` (piezo songs + UI cues) and
                `audio speaker ...` (DFPlayer Mini MP3).

  Every action can be typed three ways:  -p  /  --play  /  play
  Right after "audio buzzer" / "audio speaker" these are ACTIONS, so -q means
  queue and -th means theme there; later in the line -q / -t / -h / -d are the
  global flags again (see doc/SHELL.md).

    audio buzzer                       status (song, playing, mute, theme, gap)
    audio buzzer -p  / --play  [n]     play song n (0-based) or the current one
    audio buzzer -n  / --next          next song
    audio buzzer -b  / --back          previous song
    audio buzzer -q  / --queue <n>     select song n without playing
    audio buzzer -l  / --list          list all songs
    audio buzzer -st / --stop          stop the music
    audio buzzer -m  / --mute          mute/unmute UI cues (songs keep playing), saved
    audio buzzer -th / --theme [pack]  set the cue theme, or list themes; saved
    audio buzzer -g  / --gap [b] [a]   silence before/after a cue in ms, or show; saved
    audio buzzer -cs / --cues          list every cue name
    audio buzzer -c  / --cue <name>    play one cue
    audio buzzer -v  / --volume [v]    song loudness 0..8, saved
    audio buzzer -cv / --cuevolume [v] UI cue loudness 0..8, saved

    audio speaker                      status (found?, folder/track, volume)
    audio speaker -p / --play <folder> [track]   play /FF/TTT.mp3 (track default 1)
    audio speaker -n / --next          next track
    audio speaker -b / --back          previous track
    audio speaker -l / --loop <folder> loop all tracks of a folder
    audio speaker -s / --stop          stop (pause — the DFPlayer has no hard stop)
    audio speaker -r / --resume        continue after stop
    audio speaker -v / --volume [v]    0..30, saved
    audio speaker -i  / --info         what is playing: title, artist, time
    audio speaker -ls / --list [folder] tracks of a folder (from the SD index)

    audio                              all volumes + master
    audio -a / --all [percent]         audio MASTER 0..100 %, scales speaker + buzzer

  Every volume value can be:  20 (set)   + / - (one step)   +3 / -3 (steps)
  or nothing (show). Levels go through Volume (Audio/Volume_Manager.h), which
  applies the audio master on top.

  Uses the globals from main.cpp: Buzzer, SONGLIST, UISFX_*, DFPlayer, Volume, Persist.
*/
#include <Shell.h>
#include "Audio/Track_Index.h"     // titles from the SD card's index.txt

// ── shared: "20" / "+" / "-" / "+3" / "-3" -> new value ──────────────────────
/**
 * @brief Turn a volume word into a new value, clamped to 0..max.
 * @param step  what a bare "+" / "-" changes (1 for levels, 10 for the master %)
 * @return false (and an error is printed) if the word is not a valid value.
 */
static bool volumeValue(const char* v, int current, int max, int step, int& out, CmdContext& ctx) {
  int32_t n;
  if (strcmp(v, "+") == 0)                                            out = current + step;
  else if (strcmp(v, "-") == 0)                                       out = current - step;
  else if ((v[0] == '+' || v[0] == '-') && ShellUtil::parseInt(v, n)) out = current + (int)n;   // +3 / -3
  else if (ShellUtil::parseInt(v, n))                                 out = (int)n;               // 20
  else {
    ctx.err("value must be 0..%d, +, -, +N or -N - got '%s'", max, v);
    return false;
  }
  if (out < 0)   out = 0;
  if (out > max) out = max;
  return true;
}

/** @brief Show or change one Volume channel (speaker / music / cues), then save. */
static void volumeCommand(VolumeManager::Channel c, const ParsedArgs& a, CmdContext& ctx) {
  int max = VolumeManager::maxOf(c);
  if (!a.has(0)) {
    ctx.out("%s volume %u/%d (heard at %u, master %u %%)", VolumeManager::name(c),
            (unsigned)Volume.level(c), max, (unsigned)Volume.effective(c), (unsigned)Volume.master());
    return;
  }
  int target;
  if (!volumeValue(a.word(0), Volume.level(c), max, 1, target, ctx)) return;
  Volume.setLevel(c, target);
  Persist.save();
  if (Volume.master() == VolumeManager::MASTER_MAX)
    ctx.log("%s volume -> %d/%d (saved)", VolumeManager::name(c), target, max);
  else
    ctx.log("%s volume -> %d/%d, heard at %u (master %u %%) (saved)", VolumeManager::name(c), target, max,
            (unsigned)Volume.effective(c), (unsigned)Volume.master());
}

// ════════════════════════════════════════════════════════════════════════════
// audio buzzer
// ════════════════════════════════════════════════════════════════════════════
static const ArgSpec BUZZER_PLAY_ARGS[]  = { { "n", ARG_INT, 0, 255, true, 0, nullptr, "song number, 0-based (see -l); none = current" } };
static const ArgSpec BUZZER_QUEUE_ARGS[] = { { "n", ARG_INT, 0, 255, false, 0, nullptr, "song number, 0-based (see -l)" } };
static const ArgSpec BUZZER_THEME_ARGS[] = { { "pack", ARG_WORD, 0, 0, true, 0, nullptr, "theme name; none = list themes" } };
static const ArgSpec BUZZER_GAP_ARGS[]   = {
  { "before", ARG_INT, 0, 2000, true, 0, nullptr, "ms of silence before a cue" },
  { "after",  ARG_INT, 0, 2000, true, 0, nullptr, "ms of silence after a cue" },
};
static const ArgSpec BUZZER_CUE_ARGS[]   = { { "name", ARG_WORD, 0, 0, false, 0, nullptr, "cue name (see -cs)" } };

static void buzzerStatus(const ParsedArgs&, CmdContext& ctx) {
  ctx.out("song %u/%u: %s | %s | cues %s | theme %s | gap %u/%u ms",
          (unsigned)Buzzer.songIndex(), (unsigned)Buzzer.songCount(), Buzzer.songName(),
          Buzzer.isPlaying() ? "playing" : "silent",
          Buzzer.isMuted() ? "muted" : "on", Buzzer.cuePackName(),
          (unsigned)Buzzer.cueGapBefore(), (unsigned)Buzzer.cueGapAfter());
  ctx.out("volume: music %u/%u, cues %u/%u (master %u %%)",
          (unsigned)Volume.level(VolumeManager::MUSIC), (unsigned)SPEAKER_MAX_VOLUME,
          (unsigned)Volume.level(VolumeManager::CUES), (unsigned)SPEAKER_MAX_VOLUME, (unsigned)Volume.master());
}

static const ArgSpec BUZZER_VOLUME_ARGS[] = { { "value", ARG_WORD, 0, 0, true, 0, nullptr, "0..8, + / -, +N / -N; none = show" } };
static void buzzerMusicVolume(const ParsedArgs& a, CmdContext& ctx) { volumeCommand(VolumeManager::MUSIC, a, ctx); }
static void buzzerCueVolume(const ParsedArgs& a, CmdContext& ctx)   { volumeCommand(VolumeManager::CUES, a, ctx); }

static void buzzerPlay(const ParsedArgs& a, CmdContext& ctx) {
  if (a.has(0)) {
    if (a.num(0) >= Buzzer.songCount()) {
      ctx.err("no song %ld - there are %u (0..%u), see 'audio buzzer -l'",
              (long)a.num(0), (unsigned)Buzzer.songCount(), (unsigned)(Buzzer.songCount() - 1));
      return;
    }
    Buzzer.play((uint8_t)a.num(0));
  } else {
    Buzzer.play();
  }
}

static void buzzerNext(const ParsedArgs&, CmdContext&) { Buzzer.next(); }
static void buzzerBack(const ParsedArgs&, CmdContext&) { Buzzer.previous(); }

static void buzzerQueue(const ParsedArgs& a, CmdContext& ctx) {
  if (a.num(0) >= Buzzer.songCount()) {
    ctx.err("no song %ld - there are %u (0..%u)", (long)a.num(0),
            (unsigned)Buzzer.songCount(), (unsigned)(Buzzer.songCount() - 1));
    return;
  }
  Buzzer.select((uint8_t)a.num(0));
}

static void buzzerList(const ParsedArgs&, CmdContext& ctx) {
  for (uint8_t i = 0; i < Buzzer.songCount(); i++)
    ctx.out("  %u: %s%s", (unsigned)i, SONGLIST[i].name, (i == Buzzer.songIndex()) ? "  <- current" : "");
}

static void buzzerStop(const ParsedArgs&, CmdContext& ctx) {
  Buzzer.stop();
  ctx.log("music stopped");
}

static void buzzerMute(const ParsedArgs&, CmdContext&) {
  Buzzer.toggleMute();          // logs "cues muted/unmuted" itself
  Persist.save();
}

static void buzzerTheme(const ParsedArgs& a, CmdContext& ctx) {
  if (!a.has(0)) {
    ctx.out("themes (current: %s):", Buzzer.cuePackName());
    for (uint8_t i = 0; i < UISFX_PACK_COUNT; i++) ctx.out("  %s", UISFX_PACKS[i].name);
    return;
  }
  for (uint8_t i = 0; i < UISFX_PACK_COUNT; i++) {
    if (ShellUtil::eqNoCase(UISFX_PACKS[i].name, a.word(0))) {
      Buzzer.setCuePack(i);
      Persist.save();
      ctx.log("cue theme -> %s (saved)", UISFX_PACKS[i].name);
      return;
    }
  }
  ctx.err("unknown theme '%s' - type 'audio buzzer -th' for the list", a.word(0));
}

static void buzzerGap(const ParsedArgs& a, CmdContext& ctx) {
  if (!a.has(0)) {
    ctx.out("cue gap: %u ms before, %u ms after", (unsigned)Buzzer.cueGapBefore(), (unsigned)Buzzer.cueGapAfter());
    return;
  }
  if (!a.has(1)) {
    ctx.err("give both values: audio buzzer -g <before> <after>   e.g. -g 30 80");
    return;
  }
  Buzzer.setCueGap((uint16_t)a.num(0), (uint16_t)a.num(1));
  Persist.save();
  ctx.log("cue gap -> %ld ms before, %ld ms after (saved)", (long)a.num(0), (long)a.num(1));
}

static void buzzerCues(const ParsedArgs&, CmdContext& ctx) {
  char row[100];
  uint8_t len = 0;
  for (uint8_t i = 0; i < UISFX_CUE_COUNT; i++) {
    uint8_t n = (uint8_t)strlen(UISFX_CUES[i].name);
    if ((size_t)(len + n + 2) >= sizeof(row)) { row[len] = '\0'; ctx.out("  %s", row); len = 0; }
    if (len) row[len++] = ' ';
    memcpy(row + len, UISFX_CUES[i].name, n);
    len += n;
  }
  if (len) { row[len] = '\0'; ctx.out("  %s", row); }
}

static void buzzerCue(const ParsedArgs& a, CmdContext& ctx) {
  for (uint8_t i = 0; i < UISFX_CUE_COUNT; i++) {
    if (ShellUtil::eqNoCase(UISFX_CUES[i].name, a.word(0))) {
      if (Buzzer.isMuted()) ctx.warn("cues are muted - unmute with 'audio buzzer -m'");
      else if (Volume.effective(VolumeManager::CUES) == 0)
        ctx.warn("cue volume is 0 - raise it with 'audio buzzer -cv 5' (or the master: 'audio -a')");
      Buzzer.playCue(i);
      ctx.log("cue %s", UISFX_CUES[i].name);
      return;
    }
  }
  ctx.err("unknown cue '%s' - type 'audio buzzer -cs' for the list", a.word(0));
}

static const CommandDef D_BUZZER_STATUS = { "status", "song, playing, mute, theme, gap", nullptr,
  SHELL_NONE, SHELL_NONE, "audio buzzer", nullptr, buzzerStatus };
static const CommandDef D_BUZZER_PLAY  = { "play",  "play song n, or the current song", "-p --play",
  SHELL_LIST(BUZZER_PLAY_ARGS), SHELL_NONE, "audio buzzer -p 2", nullptr, buzzerPlay };
static const CommandDef D_BUZZER_NEXT  = { "next",  "next song (plays if music is on)", "-n --next",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, buzzerNext };
static const CommandDef D_BUZZER_BACK  = { "back",  "previous song (plays if music is on)", "-b --back",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, buzzerBack };
static const CommandDef D_BUZZER_QUEUE = { "queue", "select song n without playing", "-q --queue",
  SHELL_LIST(BUZZER_QUEUE_ARGS), SHELL_NONE, "audio buzzer -q 1", nullptr, buzzerQueue };
static const CommandDef D_BUZZER_LIST  = { "list",  "list all songs", "-l --list",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, buzzerList };
static const CommandDef D_BUZZER_STOP  = { "stop",  "stop the music", "-st --stop",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, buzzerStop };
static const CommandDef D_BUZZER_MUTE  = { "mute",  "mute/unmute UI cues (songs keep playing), saved", "-m --mute",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, buzzerMute };
static const CommandDef D_BUZZER_THEME = { "theme", "set the cue theme, or list themes; saved", "-th --theme",
  SHELL_LIST(BUZZER_THEME_ARGS), SHELL_NONE, "audio buzzer -th arcade", nullptr, buzzerTheme };
static const CommandDef D_BUZZER_GAP   = { "gap",   "silence around cues (ms), or show it; saved", "-g --gap",
  SHELL_LIST(BUZZER_GAP_ARGS), SHELL_NONE, "audio buzzer -g 30 80",
  "A cue pauses a running song. <before> is silence before the cue,\n"
  "<after> is silence before the song continues. Default 20/60 ms.", buzzerGap };
static const CommandDef D_BUZZER_CUES  = { "cues",  "list every cue name", "-cs --cues",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, buzzerCues };
static const CommandDef D_BUZZER_CUE   = { "cue",   "play one cue (pauses a song, then resumes)", "-c --cue",
  SHELL_LIST(BUZZER_CUE_ARGS), SHELL_NONE, "audio buzzer -c press", nullptr, buzzerCue };

FnCommand cmdBuzzerStatus(D_BUZZER_STATUS);
FnCommand cmdBuzzerPlay(D_BUZZER_PLAY),   cmdBuzzerNext(D_BUZZER_NEXT),   cmdBuzzerBack(D_BUZZER_BACK);
FnCommand cmdBuzzerQueue(D_BUZZER_QUEUE), cmdBuzzerList(D_BUZZER_LIST),   cmdBuzzerStop(D_BUZZER_STOP);
FnCommand cmdBuzzerMute(D_BUZZER_MUTE),   cmdBuzzerTheme(D_BUZZER_THEME), cmdBuzzerGap(D_BUZZER_GAP);
static const CommandDef D_BUZZER_VOLUME = { "volume", "song loudness 0..8 (0 = silent), saved", "-v --volume",
  SHELL_LIST(BUZZER_VOLUME_ARGS), SHELL_NONE, "audio buzzer -v 5",
  "8 = full (like before), 7 = first quieter step, 1 = very quiet.\n"
  "From about 3 down the sound gets a bit thin (piezo).", buzzerMusicVolume };
static const CommandDef D_BUZZER_CUEVOL = { "cuevolume", "UI cue loudness 0..8 (0 = silent), saved", "-cv --cuevolume",
  SHELL_LIST(BUZZER_VOLUME_ARGS), SHELL_NONE, "audio buzzer -cv -", nullptr, buzzerCueVolume };

FnCommand cmdBuzzerCues(D_BUZZER_CUES),   cmdBuzzerCue(D_BUZZER_CUE);
FnCommand cmdBuzzerVolume(D_BUZZER_VOLUME), cmdBuzzerCueVol(D_BUZZER_CUEVOL);

Command* const BUZZER_CMDS[] = {
  // music
  &cmdBuzzerPlay, &cmdBuzzerNext, &cmdBuzzerBack, &cmdBuzzerQueue, &cmdBuzzerList, &cmdBuzzerStop,
  &cmdBuzzerVolume,
  // sound effects (UI cues)
  &cmdBuzzerMute, &cmdBuzzerTheme, &cmdBuzzerGap, &cmdBuzzerCues, &cmdBuzzerCue, &cmdBuzzerCueVol,
};
CommandGroup buzzerGroup("buzzer", "piezo: songs + UI sound cues", BUZZER_CMDS, SHELL_COUNT(BUZZER_CMDS),
  "The piezo buzzer on D9. Plays the song list and the short UI cues\n"
  "(button feedback). Song numbers are 0-based, see 'audio buzzer -l'.",
  &cmdBuzzerStatus);

// ════════════════════════════════════════════════════════════════════════════
// audio speaker (DFPlayer Mini)
// ════════════════════════════════════════════════════════════════════════════
static const ArgSpec SPEAKER_PLAY_ARGS[] = {
  { "folder", ARG_INT, 1, 99,  false, 1, nullptr, "SD folder 01..99" },
  { "track",  ARG_INT, 1, 255, true,  1, nullptr, "file 001..255 in that folder" },
};
static const ArgSpec SPEAKER_LOOP_ARGS[] = { { "folder", ARG_INT, 1, 99, false, 1, nullptr, "SD folder 01..99" } };
static const ArgSpec SPEAKER_VOL_ARGS[]  = { { "value", ARG_WORD, 0, 0, true, 0, nullptr, "0..30, + / - (one step), +N / -N (N steps); none = show" } };

static bool speakerReady(CmdContext& ctx) {
  if (DFPlayer.isOK()) return true;
  ctx.err("DFPlayer not found - check wiring (D0/D1) and the SD card");
  return false;
}

static void speakerStatus(const ParsedArgs&, CmdContext& ctx) {
  char line[96];
  TrackIndex::describe(DFPlayer.currentFolder(), DFPlayer.currentTrack(), line, sizeof(line));
  ctx.out("speaker: %s%s | volume %u/30 (heard at %u)",
          !DFPlayer.isOK() ? "not found" : DFPlayer.isPlaying() ? "playing" : "ready",
          DFPlayer.isLooping() ? " (folder loop)" : "",
          (unsigned)Volume.level(VolumeManager::SPEAKER), (unsigned)Volume.effective(VolumeManager::SPEAKER));
  ctx.out("track:   %s", line);
}

/** @brief Log the current track with its title (and stop TrackIndex::watch() repeating it). */
static void logCurrentTrack(CmdContext& ctx) {
  char line[96];
  uint8_t f = DFPlayer.currentFolder(), t = DFPlayer.currentTrack();
  TrackIndex::describe(f, t, line, sizeof(line));
  TrackIndex::markAnnounced(f, t);
  ctx.log("speaker: %s", line);
}

static void speakerPlay(const ParsedArgs& a, CmdContext& ctx) {
  if (!speakerReady(ctx)) return;
  uint8_t folder = (uint8_t)a.num(0);
  uint8_t track  = (uint8_t)a.num(1);
  if (!a.has(1)) {                                       // no track: first one that exists
    int16_t first = TrackIndex::step(folder, 0, +1);
    track = first > 0 ? (uint8_t)first : 1;
  } else if (TrackIndex::countIn(folder) && !TrackIndex::find(folder, track)) {
    ctx.warn("track %u is not in folder %02u's index - missing on the SD card?", track, folder);
  }
  DFPlayer.playFolder(folder, track);
  logCurrentTrack(ctx);
}
static void speakerNext(const ParsedArgs&, CmdContext& ctx) {
  if (!speakerReady(ctx)) return;
  DFPlayer.next();                                       // skips gaps, wraps to the first
  logCurrentTrack(ctx);
}
static void speakerBack(const ParsedArgs&, CmdContext& ctx) {
  if (!speakerReady(ctx)) return;
  if (TrackIndex::step(DFPlayer.currentFolder(), DFPlayer.currentTrack(), -1) < 0) {
    ctx.warn("already at the first track");
    return;
  }
  DFPlayer.previous();
  logCurrentTrack(ctx);
}

static void speakerInfo(const ParsedArgs&, CmdContext& ctx) {
  uint8_t f = DFPlayer.currentFolder(), t = DFPlayer.currentTrack();
  const TrackInfo* info = TrackIndex::find(f, t);
  ctx.out("%s%s  %02u/%03u", !DFPlayer.isOK() ? "not found" : DFPlayer.isPlaying() ? "playing" : "stopped",
          DFPlayer.isLooping() ? " (folder loop)" : "", f, t);
  if (!info) {
    ctx.out("  not in the index (run the microssd downloader, check custom_sd_root)");
    return;
  }
  char len[8];
  uint32_t played = DFPlayer.isPlaying() ? DFPlayer.playingForMs() / 1000 : 0;
  TrackIndex::formatTime(info->seconds, len, sizeof(len));
  ctx.out("  title:  %s", info->title);
  ctx.out("  by:     %s", info->artist);
  ctx.out("  time:   %lu:%02lu / %s", (unsigned long)(played / 60), (unsigned long)(played % 60), len);
  if (TrackIndex::rotation(f) >= 0)
    ctx.out("  card:   position %u (card shift %d)", TrackIndex::cardPosition(f, t), TrackIndex::rotation(f));
}

static const ArgSpec SPEAKER_SHIFT_ARGS[] = { { "n", ARG_INT, -1, 99, true, 0, nullptr, "files in front: 1 = one too many first, 0 = sorted card, -1 = off; none = show" } };
static void speakerShift(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t f = DFPlayer.currentFolder();
  if (a.has(0)) {
    TrackIndex::setRotation(f, (int16_t)a.num(0));
    ctx.log("folder %02u card shift -> %ld (until reboot; permanent: CARD_ORDER_FIX in Track_Index.h)", f, (long)a.num(0));
  }
  int16_t r = TrackIndex::rotation(f);
  if (r < 0) { ctx.out("folder %02u: no card-order fix - track numbers are sent as they are", f); return; }
  ctx.out("folder %02u: card shift %d - file -> card position:", f, r);
  uint8_t shown = 0;
  for (uint16_t i = 0; i < TRACK_COUNT && shown < 4; i++)
    if (TRACKS[i].folder == f) { ctx.out("  %03u -> %u", TRACKS[i].track, TrackIndex::cardPosition(f, TRACKS[i].track)); shown++; }
  ctx.out("  (try: audio speaker -p %u <n>, listen, adjust with --shift)", f);
}

static const ArgSpec SPEAKER_LIST_ARGS[] = { { "folder", ARG_INT, 1, 99, true, 2, nullptr, "SD folder; none = the current one" } };
static void speakerList(const ParsedArgs& a, CmdContext& ctx) {
  uint8_t folder = a.has(0) ? (uint8_t)a.num(0) : DFPlayer.currentFolder();
  uint16_t n = TrackIndex::countIn(folder);
  if (!n) { ctx.out("folder %02u: no index (no index.txt, or custom_sd_root is wrong)", folder); return; }
  ctx.out("folder %02u: %u tracks", folder, n);
  char len[8];
  for (uint16_t i = 0; i < TRACK_COUNT; i++) {
    const TrackInfo& t = TRACKS[i];
    if (t.folder != folder) continue;
    TrackIndex::formatTime(t.seconds, len, sizeof(len));
    bool playing = DFPlayer.isPlaying() && DFPlayer.currentFolder() == folder && DFPlayer.currentTrack() == t.track;
    ctx.out("  %03u %5s  %s%s", t.track, len, t.title, playing ? "  <- playing" : "");
  }
}
static void speakerLoop(const ParsedArgs& a, CmdContext& ctx) {
  if (!speakerReady(ctx)) return;
  DFPlayer.playFolderLoop((uint8_t)a.num(0));
  ctx.log("speaker: looping folder %02ld (%u tracks in the index)", (long)a.num(0), TrackIndex::countIn((uint8_t)a.num(0)));
  logCurrentTrack(ctx);
}
static void speakerStop(const ParsedArgs&, CmdContext& ctx) {
  if (!speakerReady(ctx)) return;
  DFPlayer.pause();
  ctx.log("speaker: stopped (resume with 'audio speaker -r')");
}
static void speakerResume(const ParsedArgs&, CmdContext& ctx) {
  if (!speakerReady(ctx)) return;
  DFPlayer.resume();
  ctx.log("speaker: resumed");
}

static void speakerVolume(const ParsedArgs& a, CmdContext& ctx) { volumeCommand(VolumeManager::SPEAKER, a, ctx); }

static const CommandDef D_SPEAKER_STATUS = { "status", "found?, folder/track, volume", nullptr,
  SHELL_NONE, SHELL_NONE, "audio speaker", nullptr, speakerStatus };
static const CommandDef D_SPEAKER_PLAY   = { "play",   "play folder (+ track, default 1)", "-p --play",
  SHELL_LIST(SPEAKER_PLAY_ARGS), SHELL_NONE, "audio speaker -p 2 5", nullptr, speakerPlay };
static const CommandDef D_SPEAKER_NEXT   = { "next",   "next track", "-n --next",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, speakerNext };
static const CommandDef D_SPEAKER_BACK   = { "back",   "previous track", "-b --back",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, speakerBack };
static const CommandDef D_SPEAKER_LOOP   = { "loop",   "loop all tracks of a folder", "-l --loop",
  SHELL_LIST(SPEAKER_LOOP_ARGS), SHELL_NONE, "audio speaker -l 2", nullptr, speakerLoop };
static const CommandDef D_SPEAKER_STOP   = { "stop",   "stop (pause - DFPlayer has no hard stop)", "-s --stop",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, speakerStop };
static const CommandDef D_SPEAKER_INFO = { "info", "what is playing: title, artist, time", "-i --info",
  SHELL_NONE, SHELL_NONE, "audio speaker -i", nullptr, speakerInfo };
static const CommandDef D_SPEAKER_LIST = { "list", "tracks of a folder (from the SD index)", "-ls --list",
  SHELL_LIST(SPEAKER_LIST_ARGS), SHELL_NONE, "audio speaker -ls 2",
  "Names come from the index.txt files next to the MP3s (microssd tool),\n"
  "built into the firmware on every 'pio run' (custom_sd_root).", speakerList };
static const CommandDef D_SPEAKER_SHIFT = { "shift", "TEMP: card-order fix (which file the module plays first)", "-sh --shift",
  SHELL_LIST(SPEAKER_SHIFT_ARGS), SHELL_NONE, "audio speaker --shift 1",
  "This DFPlayer clone plays the Nth file in CARD order, not '00N'.\n"
  "Shift 1 = one file too many in front (Shout on the current card).\n"
  "Changes last until reboot; the default is CARD_ORDER_FIX in\n"
  "include/Audio/Track_Index.h. Set 0 there once the card is sorted.", speakerShift };
static const CommandDef D_SPEAKER_RESUME = { "resume", "continue after stop", "-r --resume",
  SHELL_NONE, SHELL_NONE, nullptr, nullptr, speakerResume };
static const CommandDef D_SPEAKER_VOLUME = { "volume", "set / step / show the volume (0..30), saved", "-v --volume",
  SHELL_LIST(SPEAKER_VOL_ARGS), SHELL_NONE, "audio speaker -v +",
  "audio speaker -v 20   set to 20\n"
  "audio speaker -v +    one step louder,  -v -   one step quieter\n"
  "audio speaker -v +5   five steps louder, -v -5  five steps quieter\n"
  "audio speaker -v      show the current volume", speakerVolume };

FnCommand cmdSpeakerStatus(D_SPEAKER_STATUS);
FnCommand cmdSpeakerPlay(D_SPEAKER_PLAY), cmdSpeakerNext(D_SPEAKER_NEXT), cmdSpeakerBack(D_SPEAKER_BACK);
FnCommand cmdSpeakerLoop(D_SPEAKER_LOOP), cmdSpeakerStop(D_SPEAKER_STOP), cmdSpeakerResume(D_SPEAKER_RESUME);
FnCommand cmdSpeakerVolume(D_SPEAKER_VOLUME);
FnCommand cmdSpeakerInfo(D_SPEAKER_INFO), cmdSpeakerList(D_SPEAKER_LIST), cmdSpeakerShift(D_SPEAKER_SHIFT);

Command* const SPEAKER_CMDS[] = {
  &cmdSpeakerPlay, &cmdSpeakerNext, &cmdSpeakerBack, &cmdSpeakerLoop,
  &cmdSpeakerStop, &cmdSpeakerResume, &cmdSpeakerVolume, &cmdSpeakerInfo, &cmdSpeakerList, &cmdSpeakerShift,
};
CommandGroup speakerGroup("speaker", "DFPlayer Mini MP3 from the SD card", SPEAKER_CMDS, SHELL_COUNT(SPEAKER_CMDS),
  "MP3s from the SD card: folders 01..99, files 001..255 (doc/SD_CARD.md).\n"
  "Folder 02 is the Party playlist.",
  &cmdSpeakerStatus);

// ════════════════════════════════════════════════════════════════════════════
// audio  (+ master)
// ════════════════════════════════════════════════════════════════════════════
static const ArgSpec AUDIO_MASTER_ARGS[] = { { "percent", ARG_WORD, 0, 0, true, 0, nullptr, "0..100, + / - (10 %), +N / -N; none = show" } };

static void audioStatus(const ParsedArgs&, CmdContext& ctx) {
  for (uint8_t i = 0; i < VolumeManager::CHANNEL_COUNT; i++) {
    VolumeManager::Channel c = (VolumeManager::Channel)i;
    ctx.out("  %-8s %2u/%-2u -> heard at %2u", VolumeManager::name(c), (unsigned)Volume.level(c),
            (unsigned)VolumeManager::maxOf(c), (unsigned)Volume.effective(c));
  }
  ctx.out("  master   %u %%%s", (unsigned)Volume.master(), Buzzer.isMuted() ? "   (cues muted)" : "");
}

static void audioMaster(const ParsedArgs& a, CmdContext& ctx) {
  if (!a.has(0)) { audioStatus(a, ctx); return; }
  int target;
  if (!volumeValue(a.word(0), Volume.master(), VolumeManager::MASTER_MAX, 10, target, ctx)) return;
  Volume.setMaster(target);
  Persist.save();
  ctx.log("audio master -> %d %% (saved)", target);
}

static const CommandDef D_AUDIO_STATUS = { "status", "all volumes + master", nullptr,
  SHELL_NONE, SHELL_NONE, "audio", nullptr, audioStatus };
static const CommandDef D_AUDIO_MASTER = { "master", "audio master 0..100 %: scales speaker + buzzer, saved", "-a --all",
  SHELL_LIST(AUDIO_MASTER_ARGS), SHELL_NONE, "audio -a 50",
  "Every channel keeps its own level; the master scales all of them:\n"
  "  heard = level * master / 100   (speaker 20, master 50 % -> 10)\n"
  "+ / - change the master by 10 %, +N / -N by N %.", audioMaster };
FnCommand cmdAudioStatus(D_AUDIO_STATUS), cmdAudioMaster(D_AUDIO_MASTER);

Command* const AUDIO_CMDS[] = { &buzzerGroup, &speakerGroup, &cmdAudioMaster };
CommandGroup audioGroup("audio", "sound: piezo buzzer + MP3 speaker", AUDIO_CMDS, SHELL_COUNT(AUDIO_CMDS),
  "Speaker (MP3) 0..30, buzzer music and cues 0..8, plus a master in %.\n"
  "Type 'audio' alone to see all volumes.",
  &cmdAudioStatus);
