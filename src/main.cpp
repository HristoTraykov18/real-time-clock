
// Real time clock software
// Developed by Hristo Traykov, NEON.BG (Sofia)
// Current version 1.11.x
// DO NOT FORGET TO SETUP PROPERLY IN TOOLS
// USE FLOAT FIRMWARE
//
// ESP8266 core version 3.1.2
// LittleFS version 1.5.2
// TM1637 library is edited
// RTClib version 2.1.4
// DallasTemperature version 4.0.6

#include "Globals.h" // Global definitions, constants, variables, and core library imports

#include "CoreUtils.h"
#include "BrightnessModule.h"
#include "ServerUtils.h"

void setup() {
  WiFi.disconnect();
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  wifi_country_t country_settings = { "EU", 1, 13, WIFI_COUNTRY_POLICY_MANUAL };
  wifi_set_country(&country_settings);

  if constexpr (DEBUG_MESSAGES) {
    Serial.begin(115200); // Serial monitor
    Serial.println("\n\n");
  }

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  initializeModuleRTC(); // Core Utils
  initializeFileSystem(); // Core Utils
  initializeServers(); // Server Utils

  daylight_saving_active = daylight_saving_enabled && isDaylightSavingPeriod();

  if constexpr (HAS_GPS_MODULE) {
    if (set_time_with_gps)
      activateGPS(); // GPS module function
  }

  if constexpr (HAS_TEMPERATURE_MODULE) {
    temperatureSensor.begin(); // Enumerate the 1-Wire bus. Required before reading by index
    temperatureSensor.requestTemperatures();
    current_temperature = temperatureSensor.getTempCByIndex(0);
  }

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("MAC: "));
    Serial.println(WiFi.macAddress());
  }

  tm1637.setBrightness(display_brightness); // Set brightness of the 7-digit display (TM1637)
  initializeSoftAP(); // Network Utils
  radio_settle_until_ms = millis() + RADIO_SETTLE_PERIOD;

  apConnectHandler = WiFi.onSoftAPModeStationConnected(
    [](const WiFiEventSoftAPModeStationConnected&) {
      ap_station_associated = true;
      last_http_activity_ms = millis();

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("\n=== Device connected to softAP ==="));
  });

  apDisconnectHandler = WiFi.onSoftAPModeStationDisconnected(
    [](const WiFiEventSoftAPModeStationDisconnected&) {
      evictStaleStations();
      ap_station_associated = false;
      radio_settle_until_ms = millis() + RADIO_SETTLE_PERIOD;

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("\n=== Device disconnected from softAP ==="));
  });

  staDisconnectHandler = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected& event) {
      sta_disconnect_count++;

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("\n=== ESP disconnected from STA ==="));
  });

  delay(RADIO_SETTLE_PERIOD);

  if (networkReconnect() == WL_CONNECTED) // Network Utils
    autoUpdateTime(true); // Core Utils
}

void loop() {
  server.handleClient();
  if (software_update_server_active) softwareUpdateServer.handleClient();

  if constexpr (HAS_GPS_MODULE)
    serviceGPS(); // GPS module function

  yield();
  second_now = rtc.now().second();

  if (second_now != last_second) {
    if (work_mode_is_timer)
      timerCountdown(); // Core Utils
    else
      autoUpdateTime(); // Core Utils

    visualizeOnDisplay(); // Display Utils

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("WiFi status: "));
      Serial.print(WiFi.status());
      Serial.print(F(", "));
      Serial.print(F("Connected devices: "));
      Serial.println(WiFi.softAPgetStationNum());
    }

    checkForUserConnection(); // Network Utils
    manageStaReconnect(); // Network Utils
  }
}
