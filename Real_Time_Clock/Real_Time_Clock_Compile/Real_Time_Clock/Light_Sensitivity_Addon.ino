
// ------------------------------------- Change the brightness of the clock depending on the light ------------------------------------ //
void AutoBrightness() {
  if (hasLightSensor && autoBrightness && flashesOnConnect == 0) {
    lastDisplayBrightness = displayBrightness;
    lightSensorValue = analogRead(0); // Read from the light sensor
    
    if (lightSensorValue <= 50)
      displayBrightness = 0;
    else if (lightSensorValue > 50 && lightSensorValue <= 600)
      displayBrightness = 2;
    else
      displayBrightness = 4;
  }
  FlashDisplay();
}
