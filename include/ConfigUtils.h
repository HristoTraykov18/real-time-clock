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
 * @brief Validates the manual brightness level (a LIGHT_LEVELS row index), applies the display
 * brightness of that row and stores the level in the ESP filesystem when it differs from the stored value.
 * @param new_value Row index formatted as string ("0" to LIGHT_LEVELS_COUNT - 1).
 * @return true when the level is valid and applied, false when it is rejected (nothing changes).
 */
bool editManualBrightness(const char new_value[]);

/**
 * @brief Modifies the RTC configuration file in the ESP filesystem.
 * Skips the write when the stored value already equals the new one.
 * @param new_value Integer value formatted as string.
 * @param id Identifies which setting to rewrite. Indexes SETTING_TAGS.
 */
void editSettingsFile(const char new_value[], SettingId id);

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

/**
 * @brief Converts a manual brightness level to its LIGHT_LEVELS row index.
 * @param value Exactly one digit, "0" to LIGHT_LEVELS_COUNT - 1.
 * @return int8_t Row index, or -1 for any other text (e.g. a tampered request or an old stored value).
 */
int8_t lightLevelIndex(const char value[]);
