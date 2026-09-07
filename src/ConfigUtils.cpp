// ConfigUtils.cpp
#include "ConfigUtils.h"
#include "CoreUtils.h"


void editAutoBrightness(const char new_value[]) {
  if (auto_brightness != (strcmp(new_value, "true") == 0)) {
    editSettingsFile(new_value, SettingId::AutoBrightness);
    auto_brightness = !auto_brightness;
  }
}


void editDaylightSavingEnabled(const char new_value[]) {
  if (daylight_saving_enabled != (strcmp(new_value, "true") == 0)) {
    editSettingsFile(new_value, SettingId::DaylightSavingEnabled);
    daylight_saving_enabled = !daylight_saving_enabled;
    daylightSavingChange(); // Core Utils
  }
}


void editManualBrightness(const char new_value[]) {
  if (display_brightness != atoi(new_value))
    editSettingsFile(new_value, SettingId::ManualBrightness);

  if (!auto_brightness)
    last_display_brightness = display_brightness;
}


void editSettingsFile(const char new_value[], SettingId id) {
  const SettingTag& tag = SETTING_TAGS[(uint8_t)id];
  char buf[512]; // Enough to keep the entire file content
  File f = LittleFS.open(SETTINGS_FILE, "r");

  if (!f) {
    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[editSettingsFile] Could not open "));
      Serial.print(SETTINGS_FILE);
      Serial.println(F(" for reading"));
    }

    return;
  }

  size_t len = f.read((uint8_t*)buf, sizeof(buf) - 1);
  f.close();
  buf[len] = '\0';

  // Find the end of the opening tag (where the value starts)
  char *value_start = strstr(buf, tag.start);

  if (!value_start) return;

  value_start += strlen(tag.start);

  // Find the start of the closing tag (where the value ends)
  char *value_end = strstr(value_start, tag.end);

  if (!value_end) return;

  f = LittleFS.open(SETTINGS_FILE, "w");

  if (!f) {
    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[editSettingsFile] Could not open "));
      Serial.print(SETTINGS_FILE);
      Serial.println(F(" for writing"));
    }

    return;
  }

  f.write((uint8_t*)buf, value_start - buf);  // Everything before the value
  f.write((uint8_t*)new_value, strlen(new_value));  // The new value
  f.write((uint8_t*)value_end, len - (value_end - buf));  // Closing tag onwards
  f.close();
}


void editTimeSyncMode(const char new_value[]) {
  if constexpr (HAS_GPS_MODULE) {
    if (set_time_with_gps != (strcmp(new_value, "gps") == 0)) {
      editSettingsFile(new_value, SettingId::TimeSyncMode);
      set_time_with_gps = !set_time_with_gps;
    }
  }
  else
    new_value = nullptr; // Prevent unused parameter warning
}


void editTimerDuration(const char new_value[]) {
  int16_t new_duration = atoi(new_value);

  if (new_duration != timer_duration)
    editSettingsFile(new_value, SettingId::TimerDuration);

  timer_millis = millis();
  timer_millis_offset = 0;
  timer_status = 1;
  timer_duration = new_duration;
}

void editTimezoneOffset(const char new_value[]) {
  int16_t new_timezone = atoi(new_value);

  if (new_timezone != timezone) {
    editSettingsFile(new_value, SettingId::TimezoneOffset);
    timezone = new_timezone;
  }
}

void editWorkMode(const char new_value[]) {
  if (work_mode_is_timer != (strcmp(new_value, "timer") == 0)) {
    editSettingsFile(new_value, SettingId::WorkMode);
    work_mode_is_timer = !work_mode_is_timer;
  }
}
