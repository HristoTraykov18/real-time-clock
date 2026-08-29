// ServerUtils.cpp
#include "ServerUtils.h"

// _____________________________________________ Web interface handling functions _____________________________________________ //

static char response_buf[256]; // 256 is enough to keep the longest response message


const char* createReply(const char* prefix, const char* ssid, bool time_updated) {
  snprintf(response_buf, sizeof(response_buf), "%s %s.\n%s", prefix, ssid,
           time_updated ? "Успешна актуализация на времето през Интернет!"
                        : "Неуспешна актуализация на времето, моля опитайте отново!");

  return response_buf;
}


void handleActivateSoftwareUpdate() {
  if (!software_update_server_active) {
    softwareUpdateServer.begin();
    software_update_server_active = true;

    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("OTA update server started"));
  }

  sendWebpageResponse("OK");
}


const char* handleBrightnessControl() {
  const String& auto_brightness_control = server.arg("autoBrightnessControl");

  if (auto_brightness_control == "false") {
    const String& manual_brightness_level = server.arg("manualBrightnessLevel");

    editManualBrightness(manual_brightness_level.c_str());
    display_brightness = manual_brightness_level.toInt();
  }
  else
    display_brightness = DEFAULT_BRIGHTNESS;

  editAutoBrightness(auto_brightness_control.c_str());

  return "Настройката за яркост е запазена!";
}


void handleDeleteCreds() {
  const bool had_creds = LittleFS.exists("creds.txt");
  sendWebpageResponse(had_creds ? "Запазената мрежа е изтрита!" : "Няма запазена мрежа!");

  if (had_creds) {
    LittleFS.remove("creds.txt");
    WiFi.disconnect(true);
  }
}


void handleDeviceMonitoring() {
  snprintf(response_buf, sizeof(response_buf),
    "Max free block size: %u\nCurrent Free Heap: %u\nHeap fragmentation: %u",
    (unsigned) ESP.getMaxFreeBlockSize(),
    (unsigned) ESP.getFreeHeap(),
    (unsigned) ESP.getHeapFragmentation());
  sendWebpageResponse(response_buf);
}


void handleExtendSession() {
  sendWebpageResponse("Успешно удължихте сесията си!");
  last_http_activity_ms = millis();
}


const char* handleGPSTimeSync() {
  if constexpr (HAS_GPS_MODULE) {
    editTimeSyncMode("gps");
    uint8_t attempt = 0;

    while (attempt < GPS_MAX_CONNECT_ATTEMPTS) {
      serviceGPS();

      if (autoUpdateTime(true))
        return "Успешно сверяване през GPS.";

      attempt++;
      delay(500);
    }

    return "Неуспешно сверяване през GPS.\nУверете се, че часовникът е на открито и опитайте отново.";
  }
  else
    return "Часовникът няма инсталиран GPS модул.";
}


const char* handleManualTimeSync() {
  char ssid_buf[33]; // Max SSID len (32) + NUL = 33
  currentSsid(ssid_buf);

  if (networkReconnect() == WL_CONNECTED)
    return createReply("Часовникът е свързан с мрежа", ssid_buf, autoUpdateTime(true));

  if (set_time_with_gps)
    return handleGPSTimeSync();

  manualTimeUpdate();

  if (!LittleFS.exists("creds.txt"))
    return "Часовникът се свери автоматично от устройството Ви.";

  snprintf(response_buf, sizeof(response_buf),
    "Неуспешно свързване със запаметената мрежа %s!\n"
    "Часовникът се свери автоматично от устройството Ви.", ssid_buf);

  return response_buf;
}


void handleSessionTimeout() {
  sendWebpageResponse("Сесията Ви изтече!\nЗа да използвате настройките, моля свържете се с часовника отново!");
  evictStaleStations();
}


const char* handleTimerControl() {
  const String& status_val = server.arg("status");

  if (status_val == "start") {
    editTimerDuration(server.arg("duration").c_str());

    return "Таймерът е стартиран!";
  }
  else if (status_val == "pause") {
    timer_millis_offset = millis() - timer_millis;
    timer_status = 0;

    return "Таймерът е паузиран!";
  }
  else if (status_val == "resume") {
    timer_millis = millis() - timer_millis_offset;
    timer_status = 1;

    return "Таймерът е рестартиран!";
  }
  else
    return "Невалидна команда за таймер";
}


void handleWebInterface() {
  const char* response = nullptr;
  const String& time_sync_mode = server.arg("timeSyncMode");
  const String& daylight_saving = server.arg("daylightSavingEnabled");
  const String& work_mode = server.arg("workMode");

  if (work_mode.length() > 0)
    editWorkMode(work_mode.c_str());

  if (time_sync_mode == "wifi")
    response = handleWifiTimeSync(server.arg("ssid"));
  else if (time_sync_mode == "gps")
    response = handleGPSTimeSync();
  else if (time_sync_mode == "js")
    response = handleManualTimeSync();
  else if (daylight_saving.length() > 0) {
    editDaylightSavingEnabled(daylight_saving.c_str());
    response = "Промените са запазени.";
  }
  else if (server.hasArg("autoBrightnessControl"))
    response = handleBrightnessControl();
  else if (server.hasArg("status") && work_mode_is_timer)
    response = handleTimerControl();
  else
    streamFileToClient("/index.html", "text/html"); // Show main page at the begining

  if (response)
    sendWebpageResponse(response);

  if (server.hasArg("timezoneHoursOffset"))
    editTimezoneOffset(server.arg("timezoneHoursOffset").c_str());

  if constexpr (DEBUG_MESSAGES) {
    uint8_t argCount = server.args();

    for (uint8_t i = 0; i < argCount; i++) {
      Serial.print(server.argName(i));
      Serial.print(F(" = "));
      Serial.println(server.arg(i));
    }
  }
}


const char* handleWifiTimeSync(const String& ssid) {
  const char* response;
  char ssid_buf[33]; // Max SSID len (32) + NUL = 33
  currentSsid(ssid_buf);

  const bool has_new_credentials = ssid != "" && (server.arg("pass")).length() > 7;

  if (WiFi.status() != WL_CONNECTED) {
    if (has_new_credentials)
      response = validateNetworkInput(ssid, server.arg("pass"), server.arg("isHiddenNetwork"));
    else if (set_time_with_gps)
      response = "Промените са запазени.";
    else
      response = "Моля въведете име и парола на мрежата!";
  }
  else if (currentSsidEquals(ssid.c_str()))
    response = createReply("Часовникът вече е свързан с мрежа", ssid_buf, autoUpdateTime(true));
  else if (has_new_credentials)
    response = validateNetworkInput(ssid, server.arg("pass"), server.arg("isHiddenNetwork"));
  else
    response = createReply("Настоящата мрежа е", ssid_buf, autoUpdateTime(true));

  editTimeSyncMode("wifi");

  return response;
}


void initializeServers() {
  while (!udp.begin(2390)) {
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("Initializing UDP for NTP"));
  }

  const char* header_keys[] = { "Range" };
  const size_t header_keys_count = 1;

  server.collectHeaders(header_keys, header_keys_count);
  server.on("/", handleWebInterface); // 192.168.4.1 & IP in connected network
  server.on("/info", sendClockInfo);
  server.on("/additional-settings", sendAdditionalSettings);
  server.on("/delete-creds", handleDeleteCreds);
  server.on("/timeout", handleSessionTimeout);
  server.on("/extend", handleExtendSession);
  server.on("/activate-update", handleActivateSoftwareUpdate);
  server.on("/body.html", [] () { streamFileToClient("/body.html", "text/html"); });
  server.on("/neonLogoIcon.ico", [] () { streamFileToClient("/neonLogoIcon.ico", "image/x-icon"); });
  server.on("/mainStyle.css", [] () { streamFileToClient("/mainStyle.css", "text/css"); });
  server.on("/mainScript.js", [] () { streamFileToClient("/mainScript.js", "text/javascript"); });
  server.on("/settings", [] () { streamFileToClient("/espSettings.xml", "text/xml"); });
  server.on("/m", handleDeviceMonitoring);
  server.begin();

  const char *UPDATE_PATH = "/sourceControl";
  const char *UPDATE_UNAME = "ghost";
  const char *UPDATE_PASS = "m%O0gsLKOkDl";

  httpUpdater.setup(&softwareUpdateServer, UPDATE_PATH, UPDATE_UNAME, UPDATE_PASS);

  if constexpr (DEBUG_MESSAGES) {
    Serial.println(F("Web server started"));
    Serial.println(F("Software update server configured (not started)"));
  }
}


void sendAdditionalSettings() {
  char response[16];
  snprintf(response, sizeof(response), "%d|%s",
           timezone,
           LittleFS.exists("creds.txt") ? "true" : "false");

  server.send(200, "text/plain", response);
  last_http_activity_ms = millis();

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("Additional settings: "));
    Serial.println(response);
  }
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
  char mac_buf[18]; // XX:XX:XX:XX:XX:XX + NUL = 18
  WiFi.macAddress(mac_raw); // Gets the 6 raw bytes
  snprintf(mac_buf, sizeof(mac_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
          mac_raw[0], mac_raw[1], mac_raw[2], mac_raw[3], mac_raw[4], mac_raw[5]);

  DateTime now = rtc.now();
  char response[90];
  char ssid_buf[33];
  currentSsid(ssid_buf);

  snprintf(response, sizeof(response), "%s|%s%d|%s|%s|%02d:%02d:%02d",
           is_connected ? ssid_buf : "-",
           is_connected ? "" : "-",
           rssi,
           ip_buf,
           mac_buf,
           now.hour(), now.minute(), now.second());

  server.send(200, "text/plain", response);
  last_http_activity_ms = millis();

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("Clock info: "));
    Serial.println(response);
  }
}


void sendNetworksList() {
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {
    unsigned long start_ms = millis();

    while (WiFi.scanComplete() == WIFI_SCAN_RUNNING && millis() - start_ms < 3000) {
      delay(50);
      yield();
    }

    n = WiFi.scanComplete();
  }

  // No cached result available - fall back to a blocking scan
  if (n < 0)
    n = WiFi.scanNetworks(false, false);

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain", "");

  char line_buf[64];

  for (int i = 0; i < n; i++) {
    snprintf(line_buf, sizeof(line_buf), "%s|%d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    server.sendContent(line_buf);
  }

  server.sendContent("");           // End chunked transfer
  WiFi.scanDelete();                // Free scan memory
  WiFi.scanNetworks(true, false);   // Pre-scan for next refresh request

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("Networks sent: "));
    Serial.println(n);
  }
}


void sendWebpageResponse(const char *webpage_response) {
  server.send(200, "text/plain", webpage_response);
  last_http_activity_ms = millis();

  if constexpr (DEBUG_MESSAGES)
    Serial.println(webpage_response);
}


void streamFileToClient(const char *filename, const char *content_type) {
  File data_file = LittleFS.open(filename, "r");
  const size_t file_size  = data_file.size();
  size_t range_start = 0;
  size_t range_end = (file_size > 0) ? file_size - 1 : 0;
  bool is_partial = false;

  // Parse "Range: bytes=START-" or "Range: bytes=START-END"
  if (server.hasHeader("Range")) {
    const String& r = server.header("Range");
    int eq   = r.indexOf('=');
    int dash = (eq > 0) ? r.indexOf('-', eq + 1) : -1;

    if (dash > eq) {
      range_start = r.substring(eq + 1, dash).toInt();
      const String& end_str = r.substring(dash + 1);

      if (end_str.length() > 0)
        range_end = end_str.toInt();

      is_partial = true;
    }

    if (range_start >= file_size || range_end >= file_size || range_start > range_end) {
      char cr_buf[40];
      snprintf(cr_buf, sizeof(cr_buf), "bytes */%u", (unsigned) file_size);

      server.sendHeader("Content-Range", cr_buf);
      server.send(416, "text/plain", "");
      data_file.close();

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("Range not satisfiable!"));

      return;
    }
  }

  server.sendHeader("Accept-Ranges", "bytes");

  const size_t length = range_end - range_start + 1;
  size_t sent = 0;

  if (is_partial) {
    data_file.seek(range_start);

    char cr_buf[64];
    snprintf(cr_buf, sizeof(cr_buf), "bytes %u-%u/%u", (unsigned) range_start, (unsigned) range_end, (unsigned) file_size);

    server.sendHeader("Content-Range", cr_buf);
    server.setContentLength(length);
    server.send(206, content_type, "");

    WiFiClient client = server.client();
    uint8_t buf[256];
    size_t remaining = length;

    while (remaining > 0 && client.connected()) {
      size_t chunk = (remaining < sizeof(buf)) ? remaining : sizeof(buf);
      int read = data_file.read(buf, chunk);

      if (read <= 0)
        break;

      size_t wrote = client.write(buf, read);
      sent += wrote;

      if (wrote < (size_t)read)
        break;

      remaining -= read;
    }
  }
  else
    sent = server.streamFile(data_file, content_type);

  data_file.close();
  last_http_activity_ms = millis();

  if constexpr (DEBUG_MESSAGES) {
    if (sent != length) {
      Serial.print(F("Incomplete stream: "));
      Serial.print(filename);
      Serial.print(F(" "));
      Serial.print(sent);
      Serial.print(F("/"));
      Serial.println(length);
    }
  }
}


const char* validateNetworkInput(const String& ssid, const String& pass, const String& is_hidden) {
  if (is_hidden == "true" || networkIsInRange(ssid.c_str())) {
    wl_status_t connection_status = connectClockToNetwork(ssid.c_str(), pass.c_str(), is_hidden == "true");

    if (connection_status == WL_CONNECTED) {
      if (autoUpdateTime(true))
        snprintf(response_buf, sizeof(response_buf),
          "Часовникът се свърза с мрежа %s.\nУспешна актуализация на времето през Интернет!",
          ssid.c_str());
      else {
        snprintf(response_buf, sizeof(response_buf),
          "Часовникът се свърза с мрежа %s.\nНеуспешна актуализация на времето, моля опитайте отново!",
          ssid.c_str());
      }
    }
    else if (connection_status == WL_WRONG_PASSWORD)
      return "Грешна парола!\nМоля опитайте отново.";
    else
      snprintf(response_buf, sizeof(response_buf), "Неуспешно свързване с мрежа %s.", ssid.c_str());
  }
  else
    snprintf(response_buf, sizeof(response_buf), "Мрежата %s не е в обхват, или е скрита.", ssid.c_str());

  return response_buf;
}
