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
#include <FS.h>
#include <LittleFS.h>
#include <RTClib.h>
#include <time.h>
#include "TM1637Display.h"

// Libraries, boards pinout and RTC reset commands, based on the target platform built via PlatformIO
#include "EspPlatform.h"
/* ---------------------------------------------------------------------------------------------- */

/* ----------------------------------- Constants and variables ----------------------------------- */
constexpr bool DEBUG_MESSAGES = true; // Set to true to enable debug serial messages throughout the code
constexpr const char* ESP_SSID = "Test"; // ESP soft access point name | CHANGE NUMBER FOR EACH DEVICE!
constexpr const char* ESP_PASS = "Test1234"; // ESP soft access point password
constexpr const char* OTA_USER = "admin"; // Software update login | rtc_setup writes the value from rtc_secrets.ini
constexpr const char* OTA_PASS = "ChangeMe1234"; // Software update password | rtc_setup writes the value from rtc_secrets.ini
constexpr const char* SETTINGS_FILE = "/espSettings.xml";
constexpr const char* CREDS_FILE = "/creds.txt";
constexpr const char* EU_NTP_SERVER_1 = "0.europe.pool.ntp.org"; // NTP pool for IP addresses

struct SettingTag {
  const char* start;
  const char* end;
};

enum class SettingId : uint8_t {
  DaylightSavingEnabled,
  TimeSyncMode,
  AutoBrightness,
  ManualBrightness,
  TimezoneOffset,
  WorkMode,
  TimerDuration,
  Count
};

constexpr SettingTag SETTING_TAGS[] = {
  { "<daylightSavingEnabled>",  "</daylightSavingEnabled>"  },
  { "<timeSyncMode>",           "</timeSyncMode>"           },
  { "<autoBrightnessControl>",  "</autoBrightnessControl>"  },
  { "<manualBrightnessLevel>",  "</manualBrightnessLevel>"  },
  { "<timezoneHoursOffset>",    "</timezoneHoursOffset>"    },
  { "<workMode>",               "</workMode>"               },
  { "<timerDuration>",          "</timerDuration>"          }
};

static_assert(sizeof(SETTING_TAGS) / sizeof(SETTING_TAGS[0]) == (size_t)SettingId::Count,
              "SETTING_TAGS must have exactly one entry per SettingId");

constexpr uint8_t DEFAULT_BRIGHTNESS = 2; // The default display brightness
constexpr uint8_t UPDATE_HOUR = 3; // Request time from NTP server at 3:00 in the morning
constexpr uint8_t GPS_MAX_CONNECT_ATTEMPTS = 20;
constexpr uint8_t GPS_CONNECT_ATTEMPT_DELAY = 250;
constexpr unsigned long AP_CONNECTION_TIMEOUT = 610000UL; // Inactivity timeout duration for clients connected to the ESP
constexpr unsigned long RADIO_SETTLE_PERIOD = 5000UL; // Grace period for the Wi-Fi radio to settle after softAP events

inline uint8_t display_brightness = DEFAULT_BRIGHTNESS;
inline uint8_t last_display_brightness = DEFAULT_BRIGHTNESS;
inline uint8_t timer_status = 0; // 0 - Paused; 1 - Running (Timer mode)
inline int8_t timezone_offset;
inline int8_t second_now = 0;
inline int8_t last_second = -1; // Used to check if the current second is different than the last
inline int8_t blink_count = 0; // Amount of flashes when someone connects to the ESP / ESP connects to NTP server
inline int32_t timer_duration = 0; // Remaining timer duration in seconds. 32 bit, the web UI allows up to 99:59:59 (359999 s)
inline unsigned long timer_millis = 0;        // millis() timestamp of when the current timer-second began (or when resume was called)
inline unsigned long timer_millis_offset = 0; // ms already elapsed within the current second at the moment of pause
inline unsigned long last_http_activity_ms = 0; // Updated on every HTTP request
inline unsigned long radio_settle_until_ms = 0; // millis() timestamp until which station reconnect attempts are postponed

inline bool auto_brightness = true; // Used for brightness module
inline bool connected_to_ntp = false;
inline bool display_client_connected = false; // Triggers a TM1637 flashing, when a client connects to the ESP network
inline bool daylight_saving_enabled; // Daylight saving mode - ON/OFF
inline bool daylight_saving_active;
inline bool work_mode_is_timer = false; // false = RTC mode, true = Timer mode
inline bool software_update_server_active = false; // true when the OTA update server is listening
inline volatile bool ap_station_associated = false; // Set/Cleared by SoftAP events
inline volatile uint16_t sta_disconnect_count = 0; // Bumped by every STA disconnect event

// ---------------- Objects ---------------- //
inline WiFiUDP udp;
inline RTC_DS3231 rtc;
inline WebServerClass server(80);
inline TM1637Display tm1637(CLK, DIO);
inline WebServerClass softwareUpdateServer(1394);
/* ----------------------------------------- */

#ifdef  BRIGHTNESS_MODULE
/* ---------------------------------- BRIGHTNESS MODULE SPECIFIC ---------------------------------- */
    #include "BrightnessModule.h"

    constexpr bool HAS_BRIGHTNESS_MODULE = true;
#else
/* ------------------------------------- NO BRIGHTNESS MODULE ------------------------------------- */
    void autoSetBrightness();

    constexpr bool HAS_BRIGHTNESS_MODULE = false;
/* ----------------------------------------------------------------------------------------------- */
#endif

/**
 * @brief Acquisition state of the GPS module.
 * Disabled  - GPS is not the selected time source, the serial line is not read.
 * Searching - GPS is selected and an acquisition window is open, but no usable fix yet.
 * Locked    - a valid fix newer than GPS_MAX_FIX_AGE is available.
 * TimedOut  - no fix for GPS_ACQUISITION_TIMEOUT, the module is treated as unreachable.
 */
enum class GpsState : uint8_t { Disabled, Searching, Locked, TimedOut };

#ifdef  GPS_MODULE
/* ------------------------------------- GPS MODULE SPECIFIC ------------------------------------- */
    #include <TinyGPS++.h>
    #include "GPSModule.h"

    constexpr int GPS_BAUD_RATE = 9600;
    constexpr unsigned long GPS_ACQUISITION_TIMEOUT = 180000UL; // Time allowed to acquire a fix before falling back
    constexpr unsigned long GPS_MAX_FIX_AGE = 2000UL; // A fix that was not refreshed within this window is stale

    inline bool set_time_with_gps = false;

    inline TinyGPSPlus gps;
    inline GpsState gps_state = GpsState::Disabled; // gpsSerial is declared in EspPlatform.h

    constexpr bool HAS_GPS_MODULE = true;
/* ----------------------------------------------------------------------------------------------- */
#else
/* ---------------------------------------- NO GPS MODULE ---------------------------------------- */
    inline bool set_time_with_gps = false;

    GpsState gpsState();
    void activateGPS();
    void serviceGPS();
    bool updateTimeFromGPS();

    constexpr bool HAS_GPS_MODULE = false;
/* ----------------------------------------------------------------------------------------------- */
#endif


#ifdef  TEMPERATURE_MODULE
/* --------------------------------- TEMPERATURE MODULE SPECIFIC --------------------------------- */
    #include <OneWire.h>
    #include <DallasTemperature.h>
    #include "TemperatureModule.h"

    inline OneWire oneWire(ONE_WIRE_BUS);
    inline DallasTemperature temperatureSensor(&oneWire); // Object init

    constexpr bool HAS_TEMPERATURE_MODULE = true;

/* ----------------------------------------------------------------------------------------------- */
#else
/* ------------------------------------ NO TEMPERATURE MODULE ------------------------------------ */
    void initializeTemperatureSensor();
    void printCurrentTimeOrTemperature();

    constexpr bool HAS_TEMPERATURE_MODULE = false;
/* ----------------------------------------------------------------------------------------------- */
#endif
