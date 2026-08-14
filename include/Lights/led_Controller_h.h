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


// ARGB Led Chain. CLASS
template<uint8_t DATA_PIN, uint8_t Led_Number, EOrder COLOR_ORDER = GRB>
class ARGB {
public:

// Setup function
    // Power pin is runtime — 0xFF means "no power pin, ignore it"
    ARGB(uint8_t power_pin = 0xFF) : _power_pin(power_pin), _brightness(255) {}
    
    // Hardware init — call from setup()
    void Setup() {
        if (_power_pin != 0xFF) {
        pinMode(_power_pin, OUTPUT);
        digitalWrite(_power_pin, HIGH);
        }
        FastLED.addLeds<WS2812, DATA_PIN, COLOR_ORDER>(_leds, Led_Number);
        FastLED.setBrightness(_brightness);
    }




/* 
Settings Functions
    setBrightness,getBrightness
    show
*/

    // Brightness Functions

        // Set the brightness level (0..255)
            void setBrightness(uint8_t b) {
                _brightness = b;
                FastLED.setBrightness(b);
            };

        // Get the current brightness level (0..255)
            uint8_t getBrightness() const { return _brightness; };

    // Show function

        // Show the current LED state on the hardware. Call this after making changes to the LEDs to update the display.
            void show() { FastLED.show(); };

    // Clear function
        // Clear all LEDs (turn them off)
            void clear() {
                fill_solid(_leds, Led_Number, CRGB::Black);
            }

/*
Singe LED control functions
    Set a Singel LED in the ring to a specific color. 
    The RGB version takes r,g,b values, while the HSV and hex versions convert to RGB and call the RGB version internally.
*/

// Set 

    // rgb Version
    // led_index, r, g, b
        void Set_Single(uint8_t led_position_index, uint8_t r, uint8_t g, uint8_t b) {
        if (led_position_index >= Led_Number) return;
        _leds[led_position_index] = CRGB(r, g, b);
        }

    // HSV Version
    // led_index, hue, saturation, value
        void Set_Single_HSV(uint8_t led_position_index, uint8_t hue, uint8_t sat = 255, uint8_t val = 255) {
        if (led_position_index >= Led_Number) return;
        _leds[led_position_index] = CHSV(hue, sat, val);
        }

    // hex version
    // led_index, hex_color
        void Set_Single_Hex(uint8_t led_position_index, uint32_t hex_color) {
        Set_Single(led_position_index, hexR(hex_color), hexG(hex_color), hexB(hex_color));
        }


// FadeTO (animation):
// is a simple animation that transitions a single LED from its current color to a target color over

    // Rgb version
    // led_index, r, g, b, steps, frame_delay_ms
        void FadeTO_Single(uint8_t led_position_index, uint8_t r, uint8_t g, uint8_t b,
                    uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
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

    // HSV version
    // led_index, hue, saturation, value, steps, frame_delay_ms
        void FadeTO_Single_HSV(uint8_t led_position_index, uint8_t hue, uint8_t sat = 255, uint8_t val = 255,
                        uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
        CRGB t;
        hsv2rgb_rainbow(CHSV(hue, sat, val), t);
        FadeTO_Single(led_position_index, t.r, t.g, t.b, steps, frame_delay_ms);
        }

    // hex version
    // led_index, hex_color, steps, frame_delay_ms
        void FadeTO_Single_Hex(uint8_t led_position_index, uint32_t hex_color,
                        uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
        FadeTO_Single(led_position_index, hexR(hex_color), hexG(hex_color), hexB(hex_color), steps, frame_delay_ms);
        }

/*
All LEDs control functions
    control all LEDs in the ring together, either setting them to a 
    specific color or animating them to transition to a new color.
*/

// All_Set:

    // RGB version
    // r, g, b
        void All_Set(uint8_t r, uint8_t g, uint8_t b) {
        fill_solid(_leds, Led_Number, CRGB(r, g, b));
        }

    // HSV version
    // hue, saturation, value
        void All_Set_HSV(uint8_t hue, uint8_t sat = 255, uint8_t val = 255) {
        fill_solid(_leds, Led_Number, CHSV(hue, sat, val));
        }

    // hex version
    // hex_color
        void All_Set_Hex(uint32_t hex_color) {
        All_Set(hexR(hex_color), hexG(hex_color), hexB(hex_color));
        }


// All_FadeTo (animation):

    // RGB version
    // r, g, b, steps, frame_delay_ms
        void All_FadeTo(uint8_t r, uint8_t g, uint8_t b,
                        uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
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

    // HSV version
    // hue, saturation, value, steps, frame_delay_ms
        void All_FadeTo_HSV(uint8_t hue, uint8_t sat = 255, uint8_t val = 255,
                            uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
        CRGB t = hsvToRgb(hue, sat, val);
        All_FadeTo(t.r, t.g, t.b, steps, frame_delay_ms);
        }

    // hex version
    // hex_color, steps, frame_delay_ms
        void All_FadeTo_Hex(uint32_t hex_color,
                            uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
        All_FadeTo(hexR(hex_color), hexG(hex_color), hexB(hex_color), steps, frame_delay_ms);
        }


// WipeTo (animation):
// instead of fading all LEDs together, it fades them in one by one, creating a "Wipe" effect.
     
    // RGB version
    // r, g, b, fade_steps, frame_delay_ms
        void WipeTo(uint8_t r, uint8_t g, uint8_t b,
                    uint16_t fade_steps = 15, uint16_t frame_delay_ms = 15) {
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

    // HSV Version
    // Hsv,fead_steps,frame_delay_ms
        void WipeTo_HSV(uint8_t hue, uint8_t sat = 255, uint8_t val = 255,
                        uint16_t fade_steps = 15, uint16_t frame_delay_ms = 15) {
        CRGB t = hsvToRgb(hue, sat, val);
        WipeTo(t.r, t.g, t.b, fade_steps, frame_delay_ms);
        }

    // hex version
    // hex_color, fade_steps, frame_delay_ms
        void WipeTo_Hex(uint32_t hex_color,
                        uint16_t fade_steps = 15, uint16_t frame_delay_ms = 15) {
        WipeTo(hexR(hex_color), hexG(hex_color), hexB(hex_color), fade_steps, frame_delay_ms);
        }

/*
Other Animations:
    You can add more complex animations as needed, such as a "disco" mode that rapidly changes colors in a random pattern, or a "breathing" mode that smoothly fades the LEDs in and out. The implementation of these animations would depend on the specific effects you want to achieve, but they would generally involve using the existing color setting and fading functions in creative ways within the main loop of your program.
*/

// Disco mode: Randomly changes the color of the LEDs in a disco-like pattern. The RGB version sets the LEDs to random colors, while the HSV version randomizes the hue and keeps saturation and value at maximum. The hex version converts to RGB and calls the RGB version internally.

    void Disco_animation() {
    for (uint8_t i = 0; i < Led_Number; i++) {
        _leds[i] = CHSV(random8(), 255, 255);
    }
    FastLED.show();
    }

    void Disco_step() {
    for (uint8_t i = 0; i < Led_Number; i++) {
        _leds[i] = CHSV(random8(), 255, 255);
    }
    }
//Loading Cirle
//   ring.Loading_Circle_step();           // grow one LED, default hue 0 (red)
//   ring.Loading_Circle_step(96);         // grow one LED in green
void Loading_Circle_step(uint8_t hue = 0) {
  if (_loading_progress < Led_Number) {
    _leds[_loading_progress] = CHSV(hue, 255, 255);
    _loading_progress++;
    show();
  }
}

// Wipe back to empty.
    void Loading_Circle_reset() {
    _loading_progress = 0;
    // optional: also clear the buffer
    fill_solid(_leds, Led_Number, CRGB::Black);
    }

// How many LEDs are currently lit.
    uint8_t Loading_Circle_progress() const { return _loading_progress; }
private:

//Helpers

    // Hex to RGB conversion helpers
        static inline uint8_t hexR(uint32_t h) { return (h >> 16) & 0xFF; }
        static inline uint8_t hexG(uint32_t h) { return (h >>  8) & 0xFF; }
        static inline uint8_t hexB(uint32_t h) { return  h        & 0xFF; }

    // HSV to RGB conversion helper
        CRGB hsvToRgb(uint8_t hue, uint8_t sat, uint8_t val) {
        CRGB rgb;
            CHSV hsv(hue, sat, val);
        hsv2rgb_rainbow(hsv, rgb);
        return rgb;
        }

// Member variables
    CRGB _leds[Led_Number];
    uint8_t _power_pin;
    uint8_t _brightness;
    uint8_t _loading_progress = 0;

//End of ARGB class
};



// RGB Led CLASS (per colour led)
template<uint8_t r_pin, uint8_t g_pin, uint8_t b_pin, bool LED_COMMON_ANODE = false>
class RGBLeds{
public:

// Setup function

    // Power pin is runtime — 0xFF means "no power pin, ignore it"
        RGBLeds() : _LED_COMMON_ANODE(LED_COMMON_ANODE) {}

    // Hardware init — call from setup()
        void Setup() {
            pinMode(r_pin, OUTPUT);
            pinMode(g_pin, OUTPUT);
            pinMode(b_pin, OUTPUT);
        }

/* 
Settings Functions
    setBrightness,getBrightness
    show
*/

    // Brightness
        // setBrightness 
            void setBrightness(uint8_t b) {
                _Brightness = b;
            };

        // getBrightness
            u_int8_t getBrightness() const { return _Brightness; };

    // Show function
        void show() {
        if (_LED_COMMON_ANODE) {
            analogWrite(r_pin, 255 - _Color.r);
            analogWrite(g_pin, 255 - _Color.g);
            analogWrite(b_pin, 255 - _Color.b);
        } else {
            analogWrite(r_pin, _Color.r);
            analogWrite(g_pin, _Color.g);
            analogWrite(b_pin, _Color.b);
        }
        }

    // Clear function
        void clear() {
        _Color = CRGB::Black;
        show();
        }

/*
Singe LED control functions
    Set a Singel LED in the ring to a specific color. 
    The RGB version takes r,g,b values, while the HSV and hex versions convert to RGB and call the RGB version internally.
*/

    // Set 

        // rgb Version
        // r, g, b
            void ledSet(uint8_t r, uint8_t g, uint8_t b) {
            _Color = CRGB(r, g, b);
            show();
            }

        // Hsv version of ledSet
        // hue, saturation, value
            void ledSetHSV(uint8_t hue, uint8_t sat = 255, uint8_t val = 255) {
            CRGB c = hsvToRgb(hue, sat, val);
            ledSet(c.r, c.g, c.b);
            }

        // Hex version of ledSet
        // hex_color
            void ledSetHex(uint32_t hex_color) {
            ledSet(hexR(hex_color), hexG(hex_color), hexB(hex_color));
            }

    // fadeTo (animation):

        // RGB version
        // r, g, b, steps, frame_delay_ms
            void fadeTo(uint8_t r, uint8_t g, uint8_t b,
                        uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
            CRGB start = _Color;
            CRGB target(r, g, b);
            for (uint16_t s = 1; s <= steps; s++) {
                uint8_t amt = (uint16_t)(s * 255) / steps;
                _Color = blend(start, target, amt);
                show();
                delay(frame_delay_ms);
            }
            }
            
        // HSV version
        // hue, sat, val, steps, frame_delay_ms
            void fadeToHSV(uint8_t hue, uint8_t sat = 255, uint8_t val = 255,
                        uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
            CRGB t;
            hsv2rgb_rainbow(CHSV(hue, sat, val), t);
            fadeTo(t.r, t.g, t.b, steps, frame_delay_ms);
            }
        
        // Hex version
        // hex_color, steps, frame_delay_ms
            void fadeToHex(uint32_t hex_color,
                        uint16_t steps = 30, uint16_t frame_delay_ms = 20) {
            fadeTo(hexR(hex_color), hexG(hex_color), hexB(hex_color), steps, frame_delay_ms);
            }

    // Disco mode (animation):
        // Sets the LED to a random hue at full saturation/value. Call repeatedly (e.g. on a timer) for a disco effect.
            void Disco_step() {
            _Color = CHSV(random8(), 255, 255);
            show();
            }

private:

//Helpers

    // Hex to RGB conversion helpers
        static inline uint8_t hexR(uint32_t h) { return (h >> 16) & 0xFF; }
        static inline uint8_t hexG(uint32_t h) { return (h >>  8) & 0xFF; }
        static inline uint8_t hexB(uint32_t h) { return  h        & 0xFF; }

    // HSV to RGB conversion helper
        CRGB hsvToRgb(uint8_t hue, uint8_t sat, uint8_t val) {
        CRGB rgb;
            CHSV hsv(hue, sat, val);
        hsv2rgb_rainbow(hsv, rgb);
        return rgb;
        }

// Member variables
    CRGB _Color;
    bool _LED_COMMON_ANODE;
    uint8_t _Brightness;

//End of RGBLed class
};
#endif // LED_CONTROLLER_H