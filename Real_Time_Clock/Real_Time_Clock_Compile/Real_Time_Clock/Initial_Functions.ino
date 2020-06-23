
// ____________________________________________________ Initial functions _____________________________________________________ //

// -------------------------- Check if the RTC is connected and if it is unsynchronised - reset it ----------------------------- //
void InitialRTC_Check() {
  tm1637.setBrightness(DEFAULT_BRIGHTNESS); // Set default brightness
  uint8_t rtcError[] = {(SEG_A | SEG_D | SEG_E | SEG_F), 
                        (SEG_A | SEG_D), 
                        (SEG_A | SEG_D), 
                        (SEG_A | SEG_B | SEG_C | SEG_D)};
  tm1637.setSegments(rtcError); // Show 'Err' message by default in case the RTC does not work
  
  while (!rtc.begin()) { // If the RTC is not found do not boot
    Reset_RTC(); // Sometimes the RTC becomes unsynchronised while switching power source - reset it
  }
  
  while ((rtc.now().hour() == 165 && rtc.now().minute() == 165 && (rtc.now().second() == 85 || rtc.now().second() == 65)) ||
         (rtc.now().hour() == 0 && rtc.now().minute() == 0 && rtc.now().second() == 0)  || (rtc.now().hour() == 6 && rtc.now().minute() == 65)) {
    delay(250);
    Reset_RTC();
    delay(250);
  }
}

// ---------------------------------------------------- Initialize server --------------------------------------------------- //
void ServersInitialization() {
  udp.begin(2390); // UDP
  
  server.on("/", HandleWebInterface); // 192.168.4.1
  server.on("/neonLogoIcon.ico", SendLogoIcon);
  server.on("/mainStyle.css", SendStyling);
  server.on("/Lock.png", SendLockImage);
  server.on("/mainScript.js", SendScript);
  server.on("/espSettings.xml", SendSettings);
  server.on("/ip", SendIP);
  // server.onNotFound(HandleNotFoundWebRequests); // If path is not found we can handle by URI
  server.begin(); // Local web server

  httpUpdater.setup(&updateServer, UPDATE_PATH, UPDATE_UNAME, UPDATE_PASS); // Set software update server
  updateServer.begin(); // and start it
}

// ------------------------------------------- Initialize and setup ESP file system ------------------------------------------ //
void FileSystemInitialization() {
  LittleFS.begin();

  if (hasGPS && LittleFS.exists("/indexGPS.html")) {
    LittleFS.remove("/indexNoGPS.html");
    LittleFS.rename("/indexGPS.html", "/networkSetup.html");
  }
  else if (!hasGPS && LittleFS.exists("/indexNoGPS.html")) {
    LittleFS.remove("/indexGPS.html");
    LittleFS.rename("/indexNoGPS.html", "/networkSetup.html");
  }
  else if (LittleFS.exists("/networkSetup.html")) {
  }
  else {
  }

  webpageSetup = ReadPage("/networkSetup.html");
  mainStyling = ReadPage("/mainStyle.css");
  mainScript = ReadPage("/mainScript.js");
  GetClockSettings();
}
