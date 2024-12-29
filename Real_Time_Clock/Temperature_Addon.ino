
// _______________________________________________ Temperature module functions _____________________________________________ //

#ifdef  TEMPERATURE_MODULE
// ---------------------------------------- Print the current temperature to the TM1637 --------------------------------------- //
void printCurrentTemperature() {
  if (display_state_duration == 4) {
    temperatureSensor.requestTemperatures();
    current_temperature = temperatureSensor.getTempCByIndex(0);
  }

  uint8_t temperature[] = {0, 0, 0, 0};

  if (current_temperature == 0) { // Zero case
    temperature[0] = 0;
    temperature[1] = tm1637.encodeDigit(0);
    temperature[2] = (SEG_A | SEG_B | SEG_F | SEG_G); // Degree sign
    temperature[3] = (SEG_A | SEG_D | SEG_E | SEG_F); // Letter C
  }
  else if (current_temperature < 0 && current_temperature > -10) { // Temp less than 0 and more than -10
    temperature[0] = SEG_G;
    temperature[1] = tm1637.encodeDigit(abs(current_temperature) % 10);
    temperature[2] = (SEG_A | SEG_B | SEG_F | SEG_G);
    temperature[3] = (SEG_A | SEG_D | SEG_E | SEG_F);
  }
  else if (current_temperature > 99 || current_temperature <= -79) { // Error message
    temperature[0] = 0b00000000; // Empty sign
    temperature[1] = (SEG_A | SEG_D | SEG_E | SEG_F | SEG_G); // Letter E
    temperature[2] = (SEG_E | SEG_G); // Letter r
    temperature[3] = (SEG_E | SEG_G); // Letter r
  }
  else if (current_temperature <= -10) { // Temp less than -10
    temperature[0] = SEG_G;
    temperature[1] = tm1637.encodeDigit(abs(current_temperature) / 10);
    temperature[2] = tm1637.encodeDigit(abs(current_temperature) % 10);
    temperature[3] = (SEG_A | SEG_B | SEG_F | SEG_G); // Degree sign
  }
  else if (current_temperature > 0 && current_temperature < 10) {
    temperature[0] = 0b00000000; // Empty sign
    temperature[1] = tm1637.encodeDigit(current_temperature % 10);
    temperature[2] = (SEG_A | SEG_B | SEG_F | SEG_G); // Degree sign
    temperature[3] = (SEG_A | SEG_D | SEG_E | SEG_F); // Letter C
  }
  else { // Every other case (10 to 99)
    temperature[0] = tm1637.encodeDigit(current_temperature / 10);
    temperature[1] = tm1637.encodeDigit(current_temperature % 10);
    temperature[2] = (SEG_A | SEG_B | SEG_F | SEG_G); // Degree sign
    temperature[3] = (SEG_A | SEG_D | SEG_E | SEG_F); // letter C
  }

  tm1637.setSegments(temperature);
  last_second = second_now;
  display_state_duration--;

  if (display_state_duration == 0) {
    display_state_duration = 7;
    display_time = true;
  }
}

// ------------------------------------- Print the current time or temperature to the TM1637 ------------------------------------- //
void printCurrentTimeOrTemperature() {
  // Get actual time, update dots, flash if someone connects once per second to save CPU power
  if (display_time) {
    printCurrentTime();
    display_state_duration--;

    if (display_state_duration == 0) {
      display_state_duration = 4;
      display_time = false;
    }
  }
  else
    printCurrentTemperature(); // Temperature module function
}
#endif
