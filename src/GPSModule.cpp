// GPSModule.cpp
#include "GPSModule.h"


void activateGPS() {
  gps_connect_attempts_left = 180;
  gpsSerial.begin(GPS_BAUD_RATE);
}


bool updateTimeFromGPS(TinyGPSDate &d, TinyGPSTime &t) {
  double altitude_meters = gps.altitude.meters();
  double longitude = gps.location.lng();
  double latitude = gps.location.lat();

  if (longitude != 0.000000 || latitude != 0.000000) {
    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("Satellite Count: "));
      Serial.println(gps.satellites.value());
      Serial.print(F("Latitude: "));
      Serial.println(latitude);
      Serial.print(F("Longitude: "));
      Serial.println(longitude);
      Serial.print(F("Altitude Meters: "));
      Serial.println(altitude_meters);
    }

    int current_year = d.year();
    int current_month = d.month();
    int current_day = d.day();
    int current_hour = t.hour();
    int current_minute = t.minute();
    int current_second = t.second();

    current_hour += timezone;
    gps_connect_attempts_left = 0;

    rtc.adjust(DateTime(current_year, current_month, current_day, current_hour, current_minute, current_second));
    displayClockJustUpdated(true);

    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("Time updated from GPS\n"));

    return true;
  }

  return false;
}
