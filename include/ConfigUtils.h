/**
 * @file ConfigUtils.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Configuration functions for handling XML files and updating local variables.
 * @version 1.0
 * @date 2026-05-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"

/**
 * @brief Updates the auto-brightness local boolean and stores the value in the ESP filesystem.
 * @param new_value String ("true"/"false").
 */
void editAutoBrightness(const char new_value[]);

/**
 * @brief Updates the daylight saving enabled local boolean and stores the value in the ESP filesystem.
 * @param new_value String ("true"/"false").
 */
void editDaylightSavingEnabled(const char new_value[]);

/**
 * @brief Updates the manual brightness local integer and stores the value in the ESP filesystem.
 * @param new_value Integer value formatted as string.
 */
void editManualBrightness(const char new_value[]);

/**
 * @brief Modifies the RTC configuration file in the ESP filesystem.
 * @param new_value Integer value formatted as string.
 * @param tags_id Numerical ID identifier referencing specific matching XML target tags.
 */
void editSettingsFile(const char new_value[], uint8_t tags_id);

/**
 * @brief Updates the time synchronization mode between GPS or NTP server
 * and stores the value in the ESP filesystem.
 * @param new_value String ("gps"/"ntp").
 */
void editTimeSyncMode(const char new_value[]);

/**
 * @brief Updates the timer duration local integer and stores the value in the ESP filesystem.
 * @param new_value Integer value formatted as string.
 */
void editTimerDuration(const char new_value[]);

/**
 * @brief Updates the timezone offset local integer and stores the value in the ESP filesystem.
 * @param new_value Integer value formatted as string.
 */
void editTimezoneOffset(const char new_value[]);

/**
 * @brief Updates the work mode local boolean and stores the value in the ESP filesystem.
 * @param new_value String ("rtc"/"timer").
 */
void editWorkMode(const char new_value[]);
