// Real time clock software
// Developed by Hristo Traykov
// Current version 1.1
// ESP8266 core version 2.6.3

#include <SoftwareSerial.h> // Use the default Arduino library. Using the ESP8266 SoftwareSerial causes stack overflow
#include <ESP8266WiFi.h> // Keep on top so the IDE uses ESP8266 libraries
#include <FS.h>
#include <LittleFS.h>
#include <RTClib.h>
#include <OneWire.h>
#include <WiFiUdp.h>
#include <TinyGPS++.h>
#include <TM1637Display.h>
#include <ESP8266WebServer.h>
#include <DallasTemperature.h>
#include <ESP8266HTTPUpdateServer.h>

// ------------------ Definitions ------------------ //
// #define SCL D1 | By default on the ESP8266 | For the RTC
// #define SDA D2 | By default on the ESP8266 | For the RTC
#define ONE_WIRE_BUS  D3 // Temperature sensor
#define CLK           D4 // Display clock input
#define DIO           D5 // Display data input
#define LED_PIN       16 // Integrated LED, set to D6 if external LED is going to be used
#define GPS_RX        D7 // TX from GPS module
#define GPS_TX        D8 // RX from GPS module
#define MP3_RX         9 // TX from DFPlayer Mini
#define MP3_TX        10 // RX from DFPlayer Mini

// --- Used when the RTC becomes unsynchronised --- //
#define SDA_LOW()   (GPES = (1 << SDA))
#define SDA_HIGH()  (GPEC = (1 << SDA))
#define SCL_LOW()   (GPES = (1 << SCL))
#define SCL_HIGH()  (GPEC = (1 << SCL))
#define SDA_READ()  ((GPI & (1 << SDA)) != 0)

// ----------------------------------------- Constants and variables ----------------------------------------- //
const char * PROGMEM EU_NTP_SERVER_1 = "0.europe.pool.ntp.org";
const char * PROGMEM HOST = "esp8266_Update";
const char * PROGMEM UPDATE_PATH = "/sourceControl";
const char * PROGMEM UPDATE_UNAME = "ghost";
const char * PROGMEM UPDATE_PASS = "m%O0gsLKOkDl";
const char * PROGMEM ESP_SSID = "ClockNetwork"; // ESP soft access point name | CHANGE NUMBER FOR EACH DEVICE!
const char * PROGMEM ESP_PASS = "Pass1234"; // ESP soft access point password
const char * startTags[] = { "<daylightSaving>", "<timeSetup>", "<brightnessControl>",
                            "<manualBrightnessLevel>", "<timezone>", "<IP>" };
const char * endTags[] = { "</daylightSaving>", "</timeSetup>", "</brightnessControl>", 
                          "</manualBrightnessLevel>", "</timezone>", "</IP>" };
const uint8_t connectingToNetworkAnimation[8][4] = {{(SEG_E | SEG_F), 0, 0, 0 }, {(SEG_A | SEG_B | SEG_C | SEG_D), 0, 0, 0}, // Used in for loop in 
  {(SEG_A | SEG_D), (SEG_E | SEG_F), 0, 0}, {(SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D), 0, 0}, // 'HandleWebInterface()' to animate effect on the RTC when
  {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_E | SEG_F), 0}, {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D), 0}, // the clock connects to a network
  {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_E | SEG_F)}, {(SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_D), (SEG_A | SEG_B | SEG_C | SEG_D)}};
const uint8_t DEFAULT_BRIGHTNESS = 2; // The default display brightness
const uint8_t NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
const int GPS_BAUD_RATE = 4800;
const int CONNECT_TO_NETWORK_LOOP_COUNT = 32; // Used in ConnectToLastKnownNetwork() and HandleWebInterface()
const int CONNECT_TO_NETWORK_LOOP_DELAY = 250; // Used in ConnectToLastKnownNetwork() and HandleWebInterface()

IPAddress timeServerIP; // NTP server
int manualTime,
    lightSensorValue,
    displayBrightness = DEFAULT_BRIGHTNESS,
    lastDisplayBrightness = DEFAULT_BRIGHTNESS,
    timezone = 2,
    s = 0, // Current second
    GPS_connectionTriesLeft = 180,
    showDuration = 7, // Show time for 7 seconds and temperature for 4
    currentTemp, // Current temperature
    updateHour = 3, // Request time from NTP server at 3:00 in the morning
    lastSecond = -1, // Used to check if the last current second is different than the last
    updateCounter = 3, // Times to try updating the time from NTP if there is big difference in hours/minutes
    flashesOnConnect = 0; // Amount of flashes when someone connects to the ESP / ESP connects to NTP server
bool showTime = true, // If false, show temperature
     autoBrightness = true, // Used for brightness module
     lastAutoBrightness = autoBrightness, // Used for brightness module
     hasTempSensor = true, hasGPS = false, hasLightSensor = true,
     satelliteFound = false, connectedTo_NTP = false, setTimeWithGPS = false, // Used for GPS
     activeConnection = false, // To the ESP network
     someoneJustConnected = false, // To the ESP network
     daylightSaving, // Daylight saving mode - ON/OFF
     dstHasChanged = false, // True if Daylight Saving Time has changed
     overrideSettings = false, // Triggers 'espSettings.xml' override
     timeUpdatePending = true; // Triggers time update at start if connected to NTP server
String webpageSetup, // Keeps the main page
       //webpageSuccess, // Contains the page that shows successful connection to network
       //webpageFailed, // Contains the page that shows failed connection to network
       settingsFile, // Keeps the XML file
       mainStyling,
       mainScript;
byte packetBuffer[NTP_PACKET_SIZE]; // Buffer holding incoming and outgoing packets
double latitude, longtitude, altitudeInMeters; // Latitude, longtitude and altitude in meters detected by the GPS module

// ---------------- Objects ---------------- //
WiFiUDP udp;
RTC_DS3231 rtc;
TinyGPSPlus gps;
OneWire oneWire(ONE_WIRE_BUS);
ESP8266WebServer server(80);
TM1637Display tm1637(CLK, DIO);
DallasTemperature sensor(&oneWire);
ESP8266WebServer updateServer(1394);
ESP8266HTTPUpdateServer httpUpdater;
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
// SoftwareSerial mp3Serial(MP3_RX, MP3_TX);

void setup() {
  Serial.begin(115200); // Serial monitor

  if (hasTempSensor) {
    sensor.requestTemperatures();
    currentTemp = sensor.getTempCByIndex(0);
  }

  if (hasGPS)
    gpsSerial.begin(GPS_BAUD_RATE); // Start the GPS connection through SoftwareSerial library

  pinMode(LED_PIN, OUTPUT);

  InitialRTC_Check();
  FileSystemInitialization();
  WiFi.softAP(ESP_SSID, ESP_PASS, 1, 0, 1); // Set ESP access point
  ServersInitialization();
  WiFi.begin();
  ConnectToLastKnownNetwork();
  Serial.print(F("MAC: "));
  Serial.println(WiFi.macAddress());
  tm1637.setBrightness(displayBrightness); // Set brightness of the 7-digit display (TM1637)
}

void loop() {
  server.handleClient();
  updateServer.handleClient();
  CheckForUserConnection();
  s = rtc.now().second();
  if (s != lastSecond) {
    UpdateTime(hasGPS);
    VisualizeOnDisplay(hasTempSensor);
    Serial.print(F("WiFi status: "));
    Serial.print(WiFi.status());
    Serial.print(F(", "));
    Serial.print(F("Connected devices: "));
    Serial.println(WiFi.softAPgetStationNum());
  }
  ESP_ToNetworkConnection();
}
