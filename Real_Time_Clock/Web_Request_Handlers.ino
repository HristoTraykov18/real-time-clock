
// _____________________________________________ Web interface handling functions _____________________________________________ //
void handleWebInterface() { // Handler for the main interface
  if (server.hasArg("confirmResponse")) {
    awaiting_confirmation = false;
    response_message = ""; // Global variable
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("Response confirmed"));
#endif
  }
  else if (server.hasArg("resentRequest")) {
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("Message resend requested"));
#endif
    sendWebpageResponse(response_message.c_str());
  }
  else if (server.arg("timeSyncMode") == "wifi") {
    String ssid = server.arg("ssid");

    if (WiFi.status() == WL_CONNECTED) {
      if (ssid == WiFi.SSID()) {
        sendWebpageResponse(("Часовникът вече е свързан с мрежа " + ssid + " и се свери през Интернет").c_str());
        time_update_pending = true;
      }
      else {
        sendWebpageResponse(("Часовникът беше сверен през Интернет. Настоящата мрежа е " + WiFi.SSID()).c_str());
        time_update_pending = true;
      }
    }
    else if (ssid != "" && (server.arg("pass")).length() + 1 > 7) {
      validateNetworkInput(server.arg("pass"), ssid);
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
#ifdef  GPS_MODULE
  else if (server.arg("timeSyncMode") == "gps") {
    activateGPS(); // GPS module function
    sendWebpageResponse("Часовникът ще се свери чрез GPS");
    editTimeSyncMode("gps");
    time_update_pending = true;
  }
#endif
  else if (server.arg("timeSyncMode") == "js") { // If the user opened the Web UI, update the time if needed
    if (WiFi.status() != WL_CONNECTED) {
      manualTimeUpdate();
      sendWebpageResponse("Часовникът се свери автоматично от устройството Ви");
    }
    else {
      sendWebpageResponse(("Часовникът беше сверен през Интернет. Настоящата мрежа е " + WiFi.SSID()).c_str());
      time_update_pending = true;
    }
  }
  else if (server.hasArg("daylightSavingEnabled")) {
    editDaylightSavingEnabled(server.arg("daylightSavingEnabled").c_str());
    sendWebpageResponse("Промените са запазени");
  }
  else if (server.hasArg("autoBrightnessControl")) {
    if (server.arg("autoBrightnessControl") == "false") {
      editManualBrightness(server.arg("manualBrightnessLevel").c_str());
      display_brightness = server.arg("manualBrightnessLevel").toInt();
    }
    else
      display_brightness = DEFAULT_BRIGHTNESS;

    editAutoBrightness(server.arg("autoBrightnessControl").c_str());
    sendWebpageResponse("Промените са запазени");
  }
  else if (server.arg("timeSyncMode") == "gps") {
    sendWebpageResponse("Часовникът няма инсталиран GPS модул");
  }
  else {
    streamFileToServer("/index.html", "text/html"); // Show main page at the begining
  }

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

void sendIP() { // Handler for IP address page
  server.send(200, "text/plain", WiFi.localIP().toString().c_str());
#ifdef  RTC_INFO_MESSAGES
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());
#endif
}

void sendWebpageResponse(const char *webpage_response) { // Send info to the server
  server.send(200, "text/plain", webpage_response);
  server.handleClient();

#ifdef  RTC_INFO_MESSAGES
  Serial.println(webpage_response);
#endif

  if (!awaiting_confirmation) {
    awaiting_confirmation = true;
    response_message = webpage_response;
  }
}

void streamFileToServer(const char *filename, const char *filestream_format) {
  File data_file = LittleFS.open(filename, "r");

  server.streamFile(data_file, filestream_format);
  data_file.close();
}

void validateNetworkInput(const String& pass, const String& ssid) {
  uint8_t number_of_networks = WiFi.scanNetworks(); // Scan networks in area
  bool is_network_in_range = false;

  for (uint8_t i = 0; i < number_of_networks; i++) {
#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("Network request: "));
    Serial.print(ssid);
    Serial.print(F(" | Network in range: "));
    Serial.println(WiFi.SSID(i));
#endif

    if (ssid == WiFi.SSID(i)) {
      is_network_in_range = true;

      if (connectClockToNetwork(ssid, pass)) {
        sendWebpageResponse(("Часовникът се свърза с мрежа " + ssid + ".\nIP: " + WiFi.localIP().toString()).c_str());
        break;
      }
      else
        sendWebpageResponse("Времето за опит за свързване изтече. Проверете името и паролата.");
    }
  }

  if (!is_network_in_range)
    sendWebpageResponse(("Мрежата " + ssid + " не е в обхват").c_str());
}
