// Example song (original, public domain): C major scale up and down.
// Copy this file's layout for your own songs, or generate one with
// doc/sound/songs/convert_song.py. Only this file in Songs/ is tracked by git.
#ifndef EXAMPLESONG_SONG_H
#define EXAMPLESONG_SONG_H
#include <SongPlayer.h>

// 15 notes, total duration ~3.0 s
const SongNote Example_Scale[] = {
  {  60,  180,    0u },
  {  62,  180,  200u },
  {  64,  180,  400u },
  {  65,  180,  600u },
  {  67,  180,  800u },
  {  69,  180, 1000u },
  {  71,  180, 1200u },
  {  72,  360, 1400u },
  {  71,  180, 1800u },
  {  69,  180, 2000u },
  {  67,  180, 2200u },
  {  65,  180, 2400u },
  {  64,  180, 2600u },
  {  62,  180, 2800u },
  {  60,  360, 3000u },
};
const size_t Example_Scale_notes_count = sizeof(Example_Scale) / sizeof(Example_Scale[0]);

#endif
