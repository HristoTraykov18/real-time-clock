// Modules' Specific Variables, Definitions and Constants for the Real time clock software
// Developed by Hristo Traykov, NEON.BG (Sofia)
// File version 1.0

/* AVAILABLE PLATFORMS */
// #define  PLATFORM_ESP32
// #define  PLATFORM_ESP8266
#include "Platform.h"


/* AVAILABLE MODULES */
// #define  AUDIO_MODULE
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
// #define SCL D1 | By default on the ESP8266
// #define SDA D2 | By default on the ESP8266
#define CLK           D4 // Display clock input
#define DIO           D5 // Display data input
#define LED_PIN       16 // Integrated LED

/* ----------------------------------- Constants and variables ----------------------------------- */
const PROGMEM char *ESP_SSID = "Test"; // ESP soft access point name | CHANGE NUMBER FOR EACH DEVICE!
const PROGMEM char *ESP_PASS = "Test1234"; // ESP soft access point password
const PROGMEM char *EU_NTP_SERVER_1 = "0.europe.pool.ntp.org"; // NTP pool for IP addresses
const PROGMEM char *START_TAGS[] = { "<daylightSaving>", "<timeSyncMode>", "<autoBrightnessControl>",
                                     "<manualBrightnessLevel>", "<timezoneHoursOffset>", "<IP>" };
const PROGMEM char *END_TAGS[] = { "</daylightSaving>", "</timeSyncMode>", "</autoBrightnessControl>", 
                                   "</manualBrightnessLevel>", "</timezoneHoursOffset>", "</IP>" };

const PROGMEM uint8_t DEFAULT_BRIGHTNESS = 2; // The default display brightness
const PROGMEM uint8_t NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

const PROGMEM int CONNECT_TO_NETWORK_LOOP_COUNT = 32; // Used in connectClockToNetwork() and HandleWebInterface()
const PROGMEM int CONNECT_TO_NETWORK_LOOP_DELAY = 250; // Used in connectClockToNetwork() and HandleWebInterface()
const PROGMEM int LAST_UPDATE_HOUR = 5; // If the clock has not updated it will try on the next day

uint8_t display_brightness = DEFAULT_BRIGHTNESS;
uint8_t last_display_brightness = DEFAULT_BRIGHTNESS;
uint8_t update_hour = 3; // Request time from NTP server at 3:00 in the morning
int8_t timezone;
int8_t second_now = 0;
int8_t last_second = -1; // Used to check if the last current second is different than the last
int8_t blink_count = 0; // Amount of flashes when someone connects to the ESP / ESP connects to NTP server

/* REMOVE THIS LINE */ bool has_audio_module = false; // THIS IS THE FEATURES LINE!!!!!
bool display_time = true; // If false, show temperature
bool auto_brightness = true; // Used for brightness module
bool last_auto_brightness = auto_brightness; // Used for brightness module
bool connected_to_ntp = false;
bool active_connection = false; // Active connection to the ESP network
bool someone_just_connected = false; // Someone just connected to the ESP network
bool daylight_saving; // Daylight saving mode - ON/OFF
bool override_settings = false; // Triggers 'espSettings.xml' override
bool time_update_pending = true; // Triggers time update at start if connected to NTP server
bool awaiting_confirmation = false; // Send response to the server a few times or until confirmation is received

byte packet_buffer[NTP_PACKET_SIZE]; // Buffer holding incoming and outgoing packets

String response_message; // Keeps the response message while awaiting confirmation from the server

// ---------------- Objects ---------------- //
IPAddress time_server_ip; // NTP server ip container
WiFiUDP udp;
RTC_DS3231 rtc; // !!!
ESP8266WebServer server(80);
TM1637Display tm1637(CLK, DIO);
ESP8266WebServer softwareUpdateServer(1394);
ESP8266HTTPUpdateServer httpUpdater;
/* ----------------------------------------------------------------------------------------------- */


#ifdef  AUDIO_MODULE
/* ------------------------------------ AUDIO MODULE SPECIFIC ------------------------------------ */
const PROGMEM char *FILENAMES_STR = "filenames";
const PROGMEM char *NOTIFICATIONS_STR = "notifications";
const PROGMEM char *POPUP_FILENAMES_STR = "popup_filenames";
const int AUDIO_BAUD_RATE = 9600;

int8_t audio_volume = 15;

bool notification_played_this_minute = false; // Used to prevent playing notifications more than once in a minute
/* ----------------------------------------------------------------------------------------------- */
#endif


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