/**
 * @file EspUtils.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Wrappers for the ESP8266 core functions used in the software.
 * @version 1.0
 * @date 2026-09-28
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"

/**
 * @brief Subscribes to the softAP association, softAP disassociation and station disconnect events.
 * The subscriptions live for the lifetime of the program.
 */
void espAttachWifiEventHandlers();

/**
 * @brief Deauthenticates every station currently associated to the ESP's softAP.
 */
void espEvictStations();

/**
 * @brief Mounts the LittleFS partition, formatting it when no valid filesystem is present.
 * @return bool true once the filesystem is mounted.
 */
bool espFsBegin();

/**
 * @brief Reports the largest block the heap can still hand out in one allocation.
 * @return uint32_t Size of the largest free block in bytes.
 */
uint32_t espMaxAllocHeap();

/**
 * @brief Reports how fragmented the free heap is, as a percentage.
 * @return uint8_t Percentage fragmentation of the free heap.
 */
uint8_t espHeapFragmentation();

/**
 * @brief Tells whether a Wi-Fi status value means the password was rejected.
 * The ESP8266 core reports this as a distinct status.
 * @param status Value returned by WiFi.status().
 * @return bool true if the status specifically indicates an authentication failure.
 */
bool espIsAuthFailure(int status);

/**
 * @brief Restricts the radio to the European channel set and transmit policy.
 */
void espSetCountry();

/**
 * @brief Enables or disables the Wi-Fi modem sleep mode.
 * @param enabled true allows the modem to sleep, false keeps the radio always on.
 */
void espSetWifiSleep(bool enabled);

/**
 * @brief Attaches the password protected firmware upload handler to the update server.
 * @param update_server Server the upload endpoint is registered on.
 * @param path URI the upload form is served from.
 * @param user Basic authentication user name.
 * @param pass Basic authentication password.
 */
void espSetupHttpUpdater(WebServerClass* update_server, const char* path,
                         const char* user, const char* pass);

/**
 * @brief Fills the caller's buffer with the SSID of the currently configured station network.
 * @param buf Destination buffer. Must be 33 bytes long (SSID is 32 chars at most + NUL).
 */
void espStationSsid(char* buf);

/**
 * @brief Runs a blocking scan that also reports networks which do not broadcast their SSID.
 * @param channel Single channel to scan, or 0 to scan every channel.
 * @return int Number of networks found, or -1 on failure.
 */
int espScanNetworks(uint8_t channel = 0);

/**
 * @brief Releases the memory the last scan result occupies.
 */
void espScanDelete();

/**
 * @brief BSSID of one scan result.
 * @param index Zero based index into the last scan result.
 * @return const uint8_t* Six raw address bytes, or nullptr if the index is not valid.
 */
const uint8_t* espScanBssid(int index);

/**
 * @brief Wi-Fi channel one scan result was seen on.
 * @param index Zero based index into the last scan result.
 * @return uint8_t Channel number, or 0 if the index is not valid.
 */
uint8_t espScanChannel(int index);

/**
 * @brief SSID of one scan result, as a NUL terminated string.
 * The core stores the SSID without a terminator, so it is copied into a buffer this module owns.
 * @param index Zero based index into the last scan result.
 * @return const char* SSID, or an empty string if the index is not valid.
 */
const char* espScanSsid(int index);

#ifdef  GPS_MODULE
/**
 * @brief Opens the serial port the GPS module is wired to.
 * @param baud Bit rate the receiver transmits at.
 */
void espGpsSerialBegin(unsigned long baud);
#endif
