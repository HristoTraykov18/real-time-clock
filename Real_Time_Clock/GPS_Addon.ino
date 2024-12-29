
// _____________________________________________________ GPS addon functions ___________________________________________________ //

#ifdef  GPS_MODULE
// ------------------------------------- Activate the GPS module if such exists in the clock ------------------------------------- //
void activateGPS() {
  gps_connect_attempts_left = 180;
  gpsSerial.begin(GPS_BAUD_RATE);
}

// ---------------------------------------------------- Update time from GPS --------------------------------------------------- //
bool updateTimeFromGPS(TinyGPSDate &d, TinyGPSTime &t) {
  altitude_meters = gps.altitude.meters();
  longtitude = gps.location.lng();
  latitude = gps.location.lat();

  if (longtitude != 0.000000 || latitude != 0.000000) {
#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("Satellite Count: "));
    Serial.println(gps.satellites.value());
    Serial.print(F("Latitude: "));
    Serial.println(latitude);
    Serial.print(F("Longitude: "));
    Serial.println(longtitude);
    Serial.print(F("Altitude Meters: "));
    Serial.println(altitude_meters);
#endif

    int current_year = d.year();
    int current_month = d.month();
    int current_day = d.day();
    int current_hour = t.hour();
    int current_minute = t.minute();
    int current_second = t.second();

    current_hour += timezone;
    time_update_pending = false;
    gps_connect_attempts_left = 0;

    rtc.adjust(DateTime(current_year, current_month, current_day, current_hour, current_minute, current_second));
    displayClockJustUpdated(true);

#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("Time updated from GPS\n"));
#endif
    return true;
  }

  return false;
}
#endif
