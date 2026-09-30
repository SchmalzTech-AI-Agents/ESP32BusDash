#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "cyd35_pins.h"
#include "cyd_status_led.h"

namespace {
// The product documentation calls this a single RGB tricolor indicator. The
// vendor demo uses the GRBW/800 kHz protocol on GPIO40 but carries an
// unrelated template count of 60; only one physical on-board LED is driven.
constexpr uint8_t STATUS_LED_COUNT = 1;
constexpr uint8_t STATUS_LED_BRIGHTNESS = 20;
constexpr uint32_t UPDATE_INTERVAL_MS = 25;
constexpr uint16_t RAINBOW_PERIOD_STEPS = 480;

Adafruit_NeoPixel statusLed(STATUS_LED_COUNT, Cyd35Pins::STATUS_LED,
                            NEO_GRBW + NEO_KHZ800);
uint32_t lastUpdateMs = 0;
uint16_t hueStep = 0;

uint32_t rainbowColor(uint8_t position) {
  position = 255 - position;
  if (position < 85) return statusLed.Color(255 - position * 3, 0, position * 3, 0);
  if (position < 170) {
    position -= 85;
    return statusLed.Color(0, position * 3, 255 - position * 3, 0);
  }
  position -= 170;
  return statusLed.Color(position * 3, 255 - position * 3, 0, 0);
}
}  // namespace

void cydStatusLedBegin() {
  statusLed.begin();
  statusLed.setBrightness(STATUS_LED_BRIGHTNESS);
  statusLed.clear();
  statusLed.show();
  lastUpdateMs = 0;
}

void cydStatusLedUpdate() {
  const uint32_t now = millis();
  if (now - lastUpdateMs < UPDATE_INTERVAL_MS) return;
  lastUpdateMs = now;
  const uint8_t hue = static_cast<uint8_t>((static_cast<uint32_t>(hueStep) * 256U) /
                                            RAINBOW_PERIOD_STEPS);
  statusLed.setPixelColor(0, rainbowColor(hue));
  statusLed.show();
  hueStep = (hueStep + 1) % RAINBOW_PERIOD_STEPS;
}
