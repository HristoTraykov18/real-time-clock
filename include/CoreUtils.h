/**
 * @file CoreUtils.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Core utility functions of the Smart Clock
 * @version 1.0
 * @date 2026-05-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"
#include "DisplayUtils.h"
#include "NetworkUtils.h"

/**
 * @brief Automatically checks and triggers a time sync operation up to 5 times at a specific hour.
 * @param force_update Set to true to bypass scheduled hour checks and execute immediately.
 * @return true if time sync succeeded, false otherwise.
 */
bool autoUpdateTime(bool force_update = false);

/**
 * @brief Applies or removes the one hour Daylight Saving Time offset on the RTC, at most once per transition.
 * Does nothing when the currently applied offset already matches the Daylight Saving Time period.
 */
void daylightSavingChange();

/**
 * @brief Parses the XML configuration file stored on the filesystem to load system settings.
 */
void getInitialClockSettings();

/**
 * @brief Returns the exact numerical calendar date of the last Sunday of the current month.
 * @param now Reference to a standard RTClib DateTime instance.
 * @return uint8_t Day value (1-31).
 */
uint8_t getLastSundayDate(DateTime &now);

/**
 * @brief Mounts the LittleFS internal storage partition and triggers configuration loads.
 */
void initializeFileSystem();

/**
 * @brief Initialized hardware lines for the I2C real-time clock, verifying valid time state.
 */
void initializeModuleRTC();

/**
 * @brief Checks whether a given timestamp or current time falls into the Daylight Saving Time period.
 * @param epoch_val Pass a specific Unix timestamp, or leave empty (-1) to fetch fresh data from the RTC.
 * @return true if currently in the DST window, false if standard time.
 */
bool isDaylightSavingPeriod(time_t epoch_val = -1);

/**
 * @brief Parses a comma-separated text string provided by a client browser to manually update the RTC.
 */
void manualTimeUpdate();

/**
 * @brief Executes standard hardware pin pulling routines to force a standard bus cycle reset on I2C lines.
 */
void resetRTC();

/**
 * @brief Handles timer functionality and expiration in a non-blocking manner.
 */
void timerCountdown();

/**
 * @brief Evaluates hardware flags and updates internal time references via GPS serial or NTP network requests.
 * @return true if time data was successfully acquired, formatted, and pushed to the RTC.
 */
bool updateTime();
