#ifndef SERIAL_HANDLER_H
#define SERIAL_HANDLER_H
/*
  Serial_Handler.h — control the Discobox over the USB-C cable.

  Type commands into the Serial Monitor (end with Enter / newline).
  Reading is non-blocking: update() only processes bytes that have
  already arrived, so music and animations keep running. Commands are
  case-insensitive.

  HELP
    help          — quick overview + list of topics
    help <topic>  — full detail for one topic (gestures / settings / buzzer / lights / info)
    status        — current mode + song + playing + mute + cue state

  Everything else is grouped under those topics — see `help <topic>`
  in the Serial Monitor for the live, authoritative list (it always
  matches the running firmware). Short version:
    gestures — sp / dp / tp / hold / mode N
    settings — settings -bump/-next/-back/-list (works from any mode)
    buzzer   — songs (buzzer -play/-next/-back/-queue/-list/-stop/-mute) and UI cues (-cue/-options/-theme/-gap)
    lights   — ring / led / btn / bright / disco / fade / wipe / clear

  Note: a running animation (e.g. party disco) repaints the ring every
  frame and will overwrite manual light commands — stop it first (sp).

  Cue theme/gap/mute and brightness are power-resistant — they're saved
  to flash (see Storage/Settings_Store.h) the moment you set them here,
  so they're still set after a power loss, not just until unplugged.

  IMPORTANT: include this in main.cpp AFTER Modes.h, Settings_Store.h,
  and Settings_Items.h — it talks directly to the Modes manager, the
  Party mode, Persist, and SettingsList.
*/

#include <Arduino.h>

/**
 * @brief Non-blocking serial command interpreter for the Discobox.
 *
 * Reads bytes from the USB-C Serial port in update() and processes complete
 * lines as commands. Commands are case-insensitive. Type `help` in the Serial
 * Monitor for a full command reference. A running animation may overwrite
 * manual light commands — stop it first (send `sp`).
 */
class SerialCommander {
public:
  /**
   * @brief Read pending serial bytes and dispatch any complete lines as commands.
   *
   * Call every frame (not just every animation frame — put it in loop() before
   * the frame-rate gate so responses stay snappy). Non-blocking.
   */
  void update() {
    while (Serial.available() > 0) {
      char c = (char)Serial.read();

      if (c == '\r') continue;                 // ignore CR (Windows line ends)
      if (c == '\n') {                         // full line received
        _buf[_len] = '\0';
        if (_len > 0) handle(_buf);
        _len = 0;
        continue;
      }
      if (_len < BUF_SIZE - 1) _buf[_len++] = c;   // collect (drop overflow)
    }
  }

private:
  static const uint8_t BUF_SIZE = 32;
  char    _buf[BUF_SIZE];
  uint8_t _len = 0;

  /** @brief Lowercase a C-string in place so commands are case-insensitive. @param s Null-terminated string to convert. */
  static void toLower(char* s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s += 32;
  }

  /**
   * @brief Parse a color from text — either "255 0 0" (r g b) or "#ff00ff" / "ff00ff" (hex).
   * @param s  Input string (leading spaces are skipped).
   * @param r  Output: red channel (0..255).
   * @param g  Output: green channel (0..255).
   * @param b  Output: blue channel (0..255).
   * @return true on success; false if the string doesn't match either format.
   */
  static bool parseColor(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
    while (*s == ' ') s++;
    if (*s == '\0') return false;

    // hex form
    const char* hexStart = (*s == '#') ? s + 1 : nullptr;
    if (!hexStart && strlen(s) == 6 && strspn(s, "0123456789abcdef") == 6)
      hexStart = s;                       // bare 6-digit hex like ff00ff
    if (hexStart) {
      uint32_t h = (uint32_t)strtoul(hexStart, nullptr, 16);
      r = (h >> 16) & 0xFF;  g = (h >> 8) & 0xFF;  b = h & 0xFF;
      return true;
    }

    // "r g b" form
    int rr, gg, bb;
    if (sscanf(s, "%d %d %d", &rr, &gg, &bb) == 3) {
      r = constrain(rr, 0, 255);
      g = constrain(gg, 0, 255);
      b = constrain(bb, 0, 255);
      return true;
    }
    return false;
  }

  /**
   * @brief Dispatch a single lowercase, null-terminated command line.
   * @param line The command string to handle (modified in place by toLower).
   */
  void handle(char* line) {
    toLower(line);

    // --- gestures (same functions the physical button uses, so you get
    //     the same sound cue as a real press) ---
    if      (strcmp(line, "sp") == 0)   { Logger::log("[serial] SP");   Btn_SP(); }
    else if (strcmp(line, "dp") == 0)   { Logger::log("[serial] DP");   Btn_DP(); }
    else if (strcmp(line, "tp") == 0)   { Logger::log("[serial] TP");   Btn_TP(); }
    else if (strcmp(line, "hold") == 0 ||
             strcmp(line, "next") == 0) { Logger::log("[serial] HOLD"); Btn_Hold(); }

    // --- direct controls ---
    else if (strncmp(line, "mode ", 5) == 0) {
      int n = atoi(line + 5);
      if (n >= 1 && n <= (int)Modes.count()) Modes.setMode(n - 1);
      else Logger::warn("mode must be 1..%d", Modes.count());
    }

    // --- settings menu (brightness/mute/theme/...) — works from any mode,
    // same list Settings mode's SP/DP/TP use (see Settings_Items.h) ---
    else if (strncmp(line, "settings", 8) == 0) {
      const char* a = line + 8;
      while (*a == ' ') a++;

      if (*a == '\0') {
        Logger::log("settings: %s selected (%d/%d)  |  -bump -next -back -list",
                    SettingsList.current().name(), SettingsList.index() + 1, SettingsList.count());
        Logger::log("type 'help settings' for what each flag does");
      }
      else if (strncmp(a, "-bump", 5) == 0) {
        SettingsList.bump();                                       // logs + saves itself
        Buzzer.playCue(SettingsList.current().cueForBump());
      }
      else if (strncmp(a, "-next", 5) == 0) {
        SettingsList.next();
        Buzzer.playCue(UISFX_HOVER);
      }
      else if (strncmp(a, "-back", 5) == 0) {
        SettingsList.previous();
        Buzzer.playCue(UISFX_HOVER);
      }
      else if (strncmp(a, "-list", 5) == 0) {
        for (uint8_t i = 0; i < SettingsList.count(); i++)
          Logger::log("  %d: %s%s", i, SettingsList.at(i).name(),
                      (i == SettingsList.index()) ? "  <- selected" : "");
      }
      else Logger::warn("unknown settings flag '%s' — type 'settings'", a);
    }

    // --- buzzer (music), flag style: buzzer -p [N] / -n / -c N / -l / -s ---
    else if (strncmp(line, "buzzer", 6) == 0) {
      const char* a = line + 6;
      while (*a == ' ') a++;                      // skip spaces after "buzzer"

      if (*a == '\0') {
        Logger::log("songs: -play [N] -next -back -queue N -list -stop -mute  (N is 0-based, see -list)  |  cues: -cue <name> -options -theme <pack> -gap");
        Logger::log("type 'help buzzer' for what each flag does");
      }
      else if (strncmp(a, "-play", 5) == 0) {
        // list is 0-based (see -list) — atoi() alone can't tell "no number"
        // from "0", so check for an actual digit before trusting it.
        const char* rest = a + 5;
        while (*rest == ' ') rest++;
        if (*rest) Buzzer.play((uint8_t)atoi(rest));   // e.g. -play 0
        else        Buzzer.play();                      // no number -> current song
      }
      else if (strncmp(a, "-next", 5) == 0) Buzzer.next();
      else if (strncmp(a, "-back", 5) == 0) Buzzer.previous();
      else if (strncmp(a, "-queue", 6) == 0) {
        const char* rest = a + 6;
        while (*rest == ' ') rest++;
        if (*rest) Buzzer.select((uint8_t)atoi(rest));
        else Logger::warn("usage: buzzer -queue 1   (0-based index, see -list)");
      }
      else if (strncmp(a, "-list", 5) == 0) {
        for (uint8_t i = 0; i < Buzzer.songCount(); i++)
          Logger::log("  %d: %s%s", i, SONGLIST[i].name,
                      (i == Buzzer.songIndex()) ? "  <- current" : "");
      }
      else if (strncmp(a, "-stop", 5) == 0) {
        Buzzer.stop();
        Logger::log("[serial] music stopped");
      }
      else if (strncmp(a, "-mute", 5) == 0) { Buzzer.toggleMute(); Persist.save(); }
      else if (strncmp(a, "-cue", 4) == 0) {
        const char* name = a + 4;
        while (*name == ' ') name++;
        if (*name) Buzzer.playCue(name);
        else Logger::warn("usage: buzzer -cue press   (try 'buzzer -cues' for names)");
      }
      else if (strncmp(a, "-cues", 5) == 0) {
        // print cue names in short rows so they fit the log line width
        char row[100]; uint8_t rowLen = 0;
        for (uint8_t i = 0; i < UISFX_CUE_COUNT; i++) {
          uint8_t nameLen = strlen(UISFX_CUES[i].name);
          if ((size_t)(rowLen + nameLen + 2) >= sizeof(row)) {
            row[rowLen] = '\0';
            Logger::log("  %s", row);
            rowLen = 0;
          }
          if (rowLen > 0) { row[rowLen++] = ' '; }
          memcpy(row + rowLen, UISFX_CUES[i].name, nameLen);
          rowLen += nameLen;
        }
        if (rowLen > 0) { row[rowLen] = '\0'; Logger::log("  %s", row); }
      }
      else if (strncmp(a, "-theme", 6) == 0) {
        const char* name = a + 6;
        while (*name == ' ') name++;
        if (*name) { Buzzer.setCuePack(name); Persist.save(); }
        else {
          Logger::warn("usage: buzzer -theme arcade   (packs below)");
          for (uint8_t i = 0; i < UISFX_PACK_COUNT; i++)
            Logger::log("  %s", UISFX_PACKS[i].name);
        }
      }
      else if (strncmp(a, "-gap", 4) == 0) {
        const char* rest = a + 4;
        while (*rest == ' ') rest++;
        int before, after;
        if (*rest == '\0') {
          Logger::log("cue gap: %dms before, %dms after", Buzzer.cueGapBefore(), Buzzer.cueGapAfter());
        } else if (sscanf(rest, "%d %d", &before, &after) == 2 && before >= 0 && after >= 0) {
          Buzzer.setCueGap((uint16_t)before, (uint16_t)after);
          Persist.save();
          Logger::log("[serial] cue gap -> %dms before, %dms after", before, after);
        } else Logger::warn("usage: buzzer -g 30 80");
      }
      else Logger::warn("unknown buzzer flag '%s' — type 'buzzer'", a);
    }

    // --- audio (DFPlayer Mini MP3) ---
    else if (strncmp(line, "audio", 5) == 0) {
      const char* a = line + 5;
      while (*a == ' ') a++;

      if (*a == '\0') {
        Logger::log("audio: %s | folder %d track %d | vol %d%s",
                    !DFPlayer.isOK()      ? "not found" :
                    DFPlayer.isPlaying()  ? "playing"   : "ready",
                    DFPlayer.currentFolder(), DFPlayer.currentTrack(),
                    DFPlayer.volume(),
                    DFPlayer.isOK() ? "" : " (no module)");
        Logger::log("flags: -play F T | -loop F | -pause | -resume | -next | -prev | -stop | -vol N | -vol+ | -vol-");
      }
      else if (strncmp(a, "-play", 5) == 0) {
        const char* rest = a + 5;
        while (*rest == ' ') rest++;
        int f = 0, t = 1;
        if (sscanf(rest, "%d %d", &f, &t) >= 1 && f >= 1) {
          DFPlayer.playFolder((uint8_t)f, (uint8_t)t);
          Logger::log("[serial] audio play folder %d track %d", f, t);
        } else Logger::warn("usage: audio -play <folder> [track]  e.g. audio -play 1 3");
      }
      else if (strncmp(a, "-loop", 5) == 0) {
        const char* rest = a + 5;
        while (*rest == ' ') rest++;
        int f = atoi(rest);
        if (f >= 1) {
          DFPlayer.playFolderLoop((uint8_t)f);
          Logger::log("[serial] audio loop folder %d", f);
        } else Logger::warn("usage: audio -loop <folder>  e.g. audio -loop 1");
      }
      else if (strncmp(a, "-pause",  6) == 0) { DFPlayer.pause();   Logger::log("[serial] audio paused");  }
      else if (strncmp(a, "-resume", 7) == 0) { DFPlayer.resume();  Logger::log("[serial] audio resumed"); }
      else if (strncmp(a, "-next",   5) == 0) { DFPlayer.next();    Logger::log("[serial] audio next");    }
      else if (strncmp(a, "-prev",   5) == 0) { DFPlayer.previous(); Logger::log("[serial] audio prev");   }
      else if (strncmp(a, "-stop",   5) == 0) { DFPlayer.pause();   Logger::log("[serial] audio stopped (paused)"); }
      else if (strncmp(a, "-vol+",   5) == 0) { DFPlayer.volumeUp();   Logger::log("[serial] audio vol -> %d", DFPlayer.volume()); }
      else if (strncmp(a, "-vol-",   5) == 0) { DFPlayer.volumeDown(); Logger::log("[serial] audio vol -> %d", DFPlayer.volume()); }
      else if (strncmp(a, "-vol",    4) == 0) {
        const char* rest = a + 4;
        while (*rest == ' ') rest++;
        int v = atoi(rest);
        if (*rest && v >= 0 && v <= 30) {
          DFPlayer.setVolume((uint8_t)v);
          Persist.save();
          Logger::log("[serial] audio vol -> %d (saved)", v);
        } else Logger::warn("usage: audio -vol 0..30");
      }
      else Logger::warn("unknown audio flag '%s' — type 'audio'", a);
    }

    // --- lights ---
    else if (strncmp(line, "ring ", 5) == 0) {
      uint8_t r, g, b;
      if (parseColor(line + 5, r, g, b)) {
        RingOFLeds.setToColor(r, g, b);
        RingOFLeds.show();
        Logger::log("[serial] ring -> %d %d %d", r, g, b);
      } else Logger::warn("usage: ring 255 0 0  or  ring #ff0000");
    }
    else if (strncmp(line, "led ", 4) == 0) {
      int n = atoi(line + 4);                 // LED number first (1..16)
      const char* rest = strchr(line + 4, ' ');  // then the color
      uint8_t r, g, b;
      if (n >= 1 && n <= 16 && rest && parseColor(rest + 1, r, g, b)) {
        RingOFLeds.setToColorSingle((uint8_t)(n - 1), r, g, b);
        RingOFLeds.show();
        Logger::log("[serial] led %d -> %d %d %d", n, r, g, b);
      } else Logger::warn("usage: led 3 255 0 0  or  led 3 #ff0000  (led 1..16)");
    }
    else if (strncmp(line, "btn ", 4) == 0) {
      uint8_t r, g, b;
      if (parseColor(line + 4, r, g, b)) {
        RGBButton.setToColor(r, g, b);
        Logger::log("[serial] btn -> %d %d %d", r, g, b);
      } else Logger::warn("usage: btn 0 0 255  or  btn #0000ff");
    }
    else if (strncmp(line, "brightness", 7) == 0) {
      int v = atoi(line + 7);
      if (v >= 0 && v <= 255) {
        RingOFLeds.setBrightness((uint8_t)v);
        RingOFLeds.show();
        Persist.save();
        Logger::log("[serial] brightness -> %d", v);
      } else Logger::warn("bright must be 0..255");
    }
    else if (strcmp(line, "disco") == 0) {
      RingOFLeds.stepDisco();
      RingOFLeds.show();
      RGBButton.stepDisco();
    }
    else if (strncmp(line, "fade ", 5) == 0) {
      uint8_t r, g, b;
      if (parseColor(line + 5, r, g, b)) {
        Logger::log("[serial] fading ring...");
        RingOFLeds.fadeToColor(r, g, b);       // note: blocks ~0.6s
      } else Logger::warn("usage: fade 255 136 0  or  fade #ff8800");
    }
    else if (strncmp(line, "wipe ", 5) == 0) {
      uint8_t r, g, b;
      if (parseColor(line + 5, r, g, b)) {
        Logger::log("[serial] wiping ring...");
        RingOFLeds.wipeToColor(r, g, b);           // note: blocks ~4s
      } else Logger::warn("usage: wipe 0 255 255  or  wipe #00ffff");
    }
    else if (strcmp(line, "clear") == 0) {
      RingOFLeds.clear();
      RingOFLeds.show();
      RGBButton.clear();
      Strip.clear(); Strip.show();
      Logger::log("[serial] lights cleared");
    }

    // --- strip (WS2805 RGBCCT) ---
    else if (strncmp(line, "strip ", 6) == 0) {
      const char* a = line + 6;
      while (*a == ' ') a++;
      if (strcmp(a, "clear") == 0) {
        Strip.clear(); Strip.show();
        Logger::log("[serial] strip cleared");
      }
      else if (strncmp(a, "ww", 2) == 0 && (a[2] == '\0' || a[2] == ' ')) {
        // strip ww [0..255] — warm white channel (default 255)
        int v = (a[2] == ' ') ? atoi(a + 3) : 255;
        if (v < 0) v = 0; if (v > 255) v = 255;
        Strip.setAllWW((uint8_t)v);
        Logger::log("[serial] strip warm white %d", v);
      }
      else if (strncmp(a, "cw", 2) == 0 && (a[2] == '\0' || a[2] == ' ')) {
        // strip cw [0..255] — cool white channel (default 255)
        int v = (a[2] == ' ') ? atoi(a + 3) : 255;
        if (v < 0) v = 0; if (v > 255) v = 255;
        Strip.setAllCW((uint8_t)v);
        Logger::log("[serial] strip cool white %d", v);
      }
      else if (strncmp(a, "cct ", 4) == 0) {
        // strip cct R G B CW WW — set all 5 channels at once (each 0..255)
        int r, g, b, cw, ww;
        if (sscanf(a + 4, "%d %d %d %d %d", &r, &g, &b, &cw, &ww) == 5
            && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255
            && cw >= 0 && cw <= 255 && ww >= 0 && ww <= 255) {
          Strip.setToColorCCT((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)cw, (uint8_t)ww);
          Logger::log("[serial] strip cct R%d G%d B%d CW%d WW%d", r, g, b, cw, ww);
        } else Logger::warn("usage: strip cct 255 0 0 0 128   (R G B CW WW, each 0..255)");
      }
      else if (strcmp(a, "disco") == 0) {
        Strip.showDisco(); Logger::log("[serial] strip disco");
      }
      else if (strcmp(a, "bench") == 0) {
        // strip bench — time how long the strip code really takes (microseconds)
        unsigned long t0 = micros();
        Strip.stepDisco();                       // generate random colours for all ICs
        unsigned long t1 = micros();
        Strip.show();                            // encode + send over SPI
        unsigned long t2 = micros();
        Logger::log("[bench] stepDisco %lu us | show (encode+SPI) %lu us | total %lu us",
                    t1 - t0, t2 - t1, t2 - t0);
      }
      else if (strncmp(a, "bright", 6) == 0) {
        // strip bright N — strip brightness 0..255 (scales all channels; not saved)
        int v = atoi(a + 6);
        if (a[6] == ' ' && v >= 0 && v <= 255) {
          Strip.setBrightness((uint8_t)v);
          Logger::log("[serial] strip brightness -> %d (applies to the NEXT color command)", v);
        } else Logger::log("strip brightness = %d  (usage: strip bright 0..255)", Strip.getBrightness());
      }
      else if (strncmp(a, "hsv ", 4) == 0) {
        // strip hsv H S V — hue 0..255, saturation 0..255, value 0..255
        int h, sa, va;
        if (sscanf(a + 4, "%d %d %d", &h, &sa, &va) == 3
            && h >= 0 && h <= 255 && sa >= 0 && sa <= 255 && va >= 0 && va <= 255) {
          Strip.setToColorHSV((uint8_t)h, (uint8_t)sa, (uint8_t)va);
          Strip.show();
          Logger::log("[serial] strip hsv %d %d %d", h, sa, va);
        } else Logger::warn("usage: strip hsv 160 255 255   (hue sat val, each 0..255)");
      }
      else if (strncmp(a, "fade ", 5) == 0) {
        uint8_t r, g, b;
        if (parseColor(a + 5, r, g, b)) {
          Logger::log("[serial] fading strip...");
          Strip.fadeToColor(r, g, b);                 // blocks ~0.6 s
        } else Logger::warn("usage: strip fade 255 136 0  or  strip fade #ff8800");
      }
      else if (strncmp(a, "wipe ", 5) == 0) {
        uint8_t r, g, b;
        if (parseColor(a + 5, r, g, b)) {
          Logger::log("[serial] wiping strip... (blocks several seconds)");
          Strip.wipeToColor(r, g, b);                 // one IC after the other
        } else Logger::warn("usage: strip wipe 0 255 255  or  strip wipe #00ffff");
      }
      else if (strncmp(a, "led ", 4) == 0) {
        // strip led N <color> — set ONE IC (0-based), keep the others as they are
        int n = atoi(a + 4);
        const char* rest = strchr(a + 4, ' ');
        uint8_t r, g, b;
        if (n >= 0 && n < WS2805_N_LEDS && rest && parseColor(rest + 1, r, g, b)) {
          Strip.setToColorSingle((uint8_t)n, r, g, b);
          Strip.show();
          Logger::log("[serial] strip led %d -> %d %d %d", n, r, g, b);
        } else Logger::warn("usage: strip led 5 255 0 0   (IC 0..%d, others unchanged)", WS2805_N_LEDS - 1);
      }
      else if (strncmp(a, "fadeic ", 7) == 0) {
        // strip fadeic N <color> — fade ONE IC to a colour
        int n = atoi(a + 7);
        const char* rest = strchr(a + 7, ' ');
        uint8_t r, g, b;
        if (n >= 0 && n < WS2805_N_LEDS && rest && parseColor(rest + 1, r, g, b)) {
          Strip.fadeToColorSingle((uint8_t)n, r, g, b);   // blocks ~0.6 s
          Logger::log("[serial] strip IC %d faded -> %d %d %d", n, r, g, b);
        } else Logger::warn("usage: strip fadeic 5 255 0 0   (IC 0..%d)", WS2805_N_LEDS - 1);
      }
      else if (strncmp(a, "flicker", 7) == 0) {
        // strip flicker [seconds] — warm-white candle effect (blocks, default 4 s, max 30)
        int sec = (a[7] == ' ') ? atoi(a + 8) : 4;
        if (sec < 1) sec = 1; if (sec > 30) sec = 30;
        Logger::log("[serial] strip candle flicker for %d s...", sec);
        Strip.flickerBegin();
        unsigned long t0 = millis();
        while (millis() - t0 < (unsigned long)sec * 1000UL) { Strip.flickerUpdate(); delay(40); }
        Strip.clear(); Strip.show();
      }
      else if (strncmp(a, "ic ", 3) == 0) {
        // strip ic N <color> — light only IC N (0-based), everything else off
        int n = atoi(a + 3);
        const char* rest = strchr(a + 3, ' ');
        uint8_t r, g, b;
        if (n >= 0 && n < WS2805_N_LEDS && rest && parseColor(rest + 1, r, g, b)) {
          Strip.clear();
          Strip.setToColorSingle((uint8_t)n, r, g, b);
          Strip.show();
          Logger::log("[serial] strip IC %d -> %d %d %d", n, r, g, b);
        } else Logger::warn("usage: strip ic 0 255 0 0   (IC 0..%d)", WS2805_N_LEDS - 1);
      }
      else if (strcmp(a, "walk") == 0) {
        // light ICs one at a time (red) so you can see where it breaks. Blocks ~0.15 s per IC.
        Logger::log("[serial] strip walk...");
        for (uint8_t i = 0; i < WS2805_N_LEDS; i++) {
          Strip.clear();
          Strip.setToColorSingle(i, 255, 0, 0);
          Strip.show();
          Logger::log("IC %d", i);
          delay(150);
        }
        Strip.clear(); Strip.show();
      }
      else {
        uint8_t r, g, b;
        if (parseColor(a, r, g, b)) {
          Strip.setToColor(r, g, b);
          Logger::log("[serial] strip -> %d %d %d", r, g, b);
        } else Logger::warn("unknown strip command — type 'help lights' for the list");
      }
    }

    // --- info ---
    else if (strcmp(line, "status") == 0) {
      Logger::log("mode: %s (%d/%d)", Modes.current().name(),
                  Modes.index() + 1, Modes.count());
      Logger::log("setting: %s (%d/%d)", SettingsList.current().name(),
                  SettingsList.index() + 1, SettingsList.count());
      Logger::log("song: %s | party %s | music %s | cues %s | pack %s | gap %d/%dms",
                  Buzzer.songName(),
                  modeParty.isRunning() ? "running" : "off",
                  Buzzer.isPlaying()    ? "playing" : "silent",
                  Buzzer.isMuted()      ? "muted"   : "on",
                  Buzzer.cuePackName(),
                  Buzzer.cueGapBefore(), Buzzer.cueGapAfter());
      Logger::log("audio: %s | folder %d track %d | vol %d",
                  !DFPlayer.isOK()     ? "not found" :
                  DFPlayer.isPlaying() ? "playing"   : "ready",
                  DFPlayer.currentFolder(), DFPlayer.currentTrack(),
                  DFPlayer.volume());
    }
    else if (strcmp(line, "help") == 0) {
      Logger::log("Discobox serial console. Topics: gestures | settings | buzzer | audio | lights");
      Logger::log("  help <topic>  - what each command in that topic does");
      Logger::log("  status        - current mode, setting, song, mute, cue-gap and audio state");
    }
    else if (strncmp(line, "help ", 5) == 0) {
      const char* topic = line + 5;
      while (*topic == ' ') topic++;

      if (strcmp(topic, "gestures") == 0) {
        Logger::log("sp / dp / tp   - simulate a single/double/triple button press");
        Logger::log("                 (does whatever the CURRENT mode assigned it)");
        Logger::log("hold           - simulate the 3-second hold: switch to the next mode");
        Logger::log("next           - same as 'hold'");
        Logger::log("mode N         - jump straight to mode N (1..4), skipping the cycle");
      }
      else if (strcmp(topic, "settings") == 0) {
        Logger::log("The same list Settings mode's SP/DP/TP use (see Settings_Items.h) —");
        Logger::log("works from any mode, doesn't require switching to Settings first.");
        Logger::log("-bump        bump the SELECTED setting's value up (wraps at its max)");
        Logger::log("-next        select the next setting in the list");
        Logger::log("-back        select the previous setting in the list");
        Logger::log("-list        list every setting, marking which one is selected");
      }
      else if (strcmp(topic, "buzzer") == 0) {
        Logger::log("-- songs --");
        Logger::log("-play        play the currently selected song from the start");
        Logger::log("-play 1      play song index 1 right now, regardless of what was selected");
        Logger::log("-next        skip to the next song in the list");
        Logger::log("-back        go back to the previous song");
        Logger::log("-queue 1     select song index 1 without playing it (unless music is already on)");
        Logger::log("-list        list every song with its index (0-based), marking which one is current");
        Logger::log("-stop        stop the music completely");
        Logger::log("-mute        mute/unmute short cue sounds only (songs keep playing)");
        Logger::log("-- UI cues: short feedback blips like 'press' or 'success' --");
        Logger::log("-cue press   play one cue by name; briefly pauses a song, then resumes it");
        Logger::log("-cues        list every cue name you can pass to -cue");
        Logger::log("-theme name  switch the cue's pitch/speed theme; -theme alone lists themes");
        Logger::log("-gap 30 80   silence 30ms before a cue and 80ms after (default 20/60ms)");
        Logger::log("             -gap alone shows the current gap");
      }
      else if (strcmp(topic, "lights") == 0) {
        Logger::log("ring <color>    set every LED on the ring to one color");
        Logger::log("led N <color>   set a single ring LED (N = 1..16) to a color");
        Logger::log("btn <color>     set the button's own LED to a color");
        Logger::log("bright N        set ring brightness, 0 (off) to 255 (max)");
        Logger::log("disco           one random-color step on the ring + button");
        Logger::log("fade <color>    smoothly fade the ring to a color (~0.6s)");
        Logger::log("wipe <color>    fill the ring one LED at a time (~4s)");
        Logger::log("clear           turn every light off (ring, button and strip)");
        Logger::log("strip <color>   whole WS2805 strip to an RGB color");
        Logger::log("strip ww [0-255]   warm white channel (default 255)");
        Logger::log("strip cw [0-255]   cool white channel (default 255)");
        Logger::log("strip cct R G B CW WW   set all 5 channels (each 0..255)");
        Logger::log("strip hsv H S V         hue/sat/value, each 0..255");
        Logger::log("strip bright [0-255]    strip brightness (no number = show current)");
        Logger::log("strip fade <color>      smooth fade of the whole strip (~0.6s)");
        Logger::log("strip wipe <color>      fill IC by IC (blocks several seconds)");
        Logger::log("strip led N <color>     set ONE IC (0-based), others unchanged");
        Logger::log("strip fadeic N <color>  fade ONE IC to a color");
        Logger::log("strip ic N <color>      only IC N lit, rest off — debug");
        Logger::log("strip flicker [sec]     warm-white candle effect (default 4s, max 30)");
        Logger::log("strip walk              light ICs one by one (debug)");
        Logger::log("strip bench             time one disco frame: generate + send (us)");
        Logger::log("strip disco | strip clear");
        Logger::log("note: 1 IC = 3 LEDs, so N is 0..59 on the 3 m strip");
        Logger::log("<color> is 'r g b' (each 0..255) or hex like #ff00ff");
        Logger::log("note: a running animation (e.g. party disco) repaints the ring");
        Logger::log("every frame and will overwrite these — stop it first (sp)");
      }
      else if (strcmp(topic, "audio") == 0) {
        Logger::log("Control the DFPlayer Mini MP3 module:");
        Logger::log("-play F T    play folder F, track T  (e.g. audio -play 1 3)");
        Logger::log("-loop F      loop all tracks in folder F continuously");
        Logger::log("-pause       pause playback");
        Logger::log("-resume      resume from pause");
        Logger::log("-next        skip to the next track");
        Logger::log("-prev        go back to the previous track");
        Logger::log("-stop        pause playback (DFPlayer has no hard stop)");
        Logger::log("-vol N       set volume 0..30 and save to EEPROM  (e.g. audio -vol 20)");
        Logger::log("-vol+        volume up one step");
        Logger::log("-vol-        volume down one step");
        Logger::log("audio        (no flag) show current status: state / folder / track / vol");
      }
      else {
        Logger::warn("no such topic '%s' - try: gestures, settings, buzzer, audio, lights", topic);
      }
    }
    else {
      Logger::warn("unknown command '%s' - type 'help'", line);
    }
  }
};

#endif // SERIAL_HANDLER_H
