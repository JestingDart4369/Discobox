#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H
/*
Version 1.1
Consolidated from Ledcontrol.h + argb_Controller_h.h (2026-08-14).
Contains the ARGB (addressable LED chain) and RGBLeds (single RGB LED) classes.

This file makes the bridge between code and the led control library,
 so that the main code can be cleaner and more focused on the logic 
 rather than the details of controlling the LEDs. 
 
 It also allows for easier changes to the LED control implementation 
 in the future if needed.
*/

// Liberaries Prerequisite inport for the ARGB and RGB control
#include <FastLED.h>

// ── Shared color conversion helpers ───────────────────────
// Used by both LEDBase and ILedStrip — defined once to avoid
// ambiguity when ARGB inherits from both.
namespace LedUtil {
    static inline uint8_t hexR(uint32_t h) { return (h >> 16) & 0xFF; }
    static inline uint8_t hexG(uint32_t h) { return (h >>  8) & 0xFF; }
    static inline uint8_t hexB(uint32_t h) { return  h        & 0xFF; }
    static inline CRGB hsvToRgb(uint8_t hue, uint8_t sat, uint8_t val) {
        CRGB rgb; hsv2rgb_rainbow(CHSV(hue, sat, val), rgb); return rgb;
    }
}

class LEDBase {
public:
    // Base class for LED controllers
        virtual void setup() = 0;  // Pure virtual function for setup
        virtual void show() = 0;   // Pure virtual function to update the LEDs
        virtual void clear() = 0;  // Pure virtual function to clear the LEDs
        
    // Brightness
        /** @brief Store a brightness value for future use. @note Not applied automatically — manual show() uses _Color directly. @param b Brightness (0..255). */
            virtual void setBrightness(uint8_t b) {
                _brightness = b;
            };

        /** @brief Returns the stored brightness value. */
            uint8_t getBrightness() const { return _brightness; };

    // SetColor
        /** @brief Set all the LED in a object to an RGB color and show immediately. @param r Red (0..255). @param g Green. @param b Blue. */
            virtual void setToColor(uint8_t r, uint8_t g, uint8_t b) = 0;  // Pure virtual function to set a single LED color
    
        
        /** @brief Set all the LED in a object to an HSV color and show immediately. @param hue 0..255. @param sat Saturation. @param val Value/brightness. */
        virtual void setToColorHSV(uint8_t hue, uint8_t sat = 255, uint8_t val = 255) {
                CRGB c = LedUtil::hsvToRgb(hue, sat, val);
                setToColor(c.r, c.g, c.b);
                }

        /** @brief Set all the LED in a object to a 24-bit hex color and show immediately. @param hex_color 0xRRGGBB. */
            void setToColorHex(uint32_t hex_color) {
                setToColor(LedUtil::hexR(hex_color), LedUtil::hexG(hex_color), LedUtil::hexB(hex_color));
                }

    // FadeToColor
        /** @brief Fade all the LED in a object to a color over a number of steps. @param r Red (0..255). @param g Green. @param b Blue. @param steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
            virtual void fadeToColor(uint8_t r, uint8_t g, uint8_t b, uint16_t steps, uint16_t frame_delay_ms) = 0;  // Pure virtual function to fade to a color

        /** @brief Fade all the LED in a object to an HSV color over a number of steps. @param hue 0..255. @param sat Saturation. @param val Value/brightness. @param steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
        void fadeToColorHSV(uint8_t hue, uint8_t sat = 255, uint8_t val = 255,
                            uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
                CRGB t = LedUtil::hsvToRgb(hue, sat, val);
                fadeToColor(t.r, t.g, t.b, steps, frame_delay_ms);
                }

                /** @brief Fade all the LED in a object to a 24-bit hex color over a number of steps. @param hex_color 0xRRGGBB. @param steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
            void fadeToColorHex(uint32_t hex_color,
                        uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
            fadeToColor(LedUtil::hexR(hex_color), LedUtil::hexG(hex_color), LedUtil::hexB(hex_color), steps, frame_delay_ms);
            }
    // Disco mode (animation):
        /** @brief Sets the LED to a random hue at full saturation/value. Call repeatedly (e.g. on a timer) for a disco effect. */
        virtual void stepDisco() = 0;  // Pure virtual function for disco effect
        /** @brief Shows the disco effect. */
        virtual void showDisco() = 0;  // Pure virtual function to show disco effect

protected:
    uint8_t _brightness=255;  // Global brightness level (0-255)

};
class ILedStrip {
public:
    virtual ~ILedStrip() {}

    // WipeToColor (animation):
        /** @brief Wipe the LED strip to a specific color. @param r Red component (0-255). @param g Green component (0-255). @param b Blue component (0-255). @param fade_steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
            virtual void wipeToColor(uint8_t r, uint8_t g, uint8_t b,uint16_t fade_steps = 15, uint16_t frame_delay_ms = 15) = 0;

        /** @brief Wipe the LED strip to a specific color in HSV space. @param hue Hue (0-255). @param sat Saturation (0-255). @param val Value (0-255). @param fade_steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
            void wipeToColorHSV(uint8_t hue, uint8_t sat = 255, uint8_t val = 255,
                            uint16_t fade_steps = 15, uint16_t frame_delay_ms = 15) {
            CRGB t = LedUtil::hsvToRgb(hue, sat, val);
            wipeToColor(t.r, t.g, t.b, fade_steps, frame_delay_ms);
            }

        /** @brief Wipe the LED strip to a specific color in hex format. @param hex_color The color in hex format (0xRRGGBB). @param fade_steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
            void wipeToColorHex(uint32_t hex_color,
                            uint16_t fade_steps = 15, uint16_t frame_delay_ms = 15) {
            wipeToColor(LedUtil::hexR(hex_color), LedUtil::hexG(hex_color), LedUtil::hexB(hex_color), fade_steps, frame_delay_ms);
            }
    //setToColorSingle
        /** @brief Set a single LED to a specific color. @param led_position_index The index of the LED to set. @param r Red component (0-255). @param g Green component (0-255). @param b Blue component (0-255). */
            virtual void setToColorSingle(uint8_t led_position_index, uint8_t r, uint8_t g, uint8_t b) = 0;

        /** @brief Set a single LED to an HSV color (buffer only). @param led_position_index 0-based index. @param hue 0..255. @param sat Saturation (default 255). @param val Value/brightness (default 255). */
            void setToColorSingleHSV(uint8_t led_position_index, uint8_t hue, uint8_t sat = 255, uint8_t val = 255) {
                CRGB t = LedUtil::hsvToRgb(hue, sat, val);
                setToColorSingle(led_position_index, t.r, t.g, t.b);
            }

        /** @brief Set a single LED to a 24-bit hex color (buffer only). @param led_position_index 0-based index. @param hex_color 0xRRGGBB. */
            void setToColorSingleHex(uint8_t led_position_index, uint32_t hex_color) {
            setToColorSingle(led_position_index, LedUtil::hexR(hex_color), LedUtil::hexG(hex_color), LedUtil::hexB(hex_color));
            }
    //fadeToColorSingle
        /** @brief Fade a single LED to a specific color over a number of steps. @param led_position_index The index of the LED to fade. @param r Red component (0-255). @param g Green component (0-255). @param b Blue component (0-255). @param steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
            virtual void fadeToColorSingle(uint8_t led_position_index, uint8_t r, uint8_t g, uint8_t b,
                uint16_t steps = 30, uint16_t frame_delay_ms = 20) = 0;
        /** @brief Fade a single LED to an HSV color over a number of steps. @param led_position_index 0-based index. @param hue 0..255. @param sat Saturation (default 255). @param val Value/brightness (default 255). @param steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
            void fadeToColorSingleHSV(uint8_t led_position_index, uint8_t hue, uint8_t sat = 255, uint8_t val = 255,
                uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
            CRGB t = LedUtil::hsvToRgb(hue, sat, val);
            fadeToColorSingle(led_position_index, t.r, t.g, t.b, steps, frame_delay_ms);
            }

        /** @brief Fade a single LED to a 24-bit hex color over a number of steps. @param led_position_index 0-based index. @param hex_color 0xRRGGBB. @param steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
            void fadeToColorSingleHex(uint8_t led_position_index, uint32_t hex_color,
                            uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
            fadeToColorSingle(led_position_index, LedUtil::hexR(hex_color), LedUtil::hexG(hex_color), LedUtil::hexB(hex_color), steps, frame_delay_ms);
            }

};

/**
 * @brief WS2812 addressable LED chain controller (FastLED wrapper).
 *
 * Template parameters are compile-time constants; power_pin is optional
 * runtime — pass 0xFF (default) when no enable pin is used.
 *
 * @tparam DATA_PIN    Arduino pin connected to the data line of the WS2812 chain.
 * @tparam Led_Number  Number of LEDs in the chain.
 * @tparam COLOR_ORDER FastLED colour order (default GRB for most WS2812 strips).
 *
 * Call Setup() once in Arduino setup(), then call show() after any
 * color-setting call to push the buffer to hardware.
 */
template<uint8_t DATA_PIN, uint8_t Led_Number, EOrder COLOR_ORDER = GRB>
class ARGB : public LEDBase , public ILedStrip {
public:

// Setup function
    /** @brief Constructs the chain controller. @param power_pin Optional power-enable pin (drive HIGH to enable strip power). Pass 0xFF to ignore. */
    ARGB(uint8_t power_pin = 0xFF) : _power_pin(power_pin), _brightness(255) {}
    
    /** @brief Hardware init — registers the LED chain with FastLED and optionally enables the power pin. Call once from setup(). */
    void setup() override {
        if (_power_pin != 0xFF) {
        pinMode(_power_pin, OUTPUT);
        digitalWrite(_power_pin, HIGH);
        }
        FastLED.addLeds<WS2812, DATA_PIN, COLOR_ORDER>(_leds, Led_Number);
        FastLED.setBrightness(_brightness);
    }

// Brightness 
    /** @brief Set global brightness (0 = off, 255 = full). Applied by FastLED on the next show(). @param b Brightness level (0..255). */
        void setBrightness(uint8_t b) override {
            _brightness = b;
            FastLED.setBrightness(b);
        };
    
// Show
    /** @brief Push the internal LED buffer to the hardware. Call after any color-setting operation to update the strip. */
        void show() override {
            FastLED.show();
        };

    /** @brief Set all LEDs to black in the buffer (does not call show()). */
        void clear() override {
                fill_solid(_leds, Led_Number, CRGB::Black);
            }

/*
Singe LED control functions
    Set a Singel LED in the ring to a specific color. 
    The RGB version takes r,g,b values, while the HSV and hex versions convert to RGB and call the RGB version internally.
*/

// Set 

    /**
     * @brief Set a single LED to an RGB color (buffer only — call show() to display).
     * @param led_position_index 0-based LED index. Out-of-range is silently ignored.
     * @param r Red (0..255). @param g Green (0..255). @param b Blue (0..255).
     */
        void setToColorSingle(uint8_t led_position_index, uint8_t r, uint8_t g, uint8_t b) override {
        if (led_position_index >= Led_Number) return;
        _leds[led_position_index] = CRGB(r, g, b);
        }




// FadeTOColorSingle (animation):
    /** @brief Fade a single LED to an RGB color over a number of steps. @param led_position_index 0-based LED index. Out-of-range is silently ignored. @param r Red. @param g Green. @param b Blue. @param steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
        void fadeToColorSingle(uint8_t led_position_index, uint8_t r, uint8_t g, uint8_t b,
                    uint16_t steps = 30, uint16_t frame_delay_ms = 20) override {
        if (led_position_index >= Led_Number) return;
        CRGB start = _leds[led_position_index];
        CRGB target(r, g, b);
        for (uint16_t s = 1; s <= steps; s++) {
            uint8_t amt = (uint16_t)(s * 255) / steps;
            _leds[led_position_index] = blend(start, target, amt);
            FastLED.show();
            delay(frame_delay_ms);
        }
        }
// SetToColor:

    /** @brief Set all LEDs to an RGB color (buffer only). @param r Red. @param g Green. @param b Blue. */
        void setToColor(uint8_t r, uint8_t g, uint8_t b) override {
        fill_solid(_leds, Led_Number, CRGB(r, g, b));
        }

// FadeToColor (animation):
    /** @brief Fade all LEDs to an RGB color over a number of steps. @param r Red. @param g Green. @param b Blue. @param steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
        void fadeToColor(uint8_t r, uint8_t g, uint8_t b,
                        uint16_t steps = 30, uint16_t frame_delay_ms = 20) override {
        CRGB target(r, g, b);
        CRGB start[Led_Number];
        for (uint8_t i = 0; i < Led_Number; i++) start[i] = _leds[i];

        for (uint16_t s = 1; s <= steps; s++) {
            uint8_t amt = (uint16_t)(s * 255) / steps;
            for (uint8_t i = 0; i < Led_Number; i++) {
            _leds[i] = blend(start[i], target, amt);
            }
            FastLED.show();
            delay(frame_delay_ms);
        }
        }

// WipeTo (animation):
    /** @brief Wipe the LEDs to an RGB color over a number of steps. @param r Red. @param g Green. @param b Blue. @param fade_steps Number of steps to fade over. @param frame_delay_ms Delay between each step (ms). */
        void wipeToColor(uint8_t r, uint8_t g, uint8_t b,
                    uint16_t fade_steps = 15, uint16_t frame_delay_ms = 15) override {
        CRGB target(r, g, b);
        for (uint8_t i = 0; i < Led_Number; i++) {
            CRGB start = _leds[i];
            for (uint16_t s = 1; s <= fade_steps; s++) {
            uint8_t amt = (uint16_t)(s * 255) / fade_steps;
            _leds[i] = blend(start, target, amt);
            FastLED.show();
            delay(frame_delay_ms);
            }
        }
        }
        
// Disco mode: Randomly changes the color of the LEDs in a disco-like pattern. The RGB version sets the LEDs to random colors, while the HSV version randomizes the hue and keeps saturation and value at maximum. The hex version converts to RGB and calls the RGB version internally.

    /** @brief Set every LED to a random fully-saturated hue and call show(). One-shot disco frame. */
    void showDisco() override {
    for (uint8_t i = 0; i < Led_Number; i++) {
        _leds[i] = CHSV(random8(), 255, 255);
    }
    FastLED.show();
    }

    /** @brief Set every LED to a random fully-saturated hue (buffer only — no show()). Use this in a timed loop. */
    void stepDisco() override {
    for (uint8_t i = 0; i < Led_Number; i++) {
        _leds[i] = CHSV(random8(), 255, 255);
    }
    }
//Loading Cirle
//   ring.Loading_Circle_step();           // grow one LED, default hue 0 (red)
//   ring.Loading_Circle_step(96);         // grow one LED in green
/**
 * @brief Light the next LED in a growing arc (loading indicator).
 *
 * Each call advances the progress by one LED. Does nothing once all LEDs
 * are lit. Call Loading_Circle_reset() to start over.
 * @param hue HSV hue of the lit LEDs (default 0 = red).
 */
void stepLoadingCircle(uint8_t hue = 0) {
  if (_loadingStatusprogress < Led_Number) {
    _leds[_loadingStatusprogress] = CHSV(hue, 255, 255);
    _loadingStatusprogress++;
    show();
  }
}

/** @brief Reset the loading arc progress to zero and clear the LED buffer. */
    void resetLoadingCircle() {
    _loadingStatusprogress = 0;
    // optional: also clear the buffer
    fill_solid(_leds, Led_Number, CRGB::Black);
    }

/** @brief Returns how many LEDs are currently lit in the loading arc. */
    uint8_t progressStatusLoadingCircle() const { return _loadingStatusprogress; }
private:
// Member variables
    CRGB _leds[Led_Number];

    uint8_t _power_pin;

    uint8_t _loadingStatusprogress = 0;

};



/**
 * @brief Single RGB LED controller using three PWM pins.
 *
 * Supports both common-cathode (default) and common-anode wiring.
 *
 * @tparam r_pin            Arduino pin connected to the red channel.
 * @tparam g_pin            Arduino pin connected to the green channel.
 * @tparam b_pin            Arduino pin connected to the blue channel.
 * @tparam LED_COMMON_ANODE Set to true for common-anode LEDs (PWM values are inverted).
 */
template<uint8_t r_pin, uint8_t g_pin, uint8_t b_pin, bool LED_COMMON_ANODE = false>
class RGBLeds : public LEDBase {
    public:

    // Setup function

        // Power pin is runtime — 0xFF means "no power pin, ignore it"
            RGBLeds() : _LED_COMMON_ANODE(LED_COMMON_ANODE) {}

        /** @brief Hardware init — sets the three color pins as outputs. Call once from setup(). */
            void setup() override {
                pinMode(r_pin, OUTPUT);
                pinMode(g_pin, OUTPUT);
                pinMode(b_pin, OUTPUT);
            }
        
        /** @brief Write the current color to the PWM pins, scaled by _brightness. Inverts values for common-anode wiring. */
            void show() override {
            uint8_t r = ((uint16_t)_Color.r * _brightness) / 255;
            uint8_t g = ((uint16_t)_Color.g * _brightness) / 255;
            uint8_t b = ((uint16_t)_Color.b * _brightness) / 255;
            if (_LED_COMMON_ANODE) {
                analogWrite(r_pin, 255 - r);
                analogWrite(g_pin, 255 - g);
                analogWrite(b_pin, 255 - b);
            } else {
                analogWrite(r_pin, r);
                analogWrite(g_pin, g);
                analogWrite(b_pin, b);
            }
            }

        /** @brief Set the LED to black and call show(). */
            void clear() override {
            _Color = CRGB::Black;
            show();
            }

        // Set Color (immediate):
            /** @brief Set the LED to an RGB color and show immediately. @param r Red (0..255). @param g Green. @param b Blue. */
                void setToColor(uint8_t r, uint8_t g, uint8_t b) override {
                _Color = CRGB(r, g, b);
                show();
                }

        // fadeTo (animation):
            /** @brief Fade the LED to a new color over a specified number of steps. @param r Red (0..255). @param g Green. @param b Blue. @param steps Number of steps to take. @param frame_delay_ms Delay between each step. */
                void fadeToColor(uint8_t r, uint8_t g, uint8_t b,
                            uint16_t steps = 30, uint16_t frame_delay_ms = 20) override {
                CRGB start = _Color;
                CRGB target(r, g, b);
                for (uint16_t s = 1; s <= steps; s++) {
                    uint8_t amt = (uint16_t)(s * 255) / steps;
                    _Color = blend(start, target, amt);
                    show();
                    delay(frame_delay_ms);
                }
                }

        // Disco mode (animation):
            /** @brief Sets the LED to a random hue and shows immediately. One-shot disco frame. */
                void showDisco() override {
                _Color = CHSV(random8(), 255, 255);
                show();
                }

            /** @brief Sets the LED to a random hue (buffer only — no show()). Use in a timed loop. */
                void stepDisco() override {
                    _Color = CHSV(random8(), 255, 255);
                }

    private:
    // Member variables
        CRGB _Color;
        bool _LED_COMMON_ANODE;
};
#endif // LED_CONTROLLER_H