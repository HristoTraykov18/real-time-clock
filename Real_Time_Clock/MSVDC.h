// Modules' Specific Variables, Definitions and Constants for the Real time clock software
// Developed by Hristo Traykov, NEON.BG (Sofia)

/* AVAILABLE PLATFORMS */
// #define  PLATFORM_ESP32
// #define  PLATFORM_ESP8266
#include "Platform.h"


/* AVAILABLE MODULES */
// #define  GPS_MODULE
// #define  LIGHT_SENSITIVITY_MODULE
// #define  TEMPERATURE_MODULE
#include "SelectedModules.h"


/* -------------------------------------- COMMON LIBRARIES -------------------------------------- */
#include <SoftwareSerial.h> // Use the default Arduino library. Using the ESP8266 SoftwareSerial causes stack overflow
#include <FS.h>
#include <RTClib.h>
#include <TM1637Display.h>
#include <time.h>
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
// ------------------ Definitions ------------------ //
extern "C" bool wifi_softap_deauth(uint8_t mac[6]);

// #define SCL D1 | By default on the ESP8266
// #define SDA D2 | By default on the ESP8266
#define CLK           D4 // Display clock input
#define DIO           D5 // Display data input
#define LED_PIN       16 // Integrated LED

/* ----------------------------------- Constants and variables ----------------------------------- */
const PROGMEM char* ESP_SSID = "Test"; // ESP soft access point name | CHANGE NUMBER FOR EACH DEVICE!
const PROGMEM char* ESP_PASS = "Test1234"; // ESP soft access point password
const PROGMEM char* EU_NTP_SERVER_1 = "0.europe.pool.ntp.org"; // NTP pool for IP addresses
const PROGMEM char* START_TAGS[] = { "<daylightSavingEnabled>", "<timeSyncMode>", "<autoBrightnessControl>",
                                     "<manualBrightnessLevel>", "<timezoneHoursOffset>", "<workMode>",
                                     "<timerDuration>" };
const PROGMEM char* END_TAGS[] = { "</daylightSavingEnabled>", "</timeSyncMode>", "</autoBrightnessControl>",
                                   "</manualBrightnessLevel>", "</timezoneHoursOffset>", "</workMode>",
                                   "</timerDuration>" };

const uint8_t DEFAULT_BRIGHTNESS = 2; // The default display brightness
const uint8_t UPDATE_HOUR = 3; // Request time from NTP server at 3:00 in the morning
const unsigned long AP_CONNECTION_TIMEOUT = 610000UL; // Inactivity timeout duration for clients connected to the ESP
const unsigned long RADIO_SETTLE_PERIOD = 5000UL;

uint8_t display_brightness = DEFAULT_BRIGHTNESS;
uint8_t last_display_brightness = DEFAULT_BRIGHTNESS;
uint8_t timer_status = 0; // 0 - Paused; 1 - Running (Timer mode)
int8_t timezone;
int8_t second_now = 0;
int8_t last_second = -1; // Used to check if the current second is different than the last
int8_t blink_count = 0; // Amount of flashes when someone connects to the ESP / ESP connects to NTP server
int16_t timer_duration = 0; // Remaining timer duration in seconds
unsigned long timer_millis = 0; // millis() timestamp of when the current timer-second began (or when resume was called)
unsigned long timer_millis_offset = 0; // ms already elapsed within the current second at the moment of pause
unsigned long last_http_activity_ms = 0; // Updated on every HTTP request
unsigned long radio_settle_until_ms = 0;

bool display_time = true; // If false, show temperature
bool auto_brightness = true; // Used for brightness module
bool last_auto_brightness = auto_brightness; // Used for brightness module
bool connected_to_ntp = false;
bool active_connection = false; // Active connection to the ESP network
bool someone_just_connected = false; // Someone just connected to the ESP network
bool daylight_saving_enabled; // Daylight saving mode - ON/OFF
bool daylight_saving_active;
bool work_mode_is_timer = false; // false = RTC mode, true = Timer mode
bool software_update_server_active = false; // true when the OTA update server is listening
volatile bool ap_station_associated = false; // Set/Cleared by SoftAP events

// ---------------- Objects ---------------- //
IPAddress time_server_ip; // NTP server ip container
WiFiUDP udp;
RTC_DS3231 rtc;
ESP8266WebServer server(80);
TM1637Display tm1637(CLK, DIO);
ESP8266WebServer softwareUpdateServer(1394);
ESP8266HTTPUpdateServer httpUpdater;
WiFiEventHandler apConnectHandler;
WiFiEventHandler apDisconnectHandler;
/* ----------------------------------------------------------------------------------------------- */


#ifdef  GPS_MODULE
/* ------------------------------------- GPS MODULE SPECIFIC ------------------------------------- */
#include <TinyGPS++.h>

#define GPS_RX        D7 // TX from GPS module
#define GPS_TX        D8 // RX from GPS module

const PROGMEM int GPS_BAUD_RATE = 4800;
uint8_t gps_connect_attempts_left = 180;
double latitude; // Detected by the GPS module
double longtitude; // Detected by the GPS module
double altitude_meters; // Detected by the GPS module
bool set_time_with_gps = false;

TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
/* ----------------------------------------------------------------------------------------------- */
#endif


#ifdef  TEMPERATURE_MODULE
/* --------------------------------- TEMPERATURE MODULE SPECIFIC --------------------------------- */
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS  D3 // Temperature sensor pin

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature temperatureSensor(&oneWire); // Object init

int8_t current_temperature;
int8_t display_state_duration = 7; // Duration of the current state of the display. Time is shown for 7 seconds and temperature for 4
/* ----------------------------------------------------------------------------------------------- */
#endif
