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
#include <esp_wifi.h>

using WebServerClass = WebServer;

/* -------------------------------------- Pin assignments -------------------------------------- */
// SCL is GPIO22 and SDA is GPIO21 by default on the ESP32.
// GPIO6 to GPIO11 are wired to the SPI flash and GPIO34 upwards are input only
constexpr uint8_t CLK              = 18;
constexpr uint8_t DIO              = 19;
constexpr uint8_t LED_PIN          = 23;
constexpr uint8_t LIGHT_SENSOR_PIN = 34; // ADC1 only. ADC2 cannot be read while Wi-Fi is on
constexpr int     ONE_WIRE_BUS     = 4;
constexpr int     GPS_RX           = 16; // UART2 RX
constexpr int     GPS_TX           = 17; // UART2 TX

/* ------------------------- Used when the RTC becomes unsynchronised ------------------------- */
// No single register equivalent of GPES and GPEC, so the pin is switched between driven low
// and input instead, which leaves the same open drain behaviour
inline void SDA_LOW()  { pinMode(SDA, OUTPUT); digitalWrite(SDA, LOW); }
inline void SDA_HIGH() { pinMode(SDA, INPUT_PULLUP); }
inline void SCL_LOW()  { pinMode(SCL, OUTPUT); digitalWrite(SCL, LOW); }
inline void SCL_HIGH() { pinMode(SCL, INPUT_PULLUP); }
inline bool SDA_READ() { return digitalRead(SDA) != 0; }

#ifdef  GPS_MODULE
/* ---------------------------------- GPS module serial port ---------------------------------- */
// UART2 is free here, so the port is a hardware one and loses no characters to interrupt timing
inline HardwareSerial& gpsSerial = Serial2;
#endif
