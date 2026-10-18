/**
 * @file esp32/EspPlatform.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Libraries, board pinout and RTC reset commands for the ESP32 clock version.
 * @version 1.0
 * @date 2026-10-03
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include <Arduino.h>

/* -------------------------------------- Core libraries -------------------------------------- */
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <Wire.h>
#include <esp_wifi.h>

using WebServerClass = WebServer;

/* -------------------------------------- Pin assignments -------------------------------------- */
// SCL is GPIO22 and SDA is GPIO21 by default on the ESP32.
constexpr uint8_t CLK              = 14;
constexpr uint8_t DIO              = 13;
constexpr uint8_t LED_PIN          = 2;  // Integrated LED
constexpr uint8_t LIGHT_SENSOR_PIN = 35; // ADC1 only. ADC2 cannot be read while Wi-Fi is on

// Light sensor reading ceiling and display brightness. 12 bit ADC
constexpr int LIGHT_LEVELS[][2] = {
  {  300, 0 },
  {  700, 2 },
  { 1500, 3 },
  { 4095, 7 }
};

constexpr int     ONE_WIRE_BUS     = 32; // Temperature sensor

// OneWire drives the bus low through the GPIO enable register and skips pins above 33 without
// reporting it, so a pin picked from the input only range would never be driven and never answer
static_assert(ONE_WIRE_BUS <= 33, "OneWire cannot drive ESP32 pins above GPIO33");

constexpr int     GPS_RX           = 27; // UART2 RX
constexpr int     GPS_TX           = 34; // UART2 TX

/* ----------------------------------- Status LED switching ----------------------------------- */
inline void LED_ON()  { digitalWrite(LED_PIN, HIGH); }
inline void LED_OFF() { digitalWrite(LED_PIN, LOW); }

/* ------------------------- Used when the RTC becomes unsynchronised ------------------------- */
// No equivalent of GPES and GPEC on ESP32, so SDA/SCL are switched between input and output
// via pinMode, causing I2C tear down. The bus is brought back up via I2C_RESTART
inline void SDA_LOW()  { pinMode(SDA, OUTPUT); digitalWrite(SDA, LOW); }
inline void SDA_HIGH() { pinMode(SDA, INPUT_PULLUP); }
inline void SCL_LOW()  { pinMode(SCL, OUTPUT); digitalWrite(SCL, LOW); }
inline void SCL_HIGH() { pinMode(SCL, INPUT_PULLUP); }
inline bool SDA_READ() { return digitalRead(SDA) != 0; }
inline void I2C_RESTART() { Wire.end(); Wire.begin(); }

#ifdef  GPS_MODULE
/* ---------------------------------- GPS module serial port ---------------------------------- */
// UART2 is free here, so the port is a hardware one and loses no characters to interrupt timing
inline HardwareSerial& gpsSerial = Serial2;
#endif
