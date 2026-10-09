/**
 * @file esp8266/EspPlatform.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Libraries, board pinout and RTC reset commands for the ESP8266 clock version.
 * @version 1.0
 * @date 2026-10-03
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include <Arduino.h>

/* -------------------------------------- Core libraries -------------------------------------- */
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

extern "C" bool wifi_softap_deauth(uint8_t mac[6]); // Declared by no SDK header

using WebServerClass = ESP8266WebServer;

/* -------------------------------------- Pin assignments -------------------------------------- */
// SCL is D1 (GPIO5) and SDA is D2 (GPIO4) by default on the ESP8266
constexpr uint8_t CLK              = D4;
constexpr uint8_t DIO              = D5;
constexpr uint8_t LED_PIN          = 16; // Integrated LED
constexpr uint8_t LIGHT_SENSOR_PIN = A0; // GPIO17, the dedicated ADC input

// Light sensor reading ceiling and display brightness. 10 bit ADC
constexpr int LIGHT_LEVELS[][2] = {
  {   75, 0 },
  {  200, 2 },
  {  500, 3 },
  { 1023, 7 }
};

constexpr int     ONE_WIRE_BUS     = D3; // GPIO0, temperature sensor
constexpr int     GPS_RX           = D7; // GPIO13, ESP receives from the GPS module's TX pin
constexpr int     GPS_TX           = D8; // GPIO15, ESP transmits to the GPS module's RX pin

/* ------------------------- Used when the RTC becomes unsynchronised ------------------------- */
// GPES sets and GPEC clears the output enable bit, so the pin is driven low or released
inline void SDA_LOW()  { GPES = (1 << SDA); }
inline void SDA_HIGH() { GPEC = (1 << SDA); }
inline void SCL_LOW()  { GPES = (1 << SCL); }
inline void SCL_HIGH() { GPEC = (1 << SCL); }
inline bool SDA_READ() { return ((GPI & (1 << SDA)) != 0); }

#ifdef  GPS_MODULE
/* ---------------------------------- GPS module serial port ---------------------------------- */
#include <SoftwareSerial.h>

// Bit banged, because both hardware UARTs are taken by the console and the flash loader
inline SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
#endif
