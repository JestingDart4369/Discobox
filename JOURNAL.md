# Discobox – Development Journal

Repository: <https://github.com/JestingDart4369/Discobox>

Git-based devlog: every entry is dated and, where it was committed, links to the commit
hash (`git log --date=short` shows the same history). Newest entries first.

Dates in the 2026-10 entries come from the working session clock; the code of 2026-10-02
was committed together on 2026-10-09 (`0fe7ce6`), so `git log` shows that day.

---

## 2026-10-09

Commits: `9138a0c`, `4d7fca2`, `6014466` (repo cleanup); `0fe7ce6` (the work of 2026-10-02).
- Copyrighted songs removed from the repo, example song added, `.gitignore`, MIT license, README (`9138a0c`).
- Removed `doc/Functionality.md` and `.DS_Store` (`4d7fca2`, `6014466`).
- README: "AI usage" section.
- This journal (`JOURNAL.md`) added.

## 2026-10-02

Committed in `0fe7ce6`.

**DFPlayer**
- Global instance renamed `Audio` -> `DFPlayer` (header, `main.cpp`, settings).
- `#ifdef DFPLAYERCONTROLLER_H` guards in `Settings_Items.h` / `Settings_Store.h`, so they compile without the DFPlayer library.
- Wiring comment in `DFPlayerController.h` corrected (Serial1: D0 = RX, D1 = TX).
- Party mode now plays the DFPlayer playlist from SD folder `02`; SP starts/stops, DP/TP skip tracks. The piezo no longer plays songs in Party (it still plays the UI cues).

**WS2805 LED strip**
- Pixel count corrected: the BTF 12 V strip has 20 ICs/m with 3 LEDs per IC, so 3 m = 60 ICs (`WS2805<60>`), not 180.
- Driver rewritten to the Worldsemi datasheet: SPI at 3.0 MHz, 4 SPI bits per data bit (`0` = `1000`, `1` = `1100`), data order R, G, B, W1, W2, reset gap of at least 280 us (130 zero bytes, about 347 us).
- Added `WS2805_INDEX_SHIFT` (off-by-one between buffer index and physical IC), `WS2805_SPI_HZ`, `WS2805_CHUNK_BYTES`.
- Arduino `random()` took about 95 ms per call on the R4 (5.7 s per disco frame). Replaced with a small xorshift generator in the strip class; disco and flicker are fast now.
- Findings: the strip needs a data input of at least about 3.5 V, the Uno R4 outputs 3.3 V; a series resistor of 3 kOhm distorted the signal and was removed; BIN (blue wire) is tied to DAT.

**Serial console**
- New `audio` commands: play folder/track, loop, pause, resume, next, prev, stop, volume, volume up/down, status; `status` shows the audio state; `help audio`.
- New `strip` commands: colour, `clear`, `ww`, `cw`, `cct`, `hsv`, `bright`, `fade`, `wipe`, `led`, `fadeic`, `ic`, `flicker`, `walk`, `disco`, `bench` (timing).

**Tests, build and docs**
- Native unit tests (Unity, `pio test -e native`) with Arduino and DFPlayer mocks: 8 GestureButton, 30 DFPlayerController, 12 SongPlayer tests.
- `platformio.ini`: `build_src_filter = -<*>` for the native env (no FastLED / board headers on the PC), `default_envs = uno_r4_wifi` so `pio run` / upload no longer builds the test env.
- Architecture diagram: test layer added, `DFPlayer.setVolume()` reference fixed. Wiring diagram updated for the strip.
- `CONTEXT.md` summary file for starting new chats.

**Open issues**
- The first IC (first 3 LEDs) flickers and sometimes does not turn off; likely the 3.3 V data level at the first IC. Planned fix: a 74AHCT125 / 74HCT125 level shifter.
- The last IC on the strip does not always respond.

## 2026-09-24

Commits: `11e7e7f`, `992c0de`, `9fece36`.
- Reusable code moved into `lib/` as header-only libraries (GestureButton, Logger, SongPlayer, LedController, UiSfx).
- DFPlayerController and the first WS2805 driver added; `main.cpp` set up for the LED strip.
- Volume setting added to the settings menu and to EEPROM storage.
- Wiring diagram (`Discobox_Wiring.puml`) added, architecture diagram updated.

## 2026-09-23

Commit: `6492b30`.
- Main project prepared for the new LED strip.
- Modes, settings, storage, LED controller, buzzer player and serial handler reworked.
- First class diagram (`Discobox_Architecture.puml`).

## 2026-08-21

Commit: `50e44f4`.
- Settings management refactored (`SettingItem` / `SettingsMenu`).
- Power-resistant settings storage in EEPROM (theme, gap, mute, brightness).

## 2026-08-14

Commits: `3e59679`, `61dfd0a`.
- Buzzer completed: songs, UI sound cues, theme packs.

## 2026-07-29

Commits: `3db993e`, `01a583f`.
- Git repository created, project skeleton.
