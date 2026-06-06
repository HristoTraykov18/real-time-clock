// Real time clock software
// Developed by Hristo Traykov, NEON.BG (Sofia)
// Current version 1.9.x
// DO NOT FORGET TO SETUP PROPERLY IN TOOLS
// USE FLOAT FIRMWARE
// 
// ESP8266 core version 3.1.2
// LittleFS version 1.5.2
// TM1637 library is edited
// RTClib version 2.1.4
// DallasTemperature version 3.9.0

#include "MSVDC.h" // Modules' Specific Variables, Definitions and Constants

// For info messages like current time, date, responses sent to the server, etc.
#define RTC_INFO_MESSAGES

// --- Used when the RTC becomes unsynchronised --- //
#define SDA_LOW()   (GPES = (1 << SDA))
#define SDA_HIGH()  (GPEC = (1 << SDA))
#define SCL_LOW()   (GPES = (1 << SCL))
#define SCL_HIGH()  (GPEC = (1 << SCL))
#define SDA_READ()  ((GPI & (1 << SDA)) != 0)

bool autoUpdateTime(bool force_update=false);
bool isDaylightSavingPeriod(time_t epoch_val=-1);

void setup() {
  WiFi.disconnect();
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  wifi_country_t country_settings = { "EU", 1, 13, WIFI_COUNTRY_POLICY_MANUAL };
  wifi_set_country(&country_settings);

#ifdef  RTC_INFO_MESSAGES
  Serial.begin(115200); // Serial monitor
  Serial.println("\n\n");
#endif

  pinMode(LED_PIN, OUTPUT);
  initializeModuleRTC(); // Initial function
  initializeFileSystem(); // Initial function
  initializeServers(); // Initial function
  daylight_saving_active = isDaylightSavingPeriod();

#ifdef  GPS_MODULE
  gpsSerial.begin(GPS_BAUD_RATE); // Start the GPS connection through SoftwareSerial library
#endif

#ifdef  TEMPERATURE_MODULE
  temperatureSensor.requestTemperatures();
  current_temperature = temperatureSensor.getTempCByIndex(0);
#endif

#ifdef  RTC_INFO_MESSAGES
  Serial.print(F("MAC: "));
  Serial.println(WiFi.macAddress());
#endif

  tm1637.setBrightness(display_brightness); // Set brightness of the 7-digit display (TM1637)
  initializeSoftAP(); // Initial function
  radio_settle_until_ms = millis() + RADIO_SETTLE_PERIOD;

  apConnectHandler = WiFi.onSoftAPModeStationConnected(
    [](const WiFiEventSoftAPModeStationConnected&) {
      ap_station_associated = true;
      last_http_activity_ms = millis();

#ifdef  RTC_INFO_MESSAGES
      Serial.println(F("\n=== Device connected to softAP ==="));
#endif
  });

  apDisconnectHandler = WiFi.onSoftAPModeStationDisconnected(
    [](const WiFiEventSoftAPModeStationDisconnected&) {
      evictStaleStations();
      ap_station_associated = false;
      radio_settle_until_ms = millis() + RADIO_SETTLE_PERIOD;

#ifdef  RTC_INFO_MESSAGES
      Serial.println(F("\n=== Device disconnected from softAP ==="));
#endif
  });

  delay(RADIO_SETTLE_PERIOD);

  if (networkReconnect() == WL_CONNECTED) { // Additional function
    autoUpdateTime(true); // Additional function
    displayClockJustUpdated(false); // Additional function
  };
}

void loop() {
  server.handleClient();
  if (software_update_server_active) softwareUpdateServer.handleClient();
  yield();
  second_now = rtc.now().second();

  if (second_now != last_second) {
    if (work_mode_is_timer)
      timerCountdown(); // Additional function
    else {
      if (autoUpdateTime())
        displayClockJustUpdated(false);
    }

    visualizeOnDisplay(); // Additional function

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("WiFi status: "));
    Serial.print(WiFi.status());
    Serial.print(F(", "));
    Serial.print(F("Connected devices: "));
    Serial.println(WiFi.softAPgetStationNum());
#endif

    checkForUserConnection(); // Additional function
    manageStaReconnect(); // Additional function
  }
}
