/**
 * @file Globals.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Global definitions, constants, variables, and core library imports for an ESP8266/ESP32-based Smart Clock.
 * This file serves as the central inclusion point for all necessary dependencies and shared resources across the project
 * @version 1.0
 * @date 2026-05-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once

/* -------------------------------------- COMMON LIBRARIES -------------------------------------- */
#include <Arduino.h>
#include <SoftwareSerial.h> // Use the default Arduino library. Using the ESP8266 SoftwareSerial causes stack overflow
#include <FS.h>
#include <RTClib.h>
#include <time.h>
#include "TM1637Display.h"
/* ---------------------------------------------------------------------------------------------- */


#ifdef  PLATFORM_ESP8266
/* -------------------------------------- ESP8266 LIBRARIES -------------------------------------- */
#include <ESP8266WiFi.h> // Keep on top so the IDE uses ESP8266 libraries
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
/* ----------------------------------------------------------------------------------------------- */
#endif


/* ------------------------------------------- COMMON ------------------------------------------- */
extern "C" bool wifi_softap_deauth(uint8_t mac[6]);

/* -------------------------------------- Pin definitions -------------------------------------- */
// constexpr uint8_t SCL  = D1; | By default on the ESP8266
// constexpr uint8_t SDA  = D2; | By default on the ESP8266
constexpr uint8_t CLK     = D4; // Display clock input
constexpr uint8_t DIO     = D5; // Display data input
constexpr uint8_t LED_PIN = 16; // Integrated LED

/* ----------------------------------- Constants and variables ----------------------------------- */
constexpr const bool DEBUG_MESSAGES = true; // Set to true to enable debug serial messages throughout the code
constexpr const char* ESP_SSID = "Test"; // ESP soft access point name | CHANGE NUMBER FOR EACH DEVICE!
constexpr const char* ESP_PASS = "Test1234"; // ESP soft access point password
constexpr const char* EU_NTP_SERVER_1 = "0.europe.pool.ntp.org"; // NTP pool for IP addresses
constexpr const char* START_TAGS[] = { "<daylightSavingEnabled>", "<timeSyncMode>", "<autoBrightnessControl>",
                                     "<manualBrightnessLevel>", "<timezoneHoursOffset>", "<workMode>",
                                     "<timerDuration>" };
constexpr const char* END_TAGS[] = { "</daylightSavingEnabled>", "</timeSyncMode>", "</autoBrightnessControl>",
                                   "</manualBrightnessLevel>", "</timezoneHoursOffset>", "</workMode>",
                                   "</timerDuration>" };

constexpr const uint8_t DEFAULT_BRIGHTNESS = 2; // The default display brightness
constexpr const uint8_t UPDATE_HOUR = 3; // Request time from NTP server at 3:00 in the morning
constexpr const unsigned long AP_CONNECTION_TIMEOUT = 610000UL; // Inactivity timeout duration for clients connected to the ESP
constexpr const unsigned long RADIO_SETTLE_PERIOD = 5000UL; // Grace period for the Wi-Fi radio to settle after softAP events

inline uint8_t display_brightness = DEFAULT_BRIGHTNESS;
inline uint8_t last_display_brightness = DEFAULT_BRIGHTNESS;
inline uint8_t timer_status = 0; // 0 - Paused; 1 - Running (Timer mode)
inline int8_t timezone;
inline int8_t second_now = 0;
inline int8_t last_second = -1; // Used to check if the current second is different than the last
inline int8_t blink_count = 0; // Amount of flashes when someone connects to the ESP / ESP connects to NTP server
inline int16_t timer_duration = 0; // Remaining timer duration in seconds
inline unsigned long timer_millis = 0;        // millis() timestamp of when the current timer-second began (or when resume was called)
inline unsigned long timer_millis_offset = 0; // ms already elapsed within the current second at the moment of pause
inline unsigned long last_http_activity_ms = 0; // Updated on every HTTP request
inline unsigned long radio_settle_until_ms = 0; // millis() timestamp until which station reconnect attempts are postponed

inline bool display_time = true; // If false, show temperature
inline bool auto_brightness = true; // Used for brightness module
inline bool last_auto_brightness = auto_brightness; // Used for brightness module
inline bool connected_to_ntp = false;
inline bool active_connection = false; // Active connection to the ESP network
inline bool someone_just_connected = false; // Someone just connected to the ESP network
inline bool daylight_saving_enabled; // Daylight saving mode - ON/OFF
inline bool daylight_saving_active;
inline bool work_mode_is_timer = false; // false = RTC mode, true = Timer mode
inline bool software_update_server_active = false; // true when the OTA update server is listening
inline volatile bool ap_station_associated = false; // Set/Cleared by SoftAP events

// ---------------- Objects ---------------- //
inline IPAddress time_server_ip; // NTP server ip container
inline WiFiUDP udp;
inline RTC_DS3231 rtc;
inline ESP8266WebServer server(80);
inline TM1637Display tm1637(CLK, DIO);
inline ESP8266WebServer softwareUpdateServer(1394);
inline ESP8266HTTPUpdateServer httpUpdater;
inline WiFiEventHandler apConnectHandler; // Keeps the softAP station connected event subscription alive
inline WiFiEventHandler apDisconnectHandler; // Keeps the softAP station disconnected event subscription alive
/* ----------------------------------------- */

// --- Used when the RTC becomes unsynchronised --- //
inline void SDA_LOW()  { GPES = (1 << SDA); }
inline void SDA_HIGH() { GPEC = (1 << SDA); }
inline void SCL_LOW()  { GPES = (1 << SCL); }
inline void SCL_HIGH() { GPEC = (1 << SCL); }
inline bool SDA_READ() { return ((GPI & (1 << SDA)) != 0); }
// ------------------------------------------------ //


#ifdef  BRIGHTNESS_MODULE
/* ---------------------------------- BRIGHTNESS MODULE SPECIFIC ---------------------------------- */
    #include "BrightnessModule.h"

    inline int light_sensor_value;

    constexpr bool HAS_BRIGHTNESS_MODULE = true;
#else
/* ------------------------------------- NO BRIGHTNESS MODULE ------------------------------------- */
    constexpr bool HAS_BRIGHTNESS_MODULE = false;
/* ----------------------------------------------------------------------------------------------- */
#endif


#ifdef  GPS_MODULE
/* ------------------------------------- GPS MODULE SPECIFIC ------------------------------------- */
    #include <TinyGPS++.h>
    #include "GPSModule.h"

    constexpr int GPS_RX = D7; // TX from GPS module
    constexpr int GPS_TX = D8; // RX from GPS module

    constexpr int GPS_BAUD_RATE = 4800;
    inline uint8_t gps_connect_attempts_left = 180;
    inline bool set_time_with_gps = false;

    inline TinyGPSPlus gps;
    inline SoftwareSerial gpsSerial(GPS_RX, GPS_TX);

    constexpr bool HAS_GPS_MODULE = true;
/* ----------------------------------------------------------------------------------------------- */
#else
/* ---------------------------------------- NO GPS MODULE ---------------------------------------- */
    // Nested GPS library mocks, binding and dummy functionality
    struct DummyGPSDate {
        int year() { return 1970; }
        int month() { return 1; }
        int day() { return 1; }
    };
    struct DummyGPSTime {
        int hour() { return 0; }
        int minute() { return 0; }
        int second() { return 0; }
    };
    struct DummyLocation {
        double lng() { return 0.0; }
        double lat() { return 0.0; }
    };
    struct DummyAltitude { double meters() { return 0.0; } };
    struct DummySatellites { uint32_t value() { return 0; } };

    using TinyGPSDate = DummyGPSDate;
    using TinyGPSTime = DummyGPSTime;

    struct DummyGPSPlus {
        DummyGPSDate date;
        DummyGPSTime time;
        DummyLocation location;
        DummyAltitude altitude;
        DummySatellites satellites;

        void encode(char c) {}
    };
    struct DummySoftwareSerial {
        void begin(uint32_t baud) {}
        bool available() { return false; }
        int read() { return -1; }
    };

    constexpr int GPS_RX = 0; // TX from GPS module
    constexpr int GPS_TX = 0; // RX from GPS module

    constexpr int GPS_BAUD_RATE = 4800;
    inline uint8_t gps_connect_attempts_left;
    inline bool set_time_with_gps = false;

    inline DummySoftwareSerial gpsSerial;
    inline DummyGPSPlus gps;

    constexpr bool HAS_GPS_MODULE = false;
/* ----------------------------------------------------------------------------------------------- */
#endif


#ifdef  TEMPERATURE_MODULE
/* --------------------------------- TEMPERATURE MODULE SPECIFIC --------------------------------- */
    #include <OneWire.h>
    #include <DallasTemperature.h>
    #include "TemperatureModule.h"

    constexpr int ONE_WIRE_BUS = D3; // Temperature sensor pin

    inline OneWire oneWire(ONE_WIRE_BUS);
    inline DallasTemperature temperatureSensor(&oneWire); // Object init

    inline int8_t current_temperature;
    inline int8_t display_state_duration = 7; // Duration of the current state of the display. Time is shown for 7 seconds and temperature for 4

    constexpr bool HAS_TEMPERATURE_MODULE = true;

/* ----------------------------------------------------------------------------------------------- */
#else
/* ------------------------------------ NO TEMPERATURE MODULE ------------------------------------ */
    struct DummyTemperatureSensor {
        void requestTemperatures() {}
        int8_t getTempCByIndex(int index) { return 0; }
    };

    inline DummyTemperatureSensor temperatureSensor;
    inline int8_t current_temperature;
    inline int8_t display_state_duration;

    constexpr bool HAS_TEMPERATURE_MODULE = false;
/* ----------------------------------------------------------------------------------------------- */
#endif

/* ----------- Unconditional declarations (visible in every build) ----------- */
void autoSetBrightness();
void activateGPS();
bool updateTimeFromGPS(TinyGPSDate &d, TinyGPSTime &t);
void printCurrentTimeOrTemperature();
/* --------------------------------------------------------------------------- */
