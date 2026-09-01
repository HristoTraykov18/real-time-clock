// TemperatureModule.cpp
#include "TemperatureModule.h"

// Raw TM1637 segment patterns, one byte per digit with the bit order 0b0GFEDCBA
const uint8_t GLYPH_BLANK  = 0x00; // No segments on
const uint8_t GLYPH_MINUS  = 0x40; // G
const uint8_t GLYPH_DEGREE = 0x63; // A, B, F, G
const uint8_t GLYPH_C      = 0x39; // A, D, E, F
const uint8_t GLYPH_E      = 0x79; // A, D, E, F, G
const uint8_t GLYPH_R      = 0x50; // E, G


void printCurrentTemperature() {
  if (display_state_duration == 4) {
    temperatureSensor.requestTemperatures();
    current_temperature = temperatureSensor.getTempCByIndex(0);
  }

  uint8_t temperature[] = {0, 0, 0, 0};

  if (current_temperature == 0) { // Zero case
    temperature[0] = GLYPH_BLANK;
    temperature[1] = tm1637.encodeDigit(0);
    temperature[2] = GLYPH_DEGREE;
    temperature[3] = GLYPH_C;
  }
  else if (current_temperature < 0 && current_temperature > -10) { // Temp less than 0 and more than -10
    temperature[0] = GLYPH_MINUS;
    temperature[1] = tm1637.encodeDigit(abs(current_temperature) % 10);
    temperature[2] = GLYPH_DEGREE;
    temperature[3] = GLYPH_C;
  }
  else if (current_temperature > 99 || current_temperature <= -79) { // Error message from temperature module
    temperature[0] = GLYPH_E;
    temperature[1] = GLYPH_R;
    temperature[2] = GLYPH_R;
    temperature[3] = tm1637.encodeDigit(2);
  }
  else if (current_temperature <= -10) { // Temp less than -10
    temperature[0] = GLYPH_MINUS;
    temperature[1] = tm1637.encodeDigit(abs(current_temperature) / 10);
    temperature[2] = tm1637.encodeDigit(abs(current_temperature) % 10);
    temperature[3] = GLYPH_DEGREE;
  }
  else if (current_temperature > 0 && current_temperature < 10) {
    temperature[0] = GLYPH_BLANK;
    temperature[1] = tm1637.encodeDigit(current_temperature % 10);
    temperature[2] = GLYPH_DEGREE;
    temperature[3] = GLYPH_C;
  }
  else { // Every other case (10 to 99)
    temperature[0] = tm1637.encodeDigit(current_temperature / 10);
    temperature[1] = tm1637.encodeDigit(current_temperature % 10);
    temperature[2] = GLYPH_DEGREE;
    temperature[3] = GLYPH_C;
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
