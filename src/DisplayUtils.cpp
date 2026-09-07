// DisplayUtils.cpp
#include "DisplayUtils.h"

// Raw TM1637 segment patterns, one byte per digit with the bit order 0b0GFEDCBA
constexpr uint8_t GLYPH_BLANK      = 0x00;
constexpr uint8_t GLYPH_TOP_BOTTOM = 0x09; // A, D
constexpr uint8_t GLYPH_BOX        = 0x0F; // A, B, C, D
constexpr uint8_t GLYPH_LEFT_BAR   = 0x30; // E, F

static bool last_auto_brightness = true;


void displayClockJustUpdated(bool updated_from_gps) {
  // Effect when the clock time is set
  const uint8_t ANIMATION_LENGTH = 9;
  const uint8_t TIME_SET_ANIMATION[ANIMATION_LENGTH][4] = {
    { GLYPH_LEFT_BAR,   GLYPH_BLANK,      GLYPH_BLANK,      GLYPH_BLANK      },
    { GLYPH_BOX,        GLYPH_BLANK,      GLYPH_BLANK,      GLYPH_BLANK      },
    { GLYPH_TOP_BOTTOM, GLYPH_LEFT_BAR,   GLYPH_BLANK,      GLYPH_BLANK      },
    { GLYPH_TOP_BOTTOM, GLYPH_BOX,        GLYPH_BLANK,      GLYPH_BLANK      },
    { GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM, GLYPH_LEFT_BAR,   GLYPH_BLANK      },
    { GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM, GLYPH_BOX,        GLYPH_BLANK      },
    { GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM, GLYPH_LEFT_BAR   },
    { GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM, GLYPH_BOX        },
    { GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM, GLYPH_TOP_BOTTOM }
  };

  // Animate effect
  if (updated_from_gps) {
    for (int j = ANIMATION_LENGTH - 1; j > -1; j--) {
      tm1637.setSegments(TIME_SET_ANIMATION[j]);
      delay(75);
    }
  }
  else {
    for (int j = 0; j < ANIMATION_LENGTH; j++) {
      tm1637.setSegments(TIME_SET_ANIMATION[j]);
      delay(75);
    }
  }
}


void flashDisplay() {
  if (display_client_connected && blink_count == 0) {
    blink_count = 6;
    last_auto_brightness = auto_brightness;
    last_display_brightness = display_brightness;
    auto_brightness = false;
  }

  if (blink_count > 0) {
    if (display_brightness == last_display_brightness) {
      if (connected_to_ntp)
        display_brightness = display_brightness != 6 ? 6 : 0;
      else
        display_brightness = display_brightness != 0 ? 0 : 6;
    }
    else
      display_brightness = last_display_brightness;

    blink_count--;

    if (blink_count == 0) {
      display_client_connected = false;
      auto_brightness = last_auto_brightness;
    }

    if (!display_client_connected && !connected_to_ntp) {
      blink_count = 0;
      auto_brightness = last_auto_brightness;
      display_brightness = last_display_brightness;
    }
  }

  tm1637.setBrightness(display_brightness);
}


void visualizeOnDisplay() {
  if constexpr (HAS_BRIGHTNESS_MODULE)
    autoSetBrightness(); // Brightness module function

  flashDisplay();

  if (work_mode_is_timer) {
    printRemainingTime(); // Timer mode
  }
  else {
    if constexpr (HAS_TEMPERATURE_MODULE)
      printCurrentTimeOrTemperature(); // If the clock has temperature sensor show temperature as well
    else
      printCurrentTime();
  }

  last_second = second_now;
}
