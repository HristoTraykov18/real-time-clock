
// _____________________________________________ Web interface handling functions _____________________________________________ //
void handleWebInterface() {
  if (server.hasArg("workMode"))
    editWorkMode(server.arg("workMode").c_str());
  if (server.arg("timeSyncMode") == "wifi")
    handleWifiTimeSync(server.arg("ssid"));
#ifdef  GPS_MODULE
  else if (server.arg("timeSyncMode") == "gps") {
    activateGPS(); // GPS module function
    sendWebpageResponse("Часовникът ще се свери чрез GPS");
    editTimeSyncMode("gps");
    autoUpdateTime(true);
  }
#endif
  else if (server.arg("timeSyncMode") == "js")
    handleManualTimeSync();
  else if (server.hasArg("daylightSavingEnabled")) {
    editDaylightSavingEnabled(server.arg("daylightSavingEnabled").c_str());
    sendWebpageResponse("Промените са запазени");
  }
  else if (server.hasArg("autoBrightnessControl"))
    handleBrightnessControl();
  else if (server.hasArg("status") && work_mode_is_timer)
    handleTimerControl();
  else if (server.arg("timeSyncMode") == "gps")
    sendWebpageResponse("Часовникът няма инсталиран GPS модул");
  else
    streamFileToServer("/index.html", "text/html"); // Show main page at the begining

  if (server.hasArg("timezoneHoursOffset"))
    editTimezoneOffset(server.arg("timezoneHoursOffset").c_str());

#ifdef  RTC_INFO_MESSAGES
  uint8_t argCount = server.args();

  for (uint8_t i = 0; i < argCount; i++) {
    Serial.print(server.argName(i));
    Serial.print(F(" = "));
    Serial.println(server.arg(i));
  }
#endif
}

// --------------------------------------------- Handle brightness change synchronization --------------------------------------------- //
void handleBrightnessControl() {
  if (server.arg("autoBrightnessControl") == "false") {
    editManualBrightness(server.arg("manualBrightnessLevel").c_str());
    display_brightness = server.arg("manualBrightnessLevel").toInt();
  }
  else
    display_brightness = DEFAULT_BRIGHTNESS;

  editAutoBrightness(server.arg("autoBrightnessControl").c_str());
  sendWebpageResponse("Промените са запазени");
}

// ------------------------------------------------ Handle device monitoring requests ------------------------------------------------ //
void handleDeviceMonitoring() {
#ifdef  RTC_INFO_MESSAGES
  Serial.print(F("Current Free Heap: "));
  Serial.println(ESP.getFreeHeap());
  Serial.print(F("Heap fragmentation: "));
  Serial.println(ESP.getHeapFragmentation());
#endif
  sendWebpageResponse(("Current Free Heap: " + String(ESP.getFreeHeap()) + "\nHeap fragmentation: " + ESP.getHeapFragmentation()).c_str());
}

// ----------------------------------------------- Handle manual time synchronization ----------------------------------------------- //
void handleManualTimeSync() {
  if (networkReconnect()) {
    if (autoUpdateTime(true))
      sendWebpageResponse(("Часовникът е свързан с мрежа " + WiFi.SSID() + ".\nУспешно сверяване през Интернет.").c_str());
    else
      sendWebpageResponse(("Часовникът е свързан с мрежа " + WiFi.SSID() + ".\nНеуспешно сверяване през Интернет. Моля опитайте отново!").c_str());
  }
  else {
    manualTimeUpdate();
    sendWebpageResponse("Часовникът се свери автоматично от устройството Ви");
  }
}

// ----------------------------------------- Handle timer start / pause / restart controls ----------------------------------------- //
void handleTimerControl() {
  if (server.arg("status") == "start") {
    editTimerDuration(server.arg("duration").c_str());
    sendWebpageResponse("Таймерът е стартиран!");
  }
  else if (server.arg("status") == "pause") {
    timer_millis_offset = millis() - timer_millis;
    timer_status = 0;
    sendWebpageResponse("Таймерът е паузиран!");
  }
  else if (server.arg("status") == "resume") {
    timer_millis = millis() - timer_millis_offset;
    timer_status = 1;
    sendWebpageResponse("Таймерът е рестартиран!");
  }
  else {
    sendWebpageResponse("Невалидна команда за таймер");
  }
}

// ----------------------------------------- Handle time synchronization throught Wi-Fi ----------------------------------------- //
void handleWifiTimeSync(const String& ssid) {
  if (WiFi.status() == WL_CONNECTED) {
    if (ssid == WiFi.SSID()) {
      if (autoUpdateTime(true))
        sendWebpageResponse(("Часовникът вече е свързан с мрежа " + ssid + ".\nУспешна актуализация на времето през Интернет!").c_str());
      else
        sendWebpageResponse(("Часовникът вече е свързан с мрежа " + ssid + ".\nНеуспешна актуализация на времето, моля опитайте отново!").c_str());
    }
    else if (ssid != "" && (server.arg("pass")).length() > 7) {
      validateNetworkInput(ssid, server.arg("pass"), server.arg("isHiddenNetwork"));
    }
    else {
      if (autoUpdateTime(true))
        sendWebpageResponse(("Настоящата мрежа е " + WiFi.SSID() + "\nУспешна актуализация на времето през Интернет!").c_str());
      else
        sendWebpageResponse(("Настоящата мрежа е " + WiFi.SSID() + "\nНеуспешна актуализация на времето, моля опитайте отново!").c_str());
    }
  }
  else if (ssid != "" && (server.arg("pass")).length() > 7) {
    validateNetworkInput(ssid, server.arg("pass"), server.arg("isHiddenNetwork"));
  }
#ifdef  GPS_MODULE
  else if (set_time_with_gps)
    sendWebpageResponse("Промените са запазени!");
#endif
  else
    sendWebpageResponse("Моля въведете име и парола на мрежата!");

#ifdef  GPS_MODULE
  gps_connect_attempts_left = 0;
#endif
  editTimeSyncMode("wifi");
}

void sendClockInfo() {
  bool is_connected = WiFi.status() == WL_CONNECTED;
  String ssid = is_connected ? WiFi.SSID() : "-";
  int32_t rssi = is_connected ? abs(WiFi.RSSI()) : 0;

  uint8_t mac_raw[6];
  char mac_buf[18];
  WiFi.macAddress(mac_raw); // Gets the 6 raw bytes
  snprintf(mac_buf, sizeof(mac_buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac_raw[0], mac_raw[1], mac_raw[2], mac_raw[3], mac_raw[4], mac_raw[5]);

  char ip_buf[16] = "-";

  if (is_connected) {
    IPAddress ip = WiFi.localIP();
    snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }

  DateTime now = rtc.now();
  char response[90];
  
  snprintf(response, sizeof(response), "%s|%s%d|%s|%s|%02d:%02d:%02d",
           ssid.c_str(),
           is_connected ? "" : "-",
           rssi,
           mac_buf,
           ip_buf,
           now.hour(), now.minute(), now.second());

  server.send(200, "text/plain", response);

#ifdef RTC_INFO_MESSAGES
  Serial.print(F("Clock info: "));
  Serial.println(response);
#endif
}

void sendWebpageResponse(const char *webpage_response) {
  server.send(200, "text/plain", webpage_response);

#ifdef  RTC_INFO_MESSAGES
  Serial.println(webpage_response);
#endif
}

void streamFileToServer(const char *filename, const char *filestream_format) {
  File data_file = LittleFS.open(filename, "r");

  server.streamFile(data_file, filestream_format);
  data_file.close();
}

void validateNetworkInput(const String& ssid, const String& pass, const String& is_hidden) {
  if (networkIsInRange(ssid) || is_hidden == "true") {
    if (connectClockToNetwork(ssid, pass, is_hidden == "true")) {
      if (autoUpdateTime(true))
        sendWebpageResponse(("Часовникът се свърза с мрежа " + ssid + "\nУспешна актуализация на времето през Интернет").c_str());
      else
        sendWebpageResponse(("Часовникът се свърза с мрежа " + ssid + "\nНеуспешна актуализация на времето, моля опитайте отново!").c_str());
    }
    else
      sendWebpageResponse("Времето за опит за свързване изтече. Проверете името и паролата.");
  }
  else
    sendWebpageResponse(("Мрежата " + ssid + " не е в обхват").c_str());
}
