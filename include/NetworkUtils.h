/**
 * @file NetworkUtils.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Network utility functions.
 * Handles Wi-Fi connection management, user connection detection, and network-based time synchronization.
 * @version 1.0
 * @date 2026-05-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"
#include "CoreUtils.h"

/**
 * @brief Checks if a station is connected to the access point of the ESP and controls the OTA server lifespan.
 */
void checkForUserConnection();

/**
 * @brief Initializes connection requests to local area access points.
 * @param ssid Destination Wi-Fi Network Name.
 * @param pass Destination Wi-Fi Password.
 * @param is_hidden Set to true if target AP doesn't broadcast its SSID.
 * @param channel Optional cached Wi-Fi channel used for fast-connect (0 to perform a regular scan).
 * @param bssid Optional cached BSSID (MAC address) of the access point used for fast-connect (nullptr to ignore).
 * @return wl_status_t The Wi-Fi status reported at the end of the tracking window.
 */
wl_status_t connectClockToNetwork(const char* ssid, const char* pass, bool is_hidden,
                                  uint8_t channel = 0, const uint8_t* bssid = nullptr);

/**
 * @brief Fills the caller's buffer with the SSID of the currently configured station network (no heap allocation).
 * @param buf Destination character buffer. Must be 33 bytes long (32 SSID chars + NUL).
 */
void currentSsid(char* buf);

/**
 * @brief Compares the currently connected SSID against a target string (no heap allocation).
 * @param target Target network name to compare against.
 * @return true if the current station SSID matches the target exactly.
 */
bool currentSsidEquals(const char* target);

/**
 * @brief Deauthenticates stations that are still associated to the softAP, but not really present.
 */
void evictStaleStations();

/**
 * @brief Validates the cached BSSID availability on the cached Wi-Fi channel before a fast-connect attempt.
 * @param channel Cached Wi-Fi channel to scan.
 * @param bssid Cached BSSID (MAC address) expected to be in range.
 * @return true if the cached BSSID was found broadcasting on the cached channel.
 */
bool fastConnectCacheIsValid(uint8_t channel, const uint8_t* bssid);

/**
 * @brief Computes network transit responses by evaluating UDP frame sizes over an async lookup block.
 * @param address Targeted destination IP Address of the remote time server.
 * @return int Total byte length of parsed packets.
 */
int getPacketLength(IPAddress& address);

/**
 * @brief Configures the soft access point of the ESP, reusing the saved network channel when available.
 */
void initializeSoftAP();

/**
 * @brief Reads the stored network information from `creds.txt` into the caller's buffer and field pointers.
 * @param buf Destination character buffer that will hold the raw file content.
 * @param buf_size Size of the destination buffer in bytes.
 * @param fields Array of 5 pointers populated with SSID, password, hidden flag, channel, and BSSID strings.
 * @return true if the credentials file exists and was successfully read.
 */
bool loadNetworkInfo(char* buf, size_t buf_size, const char* fields[5]);

/**
 * @brief Manages reconnection with retry limits and hourly back-off after the ESP disconnects from a known network.
 */
void manageStaReconnect();

/**
 * @brief Triggers a wireless scan to see if an SSID target is broadcasted within range.
 * @param ssid Target wireless access point name.
 * @return true if network matches any active indexed SSID item in range.
 */
bool networkIsInRange(const char* ssid);

/**
 * @brief Reads network credentials stored locally inside `creds.txt` and attempts system re-association.
 * @return wl_status_t The Wi-Fi status reported after the reconnect attempt.
 */
wl_status_t networkReconnect();

/**
 * @brief Parses a "XX:XX:XX:XX:XX:XX" formatted MAC address string into raw bytes.
 * @param s Source MAC address string (17 characters).
 * @param bssid Destination array of 6 bytes receiving the parsed address.
 * @return true if the string was a valid MAC address and was fully parsed.
 */
bool parseBssid(const char* s, uint8_t* bssid);

/**
 * @brief Stores Wi-Fi network name, password, visibility, channel, and BSSID information in filesystem.
 * @param network_name SSID identifier array string.
 * @param network_pass Authorization password key phrase array.
 * @param is_hidden Flag status string indicating visibility settings ("true" or "false").
 * @param channel Wi-Fi channel the connection was established on.
 * @param bssid BSSID (MAC address) of the access point, 6 raw bytes.
 */
void saveNetworkInfo(const char *network_name, const char* network_pass, const char* is_hidden,
                     uint8_t channel, const uint8_t* bssid);

/**
 * @brief Formats raw data packets and dispatches an asynchronous standard 48-byte request to an active UDP port.
 * @param address Destination IP address of remote NTP server.
 */
void sendPacket(IPAddress& address);

/**
 * @brief Connects directly to a pooled European Network Time Server to capture and process epoch parameters.
 * @return true if an NTP response packet was successfully parsed and applied.
 */
bool updateTimeFromNTP();
