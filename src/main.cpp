
// Real time clock software
// Developed by Hristo Traykov, NEON.BG (Sofia)
// Current version 1.13.x
// USE FLOAT FIRMWARE
//
// ESP8266 core version 3.1.2
// ESP32 core version 3.3.11
// LittleFS version 1.5.2
// TM1637 library is edited
// RTClib version 2.1.4
// DallasTemperature version 4.0.6

#include "Globals.h" // Global definitions, constants, variables, and core library imports

#include "CoreUtils.h"
#include "BrightnessModule.h"
#include "EspUtils.h"
#include "ServerUtils.h"

void setup() {
  if constexpr (DEBUG_MESSAGES) {
    Serial.begin(115200); // Serial monitor
    Serial.println("\n\n");
  }

  pinMode(LED_PIN, OUTPUT);
  LED_OFF();
  initializePeripherals(); // Core Utils
  initializeFileSystem(); // Core Utils
  tm1637.setBrightness(display_brightness); // Set brightness of the 7-digit display (TM1637)

  daylight_saving_active = daylight_saving_enabled && isDaylightSavingPeriod();

  if constexpr (HAS_GPS_MODULE) {
    if (set_time_with_gps)
      activateGPS(); // GPS module function
  }

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_AP_STA);
  espAttachWifiEventHandlers(); // Esp Utils

  initializeSoftAP(); // Network Utils

  WiFi.disconnect();
  espSetWifiSleep(false); // Esp Utils
  espSetCountry(); // Esp Utils
  radio_settle_until_ms = millis() + RADIO_SETTLE_PERIOD;

  initializeServers(); // Server Utils

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("[setup] MAC: "));
    Serial.println(WiFi.macAddress());
    Serial.println(F("[setup] Wi-Fi initialization delay"));
  }

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

    checkForClientConnection(); // Network Utils
    manageStaReconnect(); // Network Utils
  }
}
