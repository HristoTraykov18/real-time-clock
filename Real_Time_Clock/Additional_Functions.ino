
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
  if (WiFi.softAPgetStationNum() == 1 && !active_connection) {
    someone_just_connected = true;
    active_connection = true;
  }
  else if (WiFi.softAPgetStationNum() == 0 && active_connection) {
    someone_just_connected = false;
    active_connection = false;

    if (software_update_server_active) {
      softwareUpdateServer.stop();
      software_update_server_active = false;

#ifdef  RTC_INFO_MESSAGES
      Serial.println(F("OTA update server stopped"));
#endif
    }
  }

  // Prevents network hanging
  if (WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_DISCONNECTED) {
    WiFi.disconnect();
    WiFi.begin();

#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("Network reset"));
#endif
  }
}

// ---------------------------------- Try to establish network connection with specific network ---------------------------------- //
bool connectClockToNetwork(const String& ssid, const String& pass, bool is_hidden) {
  const int CONNECT_ATTEMPT_DELAY = 100;
  bool is_connected = false;

  if ((WiFi.status() != WL_CONNECTED || WiFi.softAPgetStationNum() > 0 || is_hidden) && ssid != WiFi.SSID()) {
    WiFi.begin(ssid, pass);
    yield();

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("Trying to connect to "));
    Serial.println(ssid);
#endif

    for (int i = 0; i < CONNECT_ATTEMPT_DELAY; i++) {
      if (WiFi.status() != WL_CONNECTED) {
#ifdef  RTC_INFO_MESSAGES
        Serial.print(F("."));

        if (i == CONNECT_ATTEMPT_DELAY - 1) {
          Serial.println();
        }
#endif
      }
      else { // Save the network information
        saveNetworkInfo(ssid.c_str(), pass.c_str(), is_hidden ? "true" : "false");

#ifdef  RTC_INFO_MESSAGES
        Serial.println();
#endif
        is_connected = true;
        break;
      }

      delay(CONNECT_ATTEMPT_DELAY);
    }
  }
#ifdef  RTC_INFO_MESSAGES
  else if (WiFi.status() == WL_CONNECTED)
    Serial.println(("Connected to " + WiFi.SSID()).c_str());
#endif

  return is_connected;
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
  const uint8_t TIME_SET_ANIMATION[ANIMATION_LENGTH][4] = {{(SEG_E | SEG_F), 0, 0, 0 }, {(SEG_A | SEG_B | SEG_C | SEG_D), 0, 0, 0},
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
  char buf[512];
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

  while (millis() - start_millis < 800 && packet_length == 0) {
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

// --------------------------------------- Update the time manually from the user's device ---------------------------------------- //
void manualTimeUpdate() {
  String current_time_str = server.arg("currentTime");
  const uint8_t PARAMS_COUNT = 6;
  const char delimiter = ',';
  uint16_t current_time[PARAMS_COUNT];

  for (uint8_t i = 0; i < PARAMS_COUNT; i++) {
    String time_value = current_time_str.substring(0, current_time_str.indexOf(delimiter));
    current_time[i] = time_value.toInt();

    if (i < 5)
      current_time_str.remove(0, time_value.length() + 1);
  }

  rtc.adjust(DateTime(current_time[0], current_time[1] + 1, current_time[2],
                      current_time[3], current_time[4], current_time[5]));

  daylight_saving_active = isDaylightSavingPeriod();
}

// --------------------------------------- Check if a requested network is in range --------------------------------------- //
bool networkIsInRange(const String& ssid) {
  uint8_t number_of_networks = WiFi.scanNetworks(false, true);

  for (uint8_t i = 0; i < number_of_networks; i++) {
#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("Network request: "));
    Serial.print(ssid);
    Serial.print(F(" | Network in range: "));
    Serial.print(WiFi.SSID(i));
    Serial.print(F(" | Channel: "));
    Serial.println(WiFi.channel(i));
#endif

    if (ssid == WiFi.SSID(i))
      return true;
  }

  return false;
}

// ---------------------------------------- Attempt reconnecting to saved network ---------------------------------------- //
bool networkReconnect() {
  bool connected = WiFi.status() == WL_CONNECTED;
  const uint8_t LINES = 3;

  if (!connected) {
    if (LittleFS.exists("creds.txt")) {
      String network_data[LINES] = {};
      File f = LittleFS.open("creds.txt", "r");
      uint8_t i = 0;

      while (f.available()) {
        char current_char = char(f.read());

        if (current_char == '\n') {
          i += 1;
          continue;
        }
        else {
          network_data[i] += current_char;
        }
      }

      f.close();

      if (networkIsInRange(network_data[0]) || network_data[2] == "true")
        connected = connectClockToNetwork(network_data[0], network_data[1], network_data[2] == "true");

      if (connected)
        autoUpdateTime(true);
    }
#ifdef  RTC_INFO_MESSAGES
    else
      Serial.println(F("No creds.txt file"));
#endif
  }

  return connected;
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
void saveNetworkInfo(const char *network_name, const char* network_pass, const char* is_hidden) {
  File f = LittleFS.open("creds.txt", "w+");

  f.write(network_name);
  f.write("\n");
  f.write(network_pass);
  f.write("\n");
  f.write(is_hidden);
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
  packet_buffer[1] = 0;     // Stratum, or type of clock
  packet_buffer[2] = 6;     // Polling Interval
  packet_buffer[3] = 0xEC;  // Peer Clock Precision
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
  WiFi.hostByName(EU_NTP_SERVER_1, time_server_ip); // Get a random server from the pool
  bool time_updated = false;

  if (getNTP_PacketLength(time_server_ip)) { // If packet is received from NTP server read it and update time
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("\nNTP response received"));
#endif

    const uint8_t NTP_PACKET_SIZE = 48;
    byte packet_buffer[NTP_PACKET_SIZE];
    udp.read(packet_buffer, NTP_PACKET_SIZE); // Read the packet into the buffers

    // The timestamp starts at byte 40 of the received packet and is four bytes, or two words, long. First, esxtract the two words:
    unsigned long high_word = word(packet_buffer[40], packet_buffer[41]);
    unsigned long low_word = word(packet_buffer[42], packet_buffer[43]);
    unsigned long secs_since_1900 = high_word << 16 | low_word; // NTP time (seconds since Jan 1 1900)
    // Unix time starts on Jan 1 1970. In seconds, 70 years is 2208988800. Add one second to compensate calculation delay
    time_t epoch = secs_since_1900 - 2208988800UL + (timezone * 3600) + 1;

    // DST offset
    if (daylight_saving_enabled && isDaylightSavingPeriod(epoch))
      epoch += 3600;

    struct tm *current_time = localtime(&epoch);
    current_time->tm_year += 1900; // Year is calculated from 1900 to now, so set to current year

    rtc.adjust(DateTime(current_time->tm_year, current_time->tm_mon + 1, current_time->tm_mday,
                        current_time->tm_hour, current_time->tm_min, current_time->tm_sec));

    daylight_saving_active = isDaylightSavingPeriod();

    connected_to_ntp = true;
    displayClockJustUpdated(false);
    time_updated = true;
  }

  return time_updated;
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
