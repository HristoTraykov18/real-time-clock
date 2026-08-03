// CoreUtils.cpp
#include "CoreUtils.h"


bool autoUpdateTime(bool force_update) {
  DateTime now = rtc.now();

  if ((now.hour() == UPDATE_HOUR && now.minute() == 0 && now.second() == 0) || force_update) {
    for (uint8_t i = 0; i < 5; i++) {
      if (updateTime())
        return true;
    }

    if (daylight_saving_enabled && !force_update) {
      DateTime nowDst = rtc.now();
      uint8_t temp_hour = nowDst.hour();
      daylightSavingChange(temp_hour);

      if (nowDst.hour() != temp_hour)
        rtc.adjust(DateTime(nowDst.year(), nowDst.month(), nowDst.day(), temp_hour, nowDst.minute(), nowDst.second()));
    }
  }

  return false;
}


void daylightSavingChange(uint8_t &hour_now) {
  // Apply +1h (March) or -1h (October) exactly once per transition.
  bool is_daylight_saving_period = isDaylightSavingPeriod();

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("DST Active: "));
    Serial.println(daylight_saving_active);
    Serial.print(F("DST Period: "));
    Serial.println(is_daylight_saving_period);
  }

  if (!daylight_saving_active && is_daylight_saving_period) {
    hour_now += 1;
    daylight_saving_active = true;
  }
  else if (daylight_saving_active && !is_daylight_saving_period) {
    hour_now -= 1;
    daylight_saving_active = false;
  }
}


void getInitialClockSettings() {
  char buf[512];
  File f = LittleFS.open("/espSettings.xml", "r");
  size_t len = f.read((uint8_t*)buf, sizeof(buf) - 1);
  f.close();
  buf[len] = '\0';

  uint8_t tagsCount = sizeof(START_TAGS) / sizeof(START_TAGS[0]);
  char val[16]; // Longest stored value is "3600" or "false" - 5 chars; 16 is generous

  for (uint8_t i = 0; i < tagsCount; i++) {
    char *p = strstr(buf, START_TAGS[i]);

    if (!p) continue;

    p += strlen(START_TAGS[i]);

    char *end = strstr(p, END_TAGS[i]);
    if (!end) continue;

    size_t val_len = end - p;

    if (val_len >= sizeof(val)) val_len = sizeof(val) - 1;

    memcpy(val, p, val_len);
    val[val_len] = '\0';

    switch (i) {
      case 0: daylight_saving_enabled = strcmp(val, "true") == 0; break;
      case 1:
        if constexpr (HAS_GPS_MODULE)
          set_time_with_gps = strcmp(val, "gps") == 0;
        break;
      case 2: auto_brightness = strcmp(val, "true") == 0; break;
      case 3:
        display_brightness = atoi(val);

        if (!auto_brightness) last_display_brightness = display_brightness;
        break;
      case 4: timezone = atoi(val); break;
      case 5: work_mode_is_timer = strcmp(val, "timer") == 0; break;
      case 6: timer_duration = atoi(val); break;
    }
  }
}


uint8_t getLastSundayDate(DateTime &now) {
  int8_t day_of_the_week = now.dayOfTheWeek();
  int8_t days_until_sunday = 7 - (day_of_the_week == 0 ? 7 : day_of_the_week);
  int8_t result = 31 - (now.day() + days_until_sunday);

  return 31 - (((result % 7) + 7) % 7); // Return next Sunday date and prevent C99 standard change of modulo operator way of work
}


void initializeFileSystem() {
  while (!LittleFS.begin()) {
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("Failed to initialize file system"));
  }

  getInitialClockSettings();
}


void initializeModuleRTC() {
  tm1637.setBrightness(DEFAULT_BRIGHTNESS); // Set default brightness
  tm1637.showNumber(8888, 32); // Test all segments

  while (!rtc.begin()) { // If the RTC is not found do not boot
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("\nInitializing RTC"));

    resetRTC(); // Sometimes the RTC becomes unsynchronised while switching power source - reset it
  }

  while (rtc.now().hour() > 23 || rtc.now().minute() > 59 || rtc.now().second() > 59) {
    resetRTC();
  }
}


bool isDaylightSavingPeriod(time_t epoch_val) {
  uint8_t month_now, day_now, hour_now, last_sunday_date;

  if (epoch_val == -1) { // Read the current RTC time
    DateTime now = rtc.now();
    month_now = now.month();
    day_now   = now.day();
    hour_now  = now.hour();

    if (month_now > 3 && month_now < 10)
      return true;
    else
      last_sunday_date = getLastSundayDate(now);
  }
  else { // Derive date/time from the provided Unix timestamp
    struct tm *t = localtime(&epoch_val);
    month_now = t->tm_mon + 1; // tm_mon is 0-based
    day_now   = t->tm_mday;
    hour_now  = t->tm_hour;

    if (month_now > 3 && month_now < 10)
      return true;
    else {
      DateTime epochDt(t->tm_year + 1900, month_now, day_now, hour_now, t->tm_min, t->tm_sec);
      last_sunday_date = getLastSundayDate(epochDt);
    }
  }

  if ((month_now == 3 && (day_now > last_sunday_date || (day_now == last_sunday_date && hour_now >= 3))) || // March: DST active after 3:00 of last Sunday
      (month_now == 10 && (day_now < last_sunday_date || (day_now == last_sunday_date && hour_now < 3)))) // October: DST active before 3:00 of last Sunday
    return true;

  return false;
}


void manualTimeUpdate() {
  const String& s = server.arg("currentTime");
  const uint8_t PARAMS_COUNT = 6;
  uint16_t current_time[PARAMS_COUNT] = {0};
  uint8_t param = 0;

  for (size_t pos = 0; pos < s.length() && param < PARAMS_COUNT; pos++) {
    char c = s[pos];

    if (c == ',')
      param++;
    else if (c >= '0' && c <= '9')
      current_time[param] = current_time[param] * 10 + (c - '0');
  }

  rtc.adjust(DateTime(current_time[0], current_time[1] + 1, current_time[2],
                      current_time[3], current_time[4], current_time[5]));

  daylight_saving_active = isDaylightSavingPeriod();
}


void printCurrentTime() {
  DateTime now = rtc.now();
  int h = now.hour(); // Current hour
  int m = now.minute(); // Current minute
  int digits_to_print = ((h / 10) * 1000) + ((h % 10) * 100) + ((m / 10) * 10) + (m % 10);

  if (second_now % 2 == 1)
    tm1637.showNumber(digits_to_print, 32); // tm1637.showNumber(digits_to_print, 32, true); // OSRAM NBG_CLOCK_00001 & NBG_CLOCK_00002 ONLY
  else
    tm1637.showNumber(digits_to_print, 0); // tm1637.showNumber(digits_to_print, 0, true); // OSRAM NBG_CLOCK_00001 & NBG_CLOCK_00002 ONLY

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("Time: "));
    Serial.print(h);
    Serial.print(F(":"));
    Serial.print(m);
    Serial.print(F(":"));
    Serial.print(second_now);
    Serial.print(F(" "));
    Serial.print(now.day());
    Serial.print(F("."));
    Serial.print(now.month());
    Serial.print(F("."));
    Serial.print(now.year());
    Serial.print(F(", Weekday: "));
    Serial.print(now.dayOfTheWeek());
    Serial.print(F(", "));
  }
}


void printRemainingTime() {
  int h = timer_duration / 3600; // Remaining hours
  int m = (timer_duration / 60) % 60; // Remaining minutes
  int s = timer_duration % 60; // Remaining seconds
  int digits_to_print = ((h / 10) * 1000) + ((h % 10) * 100) + ((m / 10) * 10) + (m % 10);

  if (h < 1)
    digits_to_print = ((m / 10) * 1000) + ((m % 10) * 100) + ((s / 10) * 10) + (s % 10);

  if (timer_status == 1) {
    // Colon blinks when timer is running
    if (second_now % 2 == 1)
      tm1637.showNumber(digits_to_print, 32);
    else
      tm1637.showNumber(digits_to_print, 0);
  }
  else {
    tm1637.showNumber(digits_to_print, 32); // Colon stays lit when paused
  }

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("Timer: "));
    Serial.print(h);
    Serial.print(F(":"));
    Serial.print(m);
    Serial.print(F(":"));
    Serial.print(s);
    Serial.print(F(" | Status: "));
    Serial.print(timer_status);
    Serial.print(F(", "));
  }
}


void resetRTC() {
  digitalWrite(LED_PIN, HIGH);
  delay(250);

  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);

  while (SDA_READ() == 0) {
    SDA_HIGH();
    SCL_HIGH();

    if (SDA_READ()) {
      SDA_LOW();
      SDA_HIGH();
    }

    SCL_LOW();
  }

  if constexpr (DEBUG_MESSAGES)
    Serial.println(F("RTC reset"));

  digitalWrite(LED_PIN, LOW);
  delay(250);
}


void timerCountdown() {
  if (timer_status == 1) {
    if (millis() - timer_millis < 1000)
      return;

    timer_millis += 1000;

    if (timer_duration > 0)
      timer_duration--;
    else {
      timer_status = 0; // Stop the timer when it reaches zero

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("Timer finished"));
    }
  }
}


bool updateTime() {
  bool time_updated = false;

  if constexpr (HAS_GPS_MODULE) {
    if (set_time_with_gps) { // Check if time should be updated through GPS module
      if (gps_connect_attempts_left > 0) {
        unsigned long startMillis = millis();

        while (millis() - startMillis < 1000 && gps.satellites.value() == 0) {
          while (gpsSerial.available()) {
            gps.encode(gpsSerial.read());
          }
        }

        if (gps.satellites.value() != 0)
          time_updated = updateTimeFromGPS(gps.date, gps.time); // GPS module function

        if constexpr (DEBUG_MESSAGES) {
          Serial.print(F("Could not get time from GPS. Tries left: "));
          Serial.println(--gps_connect_attempts_left);
        }
      }
      else { // In case of timeout detatchInterrupt and try updating the time from NTP
        detachInterrupt(digitalPinToInterrupt(GPS_RX));
        networkReconnect();
        time_updated = updateTimeFromNTP();
      }
    }
    else { // In case of time update from NTP
      detachInterrupt(digitalPinToInterrupt(GPS_RX));
      networkReconnect();
      time_updated = updateTimeFromNTP();
    }
  }
  else {
    networkReconnect();
    time_updated = updateTimeFromNTP();
  }

  if constexpr (DEBUG_MESSAGES) {
    if (time_updated)
      Serial.println(F("Time updated from NTP server\n"));
    else
      Serial.println(F("\nCould not update time from NTP server\n"));
  }

  return time_updated;
}
