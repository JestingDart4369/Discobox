# Discobox

A button-controlled party box for the **Arduino Uno R4 WiFi**: LED ring and strip effects, a piezo buzzer with songs and UI sound cues, and MP3 playback through a DFPlayer Mini. One button, four gestures (single, double, triple press, hold), several modes.

## Features

- **Gesture button** – non-blocking detection of single / double / triple press and hold on one pin
- **Modes** – Party, Settings, LED and Off, managed by a mode manager
- **LEDs** – WS2812 ring (FastLED), WS2805 RGBCCT strip (custom SPI driver), PWM RGB button LED
- **Buzzer** – non-blocking MIDI-style piezo playback, UI sound cues with several theme packs, ducking of songs during cues
- **DFPlayer Mini** – playlist control, auto-next, volume
- **Persistent settings** – brightness, volume, mute and cue theme stored in EEPROM
- **Serial console** – control and inspect everything over USB (`help` lists the commands)
- **Native unit tests** – run on your computer, no hardware needed

## Hardware

| Part | Pin |
|------|-----|
| Button | D2 |
| RGB button LED | D3 / D5 / D6 |
| WS2812 ring (16 LEDs) | D4 |
| WS2805 RGBCCT strip | D7 (SPI MOSI = D11) |
| Piezo speaker | D9 |
| DFPlayer Mini RX | D1 (via 1 kΩ) |
| DFPlayer Mini TX | D0 |

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

## Disclaimer

AI (Claude) was used for the documentation, for code corrections, and for most administrative tasks in this project.
