// BrightnessModule.cpp
#include "BrightnessModule.h"

constexpr uint8_t BRIGHTNESS_CHANGE_ATTEMPTS = 3;

static uint8_t attempt = 0;


void autoSetBrightness() {
  if (auto_brightness && blink_count == 0) {
    last_display_brightness = display_brightness;
    const int light_sensor_value = analogRead(LIGHT_SENSOR_PIN);
    int8_t current_index = -1;
    int8_t target_index = -1;

    for (uint8_t i = 0; i < LIGHT_LEVELS_COUNT; i++) {
      if (current_index > -1 && target_index > -1)
        break;

      if (current_index < 0 && display_brightness == LIGHT_LEVELS[i][1])
        current_index = (int8_t)i;

      if (target_index < 0 && light_sensor_value <= LIGHT_LEVELS[i][0])
        target_index = (int8_t)i;
    }

    const int difference = abs(current_index - target_index);

    if (difference == 0)
      attempt = 0;
    else if (difference > 1)
      attempt = BRIGHTNESS_CHANGE_ATTEMPTS;
    else
      attempt++;

    if (attempt == BRIGHTNESS_CHANGE_ATTEMPTS) {
      display_brightness = (uint8_t)LIGHT_LEVELS[target_index][1];
      attempt = 0;
    }

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("Light sensor value: "));
      Serial.print(light_sensor_value);
      Serial.print(F(", "));
    }
  }
  else
    attempt = BRIGHTNESS_CHANGE_ATTEMPTS;
}
