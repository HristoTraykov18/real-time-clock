// TemperatureModule.cpp
#include "TemperatureModule.h"

// Raw TM1637 segment patterns, one byte per digit with the bit order 0b0GFEDCBA
constexpr uint8_t GLYPH_BLANK  = 0x00; // No segments on
constexpr uint8_t GLYPH_MINUS  = 0x40; // Segment G
constexpr uint8_t GLYPH_DEGREE = 0x63; // Segments A, B, F, G
constexpr uint8_t GLYPH_C      = 0x39; // Segments A, D, E, F
constexpr uint8_t TIME_STATE_DURATION = 7;
constexpr uint8_t TEMPERATURE_STATE_DURATION = 4;

static DeviceAddress sensor_address; // Looked up once, the bus holds a single sensor that never changes
static int8_t display_state_duration = 7; // Duration of the current state of the display. Time is shown for 7 seconds and temperature for 4
static int8_t current_temperature;
static bool display_time = true;


void initializeTemperatureSensor() {
  temperatureSensor.begin(); // Enumerate the 1-Wire bus. Required before the address lookup

  const bool sensor_found = temperatureSensor.getAddress(sensor_address, 0);

  temperatureSensor.requestTemperatures(); // Blocking, so the first reading is ready before setup() ends
  current_temperature = temperatureSensor.getTempC(sensor_address);
  temperatureSensor.setWaitForConversion(false); // Every later conversion is collected on a following tick

  if constexpr (DEBUG_MESSAGES)
    Serial.println(sensor_found ? F("[initializeTemperatureSensor] Temperature sensor initialized")
                                : F("[initializeTemperatureSensor] Temperature sensor not found on the 1-Wire bus"));
}


void printCurrentTemperature() {
  current_temperature = temperatureSensor.getTempC(sensor_address);

  if (display_state_duration > 1)
    temperatureSensor.requestTemperatures();

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
    setDigits(GLYPH_MINUS, GLYPH_MINUS, GLYPH_MINUS, GLYPH_MINUS);
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

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("Temperature: "));
    Serial.print(current_temperature);
    Serial.print(F("°C, "));
  }

  if (display_state_duration == 0) {
    display_state_duration = TIME_STATE_DURATION;
    display_time = true;
  }
}


void printCurrentTimeOrTemperature() {
  if (display_time) {
    printCurrentTime(); // Display Utils
    display_state_duration--;

    if (display_state_duration == 0) {
      temperatureSensor.requestTemperatures();
      display_state_duration = TEMPERATURE_STATE_DURATION;
      display_time = false;
    }
  }
  else
    printCurrentTemperature();
}
