/**
 * @file CoreUtils.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Core utility functions for handling time updates, 
 * daylight saving adjustments, RTC initialization and timer functionality.
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
 * @brief Triggers a time sync operation up to 5 times at a specific hour or on web UI request.
 * @param force_update Bypasses scheduled hour check if set to true.
 * @return bool true if time sync succeeded.
 */
bool autoUpdateTime(bool force_update = false);

/**
 * @brief Updates the RTC time via provided timestamp.
 * Applies the daylight saving offset, keeps daylight_saving_active in sync,
 * and plays time update animation.
 * @param standard_epoch Local standard time as a Unix timestamp, without a daylight saving offset.
 * @param updated_from_gps Set to true when the timestamp came from the GPS module.
 */
void applyTimeUpdate(time_t standard_epoch, bool updated_from_gps = false);

/**
 * @brief Applies or removes the one hour Daylight Saving Time offset on the RTC, at most once per transition.
 * Does nothing when the currently applied offset already matches the Daylight Saving Time period.
 */
void daylightSavingChange();

/**
 * @brief Loads system settings by parsing the XML configuration file stored on the ESP filesystem.
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
 * @brief Initializes I2C bus for the DS3231 module, verifying valid time state.
 */
void initializeModuleRTC();

/**
 * @brief Checks whether a given timestamp or current time falls into the Daylight Saving Time period.
 * @param epoch_val Local standard time as a Unix timestamp, or -1 to evaluate the current RTC time.
 * @return bool true if currently in the DST window.
 */
bool isDaylightSavingPeriod(time_t epoch_val = -1);

/**
 * @brief Parses a comma-separated text string provided by the web UI to manually update the RTC.
 */
void manualTimeUpdate();

/**
 * @brief Triggers an RTC DS3231 module reset via the I2C bus.
 */
void resetRTC();

/**
 * @brief Handles timer functionality and expiration in a non-blocking manner.
 */
void timerCountdown();

/**
 * @brief Requests a time update from the selected source.
 * GPS is used when it's the selected mode, the module is installed and has not timed out,
 * otherwise time is updated from NTP.
 * @return bool true if time data was successfully acquired and pushed to the RTC.
 */
bool updateTime();
