#pragma once
/*
  WS2805Driver.h — WS2805 RGBCCT low-level driver (single-wire, via SPI MOSI).

  Packs RGBCCT pixels (5 bytes: R,G,B,W1,W2, high bit first), bit-encodes them
  and sends them on D11 (MOSI) in one SPI transfer, followed by a >=280 us low
  reset. Does NOT own the pixel buffer — WS2805<N> below does.

  TIMING (Worldsemi WS2805 datasheet V0.3), all met by the encoding below:
    T0H 220-380 ns | T0L 580 ns-1 us | T1H 580 ns-1 us | T1L 580 ns-1 us
    TDATA (bit cycle) >= 1.25 us | RES (reset, low) >= 280 us
  Encoding: SPI at 3.0 MHz (333 ns/bit), 4 SPI bits per data bit:
    data 0 -> 1000  (333 ns high / 1000 ns low)
    data 1 -> 1100  (667 ns high /  667 ns low)
  = 1.33 us per bit, 20 SPI bytes per pixel, reset = 130 zero bytes = 347 us.
  The old 2.4 MHz 3-bit encoding (0=100, 1=110) violated T0H (417 ns > 380 ns) and
  T1L (417 ns < 580 ns).
  INPUT LEVEL: datasheet VIH >= 0.7*VDD (~3.5 V at VDD 5 V) -> the R4's 3.3 V output
  is below spec; use a 74AHCT125 / 74HCT125 level shifter (powered from 5 V).

  ══ IMPORTANT: ONE IC DRIVES SEVERAL LEDs ══
  The template/pixel count is the number of ICs (controller chips), NOT the number
  of visible LEDs. On the BTF 12V WS2805 strip (60 LED/m) there are 20 ICs per
  metre, so each IC drives 3 LEDs, and those 3 LEDs always share one colour:
      3 m strip = 180 LEDs = 60 ICs  ->  use WS2805<60>, WS2805_N_LEDS 60
  (BTF 24V variant: 10 ICs/m = 6 LEDs per IC; 84 LED/m 12V: 28 ICs/m.)
  Pixel index i = IC i = physical LEDs 3i .. 3i+2. Passing the LED count (180)
  sends data for ICs that don't exist and the strip shows garbage / wrong colours.

  Verdrahtung (12V strip):
    D11 -> DAT (gruen) UND BIN (blau), ggf. ueber 74AHCT125 Pegelwandler + ~330 Ohm
    +12V rot, GND weiss (gemeinsame Masse mit Arduino)
*/

#include <Arduino.h>
#include <SPI.h>

// Number of ICs (pixels). Set in main.cpp BEFORE including this file.
#ifndef WS2805_N_LEDS
#define WS2805_N_LEDS 60
#endif

// Index shift, in pixels. Fixes an off-by-one between the buffer index and the
// physical IC:
//   0  = none (default)
//  +1  = send one blank pixel BEFORE pixel 0 (everything moves one IC further)
//  -1  = drop pixel 0 from the stream (everything moves one IC closer)
// Try +1 or -1 if the strip looks shifted by one IC (3 LEDs).
#ifndef WS2805_INDEX_SHIFT
#define WS2805_INDEX_SHIFT 0
#endif

// SPI transfer size in bytes. 0 = send the whole frame in one SPI.transfer().
// 20 = one pixel per call (NOT recommended: gaps between calls can end the frame).
#ifndef WS2805_CHUNK_BYTES
#define WS2805_CHUNK_BYTES 0
#endif

// SPI clock in Hz. 3.0 MHz -> 333 ns per SPI bit -> 4 bits per data bit = 1.33 us
// (datasheet: bit cycle >= 1.25 us). The RA4M1 picks the nearest divider itself.
#ifndef WS2805_SPI_HZ
#define WS2805_SPI_HZ 3000000
#endif

//  Pixel type

/**
 * @brief One WS2805 RGBCCT pixel — five 8-bit channels.
 *
 * Default-constructed to all zeros (off).
 * Wire order: R, G, B, WW, CW. Swap ww/cw in the driver if colours look wrong.
 */
struct RGBCCT {
  uint8_t r, g, b, cw, ww;
  /// @brief Construct a pixel. All channels default to 0 (off).
  RGBCCT(uint8_t r=0, uint8_t g=0, uint8_t b=0, uint8_t cw=0, uint8_t ww=0)
    : r(r), g(g), b(b), cw(cw), ww(ww) {}
};

// Driver class

/**
 * @brief Low-level WS2805 driver: bit-encodes the pixel buffer and sends it over SPI MOSI.
 */
class WS2805Driver {
public:
  /** @brief Initialise the SPI bus. Call once in setup() before the first show(). */
  void begin() {
    SPI.begin();
#ifdef LOGGER_H
    Logger::log("WS2805: SPI init OK (%lu Hz, MOSI=D11, %d pixels)", (unsigned long)WS2805_SPI_HZ, WS2805_N_LEDS);
#endif
  }

  /**
   * @brief Encode and send a pixel buffer to the strip.
   * @param pixels Pointer to the RGBCCT pixel array.
   * @param count  Number of pixels to send (must match the physical IC count).
   */
  void show(const RGBCCT* pixels, uint16_t count) {
    static uint8_t _buf[(WS2805_N_LEDS + LEAD) * PIXEL_BYTES + RESET_BYTES];

    uint16_t pos = 0;
    // WS2805_INDEX_SHIFT > 0: blank pixels in front
    for (uint8_t l = 0; l < LEAD; l++) {
      for (uint8_t c = 0; c < 5; c++) encodeChannel(0, &_buf[pos + c * 4]);
      pos += PIXEL_BYTES;
    }
    // WS2805_INDEX_SHIFT < 0: skip the first pixels
    uint16_t start = (WS2805_INDEX_SHIFT < 0) ? (uint16_t)(-WS2805_INDEX_SHIFT) : 0;
    for (uint16_t i = start; i < count; i++) {
      // Wire order: R, G, B, W1 (WW), W2 (CW) — high bit first (datasheet)
      uint8_t ch[5] = { pixels[i].r, pixels[i].g, pixels[i].b, pixels[i].ww, pixels[i].cw };
      for (uint8_t c = 0; c < 5; c++) encodeChannel(ch[c], &_buf[pos + c * 4]);
      pos += PIXEL_BYTES;
    }
    // Reset: datasheet RES >= 280 us low. 130 zero bytes = 130*8/3MHz = 347 us.
    memset(&_buf[pos], 0, RESET_BYTES);
    pos += RESET_BYTES;

    SPI.beginTransaction(SPISettings(WS2805_SPI_HZ, MSBFIRST, SPI_MODE0));
    if (WS2805_CHUNK_BYTES > 0) {
      for (uint16_t off = 0; off < pos; off += WS2805_CHUNK_BYTES) {
        uint16_t n = pos - off;
        if (n > WS2805_CHUNK_BYTES) n = WS2805_CHUNK_BYTES;
        SPI.transfer(&_buf[off], n);
      }
    } else {
      SPI.transfer(_buf, pos);
    }
    SPI.endTransaction();
  }

private:
  static const uint8_t  LEAD        = (WS2805_INDEX_SHIFT > 0) ? WS2805_INDEX_SHIFT : 0;
  static const uint8_t  PIXEL_BYTES = 20;    // 5 channels x 4 SPI bytes
  static const uint16_t RESET_BYTES = 130;   // 347 us at 3 MHz (datasheet: >= 280 us)

  /** @brief Encode one 8-bit channel (MSB first) into 4 SPI bytes: 1 -> 0b1100, 0 -> 0b1000. */
  static void encodeChannel(uint8_t v, uint8_t out[4]) {
    for (uint8_t i = 0; i < 4; i++) {
      uint8_t hi = (v >> (7 - 2 * i)) & 1;
      uint8_t lo = (v >> (6 - 2 * i)) & 1;
      out[i] = (uint8_t)(((hi ? 0xC : 0x8) << 4) | (lo ? 0xC : 0x8));
    }
  }
};

// ── WS2805<N> — LEDBase + ILedStrip integration ──────────────────────────────
//
// Compiled only when LedController.h has been #include-d before this file.
// The include guard LED_CONTROLLER_H (set by LedController.h) acts as the gate.
//
// Typical Discobox usage:
//   #include <LedController.h>   // ← defines LED_CONTROLLER_H, LEDBase, ILedStrip
//   #include <WS2805Driver.h>    // ← now also compiles WS2805<N>
//
//   WS2805<180> myStrip;
//   myStrip.setup();
//   myStrip.setToColor(255, 0, 0);  myStrip.show();
//   myStrip.setToColorCCT(0, 0, 0, 0, 200);  // warm white via WW channel
//
// In projects without LedController.h (e.g. Temp), this entire block is skipped.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef LED_CONTROLLER_H

/**
 * @brief WS2805 RGBCCT strip — integrates with LEDBase / ILedStrip.
 *
 * Drop-in replacement for an ARGB<> object when the strip is WS2805
 * (5-channel, single-wire via SPI). Implements every pure-virtual from LEDBase and ILedStrip,
 * plus extra CCT / flicker helpers for the warm- and cold-white channels.
 *
 * @tparam Led_Number  Number of WS2805 pixels on the physical strip.
 *
 * Brightness (inherited _brightness from LEDBase) is applied to all channels
 * at every set-call, matching the behaviour of the ARGB<> class.
 *
 * Optional Logger support: if Logger.h was included (anywhere before this
 * file), WS2805 logs setup via Logger::log(); otherwise it stays silent.
 */
template<uint16_t Led_Number>
class WS2805 : public LEDBase, public ILedStrip {
public:

  // ── Init ──────────────────────────────────────────────────

  /** @brief Initialise SPI and send a blank frame. Call once from setup(). */
  void setup() override {
    _driver.begin();
    _rng ^= (micros() | 1);   // seed the fast PRNG
    clear(); show();
#ifdef LOGGER_H
    Logger::log("WS2805<%u>: ready", (unsigned)Led_Number);
#endif
  }

  // ── Core ──────────────────────────────────────────────────

  /** @brief Push the internal pixel buffer to the strip. */
  void show() override { _driver.show(_leds, Led_Number); }

  /** @brief Set all pixels to off in the buffer (no show). */
  void clear() override {
    for (uint16_t i = 0; i < Led_Number; i++) _leds[i] = RGBCCT();
  }

  // ── Set all ───────────────────────────────────────────────

  /** @brief Set all LEDs to an RGB colour (brightness-scaled) and show. */
  void setToColor(uint8_t r, uint8_t g, uint8_t b) override {
    RGBCCT c = _scaled(r, g, b, 0, 0);
    for (uint16_t i = 0; i < Led_Number; i++) _leds[i] = c;
    show();
  }

  /**
   * @brief Set all LEDs to a full 5-channel RGBCCT value and show.
   * Extra method beyond the LEDBase API — use when you want CW/WW.
   */
  void setToColorCCT(uint8_t r, uint8_t g, uint8_t b, uint8_t cw, uint8_t ww) {
    RGBCCT c = _scaled(r, g, b, cw, ww);
    for (uint16_t i = 0; i < Led_Number; i++) _leds[i] = c;
    show();
  }

  /** @brief Set all LEDs to warm-white and show. @param brightness WW channel (0–255). */
  void setAllWW(uint8_t brightness = 255) {
    uint8_t ww = _sc(brightness);
    RGBCCT c(ww / 8, 0, 0, 0, ww);
    for (uint16_t i = 0; i < Led_Number; i++) _leds[i] = c;
    show();
  }

  /** @brief Set all LEDs to cold-white and show. @param brightness CW channel (0–255). */
  void setAllCW(uint8_t brightness = 255) {
    uint8_t cw = _sc(brightness);
    RGBCCT c(0, 0, 0, cw, 0);
    for (uint16_t i = 0; i < Led_Number; i++) _leds[i] = c;
    show();
  }

  // ── Fade all ──────────────────────────────────────────────

  /** @brief Blocking fade from current RGB to target RGB over `steps` frames. CW/WW zeroed. */
  void fadeToColor(uint8_t r, uint8_t g, uint8_t b,
                   uint16_t steps = 30, uint16_t frame_delay_ms = 20) override {
    // Capture start values (already scaled in buffer)
    uint8_t sr[Led_Number], sg[Led_Number], sb[Led_Number];
    for (uint16_t i = 0; i < Led_Number; i++) {
      sr[i] = _leds[i].r; sg[i] = _leds[i].g; sb[i] = _leds[i].b;
    }
    uint8_t tr = _sc(r), tg = _sc(g), tb = _sc(b);
    for (uint16_t s = 1; s <= steps; s++) {
      uint8_t amt = (uint16_t)(s * 255) / steps;
      for (uint16_t i = 0; i < Led_Number; i++) {
        _leds[i].r = sr[i] + ((int16_t)(tr - sr[i]) * amt / 255);
        _leds[i].g = sg[i] + ((int16_t)(tg - sg[i]) * amt / 255);
        _leds[i].b = sb[i] + ((int16_t)(tb - sb[i]) * amt / 255);
        _leds[i].cw = 0; _leds[i].ww = 0;
      }
      show(); delay(frame_delay_ms);
    }
  }

  // ── Single LED ────────────────────────────────────────────

  /** @brief Set a single LED to an RGB colour (buffer only). Out-of-range silently ignored. */
  void setToColorSingle(uint8_t i, uint8_t r, uint8_t g, uint8_t b) override {
    if (i >= Led_Number) return;
    _leds[i] = _scaled(r, g, b, 0, 0);
  }

  /** @brief Blocking fade of a single LED to an RGB colour. CW/WW zeroed. */
  void fadeToColorSingle(uint8_t i, uint8_t r, uint8_t g, uint8_t b,
                         uint16_t steps = 30, uint16_t frame_delay_ms = 20) override {
    if (i >= Led_Number) return;
    uint8_t sr = _leds[i].r, sg = _leds[i].g, sb = _leds[i].b;
    uint8_t tr = _sc(r), tg = _sc(g), tb = _sc(b);
    for (uint16_t s = 1; s <= steps; s++) {
      uint8_t amt = (uint16_t)(s * 255) / steps;
      _leds[i].r = sr + ((int16_t)(tr - sr) * amt / 255);
      _leds[i].g = sg + ((int16_t)(tg - sg) * amt / 255);
      _leds[i].b = sb + ((int16_t)(tb - sb) * amt / 255);
      _leds[i].cw = 0; _leds[i].ww = 0;
      show(); delay(frame_delay_ms);
    }
  }

  // ── Wipe ──────────────────────────────────────────────────

  /** @brief LED-by-LED wipe: each LED fades to the target before the next starts. */
  void wipeToColor(uint8_t r, uint8_t g, uint8_t b,
                   uint16_t fade_steps = 15, uint16_t frame_delay_ms = 15) override {
    uint8_t tr = _sc(r), tg = _sc(g), tb = _sc(b);
    for (uint16_t i = 0; i < Led_Number; i++) {
      uint8_t sr = _leds[i].r, sg = _leds[i].g, sb = _leds[i].b;
      for (uint16_t s = 1; s <= fade_steps; s++) {
        uint8_t amt = (uint16_t)(s * 255) / fade_steps;
        _leds[i].r = sr + ((int16_t)(tr - sr) * amt / 255);
        _leds[i].g = sg + ((int16_t)(tg - sg) * amt / 255);
        _leds[i].b = sb + ((int16_t)(tb - sb) * amt / 255);
        _leds[i].cw = 0; _leds[i].ww = 0;
        show(); delay(frame_delay_ms);
      }
    }
  }

  // ── Disco ─────────────────────────────────────────────────

  /** @brief Set every LED to a random fully-saturated hue (buffer only). */
  void stepDisco() override {
    for (uint16_t i = 0; i < Led_Number; i++) {
      CRGB c = LedUtil::hsvToRgb(_rnd8(), 255, 255);
      _leds[i] = RGBCCT(_sc(c.r), _sc(c.g), _sc(c.b), 0, 0);
    }
  }

  /** @brief Set every LED to a random hue and show. */
  void showDisco() override { stepDisco(); show(); }

  // ── Flicker ───────────────────────────────────────────────

  /**
   * @brief Prepare the warm-white candle-flicker effect.
   * Call once before the first flickerUpdate().
   */
  void flickerBegin() {
    for (uint16_t i = 0; i < Led_Number; i++) {
      _flickCurrent[i] = 200; _flickTarget[i] = 200;
    }
  }

  /**
   * @brief Advance the flicker animation by one frame and push to strip.
   * Call every animation frame (10–25 FPS is enough).
   */
  void flickerUpdate() {
    for (uint16_t i = 0; i < Led_Number; i++) {
      if (_rndRange(0, 10) < 3) _flickTarget[i] = _rndRange(120, 255);
      int step = _rndRange(5, 25);
      if (_flickCurrent[i] < _flickTarget[i])
        _flickCurrent[i] = min(255, _flickCurrent[i] + step);
      else
        _flickCurrent[i] = max(0,   _flickCurrent[i] - step);
      uint8_t ww = _sc(_flickCurrent[i]);
      _leds[i] = RGBCCT(ww / 8, 0, 0, 0, ww);
    }
    show();
  }

private:
  WS2805Driver _driver;
  RGBCCT  _leds[Led_Number];
  uint8_t _flickCurrent[Led_Number];
  uint8_t _flickTarget[Led_Number];

  // Fast xorshift32 PRNG. Arduino random() was measured at ~95 ms per call on the
  // R4 (60 calls = 5.7 s per disco frame); this takes a few microseconds.
  uint32_t _rng = 2463534242UL;
  uint32_t _rndNext() { _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5; return _rng; }
  uint8_t  _rnd8() { return (uint8_t)(_rndNext() >> 24); }
  long     _rndRange(long lo, long hi) { return lo + (long)(_rndNext() % (uint32_t)(hi - lo)); }

  /** @brief Scale a value by the current global brightness. */
  uint8_t _sc(uint8_t v) const { return (uint16_t)v * _brightness / 255; }

  /** @brief Apply brightness scaling to all five channels and return a pixel. */
  RGBCCT _scaled(uint8_t r, uint8_t g, uint8_t b, uint8_t cw, uint8_t ww) const {
    return RGBCCT(_sc(r), _sc(g), _sc(b), _sc(cw), _sc(ww));
  }
};

#endif // LED_CONTROLLER_H
