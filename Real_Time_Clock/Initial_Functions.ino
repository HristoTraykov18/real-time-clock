
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
        daylight_saving_active = elValue == "true";
        break;

      case 2:
#ifdef  GPS_MODULE
        set_time_with_gps = elValue == "gps";
#endif
        break;

      case 3:
        auto_brightness = elValue == "true";
        break;

      case 4:
        display_brightness = elValue.toInt();

        if (!auto_brightness)
          last_display_brightness = display_brightness;

        break;

      case 5:
        timezone = elValue.toInt();
        break;
    }
  }
}

// ------------------------------------------- Initialize and setup ESP file system ------------------------------------------ //
void initializeFileSystem() {
  LittleFS.begin();
  initializeNetworkReconnect();
  getInitialClockSettings();
}

// ---------------------------------------------- Initialize the RTC module ------------------------------------------------- //
void initializeModuleRTC() {
  tm1637.setBrightness(DEFAULT_BRIGHTNESS); // Set default brightness
  uint8_t rectangle[] = {(SEG_A | SEG_D | SEG_E | SEG_F), 
                         (SEG_A | SEG_D), 
                         (SEG_A | SEG_D), 
                         (SEG_A | SEG_B | SEG_C | SEG_D)};

  tm1637.setSegments(rectangle); // Show rectangle by default in case the RTC does not work

  while (!rtc.begin()) { // If the RTC is not found do not boot
#ifdef  RTC_INFO_MESSAGES
    Serial.println(F("\nCouldn't find RTC"));
#endif

    resetRTC(); // Sometimes the RTC becomes unsynchronised while switching power source - reset it
  }

  while (rtc.now().hour() > 23 || rtc.now().minute() > 59 || rtc.now().second() > 59) {
    resetRTC();
  }
}

// ---------------------------------------------- File content reading function ---------------------------------------------- //
void initializeNetworkReconnect() {
  if (LittleFS.exists("creds.txt")) {
    String network_name = ""; // Global variable
    String network_pass = ""; // Global variable
    File f = LittleFS.open("creds.txt", "r");
    bool is_pass = false;

    while (f.available()) {
      char current_char = char(f.read());

      if (current_char == '\n') {
        if (is_pass)
          break;

        is_pass = true;

        continue;
      }

      if (is_pass)
        network_pass += current_char;
      else
        network_name += current_char;
    }

    f.close();
    connectClockToNetwork(network_name.c_str(), network_pass.c_str());
  }
#ifdef  RTC_INFO_MESSAGES
  else
    Serial.println(F("No creds.txt file"));
#endif
}

// ---------------------------------------------------- Initialize server --------------------------------------------------- //
void initializeServers() {
  udp.begin(2390);
  server.on("/", handleWebInterface); // 192.168.4.1
  server.on("/neonLogoIcon.ico", [] () { streamFileToServer("/neonLogoIcon.ico", "image/x-icon"); });
  server.on("/mainStyle.css", [] () { streamFileToServer("/mainStyle.css", "text/css"); });
  server.on("/mainScript.js", [] () { streamFileToServer("/mainScript.js", "text/javascript"); });
  server.on("/settings", [] () { streamFileToServer("/espSettings.xml", "text/xml"); });
  server.on("/getAudioVolume", [] () { streamFileToServer("/audioVolume.txt", "text/plain"); });
  server.on("/ip", sendIP);
  server.on("/reset", [] () {
  streamFileToServer("/index.html", "text/html"); // Show main page
    initializeModuleRTC();
  });

  const char *UPDATE_PATH = "/sourceControl";
  const char *UPDATE_UNAME = "ghost";
  const char *UPDATE_PASS = "m%O0gsLKOkDl";

  // server.onNotFound(HandleNotFoundWebRequests); // If path is not found we can handle by URI
  server.begin(); // Local web server
  httpUpdater.setup(&softwareUpdateServer, UPDATE_PATH, UPDATE_UNAME, UPDATE_PASS); // Set software update server
  softwareUpdateServer.begin(); // and start it
}
