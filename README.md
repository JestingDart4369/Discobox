# Discobox

A button-controlled party box for the **Arduino Uno R4 WiFi**: LED ring and strip effects, a piezo buzzer with songs and UI sound cues, and MP3 playback through a DFPlayer Mini. One button, four gestures (single, double, triple press, hold), several modes.

## Project status

**Work in progress – not finished.** The project is being built in two phases:

1. **Make it work** (current phase): firmware and electronics on a breadboard / perfboard. The button, LED ring, LED strip, buzzer and DFPlayer already run together; remaining electronics issues are listed in [JOURNAL.md](JOURNAL.md).
2. **Design the enclosure and holder** (next phase): an original enclosure designed around the parts that proved themselves in phase 1, with CAD model, mounting for all parts and the final assembly. This does not exist yet, so there are no 3D files in the repository. The enclosure is planned with a swappable button mount, so the disco-mode button can be exchanged for a different one.

## Features

- **Gesture button** – non-blocking detection of single / double / triple press and hold on one pin
- **Modes** – Party, Settings, LED and Off, managed by a mode manager
- **LEDs** – WS2812 ring (FastLED), WS2805 RGBCCT strip (custom SPI driver built to the datasheet timing, 5 channels incl. warm/cool white), PWM RGB button LED
- **Buzzer** – non-blocking MIDI-style piezo playback, UI sound cues with several theme packs, ducking of songs during cues
- **DFPlayer Mini** – playlist control, auto-next, volume
- **Persistent settings** – brightness, volume, mute and cue theme stored in EEPROM
- **Serial console** – control and inspect everything over USB (`help` lists the commands)
- **Native unit tests** – run on your computer, no hardware needed

## Hardware

| Part | Pin | Notes |
|------|-----|-------|
| Button (switch) | D2 | other contact to GND, internal pull-up (pressed = LOW) |
| RGB button LED | D3 (R) / D5 (G) / D6 (B) | LED cathode to GND, resistors are built into the button |
| WS2812 ring (16 LEDs) | D4 | data via 330 Ω, 5 V supply |
| WS2805 RGBCCT strip (12 V) | D11 (SPI MOSI) | to DAT and BIN, +12 V from the supply, common GND; 60 ICs = 180 LEDs (3 LEDs per IC); a 3.3 V to 5 V level shifter is recommended |
| Piezo speaker | D9 | other pin to GND |
| DFPlayer Mini RX | D1 (Serial1 TX) | via 1 kΩ |
| DFPlayer Mini TX | D0 (Serial1 RX) | direct |

All grounds (12 V supply, 5 V buck converter, Arduino, strip) must be connected together. The full parts list with prices and links is in [BOM.csv](BOM.csv).

The SD card layout for the DFPlayer (folder `01` = misc, `02` = Party mode, ...) is described in [doc/SD_CARD.md](doc/SD_CARD.md).

The wiring and class diagrams are in [Discobox_Wiring.puml](Discobox_Wiring.puml) and [Discobox_Architecture.puml](Discobox_Architecture.puml) (PlantUML; the wiring diagram needs Graphviz).

## Build and upload

The project uses [PlatformIO](https://platformio.org/).

```bash
pio run                # build for the Uno R4 WiFi
pio run -t upload      # build and upload
pio test -e native     # run the unit tests on your computer
```

## Project layout

```
src/main.cpp        entry point, wires everything together
include/            Buzzer player, modes, settings, serial handler
lib/                reusable header-only libraries
                    (GestureButton, Logger, SongPlayer, WS2805,
                     LedController, DFPlayerController, UiSfx)
test/               Unity tests and Arduino / DFPlayer mocks
doc/                notes and sound tooling
```

All libraries are header-only. Include order in `main.cpp` matters and is documented there with comments.

## Songs

Only the example song ([ExampleSong_song.h](include/Buzzer/Songs/ExampleSong_song.h)) is part of the repository. Other song files in `include/Buzzer/Songs/` are git-ignored because the melodies are copyrighted. The player picks up whichever song headers exist locally and skips the rest, so a fresh clone builds without them.

To add your own song, convert a MIDI-style export with [convert_song.py](doc/sound/songs/convert_song.py), or copy the layout of the example file, then include it in [Buzzer_Player.h](include/Buzzer/Buzzer_Player.h) and add a line to `SONGLIST`.

## Sound credits

- [Kenney UI Audio](https://kenney.nl) by Kenney Vleugels, CC0 (`doc/sound/kenney_ui-audio`)
- UI SFX sound packs, CC0 (`doc/sound/uisfx-sounds`)

## License

MIT, see [LICENSE](LICENSE). The bundled sound packs keep their own CC0 licenses (see above).

## AI usage

This project was developed with the help of AI (Claude by Anthropic).

* Documentation: README, code comments and diagrams
* Corrections: reviewing code, fixing bugs and writing the unit tests
* Admin tasks: repository cleanup, licensing, `.gitignore` and git history
