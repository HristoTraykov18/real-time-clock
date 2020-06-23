
// ___________________________________________________ Additional functions ___________________________________________________ //

// --------------------------- Function that checks if someone has connected to the ESP's network ---------------------------- //
void CheckForUserConnection() {
  if (WiFi.softAPgetStationNum() == 1 && !activeConnection) {
    someoneJustConnected = true;
    activeConnection = true;
  }
  else if (WiFi.softAPgetStationNum() == 0 && activeConnection) {
    someoneJustConnected = false;
    activeConnection = false;
  }

  // Prevents network hanging
  if (WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_DISCONNECTED)
    WiFi.disconnect();
}

// ------------------------------- Try to establish network connection with the last known network ------------------------- //
void ConnectToLastKnownNetwork() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Trying to connect to last known network")); // Try connecting to prevously saved network
    WiFi.begin();
    
    for (int i = 0; i < CONNECT_TO_NETWORK_LOOP_COUNT; i++) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.print(F("."));
        
        if (i == CONNECT_TO_NETWORK_LOOP_COUNT - 1) {
          Serial.println(F("\nFailed to connect"));
        }
      }
      else {
        Serial.println(F("\nConnected"));
        break;
      }
      
      delay(CONNECT_TO_NETWORK_LOOP_DELAY);
    }
  }
  else {
    Serial.println(("Connected to " + WiFi.SSID()).c_str());
  }
}

// ------------------------------------------- Daylight saving time check function -------------------------------------------- //
void DaylightSavingChange(int& hourNow) { // Hours change on last Sunday in March/October
  // Check if today is the last sunday of March/October
  int lastSundayDate = -1;
  if ((rtc.now().month() == 3 || rtc.now().month() == 10) && (31 - rtc.now().day() < 7) && rtc.now().dayOfTheWeek() == 0) {
    lastSundayDate = rtc.now().day();
    dstHasChanged = true;
  }

  if ((rtc.now().month() > 3 && rtc.now().month() < 10) ||
      ((rtc.now().month() == 3 && rtc.now().day() >= lastSundayDate) ||
      (rtc.now().month() == 10 && rtc.now().day() < lastSundayDate) && lastSundayDate != -1))
    hourNow += timezone + 1;
  else
    hourNow += timezone;
}

// ----------------------------------- Display on the TM1637 that the clock just updated time ----------------------------------- //
void DisplayClockJustUpdated(bool updatedFromGPS) {
  if (updatedFromGPS) {
    for (int j = 7; j > -1; j--) {
      tm1637.setSegments(connectingToNetworkAnimation[j]);
      delay(75);
    }
  }
  else {
    for (int j = 0; j < 8; j++) {
      tm1637.setSegments(connectingToNetworkAnimation[j]);
      delay(75);
    }
  }
}

// --------------------------------------------- Edit settings file with user input --------------------------------------------- //
// daylightSaving, setTimeWithGPS, autoBrightness, displayBrightness, timezone
void EditClockSettings(bool dS, bool setWithGPS, bool autoBr, int displayBr, const char tz[], const char ip[]) {
  int tagsCount = sizeof(startTags) / 4;
  File f = LittleFS.open("/espSettings.xml", "w");
  f.write((settingsFile.substring(0, settingsFile.indexOf(startTags[0]) + strlen(startTags[0]))).c_str());
  
  for (int i = 0; i < tagsCount; i++) {
    switch (i) { // Override each value
      case 0:
        f.write(dS ? "Active" : "Inactive"); // daylightSaving
        break;
      case 1:
        f.write(setWithGPS ? "GPS" : "WiFi"); // setTimeWithGPS
        break;
      case 2:
        f.write(autoBr ? "Auto" : "Manual"); // autoBrightness
        break;
      case 3:
        f.write(char(displayBr + '0')); // displayBrightness
        break;
      case 4:
        f.write(tz); // timezone
        break;
      case 5:
        f.write(ip); // ip
        break;
      default:
        break;
    }
    if (i < tagsCount - 1)
      f.write((settingsFile.substring(settingsFile.indexOf(endTags[i]), settingsFile.indexOf(startTags[i + 1]) + strlen(startTags[i + 1]))).c_str());
    else
      f.write((settingsFile.substring(settingsFile.indexOf(endTags[i]), settingsFile.length())).c_str());
  }
  
  f.close();
}

// ----------------------------------- If the ESP is connected to network turn on the LED ------------------------------------- //
void ESP_ToNetworkConnection() {
  if (digitalRead(LED_PIN) == LOW && WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
  }
  else if (digitalRead(LED_PIN) == HIGH && WiFi.status() == WL_CONNECTED)
    digitalWrite(LED_PIN, LOW);
}

// --------------------- Flash the display if someone connects to the ESP or if it connects to NTP server --------------------- //
void FlashDisplay() {
  if (someoneJustConnected && flashesOnConnect == 0) {
    flashesOnConnect = 6;
    lastAutoBrightness = autoBrightness;
    lastDisplayBrightness = displayBrightness;
    autoBrightness = false;
  }
  
  if (flashesOnConnect > 0) {
    if (displayBrightness == lastDisplayBrightness) {
      if (connectedTo_NTP)
        displayBrightness = displayBrightness != 6 ? 6 : 0;
      else
        displayBrightness = displayBrightness != 0 ? 0 : 6;
    }
    else
      displayBrightness = lastDisplayBrightness;
      
    flashesOnConnect--;
    
    if (flashesOnConnect == 0) {
      someoneJustConnected = false;
      autoBrightness = lastAutoBrightness;
    }
    
    if (!someoneJustConnected && !connectedTo_NTP) {
      flashesOnConnect = 0;
      autoBrightness = lastAutoBrightness;
      displayBrightness = lastDisplayBrightness;
    }
  }
  
  tm1637.setBrightness(displayBrightness);
}

// -------------------------------------- Get infomation from the xml file in the ESP ------------------------------------- //
void GetClockSettings() {
  settingsFile = ReadPage("/espSettings.xml");
  String elValue = "";
  int tagsCount = sizeof(startTags) / 4;

  for (int i = 0; i < tagsCount; i++) {
    elValue = settingsFile.substring(settingsFile.indexOf(startTags[i]) + strlen(startTags[i]), settingsFile.indexOf(endTags[i]));
    switch (i) {
      case 0:
        daylightSaving = elValue == "Active";
        break;
        
      case 1:
        setTimeWithGPS = elValue == "GPS";
        break;
        
      case 2:
        autoBrightness = elValue == "Auto";
        break;
        
      case 3:
        displayBrightness = elValue.toInt(); // Manual brightness level (set by user)
        if (!autoBrightness)
          lastDisplayBrightness = displayBrightness;
        break;
    }
  }
}

int GetNTP_PacketLength(IPAddress& address) {
  SendNTP_Packet(address);

  unsigned long startMillis = millis();
  unsigned long lastMillis = startMillis;
  unsigned long packetLength = 0;
  
  while (millis() - startMillis < 800 && packetLength == 0) {
    packetLength = udp.parsePacket();
    if (millis() != lastMillis && ((millis() - startMillis) % 100) == 0) {
      Serial.print(F("."));
      lastMillis = millis();
    }
  }
  
  return packetLength;
}

// ------------------------------------------- Print the current time to the TM1637 ------------------------------------------- //
void PrintCurrentTime() {
  int h = rtc.now().hour(); // Current hour
  int m = rtc.now().minute(); // Current minute
  int digitsToPrint = ((h / 10) * 1000) + ((h % 10) * 100) + ((m / 10) * 10) + (m % 10);

  if (s % 2 == 1)
    tm1637.showNumberDecEx(digitsToPrint, 16);
  else
    tm1637.showNumberDecEx(digitsToPrint, 0);

  lastSecond = s;

  Serial.print(F("Time: "));
  Serial.print(h);
  Serial.print(F(":"));
  Serial.print(m);
  Serial.print(F(":"));
  Serial.print(s);
  Serial.print(F(", "));
  Serial.print(rtc.now().day());
  Serial.print(F("."));
  Serial.print(rtc.now().month());
  Serial.print(F("."));
  Serial.print(rtc.now().year());
  Serial.print(F(", Day of the week: "));
  Serial.print(rtc.now().dayOfTheWeek());
  Serial.print(F(", "));
}

// ------------------------------------------- Print the current time to the TM1637 ------------------------------------------- //
void PrintCurrentTimeOrTemperature() {
  // Get actual time, update dots, flash if someone connects once per second to save CPU power
  if (showTime) {
    PrintCurrentTime();
    showDuration--;

    if (showDuration == 0) {
      showDuration = 4;
      showTime = false;
    }
  }
  else
    PrintCurrentTemperature();
}

// ---------------------------------------------- File content reading function ---------------------------------------------- //
String ReadPage(String filename) {
  File f = LittleFS.open(filename.c_str(), "r");
  String page;
  while (f.available()) {
    page += char(f.read());
  }
  f.close();

  return page;
}

// --------------------------------------------- Resets the Real-Time Clock module --------------------------------------------- //
void Reset_RTC() {
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
  
  Serial.println(F("RTC reset"));
  digitalWrite(LED_PIN, LOW);
  delay(250);
}

// ----------------------------------------------- NTP packet sending function ----------------------------------------------- //
void SendNTP_Packet(IPAddress& address) {
  Serial.println(F("Preparing NTP packet"));

  memset(packetBuffer, 0, NTP_PACKET_SIZE); // Set all bytes in the buffer to 0

  // Initialize values needed to form NTP request (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision

  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // All NTP fields have values, send a packet requesting a timestamp
  udp.beginPacket(address, 123); // NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
  Serial.print(F("NTP packet sent. Waiting response"));
}

// ------------------------------------------ Determine which update function to call --------------------------------------- //
void UpdateTime(bool hasGPS) {
  if ((rtc.now().hour() == updateHour && rtc.now().minute() == 0 && rtc.now().second() <= 5) || timeUpdatePending) {
    if (hasGPS && setTimeWithGPS) {
      if (GPS_connectionTriesLeft > 1) {
        unsigned long startMillis = millis();
        
        while (millis() - startMillis < 1000 && gps.satellites.value() == 0) {
          while (gpsSerial.available()) {
            gps.encode(gpsSerial.read());
            server.handleClient();
          }
          
          server.handleClient();
        }
        if (gps.satellites.value() != 0)
          UpdateTimeFromGPS(gps.date, gps.time); // Function in GPS_Addon
        
        Serial.print(F("Could not get time from GPS. Tries left: "));
        Serial.println(--GPS_connectionTriesLeft);
      }
      else {
        detachInterrupt(digitalPinToInterrupt(GPS_RX));
        UpdateTimeFromNTP();
      }
    }
    else {
      detachInterrupt(digitalPinToInterrupt(GPS_RX));
      
      if (hasGPS) {
        Serial.println(F("Time not updated from GPS"));
      }
      else {
        Serial.println(F("No GPS"));
      }
      UpdateTimeFromNTP();
    }
  }
}

// ----------------------------------------- Update time from Network Time Protocol server --------------------------------------- //
void UpdateTimeFromNTP() {
  WiFi.hostByName(EU_NTP_SERVER_1, timeServerIP); // Get a random server from the pool
  //SendNTP_Packet(TIME_SERVER_IP); // Send an NTP packet to a time server

  if (GetNTP_PacketLength(timeServerIP)) { // If packet is received from NTP server read it and update time
    Serial.println(F("\nResponse received"));
    udp.read(packetBuffer, NTP_PACKET_SIZE); // Read the packet into the buffers

    // The timestamp starts at byte 40 of the received packet and is four bytes, or two words, long. First, esxtract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord; // NTP time (seconds since Jan 1 1900)
    unsigned long epoch = secsSince1900 - 2208988800UL + 2; // Unix time starts on Jan 1 1970. In seconds, 70 years is 2208988800

    int yearsSince1970 = epoch / 31556736;

    // To find current month and day get current day in year
    int dayOfYear = (int(epoch / 86400) % 365) - int(yearsSince1970 * 0.24);
    int lengthOfMonths[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    if ((yearsSince1970 + 1970) % 4 == 0) {
      dayOfYear += 1;
      lengthOfMonths[2] += 1;
    }

    // And subtract the length of each month
    int monthNow;
    
    for (monthNow = 1; dayOfYear > lengthOfMonths[monthNow]; monthNow++) {
      dayOfYear -= lengthOfMonths[monthNow];
      if (dayOfYear == 0)
        dayOfYear = lengthOfMonths[monthNow];
    }

    int dayNow = dayOfYear;               // Current day
    int hourNow = (epoch % 86400) / 3600; // hour
    int minuteNow = (epoch %  3600) / 60; // minutes
    int secondNow = epoch % 60;           // and seconds

    if (daylightSaving) {                  // If daylight saving time is ON
      if (rtc.now().day() != dayNow)
        rtc.adjust(DateTime(yearsSince1970 + 1970, monthNow, dayNow, hourNow, minuteNow, secondNow)); // Write new date to RTC to get proper dayOfTheWeek()
      DaylightSavingChange(hourNow);   // change time accordingly
    }
    else
      hourNow += timezone; // Otherwise use the respective timezone
      
    hourNow %= 24;
    rtc.adjust(DateTime(yearsSince1970 + 1970, monthNow, dayNow, hourNow, minuteNow, secondNow)); // Write the new time to the RTC memory
    DisplayClockJustUpdated(false);
    connectedTo_NTP = true;
    updateHour = 3;
    timeUpdatePending = false;
    Serial.println(F("Time updated from NTP server\n"));
  }
  else { // If time update failed try after an hour up to twice
    if (updateHour < 6)
      updateHour++;
    else {
      updateHour = 3;
      int hourNow = rtc.now().hour();
      DaylightSavingChange(hourNow);
      rtc.adjust(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), hourNow, rtc.now().minute(), rtc.now().second()));
    }
    
    Serial.println(F("\nCould not update time from NTP server\n"));
    timeUpdatePending = false;
    dstHasChanged = false;
  }
}

// ------------------------------------- Displays hour and temperature or only time on the TM1637 ------------------------------------- //
void VisualizeOnDisplay(bool hasTSensor) {
  AutoBrightness(); // Light_Sensitivity_Addon
  if (hasTSensor) // If the clock has temperature sensor
    PrintCurrentTimeOrTemperature();
  else
    PrintCurrentTime();
}
