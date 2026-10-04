// CoreUtils.cpp
#include "CoreUtils.h"
#include "EspUtils.h"


void applyTimeUpdate(time_t standard_epoch, bool updated_from_gps) {
  const bool dst_applied = daylight_saving_enabled && isDaylightSavingPeriod(standard_epoch);

  if (dst_applied)
    standard_epoch += 3600;

  rtc.adjust(DateTime((uint32_t)standard_epoch));
  daylight_saving_active = dst_applied;
  displayClockJustUpdated(updated_from_gps);

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("[applyTimeUpdate] "));
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
    Serial.print(F("[daylightSavingChange] DST offset "));
    Serial.println(is_daylight_saving_period ? F("applied (+1h)") : F("removed (-1h)"));
  }
}


void getInitialClockSettings() {
  char buf[512];
  File f = LittleFS.open(SETTINGS_FILE, "r");

  if (!f) {
    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[getInitialClockSettings] Could not open "));
      Serial.print(SETTINGS_FILE);
      Serial.println(F(" for reading"));
    }

    return;
  }

  size_t len = f.read((uint8_t*)buf, sizeof(buf) - 1);
  f.close();
  buf[len] = '\0';

  char val[16]; // Longest stored value is "3600" or "false" - 5 chars; 16 is generous

  for (uint8_t i = 0; i < (uint8_t)SettingId::Count; i++) {
    const SettingTag& tag = SETTING_TAGS[i];
    char *p = strstr(buf, tag.start);

    if (!p) continue;

    p += strlen(tag.start);

    char *end = strstr(p, tag.end);
    if (!end) continue;

    size_t val_len = end - p;

    if (val_len >= sizeof(val)) val_len = sizeof(val) - 1;

    memcpy(val, p, val_len);
    val[val_len] = '\0';

    switch ((SettingId)i) {
      case SettingId::DaylightSavingEnabled: daylight_saving_enabled = strcmp(val, "true") == 0; break;
      case SettingId::TimeSyncMode:
        if constexpr (HAS_GPS_MODULE)
          set_time_with_gps = strcmp(val, "gps") == 0;
        break;
      case SettingId::AutoBrightness: auto_brightness = strcmp(val, "true") == 0; break;
      case SettingId::ManualBrightness:
        display_brightness = atoi(val);

        if (!auto_brightness) last_display_brightness = display_brightness;
        break;
      case SettingId::TimezoneOffset: timezone_offset = atoi(val); break;
      case SettingId::WorkMode: work_mode_is_timer = strcmp(val, "timer") == 0; break;
      case SettingId::TimerDuration: timer_duration = atoi(val); break;
      case SettingId::Count: break; // Prevent missing enum cases warning
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
  while (!espFsBegin()) { // Esp Utils
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("[initializeFileSystem] Failed to initialize file system"));
  }

  getInitialClockSettings();
}


void initializePeripherals() {
  tm1637.setBrightness(DEFAULT_BRIGHTNESS);
  tm1637.showNumber(8888, 32);

  while (!rtc.begin()) // If the RTC is not found do not boot
    resetRTC(); // The RTC may unsynchronize while switching power source - reset it

  while (rtc.now().hour() > 23 || rtc.now().minute() > 59 || rtc.now().second() > 59)
    resetRTC();

  if constexpr (DEBUG_MESSAGES)
    Serial.println(F("\n[initializePeripherals] RTC initialized"));

  if constexpr (HAS_TEMPERATURE_MODULE)
    initializeTemperatureSensor(); // Temperature module function
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
    Serial.println(F("[resetRTC] RTC reset"));

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
        Serial.println(F("[timerCountdown] Timer finished"));
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
