
// _________________________________________________ Light sensitivity module functions _______________________________________________ //

#ifdef  LIGHT_SENSITIVITY_MODULE
int light_sensor_value;

// ------------------------------------- Change the brightness of the clock depending on the light ------------------------------------ //
void autoSetBrightness() {
  if (auto_brightness && blink_count == 0) {
    last_display_brightness = display_brightness;
    light_sensor_value = analogRead(0); // Read from the light sensor

    if (light_sensor_value <= 50)
      display_brightness = 0;
    else if (light_sensor_value > 50 && light_sensor_value <= 500)
      display_brightness = 2;
    else if (light_sensor_value > 500 && light_sensor_value <= 750)
      display_brightness = 4;
    else
      display_brightness = 6;

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("Light sensor value: "));
    Serial.print(light_sensor_value);
    Serial.print(F(", "));
#endif
  }
}
#endif
