#pragma once
/*
  ColorParse.h — parse colour / number arguments typed into the Serial Monitor.

  Pure C++ (no Arduino calls), so it is unit-tested on the laptop
  (test/test_color_parse) and on the board (test/test_hw_strip).

  Accepted colour formats (all case-insensitive):
    255 0 0      spaces
    255,0,0      commas
    255, 0, 0    mixed
    #ff0000      hex with '#'
    ff0000       bare 6-digit hex
  Numbers outside 0..255 are clamped. Anything else returns false.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace ColorParse {

  /** @brief Clamp an int to 0..255. */
  inline uint8_t clamp8(long v) { return v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)v); }

  /** @brief true for ' ' or ','. */
  inline bool isSep(char c) { return c == ' ' || c == ','; }

  /**
   * @brief Parse exactly `n` integers separated by spaces and/or commas.
   * @param s   Input, e.g. "255 0 0" or "255,0,0". Leading/trailing spaces are fine.
   * @param out Array of at least `n` ints that receives the values.
   * @param n   How many numbers are expected.
   * @return true if exactly `n` numbers were found and nothing else follows.
   */
  inline bool parseInts(const char* s, int* out, uint8_t n) {
    while (*s == ' ') s++;
    for (uint8_t i = 0; i < n; i++) {
      char* end;
      long v = strtol(s, &end, 10);
      if (end == s) return false;               // no number here
      out[i] = (int)v;
      s = end;
      if (i + 1 < n) {                          // a separator must follow
        if (!isSep(*s)) return false;
        while (isSep(*s)) s++;
      }
    }
    while (*s == ' ') s++;
    return *s == '\0';                          // no trailing junk
  }

  /** @brief true if `s` starts with exactly 6 hex digits followed only by spaces. */
  inline bool isHex6(const char* s) {
    if (strspn(s, "0123456789abcdefABCDEF") != 6) return false;
    s += 6;
    while (*s == ' ') s++;
    return *s == '\0';
  }

  /**
   * @brief Parse a colour: "r g b", "r,g,b", "#rrggbb" or "rrggbb".
   * @return true on success (r/g/b set), false if the text is not a colour.
   */
  inline bool parseColor(const char* s, uint8_t& r, uint8_t& g, uint8_t& b) {
    while (*s == ' ') s++;
    if (*s == '\0') return false;

    const char* hex = nullptr;
    if (*s == '#') {
      if (!isHex6(s + 1)) return false;
      hex = s + 1;
    } else if (isHex6(s)) {
      hex = s;
    }
    if (hex) {
      uint32_t h = (uint32_t)strtoul(hex, nullptr, 16);
      r = (h >> 16) & 0xFF;  g = (h >> 8) & 0xFF;  b = h & 0xFF;
      return true;
    }

    int v[3];
    if (!parseInts(s, v, 3)) return false;
    r = clamp8(v[0]);  g = clamp8(v[1]);  b = clamp8(v[2]);
    return true;
  }

} // namespace ColorParse
