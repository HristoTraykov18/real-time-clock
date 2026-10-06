
// Real time clock software
// Developed by Hristo Traykov, NEON.BG (Sofia)
// Current version 1.12.x
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
#include "EspUtils.h"
#include "ServerUtils.h"

void setup() {
  WiFi.disconnect();
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  espSetWifiSleep(false); // Esp Utils
  espSetCountry(); // Esp Utils

  if constexpr (DEBUG_MESSAGES) {
    Serial.begin(115200); // Serial monitor
    Serial.println("\n\n");
  }

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  initializePeripherals(); // Core Utils
  initializeFileSystem(); // Core Utils
  initializeServers(); // Server Utils

  daylight_saving_active = daylight_saving_enabled && isDaylightSavingPeriod();

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("[setup] MAC: "));
    Serial.println(WiFi.macAddress());
  }

  tm1637.setBrightness(display_brightness); // Set brightness of the 7-digit display (TM1637)
  initializeSoftAP(); // Network Utils
  radio_settle_until_ms = millis() + RADIO_SETTLE_PERIOD;

  espAttachWifiEventHandlers(); // Esp Utils

  if constexpr (DEBUG_MESSAGES)
    Serial.println(F("[setup] Wi-Fi initialization delay"));

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
