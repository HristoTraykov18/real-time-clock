
// _____________________________________________ Web interface handling functions _____________________________________________ //
void HandleWebInterface() { // Handler for the main interface
  if (server.arg("timeSyncMode") == "auto") {
    if (server.arg("setTimeWith") == "gps") {
      timeUpdatePending = true;
      
      if (hasGPS) {
        GPS_connectionTriesLeft = 30;
        gpsSerial.begin(GPS_BAUD_RATE);
      }

      SendWebpageResponse("Часовникът ще се свери чрез GPS");
      overrideSettings = true;
    }
    else if (server.arg("setTimeWith") == "wifi") {
      int ssidValueLen = (server.arg("ssid")).length() + 1; // Store input ssid length
      char ssid[ssidValueLen];
      (server.arg("ssid")).toCharArray(ssid, ssidValueLen);
      
      if (WiFi.status() == WL_CONNECTED && (String)ssid == WiFi.SSID()) {
        SendWebpageResponse(("Часовникът вече е свързан към " + (String)ssid).c_str());
        UpdateTimeFromNTP();
      }
      else if ((String)ssid != "" && (server.arg("pass")).length() + 1 > 7) { // Check if the input is valid
        int passValueLen = (server.arg("pass")).length() + 1; // Store input password length
  
        // Make SSID and password char arrays and fill them with the input values
        char pass[passValueLen];
        (server.arg("pass")).toCharArray(pass, passValueLen);
  
        int numberOfNetworks = WiFi.scanNetworks(); // Scan networks in area
        bool isNetworkInRange = false;
        
        for (int i = 0; i < numberOfNetworks; i++) {
          
          if ((String)ssid == WiFi.SSID(i)) {
            isNetworkInRange = true;
            WiFi.begin(ssid, pass); // Check if the user's network is in range and try connecting to it
            
            for (int i = 0; i < CONNECT_TO_NETWORK_LOOP_COUNT; i++) {
              if (WiFi.status() == WL_CONNECTED) {
                SendWebpageResponse(("Часовникът се свърза с " + (String)ssid + ".\nIP: " + WiFi.localIP().toString()).c_str());
                timeUpdatePending = true;
                break;
              }
              
              if (i == CONNECT_TO_NETWORK_LOOP_COUNT - 1) {
                SendWebpageResponse("Времето за опит за свързване изтече. Проверете името и паролата");
                break;
              }
              
              delay(CONNECT_TO_NETWORK_LOOP_DELAY);
            }
          }
        }
        
        if (!isNetworkInRange)
          SendWebpageResponse(("The network " + (String)ssid + " is not in range").c_str());
      }
      else if (WiFi.status() == WL_CONNECTED) {
        SendWebpageResponse("Промените са запазени и времето е сверено по Интернет");
        UpdateTimeFromNTP();
      }
      else
        SendWebpageResponse("Промените са запазени");
      
      GPS_connectionTriesLeft = 0;
      overrideSettings = true;
    }
    else
      SendWebpageResponse("Промените са запазени");
  }
  else if (server.arg("timeSyncMode") == "manual") { // If the user used the manual time set option - set the new time
    int manualTime = (server.arg("manualTimeValue")).toInt();
    rtc.adjust(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), manualTime / 100, manualTime % 100, 0));
    overrideSettings = true;

    SendWebpageResponse("Промените са запазени и времето е ръчно настроено");
  }
  else
    server.send(200, "text/html", webpageSetup); // Show main page at the begining

  if (overrideSettings) {
    
    EditClockSettings((server.arg("daylightSaving") == "ON"), (server.arg("setTimeWith") == "gps"), (server.arg("brightnessControl") == "auto"),
                       server.arg("brightnessValue").toInt(), "2", (WiFi.localIP().toString()).c_str()); // Edit tz!
    overrideSettings = false;
    GetClockSettings();
  }
}

void SendIP() { // Handler for IP address page
  server.send(200, "text/plain", WiFi.localIP().toString());
}

void SendLockImage() { // Handler for lock image
  File dataFile = LittleFS.open("/Lock.png", "r");
  server.streamFile(dataFile, "image/png");
  dataFile.close();
}

void SendLogoIcon() { // Handler for browser tab icon
  File dataFile = LittleFS.open("/neonLogoIcon.ico", "r");
  server.streamFile(dataFile, "image/x-icon");
  dataFile.close();
}

void SendWebpageResponse(const char* webpageResponse) {
  server.send(200, "text/plain", webpageResponse);
  server.handleClient();
}

void SendScript() { // Handler for javascript
  server.send(200, "text/javascript", mainScript);
}

void SendSettings() { // Handler for user personalizations (xml settings)
  server.send(200, "text/xml", settingsFile);
}

void SendStyling() { // Handler for styling
  server.send(200, "text/css", mainStyling);
}
