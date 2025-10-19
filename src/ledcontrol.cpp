#include <Arduino.h>
#include <FastLED.h>

// Define the pin and number of pixels
#define LED_PIN 21
#define INPUT_PIN 4
#define NUM_LEDS 1

// Define the LED array
CRGB leds[NUM_LEDS];

void setup_led() {
  // Initialize FastLED
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50); // Set brightness to 50 (0-255)
}

void set_led(unsigned char red, unsigned char green, unsigned char blue) {
    leds[0] = CRGB(red,green, blue);
    FastLED.show();
}