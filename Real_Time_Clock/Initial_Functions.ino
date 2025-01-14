
// ____________________________________________________ Initial functions _____________________________________________________ //

// -------------------------------------- Get infomation from the xml file in the ESP ------------------------------------- //
void getInitialClockSettings() {
  String settings_file = readFileToString("/espSettings.xml");
  String elValue = "";
  uint8_t tagsCount = sizeof(START_TAGS) / 4;

  for (int i = 0; i < tagsCount; i++) {
    elValue = settings_file.substring(settings_file.indexOf(START_TAGS[i]) + strlen(START_TAGS[i]), settings_file.indexOf(END_TAGS[i]));

    switch (i) {
      case 0:
        daylight_saving_enabled = elValue == "true";
        break;

      case 1:
#ifdef  GPS_MODULE
        set_time_with_gps = elValue == "gps";
#endif
        break;

      case 2:
        auto_brightness = elValue == "true";
        break;

      case 3:
        display_brightness = elValue.toInt();

        if (!auto_brightness)
          last_display_brightness = display_brightness;

        break;

      case 4:
        timezone = elValue.toInt();
        break;
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
  networkReconnect();
  getInitialClockSettings();
}

// ---------------------------------------------- Initialize the RTC module ------------------------------------------------- //
void initializeModuleRTC() {
  uint8_t rectangle[] = {(SEG_A | SEG_D | SEG_E | SEG_F), 
                         (SEG_A | SEG_D), 
                         (SEG_A | SEG_D), 
                         (SEG_A | SEG_B | SEG_C | SEG_D)};

  tm1637.setBrightness(DEFAULT_BRIGHTNESS); // Set default brightness
  tm1637.setSegments(rectangle); // Show rectangle by default in case the RTC does not work

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
  server.on("/neonLogoIcon.ico", [] () { streamFileToServer("/neonLogoIcon.ico", "image/x-icon"); });
  server.on("/mainStyle.css", [] () { streamFileToServer("/mainStyle.css", "text/css"); });
  server.on("/mainScript.js", [] () { streamFileToServer("/mainScript.js", "text/javascript"); });
  server.on("/settings", [] () { streamFileToServer("/espSettings.xml", "text/xml"); });
  server.on("/ip", sendIP);
  server.on("/reset", [] () {
    streamFileToServer("/index.html", "text/html"); // Show main page
    initializeModuleRTC();
  });

  const char *UPDATE_PATH = "/sourceControl";
  const char *UPDATE_UNAME = "ghost";
  const char *UPDATE_PASS = "m%O0gsLKOkDl";

  server.begin();
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("Web server started"));
#endif
  httpUpdater.setup(&softwareUpdateServer, UPDATE_PATH, UPDATE_UNAME, UPDATE_PASS);
  softwareUpdateServer.begin();
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("Software update server started"));
#endif
}
