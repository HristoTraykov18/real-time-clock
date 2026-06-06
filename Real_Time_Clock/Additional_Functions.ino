
// ___________________________________________________ Additional functions ___________________________________________________ //

// -------------------------------------- Try to update the time from NTP/GPS up to 5 times -------------------------------------- //
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

// ----------------------------------- Check if someone has connected to the ESP's network ----------------------------------- //
void checkForUserConnection() {
  bool counted = WiFi.softAPgetStationNum() > 0;
  bool http_live = (millis() - last_http_activity_ms) < AP_CONNECTION_TIMEOUT;
  bool present = ap_station_associated && counted && http_live;

  if (present && !active_connection) {
    someone_just_connected = true;
    active_connection = true;
  }
  else if (!present && active_connection) {
    someone_just_connected = false;
    active_connection = false;

    if (software_update_server_active) {
      softwareUpdateServer.stop();
      software_update_server_active = false;

#ifdef  RTC_INFO_MESSAGES
      Serial.println(F("OTA update server stopped"));
#endif
    }

    if (counted)
      evictStaleStations();
  }
}

// ------------------------------------------ Try to establish network connection ------------------------------------------ //
wl_status_t connectClockToNetwork(const char* ssid, const char* pass, bool is_hidden,
                                  uint8_t channel = 0, const uint8_t* bssid = nullptr) {
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED && currentSsidEquals(ssid)) {
#ifdef  RTC_INFO_MESSAGES
    char ssid_buf[33]; // Max SSID len (32) + NUL = 33
    currentSsid(ssid_buf);
    Serial.print(F("Already connected to "));
    Serial.println(ssid_buf);
#endif

    return status;
  }

  WiFi.begin(ssid, pass, channel, bssid);
  yield();

#ifdef  RTC_INFO_MESSAGES
  Serial.print(F("Trying to connect to "));
  Serial.print(ssid);

  if (bssid) {
      Serial.print(F(" | Saved MAC: "));
      Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
      Serial.print(F(" (fast-connect)"));
  }

  Serial.println();
#endif

  const uint16_t CONNECT_ATTEMPT_DELAY = bssid ? 80 : 130;

  for (uint16_t i = 0; i < CONNECT_ATTEMPT_DELAY; i++) {
    status = WiFi.status();

    if (status == WL_CONNECTED) {
      saveNetworkInfo(ssid, pass, is_hidden ? "true" : "false", WiFi.channel(), WiFi.BSSID());

#ifdef  RTC_INFO_MESSAGES
      Serial.print(F("\nConnected to "));
      Serial.println(ssid);
#endif

      break;
    }

    // Unrecoverable statuses
    if (status == WL_WRONG_PASSWORD || status == WL_NO_SSID_AVAIL) {
#ifdef  RTC_INFO_MESSAGES
      Serial.print(F("\nConnection attempt stopped. Wi-Fi status "));
      Serial.println(status);
#endif

      break;
    }

#ifdef  RTC_INFO_MESSAGES
    if (i % 10 == 0)
      Serial.print(F("."));
#endif

    delay(CONNECT_ATTEMPT_DELAY);
    yield();
  }

#ifdef  RTC_INFO_MESSAGES
  Serial.println();
#endif
  return status;
}

// ----------------------------------- Fill caller's buffer with current SSID (no heap alloc) ----------------------------------- //
void currentSsid(char* buf) {
  struct station_config conf;
  wifi_station_get_config(&conf);
  memcpy(buf, conf.ssid, 32);
  buf[32] = '\0';
}

// -------------------------------- Compare current connected SSID against target (no heap alloc) -------------------------------- //
bool currentSsidEquals(const char* target) {
  if (!target)
    return false;

  size_t target_len = strlen(target);

  if (target_len > 32)
    return false;

  struct station_config conf;
  wifi_station_get_config(&conf);

  if (memcmp(conf.ssid, target, target_len) != 0)
    return false;

  return target_len == 32 || conf.ssid[target_len] == 0;
}

// ----------------------------------- Change time if needed depending on daylight saving time ----------------------------------- //
// Called only when NTP failed. Applies +1h (March) or -1h (October) exactly once per transition.
void daylightSavingChange(uint8_t &hour_now) {
  bool is_daylight_saving_period = isDaylightSavingPeriod();

#ifdef  RTC_INFO_MESSAGES
  Serial.print(F("DST Active: "));
  Serial.println(daylight_saving_active);
  Serial.print(F("DST Period: "));
  Serial.println(is_daylight_saving_period);
#endif

  if (!daylight_saving_active && is_daylight_saving_period) {
    hour_now += 1;
    daylight_saving_active = true;
  }
  else if (daylight_saving_active && !is_daylight_saving_period) {
    hour_now -= 1;
    daylight_saving_active = false;
  }
}

// ----------------------------------- Display on the TM1637 that the clock just updated time ----------------------------------- //
void displayClockJustUpdated(bool updated_from_gps) {
  // Effect when the clock time is set
  const uint8_t ANIMATION_LENGTH = 9;
  const uint8_t TIME_SET_ANIMATION[ANIMATION_LENGTH][4] = {
    {(SEG_E | SEG_F), 0, 0, 0 }, {(SEG_A | SEG_B | SEG_C | SEG_D), 0, 0, 0},
    {(SEG_A | SEG_D), (SEG_E | SEG_F), 0, 0}, {(SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D), 0, 0},
    {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_E | SEG_F), 0}, {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D), 0},
    {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_E | SEG_F)}, {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D)},
    {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_D)}
  };

  // Animate effect
  if (updated_from_gps) {
    for (int j = ANIMATION_LENGTH - 1; j > -1; j--) {
      tm1637.setSegments(TIME_SET_ANIMATION[j]);
      delay(75);
    }
  }
  else {
    for (int j = 0; j < ANIMATION_LENGTH; j++) {
      tm1637.setSegments(TIME_SET_ANIMATION[j]);
      delay(75);
    }
  }
}

// --------------------------------------------- Edit settings file with user input --------------------------------------------- //
void editAutoBrightness(const char new_value[]) {
  if (auto_brightness != (strcmp(new_value, "true") == 0)) {
    editSettingsFile(new_value, 2);
    auto_brightness = !auto_brightness;
  }
}

void editDaylightSavingEnabled(const char new_value[]) {
  if (daylight_saving_enabled != (strcmp(new_value, "true") == 0)) {
    editSettingsFile(new_value, 0);
    daylight_saving_enabled = !daylight_saving_enabled;
  }
}

void editManualBrightness(const char new_value[]) {
  editSettingsFile(new_value, 3);

  if (!auto_brightness)
    last_display_brightness = display_brightness;
}

void editSettingsFile(const char new_value[], uint8_t tags_id) {
  char buf[512]; // Enough to keep the entire file content
  File f = LittleFS.open("/espSettings.xml", "r");
  size_t len = f.read((uint8_t*)buf, sizeof(buf) - 1);
  f.close();
  buf[len] = '\0';

  // Find the end of the opening tag (where the value starts)
  char *value_start = strstr(buf, START_TAGS[tags_id]);

  if (!value_start) return;

  value_start += strlen(START_TAGS[tags_id]);

  // Find the start of the closing tag (where the value ends)
  char *value_end = strstr(value_start, END_TAGS[tags_id]);

  if (!value_end) return;

  f = LittleFS.open("/espSettings.xml", "w");
  f.write((uint8_t*)buf, value_start - buf);  // Everything before the value
  f.write((uint8_t*)new_value, strlen(new_value));  // The new value
  f.write((uint8_t*)value_end, len - (value_end - buf));  // Closing tag onwards
  f.close();
}

void editTimeSyncMode(const char new_value[]) {
#ifdef  GPS_MODULE
  if (set_time_with_gps != (strcmp(new_value, "gps") == 0)) {
    editSettingsFile(new_value, 1);
    set_time_with_gps = !set_time_with_gps;
  }
#endif
}

void editTimerDuration(const char new_value[]) {
  int16_t new_duration = atoi(new_value);

  if (new_duration != timer_duration)
    editSettingsFile(new_value, 6);

  timer_millis = millis();
  timer_millis_offset = 0;
  timer_status = 1;
  timer_duration = new_duration;
}

void editTimezoneOffset(const char new_value[]) {
  int16_t new_timezone = atoi(new_value);

  if (new_timezone != timezone) {
    editSettingsFile(new_value, 4);
    timezone = new_timezone;
  }
}

void editWorkMode(const char new_value[]) {
  if (work_mode_is_timer != (strcmp(new_value, "timer") == 0)) {
    editSettingsFile(new_value, 5);
    work_mode_is_timer = !work_mode_is_timer;
  }
}

// ----------------------------- Evict stations that are still associated, but not really present ----------------------------- //
void evictStaleStations() {
  struct station_info *si = wifi_softap_get_station_info();

  while (si != NULL) {
    char mac_addr[18];
    snprintf(mac_addr, sizeof(mac_addr), "%02X:%02X:%02X:%02X:%02X:%02X",
            si->bssid[0], si->bssid[1], si->bssid[2],
            si->bssid[3], si->bssid[4], si->bssid[5]);
    wifi_softap_deauth(si->bssid);
    si = STAILQ_NEXT(si, next);

#ifdef  RTC_INFO_MESSAGES
    Serial.print(mac_addr);
    Serial.println(F(" disconnected from softAP due to inactivity"));
#endif
  }

  wifi_softap_free_station_info();
}

// ---------------------------------- Validate cached BSSID availability on cached channel ---------------------------------- //
bool fastConnectCacheIsValid(uint8_t channel, const uint8_t* bssid) {
  if (!channel || !bssid)
    return false;

  for (uint8_t scan_attempts = 5; scan_attempts > 0; scan_attempts--) {
    int8_t number_of_networks = WiFi.scanNetworks(false, true, channel);

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("scanNetworks: "));
    Serial.println(number_of_networks);
#endif

    if (number_of_networks > 0) {
      bool is_valid = false;

      for (uint8_t i = 0; i < number_of_networks; i++) {
        const bss_info* info = WiFi.getScanInfoByIndex(i);

        if (info && memcmp(info->bssid, bssid, 6) == 0) {
          is_valid = true;

          break;
        }
      }

      WiFi.scanDelete();
      yield();

      return is_valid;
    }

    WiFi.scanDelete();
    delay(500);
  }

  return false;
}

// --------------------- Flash the display if someone connects to the ESP or if it connects to NTP server --------------------- //
void flashDisplay() {
  if (someone_just_connected && blink_count == 0) {
    blink_count = 6;
    last_auto_brightness = auto_brightness;
    last_display_brightness = display_brightness;
    auto_brightness = false;
  }

  if (blink_count > 0) {
    if (display_brightness == last_display_brightness) {
      if (connected_to_ntp)
        display_brightness = display_brightness != 6 ? 6 : 0;
      else
        display_brightness = display_brightness != 0 ? 0 : 6;
    }
    else
      display_brightness = last_display_brightness;

    blink_count--;

    if (blink_count == 0) {
      someone_just_connected = false;
      auto_brightness = last_auto_brightness;
    }

    if (!someone_just_connected && !connected_to_ntp) {
      blink_count = 0;
      auto_brightness = last_auto_brightness;
      display_brightness = last_display_brightness;
    }
  }

  tm1637.setBrightness(display_brightness);
}

// ----------------------------------------- Get the last Sunday date of the month ----------------------------------------- //
uint8_t getLastSundayDate(DateTime &now) {
  int8_t day_of_the_week = now.dayOfTheWeek();
  int8_t days_until_sunday = 7 - (day_of_the_week == 0 ? 7 : day_of_the_week);
  int8_t result = 31 - (now.day() + days_until_sunday);

  return 31 - (((result % 7) + 7) % 7); // Return next Sunday date and prevent C99 standard change of modulo operator way of work
}

// ---------------------------------- Get length of the packet received from the NTP server ---------------------------------- //
int getNTP_PacketLength(IPAddress& address) {
  sendNTP_Packet(address);

  unsigned long start_millis = millis();
  unsigned long last_millis = start_millis;
  unsigned long packet_length = 0;

  while (millis() - start_millis < 1500 && packet_length == 0) {
    packet_length = udp.parsePacket();

    if (millis() != last_millis && ((millis() - start_millis) % 100) == 0) {
#ifdef  RTC_INFO_MESSAGES
      Serial.print(F("."));
#endif

      last_millis = millis();
    }
  }

  return packet_length;
}

// --------------------------------------------- Check if it's daylight saving period ---------------------------------------------- //
// Called with no argument: reads the current RTC time (used everywhere except NTP update).
// Called with an epoch argument: derives date/time from that Unix timestamp instead of from the RTC.
bool isDaylightSavingPeriod(time_t epoch_val) {
  uint8_t month_now, day_now, hour_now, last_sunday_date;

  if (epoch_val == -1) {
    DateTime now = rtc.now();
    month_now = now.month();
    day_now   = now.day();
    hour_now  = now.hour();

    if (month_now > 3 && month_now < 10)
      return true;
    else
      last_sunday_date = getLastSundayDate(now);
  }
  else {
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

// ------------------------------------------ Read the stored network information from creds.txt ------------------------------------------ //
bool loadNetworkInfo(char* buf, size_t buf_size, const char* fields[5]) {
  for (uint8_t i = 0; i < 5; i++)
    fields[i] = nullptr;

  if (!LittleFS.exists("creds.txt")) return false;

  File f = LittleFS.open("creds.txt", "r");

  if (!f) return false;

  int n = f.read((uint8_t*)buf, buf_size - 1);
  f.close();

  if (n <= 0) return false;

  buf[n] = '\0';

  // Walk buf, replacing '\n' with '\0' to terminate each field in place.
  fields[0] = buf;
  uint8_t j = 1;

  for (int i = 0; i < n && j < 5; i++) {
    if (buf[i] == '\n') {
      buf[i] = '\0';
      fields[j++] = &buf[i + 1];
    }
  }

  return true;
}

// ----------------------------------- Manage reconnection after the ESP disconnects from a known network ----------------------------------- //
void manageStaReconnect() {
  static uint8_t retry_count = 2; // Attempt only 1 reconnect on boot, thus the value is 2
  static bool back_off_active = false;
  static uint8_t back_off_hour = 0;

  if (!LittleFS.exists("creds.txt") || 
      ap_station_associated || 
      millis() < radio_settle_until_ms) 
      return;

  if (WiFi.status() == WL_CONNECTED) {
    if (back_off_active) {
      back_off_active = false;

#ifdef  RTC_INFO_MESSAGES
      Serial.println(F("Back off disabled "));
#endif
    }

    retry_count = 0;
    return;
  }

  if (back_off_active) {
    if (rtc.now().hour() != back_off_hour) {
      back_off_active = false;
      retry_count = 0;
    }

    return;
  }

  if (retry_count < 3) {
    if (networkReconnect() == WL_CONNECTED) {
      autoUpdateTime(true);
      retry_count = 0;
    }
    else if (++retry_count >= 3) {
      back_off_active = true;
      back_off_hour = rtc.now().hour();

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("Back off enabled for "));
    Serial.print(back_off_hour + 1);
    Serial.println(F(":00"));
#endif
    }
  }
}

// --------------------------------------- Update the time manually from the user's device ---------------------------------------- //
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

// --------------------------------------- Check if a requested network is in range --------------------------------------- //
bool networkIsInRange(const char* ssid) {
  if (!ssid)
    return false;

  const size_t ssid_len = strlen(ssid);

  if (ssid_len == 0 || ssid_len > 32)
    return false;

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("Requested: "));
    Serial.println(ssid);
#endif

  int8_t number_of_networks = WiFi.scanNetworks(false, true);
  
  bool in_range = false;

  for (uint8_t i = 0; i < number_of_networks; i++) {
    const bss_info* info = WiFi.getScanInfoByIndex(i);

    if (!info)
      continue;

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("In range: "));

    for (uint8_t k = 0; k < 32 && info->ssid[k]; k++)
      Serial.write(info->ssid[k]);

    Serial.print(F(" | Channel: "));
    Serial.println(info->channel);
#endif

    if (memcmp(info->ssid, ssid, ssid_len) == 0 && (ssid_len == 32 || info->ssid[ssid_len] == 0)) {
      in_range = true;

      break;
    }
  }

  WiFi.scanDelete();
  yield();

  return in_range;
}

// ---------------------------------------- Attempt reconnecting to saved network ---------------------------------------- //
wl_status_t networkReconnect() {
  wl_status_t connection_status = WiFi.status();

  if (connection_status != WL_CONNECTED) {
    char buf[128]; // SSID (32) + pass (64) + is_hidden (5) + channel (2) + BSSID (17) + 4 linesep = 120
    const char* network_data[5];

    if (!loadNetworkInfo(buf, sizeof(buf), network_data)) {
#ifdef  RTC_INFO_MESSAGES
      Serial.println(F("No creds.txt file"));
#endif

      return connection_status;
    }

    int32_t channel = 0;
    uint8_t bssid[6];
    const uint8_t* bssid_ptr = nullptr;

    if (network_data[3] && network_data[4] && parseBssid(network_data[4], bssid)) {
      channel = atoi(network_data[3]);
      bssid_ptr = bssid;
    }

    bool is_hidden = strcmp(network_data[2], "true") == 0;

    if (bssid_ptr && fastConnectCacheIsValid(channel, bssid_ptr))
      connection_status = connectClockToNetwork(network_data[0], network_data[1], is_hidden, channel, bssid_ptr);

    if (connection_status != WL_CONNECTED) {
#ifdef  RTC_INFO_MESSAGES
      Serial.println(F("Running full Wi-Fi scan"));
#endif

      connection_status = connectClockToNetwork(network_data[0], network_data[1], is_hidden);
    }
  }

  return connection_status;
}

// ------------------------------------------------------ Parse MAC address ------------------------------------------------------ //
bool parseBssid(const char* s, uint8_t* bssid) {
  if (!s || strlen(s) != 17) // "XX:XX:XX:XX:XX:XX"
    return false;

  auto hexVal = [](char c) -> int8_t {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  };

  for (uint8_t i = 0; i < 6; i++) {
    int8_t h = hexVal(s[i * 3]);
    int8_t l = hexVal(s[i * 3 + 1]);

    if (h < 0 || l < 0)
      return false;

    bssid[i] = (h << 4) | l;
  }

  return true;
}

// --------------------------------------------- Print the current time to the TM1637 --------------------------------------------- //
void printCurrentTime() {
  DateTime now = rtc.now();
  int h = now.hour(); // Current hour
  int m = now.minute(); // Current minute
  int digits_to_print = ((h / 10) * 1000) + ((h % 10) * 100) + ((m / 10) * 10) + (m % 10);

  if (second_now % 2 == 1)
    tm1637.showNumber(digits_to_print, 32); // tm1637.showNumber(digits_to_print, 32, true); // OSRAM NBG_CLOCK_00001 & NBG_CLOCK_00002 ONLY
  else
    tm1637.showNumber(digits_to_print, 0); // tm1637.showNumber(digits_to_print, 0, true); // OSRAM NBG_CLOCK_00001 & NBG_CLOCK_00002 ONLY

#ifdef  RTC_INFO_MESSAGES
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
#endif
}

// ------------------------------------------ Print the remaining timer time to the TM1637 ------------------------------------------ //
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
    tm1637.showNumber(digits_to_print, 32); // Colon stays solid when paused
  }

#ifdef  RTC_INFO_MESSAGES
  Serial.print(F("Timer: "));
  Serial.print(h);
  Serial.print(F(":"));
  Serial.print(m);
  Serial.print(F(":"));
  Serial.print(s);
  Serial.print(F(" | Status: "));
  Serial.print(timer_status);
  Serial.print(F(", "));
#endif
}

// --------------------------------------------- Resets the Real-Time Clock module --------------------------------------------- //
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

#ifdef  RTC_INFO_MESSAGES
  Serial.println(F("RTC reset"));
#endif

  digitalWrite(LED_PIN, LOW);
  delay(250);
}

// ----------------------------------------------- Save new network information ----------------------------------------------- //
void saveNetworkInfo(const char *network_name, const char* network_pass, const char* is_hidden,
                     uint8_t channel, const uint8_t* bssid) {

  char buf[128]; // SSID (32) + pass (64) + is_hidden (5) + channel (2) + BSSID (17) + 4 linesep = 124
  int n = snprintf(buf, sizeof(buf),
                   "%s\n%s\n%s\n%u\n%02X:%02X:%02X:%02X:%02X:%02X",
                   network_name, network_pass, is_hidden, channel,
                   bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

  if (n <= 0 || n >= (int)sizeof(buf)) {
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("\nSave Network Info: snprintf overflow"));
#endif
    return;
  }

  File f = LittleFS.open("creds.txt", "w");

  if (!f) {
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("\nSave Network Info: Failed to open creds.txt"));
#endif
    return;
  }

  f.write((const uint8_t*)buf, (size_t)n);
  f.close();
}

// ----------------------------------------------- NTP packet sending function ----------------------------------------------- //
void sendNTP_Packet(IPAddress& address) {
#ifdef  RTC_INFO_MESSAGES
  Serial.println(F("Preparing NTP packet"));
#endif

  const uint8_t NTP_PACKET_SIZE = 48;
  byte packet_buffer[NTP_PACKET_SIZE];

  // Initialize values needed to form NTP request (see URL above for details on the packets)
  packet_buffer[0] = 0b11100011; // LI, Version, Mode
  packet_buffer[1] = 0; // Stratum, or type of clock
  packet_buffer[2] = 6; // Polling Interval
  packet_buffer[3] = 0xEC; // Peer Clock Precision
  packet_buffer[12]  = 49;
  packet_buffer[13]  = 0x4E;
  packet_buffer[14]  = 49;
  packet_buffer[15]  = 52;

  // All NTP fields have values, send a packet requesting a timestamp
  udp.beginPacket(address, 123); // NTP requests are to port 123
  udp.write(packet_buffer, NTP_PACKET_SIZE);
  udp.endPacket();

#ifdef  RTC_INFO_MESSAGES
  Serial.print(F("NTP packet sent. Waiting response"));
#endif
}

// ---------------------------------- Countdown one second and handle timer expiry ---------------------------------- //
void timerCountdown() {
  if (timer_status == 1) {
    if (millis() - timer_millis < 1000)
      return;

    timer_millis += 1000;

    if (timer_duration > 0)
      timer_duration--;
    else {
      timer_status = 0; // Stop the timer when it reaches zero

#ifdef  RTC_INFO_MESSAGES
      Serial.println(F("Timer finished"));
#endif
    }
  }
}

// ------------------------------------------ Determine which update function to call --------------------------------------- //
bool updateTime() { // Check if it's the right time to update the time or if time update is requested
  bool time_updated = false;

#ifdef  GPS_MODULE
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

#ifdef  RTC_INFO_MESSAGES
      Serial.print(F("Could not get time from GPS. Tries left: "));
      Serial.println(--gps_connect_attempts_left);
#endif
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
#else
  networkReconnect();
  time_updated = updateTimeFromNTP();
#endif

#ifdef  RTC_INFO_MESSAGES
  if (time_updated)
    Serial.println(F("Time updated from NTP server\n"));
  else
    Serial.println(F("\nCould not update time from NTP server\n"));
#endif

  return time_updated;
}

// ----------------------------------------- Update time from Network Time Protocol server --------------------------------------- //
bool updateTimeFromNTP() {
  if (!WiFi.hostByName(EU_NTP_SERVER_1, time_server_ip)) {
#ifdef RTC_INFO_MESSAGES
  Serial.println(F("NTP DNS resolution failed"));
#endif

    return false;
  }

  while (udp.parsePacket() > 0)
    while (udp.available()) udp.read();

  if (!getNTP_PacketLength(time_server_ip)) return false;

  if (udp.remoteIP() != time_server_ip) {
#ifdef RTC_INFO_MESSAGES
    Serial.println(F("NTP response from unexpected source discarded"));
#endif
    return false;
  }

#ifdef  RTC_INFO_MESSAGES
  Serial.println(F("\nNTP response received"));
#endif

  const uint8_t NTP_PACKET_SIZE = 48;
  byte packet_buffer[NTP_PACKET_SIZE];
  int read_count = udp.read(packet_buffer, NTP_PACKET_SIZE);

  if (read_count != NTP_PACKET_SIZE) return false;

  // Validate protocol network_data
  uint8_t li = (packet_buffer[0] >> 6) & 0x03;
  uint8_t mode = packet_buffer[0] & 0x07;
  uint8_t stratum = packet_buffer[1];

  if (li == 3 || mode != 4 || stratum == 0 || stratum > 15) {
#ifdef RTC_INFO_MESSAGES
    Serial.print(F("NTP packet rejected: LI="));
    Serial.print(li);
    Serial.print(F(" | mode="));
    Serial.print(mode);
    Serial.print(F(" | stratum="));
    Serial.println(stratum);
#endif

    return false;
  }

  unsigned long secs_since_1900 =
    ((unsigned long) packet_buffer[40] << 24) |
    ((unsigned long) packet_buffer[41] << 16) |
    ((unsigned long) packet_buffer[42] << 8)  |
     (unsigned long) packet_buffer[43];

  if (secs_since_1900 == 0) return false;
  if (secs_since_1900 < 2208988800UL + 1577836800UL) return false;

  // Unix time starts on Jan 1 1970. In seconds, 70 years is 2208988800. Add one second to compensate calculation delay
  time_t epoch = secs_since_1900 - 2208988800UL + (timezone * 3600L) + 1;

  // DST offset
  if (daylight_saving_enabled && isDaylightSavingPeriod(epoch))
    epoch += 3600;

  struct tm *current_time = localtime(&epoch);
  current_time->tm_year += 1900; // Year is calculated from 1900 to now, so set to current year

  rtc.adjust(DateTime(current_time->tm_year, current_time->tm_mon + 1, current_time->tm_mday,
                      current_time->tm_hour, current_time->tm_min, current_time->tm_sec));

  daylight_saving_active = isDaylightSavingPeriod();

  connected_to_ntp = true;

  return true;
}

// ------------------------------------- Displays time and temperature or only time on the TM1637 ------------------------------------- //
void visualizeOnDisplay() {
#ifdef  LIGHT_SENSITIVITY_MODULE
  autoSetBrightness(); // Light sensitivity module function
#endif

  flashDisplay(); // Additional function

  if (work_mode_is_timer) {
    printRemainingTime(); // Timer mode
  }
  else {
#ifdef  TEMPERATURE_MODULE
    printCurrentTimeOrTemperature(); // If the clock has temperature sensor show temperature as well
#else
    printCurrentTime();
#endif
  }

  last_second = second_now;
}
