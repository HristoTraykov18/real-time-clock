/**
 * @file ServerUtils.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief HTTP request handlers for the Smart Clock's web interface.
 * @version 1.0
 * @date 2026-05-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"
#include "CoreUtils.h"
#include "ConfigUtils.h"

/**
 * @brief Starts the OTA software update server on first request and confirms activation to the client.
 */
void handleActivateSoftwareUpdate();

/**
 * @brief Applies automatic or manual brightness settings received from the client and persists them to the filesystem.
 * @return const char* Localized confirmation message for the client.
 */
const char* handleBrightnessControl();

/**
 * @brief Deletes the saved network credentials file and disconnects from the current network, if credentials exist.
 */
void handleDeleteCreds();

/**
 * @brief Reports device health metrics (free heap, max free block, heap fragmentation) to the client.
 */
void handleDeviceMonitoring();

/**
 * @brief Extends the active client session by refreshing the HTTP activity timestamp.
 */
void handleExtendSession();

/**
 * @brief Synchronizes the RTC over the Internet if a network is available, otherwise applies the client-provided time.
 * @return const char* Localized status message describing the used synchronization path.
 */
const char* handleManualTimeSync();

/**
 * @brief Notifies the client of an expired session and deauthenticates stale softAP stations.
 */
void handleSessionTimeout();

/**
 * @brief Executes timer start / pause / resume commands received from the client.
 * @return const char* Localized confirmation or error message for the client.
 */
const char* handleTimerControl();

/**
 * @brief Serves the main web page and dispatches client requests to the matching handler.
 */
void handleWebInterface();

/**
 * @brief Handles time synchronization through Wi-Fi, validating credentials and updating time when connected.
 * @param ssid Target wireless access point name provided by the client.
 * @return const char* Localized status message describing the connection and synchronization result.
 */
const char* handleWifiTimeSync(const String& ssid);

/**
 * @brief Mounts HTTP endpoint listeners, server routes, and configures software updates.
 */
void initializeServers();

/**
 * @brief Sends the timezone offset and saved-network availability to the client.
 */
void sendAdditionalSettings();

/**
 * @brief Sends connection status, signal strength, IP, MAC, and current time as a formatted response to the client.
 */
void sendClockInfo();

/**
 * @brief Streams the list of Wi-Fi networks in range with their signal strength as a chunked response to the client.
 */
void sendNetworksList();

/**
 * @brief Sends a plain text response to the client and refreshes the HTTP activity timestamp.
 * @param webpage_response Text message payload to deliver.
 */
void sendWebpageResponse(const char *webpage_response);

/**
 * @brief Streams a file from the filesystem to the client, honoring HTTP Range requests for partial content.
 * @param filename Path of the file inside the LittleFS storage.
 * @param content_type MIME type reported to the client.
 */
void streamFileToClient(const char *filename, const char *content_type);

/**
 * @brief Validates client-provided network credentials and attempts connection followed by a time update.
 * @param ssid Target wireless access point name.
 * @param pass Network password provided by the client.
 * @param is_hidden Flag status string indicating visibility settings ("true" or "false").
 * @return const char* Localized status message describing the connection and synchronization result.
 */
const char* validateNetworkInput(const String& ssid, const String& pass, const String& is_hidden);
