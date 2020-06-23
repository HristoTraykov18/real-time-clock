
// ---------------------------------------- Print the current temperature to the TM1637 --------------------------------------- //
void PrintCurrentTemperature() {
  if (showDuration == 4) {
    sensor.requestTemperatures();
    currentTemp = sensor.getTempCByIndex(0);
  }
  
  uint8_t temperature[] = {0, 0, 0, 0};
  
  if (currentTemp == 0) { // Zero case
    temperature[0] = 0;
    temperature[1] = tm1637.encodeDigit(0);
    temperature[2] = (SEG_A | SEG_B | SEG_F | SEG_G); // Degree sign
    temperature[3] = (SEG_A | SEG_D | SEG_E | SEG_F); // Letter C
  }
  else if (currentTemp < 0 && currentTemp > -10) { // Temp less than 0 and more than -10
    temperature[0] = SEG_G;
    temperature[1] = tm1637.encodeDigit(abs(currentTemp) % 10);
    temperature[2] = (SEG_A | SEG_B | SEG_F | SEG_G);
    temperature[3] = (SEG_A | SEG_D | SEG_E | SEG_F);
  }
  else if (currentTemp > 99 || currentTemp <= -79) {  // Error message
    temperature[0] = 0b00000000; // Empty sign
    temperature[1] = (SEG_A | SEG_D | SEG_E | SEG_F | SEG_G); // Letter E
    temperature[2] = (SEG_E | SEG_G); // Letter r
    temperature[3] = (SEG_E | SEG_G); // Letter r
  }
  else if (currentTemp <= -10) {  // Temp less than -10
    temperature[0] = SEG_G;
    temperature[1] = tm1637.encodeDigit(abs(currentTemp) / 10);
    temperature[2] = tm1637.encodeDigit(abs(currentTemp) % 10);
    temperature[3] = (SEG_A | SEG_B | SEG_F | SEG_G); // Degree sign
  }
  else if (currentTemp > 0 && currentTemp < 10) {
    temperature[0] = 0b00000000; // Empty sign
    temperature[1] = tm1637.encodeDigit(currentTemp % 10);
    temperature[2] = (SEG_A | SEG_B | SEG_F | SEG_G); // Degree sign
    temperature[3] = (SEG_A | SEG_D | SEG_E | SEG_F); // Letter C
  }
  else {  // Every other case (10 to 99)
    temperature[0] = tm1637.encodeDigit(currentTemp / 10);
    temperature[1] = tm1637.encodeDigit(currentTemp % 10);
    temperature[2] = (SEG_A | SEG_B | SEG_F | SEG_G); // Degree sign
    temperature[3] = (SEG_A | SEG_D | SEG_E | SEG_F); // letter C
  }
  
  tm1637.setSegments(temperature);
  lastSecond = s;
  showDuration--;
  
  if (showDuration == 0) {
    showDuration = 7;
    showTime = true;
  }
}
