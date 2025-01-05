
// ___________________________________________________ Additional functions ___________________________________________________ //

// ----------------------------------- Check if someone has connected to the ESP's network ----------------------------------- //
void checkForUserConnection() {
  if (WiFi.softAPgetStationNum() == 1 && !active_connection) {
    someone_just_connected = true;
    active_connection = true;
  }
  else if (WiFi.softAPgetStationNum() == 0 && active_connection) {
    someone_just_connected = false;
    active_connection = false;
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
bool connectClockToNetwork(const String& ssid, const String& pass) {
  bool is_connected = false;

  if ((WiFi.status() != WL_CONNECTED || WiFi.softAPgetStationNum() > 0) && ssid != WiFi.SSID()) {
    WiFi.begin(ssid, pass);

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("Trying to connect to "));
    Serial.println(ssid);
#endif

    for (int i = 0; i < CONNECT_TO_NETWORK_LOOP_COUNT; i++) {
      if (WiFi.status() != WL_CONNECTED) {
#ifdef  RTC_INFO_MESSAGES
        Serial.print(F("."));
#endif

        if (i == CONNECT_TO_NETWORK_LOOP_COUNT - 1) {
#ifdef  RTC_INFO_MESSAGES
          Serial.println();
#endif
        }
      }
      else { // Save the network information and set time update variable
        time_update_pending = true;
        saveNetworkInfo(ssid.c_str(), pass.c_str());

#ifdef  RTC_INFO_MESSAGES
        Serial.println();
#endif
        is_connected = true;
        break;
      }

      delay(CONNECT_TO_NETWORK_LOOP_DELAY);
    }
  }
#ifdef  RTC_INFO_MESSAGES
  else
    Serial.println(("Connected to " + WiFi.SSID()).c_str());
#endif

  return is_connected;
}

// ----------------------------------- Change time if needed depending on daylight saving time ----------------------------------- //
// Daylight saving time change is on last Sunday in March/October
void daylightSavingChange(uint8_t &hour_now) {
  bool is_daylight_saving_period = isDaylightSavingPeriod();

  if (!daylight_saving_applied && is_daylight_saving_period) {
    hour_now += 1;
    daylight_saving_applied = true;
  }
  else if (daylight_saving_applied && !is_daylight_saving_period) {
    hour_now -= 1;
    daylight_saving_applied = false;
  }
}

// ----------------------------------- Display on the TM1637 that the clock just updated time ----------------------------------- //
void displayClockJustUpdated(bool updated_from_gps) {
  // Effect when the clock time is set
  const uint8_t TIME_SET_ANIMATION[8][4] = {{(SEG_E | SEG_F), 0, 0, 0 }, {(SEG_A | SEG_B | SEG_C | SEG_D), 0, 0, 0},
    {(SEG_A | SEG_D), (SEG_E | SEG_F), 0, 0}, {(SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D), 0, 0},
    {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_E | SEG_F), 0}, {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D), 0},
    {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_E | SEG_F)}, {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D)}
  };

  // Animate effect
  if (updated_from_gps) {
    for (int j = 7; j > -1; j--) {
      tm1637.setSegments(TIME_SET_ANIMATION[j]);
      delay(75);
    }
  }
  else {
    for (int j = 0; j < 8; j++) {
      tm1637.setSegments(TIME_SET_ANIMATION[j]);
      delay(75);
    }
  }
}

// --------------------------------------------- Edit settings file with user input --------------------------------------------- //
void editClockSettings(const char new_value[], uint8_t tags_id) {
  switch (tags_id) {
    case 0:
      if (daylight_saving_enabled != (strcmp(new_value, "true") == 0)) {
        editSettingsFile(new_value, tags_id);
        daylight_saving_enabled = !daylight_saving_enabled;
      }

      break;

    case 1:
      if (daylight_saving_applied != (strcmp(new_value, "true") == 0)) {
        editSettingsFile(new_value, tags_id);
        daylight_saving_applied = !daylight_saving_applied;
      }

      break;

    case 2:
#ifdef  GPS_MODULE
      if (set_time_with_gps != (strcmp(new_value, "gps") == 0)) {
        editSettingsFile(new_value, tags_id);
        set_time_with_gps = !set_time_with_gps;
      }
#endif

      break;

    case 3:
      if (auto_brightness != (strcmp(new_value, "true") == 0)) {
        editSettingsFile(new_value, tags_id);
        auto_brightness = !auto_brightness;
      }

      break;

    case 4:
      editSettingsFile(new_value, tags_id); // Manual brightness level (set by user)

      if (!auto_brightness)
        last_display_brightness = display_brightness;

      break;

    case 5:
      editSettingsFile(new_value, tags_id); // Timezone
      break;
  }
}

void editSettingsFile(const char new_value[], uint8_t tags_id) {
  String settings_file = readFileToString("/espSettings.xml");
  File f = LittleFS.open("/espSettings.xml", "w");

  // Rewrite everything before the opening tag
  f.write((settings_file.substring(0, settings_file.indexOf(START_TAGS[tags_id]) + strlen(START_TAGS[tags_id]))).c_str());
  f.write(new_value);

  // Rewrite everything after the closing tag
  f.write((settings_file.substring(settings_file.indexOf(END_TAGS[tags_id]), settings_file.length())).c_str());
  f.close();
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
uint8_t getLastSundayDate() {
  uint8_t day_of_the_week = rtc.now().dayOfTheWeek();
  uint8_t days_until_sunday = 7 - (day_of_the_week == 0 ? 7 : day_of_the_week);
  uint8_t next_sunday_date = rtc.now().day() + days_until_sunday;

  return 31 - ((31 - next_sunday_date) % 7);
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
bool isDaylightSavingPeriod() {
  bool is_period = false;

  if (rtc.now().month() > 3 && rtc.now().month() < 10) {
    is_period = true;
  }
  else {
    uint8_t last_sunday_date = getLastSundayDate();

    if ((rtc.now().month() == 3 && rtc.now().day() > last_sunday_date) ||
        (rtc.now().month() == 10 && rtc.now().day() < last_sunday_date)) {
      is_period = true;
    }
  }

  return is_period;
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
  editClockSettings(isDaylightSavingPeriod() ? "true" : "false", 1);
}

// --------------------------------------------- Print the current time to the TM1637 --------------------------------------------- //
void printCurrentTime() {
  int h = rtc.now().hour(); // Current hour
  int m = rtc.now().minute(); // Current minute
  int digits_to_print = ((h / 10) * 1000) + ((h % 10) * 100) + ((m / 10) * 10) + (m % 10);

  if (second_now % 2 == 1)
    tm1637.showNumber(digits_to_print, 32); // tm1637.showNumber(digits_to_print, 32, true); // OSRAM NBG_CLOCK_00001 & NBG_CLOCK_00002 ONLY
  else
    tm1637.showNumber(digits_to_print, 0); // tm1637.showNumber(digits_to_print, 0, true); // OSRAM NBG_CLOCK_00001 & NBG_CLOCK_00002 ONLY

  last_second = second_now;

#ifdef  RTC_INFO_MESSAGES
  Serial.print(F("Time: "));
  Serial.print(h);
  Serial.print(F(":"));
  Serial.print(m);
  Serial.print(F(":"));
  Serial.print(second_now);
  Serial.print(F(" "));
  Serial.print(rtc.now().day());
  Serial.print(F("."));
  Serial.print(rtc.now().month());
  Serial.print(F("."));
  Serial.print(rtc.now().year());
  Serial.print(F(", Day of the week: "));
  Serial.print(rtc.now().dayOfTheWeek());
  Serial.print(F(", "));
#endif
}

// ---------------------------------------------- File content reading function ---------------------------------------------- //
String readFileToString(const char *filename) {
  File f = LittleFS.open(filename, "r");
  String page;

  while (f.available())
    page += char(f.read());

  f.close();

  return page;
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
void saveNetworkInfo(const char *network_name, const char* network_pass) {
  File f = LittleFS.open("creds.txt", "w+");

  f.write(network_name);
  f.write("\n");
  f.write(network_pass);
  f.close();
}

// ----------------------------------------------- NTP packet sending function ----------------------------------------------- //
void sendNTP_Packet(IPAddress& address) {
#ifdef  RTC_INFO_MESSAGES
  Serial.println(F("Preparing NTP packet"));
#endif

  memset(packet_buffer, 0, NTP_PACKET_SIZE); // Set all bytes in the buffer to 0

  // Initialize values needed to form NTP request (see URL above for details on the packets)
  packet_buffer[0] = 0b11100011; // LI, Version, Mode
  packet_buffer[1] = 0;     // Stratum, or type of clock
  packet_buffer[2] = 6;     // Polling Interval
  packet_buffer[3] = 0xEC;  // Peer Clock Precision

  // 8 bytes of zero for Root Delay & Root Dispersion
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

// ------------------------------------------ Determine which update function to call --------------------------------------- //
void updateTime() { // Check if it's the right time to update the time or if time update is requested
  if ((rtc.now().hour() == update_hour && rtc.now().minute() == 0 && rtc.now().second() < 3) || time_update_pending) {
    bool time_updated = false;

#ifdef  GPS_MODULE
    if (set_time_with_gps) { // Check if time should be updated through GPS module
      if (gps_connect_attempts_left > 0) {
        unsigned long startMillis = millis();

        while (millis() - startMillis < 1000 && gps.satellites.value() == 0) {
          while (gpsSerial.available()) {
            gps.encode(gpsSerial.read());
            server.handleClient();
          }

          server.handleClient();
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
        time_updated = updateTimeFromNTP();
      }
    }
    else { // In case of time update from NTP
      detachInterrupt(digitalPinToInterrupt(GPS_RX));
      time_updated = updateTimeFromNTP();
    }
#else
    time_updated = updateTimeFromNTP();
#endif

#ifdef  RTC_INFO_MESSAGES
    if (time_updated) {
      daylight_saving_applied = false;
      Serial.println(F("Time updated from NTP server\n"));
    }
    else
      Serial.println(F("\nCould not update time from NTP server\n"));
#else
    if (time_updated)
      daylight_saving_applied = false;
#endif

    if (daylight_saving_enabled) {
      uint8_t temp_hour = rtc.now().hour();
      daylightSavingChange(temp_hour);

      if (rtc.now().hour() != temp_hour) {
        rtc.adjust(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(),
                            temp_hour, rtc.now().minute(), rtc.now().second()));
        editClockSettings(daylight_saving_applied ? "true" : "false", 1);
      }
    }
  }
}

// ----------------------------------------- Update time from Network Time Protocol server --------------------------------------- //
bool updateTimeFromNTP() {
  WiFi.hostByName(EU_NTP_SERVER_1, time_server_ip); // Get a random server from the pool
  bool time_updated = false;

  if (getNTP_PacketLength(time_server_ip)) { // If packet is received from NTP server read it and update time
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("\nNTP response received"));
#endif

    udp.read(packet_buffer, NTP_PACKET_SIZE); // Read the packet into the buffers

    // The timestamp starts at byte 40 of the received packet and is four bytes, or two words, long. First, esxtract the two words:
    unsigned long high_word = word(packet_buffer[40], packet_buffer[41]);
    unsigned long low_word = word(packet_buffer[42], packet_buffer[43]);
    unsigned long secs_since_1900 = high_word << 16 | low_word; // NTP time (seconds since Jan 1 1900)
    // Unix time starts on Jan 1 1970. In seconds, 70 years is 2208988800. Add two seconds to compencate the delay
    time_t epoch = secs_since_1900 - 2208988800UL + 1;

    struct tm *current_time = localtime(&epoch);
    current_time->tm_year += 1900; // Year is calculated from 1900 to now, so set to current year
    current_time->tm_hour += timezone;

    rtc.adjust(DateTime(current_time->tm_year, current_time->tm_mon + 1, current_time->tm_mday,
                        current_time->tm_hour, current_time->tm_min, current_time->tm_sec));

    connected_to_ntp = true;
    displayClockJustUpdated(false);
    time_updated = true;
  }
  else if (!time_update_pending) {
    if (update_hour < LAST_UPDATE_HOUR)
      update_hour++;
    else
      update_hour = 3;
  }

  time_update_pending = false;
  return time_updated;
}

// ------------------------------------- Displays time and temperature or only time on the TM1637 ------------------------------------- //
void visualizeOnDisplay() {
#ifdef  LIGHT_SENSITIVITY_MODULE
  autoSetBrightness(); // Light sensitivity module function
#endif

  flashDisplay(); // Additional function

#ifdef  TEMPERATURE_MODULE
  printCurrentTimeOrTemperature(); // If the clock has temperature sensor show temperature as well
#else
  printCurrentTime();
#endif
}
