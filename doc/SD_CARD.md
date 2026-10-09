# SD card structure

The DFPlayer Mini plays MP3 files from a microSD card. This document describes how the card is organised.

## Card requirements

- microSD card, **FAT32** (8 GB card in the [BOM](../BOM.csv)); format it as FAT32 / MS-DOS (FAT) before copying.
- MP3 files, 44.1 kHz, 128 kbit/s stereo recommended.
- Folder names are **two digits** (`01`..`99`), file names are **three digits** (`001.mp3`..`255.mp3`). The DFPlayer addresses files by these numbers, not by their song name.
- Max. 255 files per folder (the firmware uses `playFolder(folder, track)` which plays `/FF/TTT.mp3`).

## Folder layout

| Folder | Used for | Status |
|--------|----------|--------|
| `01` | Miscellaneous: jingles, announcements, sound effects, anything not tied to a mode | reserved |
| `02` | **Mode 1 – Party**: music playlist (`PARTY_FOLDER = 2` in `Modes.h`) | in use |
| `03` | Mode 2 – LinkClick | reserved |
| `04` | Mode 3 | reserved |
| `05`.. | Further modes, in the order of `MODE_LIST` in `Modes.h` | reserved |

Rule: **mode number N uses folder N+1**, folder `01` is always the misc folder.

```
SD card (FAT32)
├── 01/                 misc (jingles, effects)
│   ├── 001.mp3
│   └── 002.mp3
├── 02/                 Party mode playlist
│   ├── 001.mp3
│   ├── 002.mp3
│   └── ...             (tracks play in numeric order, auto-next)
├── 03/                 LinkClick mode
├── 04/                 Mode 3
└── lists/              optional: track-name lists (see below), ignored by the DFPlayer
    └── 02.json
```

## How the firmware uses it

- **Party mode** starts `DFPlayer.playFolder(2, 1)` with auto-next on: it plays `/02/001.mp3`, `/02/002.mp3`, ... and restarts at track 1 when the folder ends. Double press = next track, triple press = previous track, single press = stop / start.
- From the serial console: `audio -play <folder> <track>` (e.g. `audio -play 1 1` for the first misc file), `audio -loop <folder>`, `audio -next`, `audio -prev`, `audio -pause`, `audio -resume`, `audio -stop`, `audio -vol <0-30>`.
- If the card is missing or unreadable, Party mode shows a warning and runs the lights only.

## Preparing the card

Download a track as MP3 (named by number so the DFPlayer can find it):

```bash
yt-dlp -x --audio-format mp3 --audio-quality 128K \
  --postprocessor-args "ffmpeg:-ar 44100 -ac 2" \
  --no-embed-thumbnail --no-embed-metadata \
  -o "~/Downloads/%(title)s.%(ext)s" "<youtube-url>"
```

Then number the files into the folder: rename by hand to `001.mp3`, `002.mp3`, ... and keep the order stable, because the number is the track ID. A helper script (`mp3_prepare.py`, not part of this repository) can copy a source folder into a numbered DFPlayer folder and write the track list:

```bash
python3 mp3_prepare.py ~/Downloads/Party /Volumes/SDCARD/02
```

After copying on macOS, remove the hidden files the Finder adds, otherwise the DFPlayer can count them as tracks or play wrong files:

```bash
dot_clean /Volumes/SDCARD
find /Volumes/SDCARD -name ".DS_Store" -delete
rm -rf /Volumes/SDCARD/.Spotlight-V100 /Volumes/SDCARD/.fseventsd /Volumes/SDCARD/.Trashes
```

Eject the card properly before putting it into the DFPlayer.

## Notes

- The songs themselves are **not** part of the repository (copyright). Only this structure is documented.
- The `lists/` folder (e.g. `02.json`) only maps track numbers to song names for humans; the DFPlayer never reads it.
- A new mode that should play music needs: a folder `NN` on the card, a folder constant in `Modes.h` and a `playFolder(NN, 1)` call in the mode's start action.
