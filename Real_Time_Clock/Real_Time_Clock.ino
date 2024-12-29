// Real time clock software
// Developed by Hristo Traykov, NEON.BG (Sofia)
// Current version 1.2.x-b2
// DO NOT FORGET TO SETUP PROPERLY IN TOOLS
// USE FLOAT FIRMWARE
// 
// ESP8266 core version 3.1.2
// ESP file system plugin version 2.6.0
// TM1637 library is edited
// RTClib version 2.1.1
// DallasTemperature version 3.8.0

#include "MSVDC.h" // Modules' Specific Variables, Definitions and Constants

// For info messages like current time, date, responses sent to the server, etc.
#define RTC_INFO_MESSAGES

// --- Used when the RTC becomes unsynchronised --- //
#define SDA_LOW()   (GPES = (1 << SDA))
#define SDA_HIGH()  (GPEC = (1 << SDA))
#define SCL_LOW()   (GPES = (1 << SCL))
#define SCL_HIGH()  (GPEC = (1 << SCL))
#define SDA_READ()  ((GPI & (1 << SDA)) != 0)


void setup() {
#ifdef  RTC_INFO_MESSAGES
  Serial.begin(115200); // Serial monitor
#endif

  pinMode(LED_PIN, OUTPUT);
  initializeModuleRTC(); // Initial function
  initializeFileSystem(); // Initial function
  WiFi.softAP(ESP_SSID, ESP_PASS, 1, 0, 1); // Set ESP access point
  initializeServers(); // Initial function

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
}

void loop() {
  server.handleClient();
  softwareUpdateServer.handleClient();
  second_now = rtc.now().second();

  if (second_now != last_second) {
    updateTime(); // Additional function
    visualizeOnDisplay(); // Additional function

#ifdef  RTC_INFO_MESSAGES
    Serial.print(F("WiFi status: "));
    Serial.print(WiFi.status());
    Serial.print(F(", "));
    Serial.print(F("Connected devices: "));
    Serial.println(WiFi.softAPgetStationNum());
    Serial.println(display_brightness);
#endif

    checkForUserConnection(); // Additional function
  }
}
