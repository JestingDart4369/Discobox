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
    help <topic>  — full detail for one topic (gestures / buzzer / lights / info)
    status        — current mode + song + playing + mute + cue state

  Everything else is grouped under those topics — see `help <topic>`
  in the Serial Monitor for the live, authoritative list (it always
  matches the running firmware). Short version:
    gestures — sp / dp / tp / hold / mode N
    buzzer   — songs (buzzer -p/-n/-b/-c/-l/-s/-m) and UI cues (-q/-o/-y/-g)
    lights   — ring / led / btn / bright / disco / fade / wipe / clear

  Note: a running animation (e.g. party disco) repaints the ring every
  frame and will overwrite manual light commands — stop it first (sp).

  IMPORTANT: include this in main.cpp AFTER Modes.h — it talks
  directly to the Modes manager and the Party mode.
*/

#include <Arduino.h>

class SerialCommander {
public:
  // Call every frame — reads whatever bytes arrived, handles full lines.
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

  // lowercase a line in place, so commands are case-insensitive
  static void toLower(char* s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s += 32;
  }

  // Parse a color from text: either "255 0 0" or hex "#ff00ff" / "ff00ff".
  // Returns true on success and fills r,g,b.
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

    // --- buzzer (music), flag style: buzzer -p [N] / -n / -c N / -l / -s ---
    else if (strncmp(line, "buzzer", 6) == 0) {
      const char* a = line + 6;
      while (*a == ' ') a++;                      // skip spaces after "buzzer"

      if (*a == '\0') {
        Logger::log("songs: -p [N] -n -b -c N -l -s -m  |  cues: -q <name> -o -y <pack> -g");
        Logger::log("type 'help buzzer' for what each flag does");
      }
      else if (strncmp(a, "-p", 2) == 0) {
        int n = atoi(a + 2);                      // 0 if no number given
        if (n >= 1) Buzzer.play((uint8_t)(n - 1));
        else        Buzzer.play();
      }
      else if (strncmp(a, "-n", 2) == 0) Buzzer.next();
      else if (strncmp(a, "-b", 2) == 0) Buzzer.previous();
      else if (strncmp(a, "-c", 2) == 0) {
        int n = atoi(a + 2);
        if (n >= 1) Buzzer.select((uint8_t)(n - 1));
        else Logger::warn("usage: buzzer -c 2");
      }
      else if (strncmp(a, "-l", 2) == 0) {
        for (uint8_t i = 0; i < Buzzer.songCount(); i++)
          Logger::log("  %d: %s%s", i + 1, SONGLIST[i].name,
                      (i == Buzzer.songIndex()) ? "  <- current" : "");
      }
      else if (strncmp(a, "-s", 2) == 0) {
        Buzzer.stop();
        Logger::log("[serial] music stopped");
      }
      else if (strncmp(a, "-m", 2) == 0) Buzzer.toggleMute();
      else if (strncmp(a, "-q", 2) == 0) {
        const char* name = a + 2;
        while (*name == ' ') name++;
        if (*name) Buzzer.playCue(name);
        else Logger::warn("usage: buzzer -q press   (try 'buzzer -o' for names)");
      }
      else if (strncmp(a, "-o", 2) == 0) {
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
      else if (strncmp(a, "-y", 2) == 0) {
        const char* name = a + 2;
        while (*name == ' ') name++;
        if (*name) Buzzer.setCuePack(name);
        else {
          Logger::warn("usage: buzzer -y arcade   (packs below)");
          for (uint8_t i = 0; i < UISFX_PACK_COUNT; i++)
            Logger::log("  %s", UISFX_PACKS[i].name);
        }
      }
      else if (strncmp(a, "-g", 2) == 0) {
        const char* rest = a + 2;
        while (*rest == ' ') rest++;
        int before, after;
        if (*rest == '\0') {
          Logger::log("cue gap: %dms before, %dms after", Buzzer.cueGapBefore(), Buzzer.cueGapAfter());
        } else if (sscanf(rest, "%d %d", &before, &after) == 2 && before >= 0 && after >= 0) {
          Buzzer.setCueGap((uint16_t)before, (uint16_t)after);
          Logger::log("[serial] cue gap -> %dms before, %dms after", before, after);
        } else Logger::warn("usage: buzzer -g 30 80");
      }
      else Logger::warn("unknown buzzer flag '%s' — type 'buzzer'", a);
    }

    // --- lights ---
    else if (strncmp(line, "ring ", 5) == 0) {
      uint8_t r, g, b;
      if (parseColor(line + 5, r, g, b)) {
        RingOFLeds.All_Set(r, g, b);
        RingOFLeds.show();
        Logger::log("[serial] ring -> %d %d %d", r, g, b);
      } else Logger::warn("usage: ring 255 0 0  or  ring #ff0000");
    }
    else if (strncmp(line, "led ", 4) == 0) {
      int n = atoi(line + 4);                 // LED number first (1..16)
      const char* rest = strchr(line + 4, ' ');  // then the color
      uint8_t r, g, b;
      if (n >= 1 && n <= 16 && rest && parseColor(rest + 1, r, g, b)) {
        RingOFLeds.Set_Single((uint8_t)(n - 1), r, g, b);
        RingOFLeds.show();
        Logger::log("[serial] led %d -> %d %d %d", n, r, g, b);
      } else Logger::warn("usage: led 3 255 0 0  or  led 3 #ff0000  (led 1..16)");
    }
    else if (strncmp(line, "btn ", 4) == 0) {
      uint8_t r, g, b;
      if (parseColor(line + 4, r, g, b)) {
        RGBButton.ledSet(r, g, b);
        Logger::log("[serial] btn -> %d %d %d", r, g, b);
      } else Logger::warn("usage: btn 0 0 255  or  btn #0000ff");
    }
    else if (strncmp(line, "bright ", 7) == 0) {
      int v = atoi(line + 7);
      if (v >= 0 && v <= 255) {
        RingOFLeds.setBrightness((uint8_t)v);
        RingOFLeds.show();
        Logger::log("[serial] brightness -> %d", v);
      } else Logger::warn("bright must be 0..255");
    }
    else if (strcmp(line, "disco") == 0) {
      RingOFLeds.Disco_step();
      RingOFLeds.show();
      RGBButton.Disco_step();
    }
    else if (strncmp(line, "fade ", 5) == 0) {
      uint8_t r, g, b;
      if (parseColor(line + 5, r, g, b)) {
        Logger::log("[serial] fading ring...");
        RingOFLeds.All_FadeTo(r, g, b);       // note: blocks ~0.6s
      } else Logger::warn("usage: fade 255 136 0  or  fade #ff8800");
    }
    else if (strncmp(line, "wipe ", 5) == 0) {
      uint8_t r, g, b;
      if (parseColor(line + 5, r, g, b)) {
        Logger::log("[serial] wiping ring...");
        RingOFLeds.WipeTo(r, g, b);           // note: blocks ~4s
      } else Logger::warn("usage: wipe 0 255 255  or  wipe #00ffff");
    }
    else if (strcmp(line, "clear") == 0) {
      RingOFLeds.clear();
      RingOFLeds.show();
      RGBButton.clear();
      Logger::log("[serial] lights cleared");
    }

    // --- info ---
    else if (strcmp(line, "status") == 0) {
      Logger::log("mode: %s (%d/%d)", Modes.current().name(),
                  Modes.index() + 1, Modes.count());
      Logger::log("song: %s | party %s | music %s | cues %s | pack %s | gap %d/%dms",
                  Buzzer.songName(),
                  modeParty.isRunning() ? "running" : "off",
                  Buzzer.isPlaying()    ? "playing" : "silent",
                  Buzzer.isMuted()      ? "muted"   : "on",
                  Buzzer.cuePackName(),
                  Buzzer.cueGapBefore(), Buzzer.cueGapAfter());
    }
    else if (strcmp(line, "help") == 0) {
      Logger::log("Discobox serial console. Topics: gestures | buzzer | lights");
      Logger::log("  help <topic>  - what each command in that topic does");
      Logger::log("  status        - current mode, song, mute and cue-gap state");
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
      else if (strcmp(topic, "buzzer") == 0) {
        Logger::log("-- songs --");
        Logger::log("-p       play the currently selected song from the start");
        Logger::log("-p 2     play song 2 right now, regardless of what was selected");
        Logger::log("-n       skip to the next song in the list");
        Logger::log("-b       go back to the previous song");
        Logger::log("-c 2     select song 2 without playing it (unless music is already on)");
        Logger::log("-l       list every song, marking which one is current");
        Logger::log("-s       stop the music completely");
        Logger::log("-m       mute/unmute short cue sounds only (songs keep playing)");
        Logger::log("-- UI cues: short feedback blips like 'press' or 'success' --");
        Logger::log("-q press play one cue by name; briefly pauses a song, then resumes it");
        Logger::log("-o       list every cue name you can pass to -q");
        Logger::log("-y name  switch the cue's pitch/speed theme; -y alone lists themes");
        Logger::log("-g 30 80 silence 30ms before a cue and 80ms after (default 20/60ms)");
        Logger::log("         -g alone shows the current gap");
      }
      else if (strcmp(topic, "lights") == 0) {
        Logger::log("ring <color>    set every LED on the ring to one color");
        Logger::log("led N <color>   set a single ring LED (N = 1..16) to a color");
        Logger::log("btn <color>     set the button's own LED to a color");
        Logger::log("bright N        set ring brightness, 0 (off) to 255 (max)");
        Logger::log("disco           one random-color step on the ring + button");
        Logger::log("fade <color>    smoothly fade the ring to a color (~0.6s)");
        Logger::log("wipe <color>    fill the ring one LED at a time (~4s)");
        Logger::log("clear           turn every light off");
        Logger::log("<color> is 'r g b' (each 0..255) or hex like #ff00ff");
        Logger::log("note: a running animation (e.g. party disco) repaints the ring");
        Logger::log("every frame and will overwrite these — stop it first (sp)");
      }
      else {
        Logger::warn("no such topic '%s' - try: gestures, buzzer, lights", topic);
      }
    }
    else {
      Logger::warn("unknown command '%s' - type 'help'", line);
    }
  }
};

#endif // SERIAL_HANDLER_H
