
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

// ----------------------------------------- Handle software update server activation ----------------------------------------- //
void handleActivateSoftwareUpdate() {
  if (!software_update_server_active) {
    softwareUpdateServer.begin();
    software_update_server_active = true;

#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("OTA update server started"));
#endif
  }

  sendWebpageResponse("ok");
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

// ----------------------------------------- Extend the current session to prevent timeout ----------------------------------------- //
void handleExtendSession() {
  sendWebpageResponse("Успешно удължихте сесията си!");
  last_http_activity_ms = millis();
}

// ----------------------------------------- Handle deletion of saved network credentials ----------------------------------------- //
void handleDeleteCreds() {
  if (LittleFS.exists("creds.txt")) {
    LittleFS.remove("creds.txt");
    sendWebpageResponse("Запазената мрежа е изтрита!");
    WiFi.disconnect(true);
  }
  else
    sendWebpageResponse("Няма запазена мрежа!");
}

// ------------------------------------------------ Handle device monitoring requests ------------------------------------------------ //
void handleDeviceMonitoring() {
  sendWebpageResponse(("Max free block size: " + String(ESP.getMaxFreeBlockSize()) + "\nCurrent Free Heap: " + \
    String(ESP.getFreeHeap()) + "\nHeap fragmentation: " + String(ESP.getHeapFragmentation())).c_str());
}

// ----------------------------------------------- Handle manual time synchronization ----------------------------------------------- //
void handleManualTimeSync() {
  if (networkReconnect()) {
    if (autoUpdateTime(true))
      sendWebpageResponse(("Часовникът е свързан с мрежа " + WiFi.SSID() + ".\nУспешно сверяване през Интернет!").c_str());
    else
      sendWebpageResponse(("Часовникът е свързан с мрежа " + WiFi.SSID() + ".\nНеуспешно сверяване през Интернет. Моля опитайте отново!").c_str());
  }
  else {
    manualTimeUpdate();

    if (LittleFS.exists("creds.txt"))
      sendWebpageResponse(("Неуспешно свързване със запаметената мрежа " + WiFi.SSID() + "!\nЧасовникът се свери автоматично от устройството Ви.").c_str());
    else
      sendWebpageResponse("Часовникът се свери автоматично от устройството Ви.");
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

// ---------------------------------------------- Handle user inactivity timeout ---------------------------------------------- //
void handleSessionTimeout() {
  sendWebpageResponse("Сесията Ви изтече!\nЗа да използвате настройките, моля свържете се с часовника отново!");
  evictStaleStations();
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
  int32_t rssi = is_connected ? abs(WiFi.RSSI()) : 0;
  char ip_buf[16] = "-";

  if (is_connected) {
    IPAddress ip = WiFi.localIP();
    snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }

  uint8_t mac_raw[6];
  char mac_buf[18];
  WiFi.macAddress(mac_raw); // Gets the 6 raw bytes
  snprintf(mac_buf, sizeof(mac_buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac_raw[0], mac_raw[1], mac_raw[2], mac_raw[3], mac_raw[4], mac_raw[5]);

  DateTime now = rtc.now();
  char response[90];

  snprintf(response, sizeof(response), "%s|%s%d|%s|%s|%02d:%02d:%02d",
           is_connected ? WiFi.SSID().c_str() : "-",
           is_connected ? "" : "-",
           rssi,
           ip_buf,
           mac_buf,
           now.hour(), now.minute(), now.second());

  server.send(200, "text/plain", response);
  last_http_activity_ms = millis();

#ifdef RTC_INFO_MESSAGES
  Serial.print(F("Clock info: "));
  Serial.println(response);
#endif
}

void sendAdditionalSettings() {
  char timezone_buf[1];
  itoa(timezone, timezone_buf, 10);

  char response[8];
  snprintf(response, sizeof(response), "%s|%s",
           timezone_buf,
           LittleFS.exists("creds.txt") ? "true" : "false");

  server.send(200, "text/plain", response);
  last_http_activity_ms = millis();

#ifdef RTC_INFO_MESSAGES
  Serial.print(F("Additional settings: "));
  Serial.println(response);
#endif
}

void sendWebpageResponse(const char *webpage_response) {
  server.send(200, "text/plain", webpage_response);
  last_http_activity_ms = millis();

#ifdef  RTC_INFO_MESSAGES
  Serial.println(webpage_response);
#endif
}

void streamFileToServer(const char *filename, const char *filestream_format) {
  File data_file = LittleFS.open(filename, "r");
  server.streamFile(data_file, filestream_format);
  data_file.close();

  last_http_activity_ms = millis();
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
