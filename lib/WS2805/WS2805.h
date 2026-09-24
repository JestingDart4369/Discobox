#pragma once
/*
  WS2805Driver.h — WS2805 RGBCCT low-level SPI driver.

  Handles only the hardware communication layer:
    - Bit-encoding each channel value to WS2805 protocol (1-bit=110, 0-bit=100)
    - Assembling the SPI buffer
    - Sending it to the strip at 2.4 MHz

  Does NOT own the pixel buffer — that belongs to WS2805Strip (WS2805Strip.h).
  The strip class calls driver.show(leds, n) to push its buffer to hardware.

  Verdrahtung:
    D11 (MOSI) → Datenleitung (300–470Ω Widerstand empfohlen)
*/

#include <Arduino.h>
#include <SPI.h>

// Fallback falls WS2805_N_LEDS nicht in main.cpp definiert wurde.
// MUSS vor der Klasse stehen, da _buf[] die Konstante beim Kompilieren braucht.
#ifndef WS2805_N_LEDS
#define WS2805_N_LEDS 180
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
 * @brief Low-level WS2805 SPI communication driver.
 *
 * Encodes an RGBCCT pixel buffer into the WS2805 bit stream and transfers
 * it over SPI in one transaction. This class handles ONLY the hardware
 * communication — no pixel buffer, no effects.
 *
 * Used by WS2805Strip (WS2805Strip.h) which owns the pixel buffer and
 * calls show() to push it to hardware.
 */
class WS2805Driver {
public:

  /**
   * @brief Initialise the SPI bus. Call once in setup() before the first show().
   */
  void begin() {
    SPI.begin();
#ifdef LOGGER_H
    Logger::log("WS2805: SPI init OK (2.4 MHz, MOSI=D11)");
#endif
  }

  /**
   * @brief Encode and send a pixel buffer to the strip over SPI.
   *
   * Encodes all pixels into the internal SPI buffer using WS2805 bit encoding
   * (each bit → 3 SPI bits: 1 = 0b110, 0 = 0b100), then transfers in one
   * SPI transaction at 2.4 MHz followed by a reset pulse.
   *
   * @param pixels Pointer to the RGBCCT pixel array.
   * @param count  Number of pixels to send (must match the physical strip length).
   */
  void show(const RGBCCT* pixels, uint16_t count) {
    // Pre-encode all pixels into a single buffer, then send in one SPI.transfer().
    // Single large transfer is faster than 180 × SPI.transfer(15) due to less
    // per-call overhead. On R4 WiFi this reduces blocking time from ~14ms to ~9ms.
    // Buffer size: 180 pixels × 15 bytes + 20 bytes reset = 2720 bytes.
    static uint8_t _buf[WS2805_N_LEDS * 15 + 20];

    uint16_t pos = 0;
    for (uint16_t i = 0; i < count; i++) {
      // Wire order: R, G, B, WW, CW
      uint8_t ch[5] = { pixels[i].r, pixels[i].g, pixels[i].b, pixels[i].ww, pixels[i].cw };
      for (uint8_t c = 0; c < 5; c++) encodeChannel(ch[c], &_buf[pos + c * 3]);
      pos += 15;
    }
    // Reset pulse: 20 zero bytes (>50 µs low at 2.4 MHz)
    memset(&_buf[pos], 0, 20);
    pos += 20;

    SPI.beginTransaction(SPISettings(2400000, MSBFIRST, SPI_MODE0));
    SPI.transfer(_buf, pos);
    SPI.endTransaction();
  }

private:
  static const uint8_t RESET_BYTES = 20;

  /**
   * @brief Encode one 8-bit channel value into 3 SPI bytes (WS2805 protocol).
   * @param v   Channel value (0–255).
   * @param out Output buffer — must be at least 3 bytes.
   */
  static void encodeChannel(uint8_t v, uint8_t out[3]) {
    uint32_t bits = 0;
    for (int i = 7; i >= 0; i--)
      bits = (bits << 3) | ((v >> i & 1) ? 0b110 : 0b100);
    out[0] = bits >> 16;
    out[1] = bits >> 8;
    out[2] = bits & 0xFF;
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
 * (5-channel, SPI). Implements every pure-virtual from LEDBase and ILedStrip,
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
      CRGB c = LedUtil::hsvToRgb(random(256), 255, 255);
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
      if (random(10) < 3) _flickTarget[i] = random(120, 255);
      int step = random(5, 25);
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

  /** @brief Scale a value by the current global brightness. */
  uint8_t _sc(uint8_t v) const { return (uint16_t)v * _brightness / 255; }

  /** @brief Apply brightness scaling to all five channels and return a pixel. */
  RGBCCT _scaled(uint8_t r, uint8_t g, uint8_t b, uint8_t cw, uint8_t ww) const {
    return RGBCCT(_sc(r), _sc(g), _sc(b), _sc(cw), _sc(ww));
  }
};

#endif // LED_CONTROLLER_H
