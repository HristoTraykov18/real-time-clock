// CoreUtils.cpp
#include "CoreUtils.h"


void applyTimeUpdate(time_t standard_epoch, bool updated_from_gps) {
  const bool dst_applied = daylight_saving_enabled && isDaylightSavingPeriod(standard_epoch);

  if (dst_applied)
    standard_epoch += 3600;

  rtc.adjust(DateTime((uint32_t)standard_epoch));
  daylight_saving_active = dst_applied;
  displayClockJustUpdated(updated_from_gps);

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(updated_from_gps ? F("Time updated from GPS") : F("Time updated from NTP server"));
    Serial.println(dst_applied ? F(" (+1h DST)\n") : F("\n"));
  }
}


bool autoUpdateTime(bool force_update) {
  DateTime now = rtc.now();

  if ((now.hour() == UPDATE_HOUR && now.minute() == 0 && now.second() == 0) || force_update) {
    for (uint8_t i = 0; i < 5; i++) {
      if (updateTime())
        return true;
    }

    if (daylight_saving_enabled && !force_update)
      daylightSavingChange();
  }

  return false;
}


void daylightSavingChange() {
  // Apply +1h (March) or -1h (October) exactly once per transition
  const bool is_daylight_saving_period = daylight_saving_enabled && isDaylightSavingPeriod();

  if (is_daylight_saving_period == daylight_saving_active)
    return;

  DateTime now = rtc.now();

  // Shift through the Unix timestamp so the calendar day, month and year roll over correctly
  rtc.adjust(DateTime(is_daylight_saving_period ? now.unixtime() + 3600UL
                                                : now.unixtime() - 3600UL));

  daylight_saving_active = is_daylight_saving_period;

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("DST offset "));
    Serial.println(is_daylight_saving_period ? F("applied (+1h)") : F("removed (-1h)"));
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
  tm1637.setBrightness(DEFAULT_BRIGHTNESS);
  tm1637.showNumber(8888, 32); // Test all segments

  while (!rtc.begin()) { // If the RTC is not found do not boot
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("\nInitializing RTC"));

    resetRTC(); // Sometimes the RTC becomes unsynchronized while switching power source - reset it
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

    if (daylight_saving_active && hour_now > 0)
       hour_now -= 1;

    if (month_now > 3 && month_now < 10)
      return true;
    else
      last_sunday_date = getLastSundayDate(now);
  }
  else { // Derive date/time from the provided Unix timestamp
    DateTime epoch_dt((uint32_t)epoch_val); // The offset is already contained in the timestamp
    month_now = epoch_dt.month();
    day_now   = epoch_dt.day();
    hour_now  = epoch_dt.hour();

    if (month_now > 3 && month_now < 10)
      return true;
    else
      last_sunday_date = getLastSundayDate(epoch_dt);
  }

  if ((month_now == 3 && (day_now > last_sunday_date || (day_now == last_sunday_date && hour_now >= UPDATE_HOUR))) || // March: DST active after 3:00 of last Sunday
      (month_now == 10 && (day_now < last_sunday_date || (day_now == last_sunday_date && hour_now < UPDATE_HOUR - 1)))) // October: DST active before 2:00 standard time (3:00 displayed) of last Sunday
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

  daylight_saving_active = daylight_saving_enabled && isDaylightSavingPeriod();
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
  digitalWrite(LED_PIN, LOW);
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

  digitalWrite(LED_PIN, HIGH);
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
  if constexpr (HAS_GPS_MODULE) {
    if (set_time_with_gps && gpsState() != GpsState::TimedOut)
      return updateTimeFromGPS(); // GPS module function
  }

  networkReconnect(); // Network Utils
  return updateTimeFromNTP(); // Network Utils
}
