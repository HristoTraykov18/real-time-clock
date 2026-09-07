// TemperatureModule.cpp
#include "TemperatureModule.h"

// Raw TM1637 segment patterns, one byte per digit with the bit order 0b0GFEDCBA
constexpr uint8_t GLYPH_BLANK  = 0x00; // No segments on
constexpr uint8_t GLYPH_MINUS  = 0x40; // G
constexpr uint8_t GLYPH_DEGREE = 0x63; // A, B, F, G
constexpr uint8_t GLYPH_C      = 0x39; // A, D, E, F
constexpr uint8_t GLYPH_E      = 0x79; // A, D, E, F, G
constexpr uint8_t GLYPH_R      = 0x50; // E, G

static int8_t display_state_duration = 7; // Duration of the current state of the display. Time is shown for 7 seconds and temperature for 4
static bool display_time = true;


void printCurrentTemperature() {
  if (display_state_duration == 4) {
    temperatureSensor.requestTemperatures();
    current_temperature = temperatureSensor.getTempCByIndex(0);
  }

  uint8_t temperature[4];
  auto setDigits = [&temperature](uint8_t first, uint8_t second, uint8_t third, uint8_t fourth) {
    temperature[0] = first;
    temperature[1] = second;
    temperature[2] = third;
    temperature[3] = fourth;
  };

  if (current_temperature == 0) // Zero case
    setDigits(GLYPH_BLANK, tm1637.encodeDigit(0), GLYPH_DEGREE, GLYPH_C);
  else if (current_temperature < 0 && current_temperature > -10) // Temp less than 0 and more than -10
    setDigits(GLYPH_MINUS, tm1637.encodeDigit(abs(current_temperature) % 10), GLYPH_DEGREE, GLYPH_C);
  else if (current_temperature > 99 || current_temperature <= -79) // Error message from temperature module
    setDigits(GLYPH_E, GLYPH_R, GLYPH_R, tm1637.encodeDigit(2));
  else if (current_temperature <= -10) { // Temp less than -10
    const uint8_t abs_temp = abs(current_temperature);
    setDigits(GLYPH_MINUS, tm1637.encodeDigit(abs_temp / 10), tm1637.encodeDigit(abs_temp % 10), GLYPH_DEGREE);
  }
  else if (current_temperature > 0 && current_temperature < 10)
    setDigits(GLYPH_BLANK, tm1637.encodeDigit(current_temperature % 10), GLYPH_DEGREE, GLYPH_C);
  else { // Every other case (10 to 99)
    setDigits(tm1637.encodeDigit(current_temperature / 10),
              tm1637.encodeDigit(current_temperature % 10),
              GLYPH_DEGREE, GLYPH_C);
  }

  tm1637.setSegments(temperature);
  last_second = second_now;
  display_state_duration--;

  if (display_state_duration == 0) {
    display_state_duration = 7;
    display_time = true;
  }
}


void printCurrentTimeOrTemperature() {
  if (display_time) {
    printCurrentTime();
    display_state_duration--;

    if (display_state_duration == 0) {
      display_state_duration = 4;
      display_time = false;
    }
  }
  else
    printCurrentTemperature();
}
