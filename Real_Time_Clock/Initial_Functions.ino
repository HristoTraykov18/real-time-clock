
// ____________________________________________________ Initial functions _____________________________________________________ //

// -------------------------------------- Get infomation from the xml file in the ESP ------------------------------------- //
void getInitialClockSettings() {
  char buf[512];
  File f = LittleFS.open("/espSettings.xml", "r");
  size_t len = f.read((uint8_t*)buf, sizeof(buf) - 1);
  f.close();
  buf[len] = '\0';

  uint8_t tagsCount = sizeof(START_TAGS) / sizeof(START_TAGS[0]);
  char val[16]; // Longest stored value is "3600" or "false" - 5 chars; 16 is generous

  for (uint8_t i = 0; i < tagsCount; i++) {
    char *p = strstr(buf, START_TAGS[i]);

    if (!p) continue;

    p += strlen(START_TAGS[i]);

    char *end = strstr(p, END_TAGS[i]);
    if (!end) continue;

    size_t val_len = end - p;

    if (val_len >= sizeof(val)) val_len = sizeof(val) - 1;
  
    memcpy(val, p, val_len);
    val[val_len] = '\0';

    switch (i) {
      case 0: daylight_saving_enabled = strcmp(val, "true") == 0; break;
      case 1:
#ifdef GPS_MODULE
        set_time_with_gps = strcmp(val, "gps") == 0;
#endif
        break;
      case 2: auto_brightness = strcmp(val, "true") == 0; break;
      case 3:
        display_brightness = atoi(val);

        if (!auto_brightness) last_display_brightness = display_brightness;
        break;
      case 4: timezone = atoi(val); break;
      case 5: work_mode_is_timer = strcmp(val, "timer") == 0; break;
      case 6: timer_duration = atoi(val); break;
    }
  }
}

// ------------------------------------------- Initialize and setup ESP file system ------------------------------------------ //
void initializeFileSystem() {
  while (!LittleFS.begin()) {
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("Failed to initialize file system"));
#endif
  }
  getInitialClockSettings();
}

// ---------------------------------------------- Initialize the RTC module ------------------------------------------------- //
void initializeModuleRTC() {
  tm1637.setBrightness(DEFAULT_BRIGHTNESS); // Set default brightness
  tm1637.showNumber(8888, 32); // Test all segments

  while (!rtc.begin()) { // If the RTC is not found do not boot
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("\nInitializing RTC"));
#endif

    resetRTC(); // Sometimes the RTC becomes unsynchronised while switching power source - reset it
  }

  while (rtc.now().hour() > 23 || rtc.now().minute() > 59 || rtc.now().second() > 59) {
    resetRTC();
  }
}

// ---------------------------------------------- Initialize server --------------------------------------------- //
void initializeServers() {
  while (!udp.begin(2390)) {
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("Initializing UDP for NTP"));
#endif
  }
  server.on("/", handleWebInterface); // 192.168.4.1
  server.on("/info", sendClockInfo);
  server.on("/additional-settings", sendAdditionalSettings);
  server.on("/delete-creds", handleDeleteCreds);
  server.on("/activate-update", handleActivateSoftwareUpdate);
  server.on("/neonLogoIcon.ico", [] () { streamFileToServer("/neonLogoIcon.ico", "image/x-icon"); });
  server.on("/mainStyle.css", [] () { streamFileToServer("/mainStyle.css", "text/css"); });
  server.on("/mainScript.js", [] () { streamFileToServer("/mainScript.js", "text/javascript"); });
  server.on("/settings", [] () { streamFileToServer("/espSettings.xml", "text/xml"); });
  server.on("/m", handleDeviceMonitoring);
  server.on("/reset", [] () {
    streamFileToServer("/index.html", "text/html"); // Show main page
    initializeModuleRTC();
  });
  server.begin();

  const char *UPDATE_PATH = "/sourceControl";
  const char *UPDATE_UNAME = "ghost";
  const char *UPDATE_PASS = "m%O0gsLKOkDl";

  httpUpdater.setup(&softwareUpdateServer, UPDATE_PATH, UPDATE_UNAME, UPDATE_PASS);
  softwareUpdateServer.onNotFound([UPDATE_PATH] () {
    softwareUpdateServer.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    softwareUpdateServer.send(302, "text/plain", "");
  });
#ifdef  RTC_INFO_MESSAGES
  Serial.println(F("Web server started"));
  Serial.println(F("Software update server configured (not started)"));
#endif
}
