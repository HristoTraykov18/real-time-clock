
// ---------------------------------------------------- Update time from GPS --------------------------------------------------- //
void UpdateTimeFromGPS(TinyGPSDate &d, TinyGPSTime &t) {
  altitudeInMeters = gps.altitude.meters();
  longtitude = gps.location.lng();
  latitude = gps.location.lat();
  if (longtitude != 0.000000 || latitude != 0.000000) {
    Serial.print(F("Satellite Count: "));
    Serial.println(gps.satellites.value());
    Serial.print(F("Latitude: "));
    Serial.println(latitude);
    Serial.print(F("Longitude: "));
    Serial.println(longtitude);
    Serial.print(F("Altitude Meters: "));
    Serial.println(altitudeInMeters);
    
    int yearNow = d.year();
    int monthNow = d.month();
    int dayNow = d.day();
    int hourNow = t.hour();
    int minuteNow = t.minute();
    int secondNow = t.second();

    if (daylightSaving) {                  // If daylight saving time is ON
      if (rtc.now().day() != dayNow)
        rtc.adjust(DateTime(yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow)); // Write new date to RTC to get proper dayOfTheWeek()
      DaylightSavingChange(hourNow);   // Change time accordingly
    }
    else
      hourNow += timezone; // Otherwise use the respective timezone
    
    hourNow %= 24;
    rtc.adjust(DateTime(yearNow, monthNow, dayNow, hourNow, minuteNow, secondNow)); // Write the new time to the RTC memory
    Serial.println(F("Time updated from GPS\n"));
    DisplayClockJustUpdated(true);
    GPS_connectionTriesLeft = 0;
    timeUpdatePending = false;
  }
}
