/**
 * @file ConfigUtils.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Configuration utility functions for an ESP8266/ESP32-based Smart Clock.
 * Handles editing of XML configuration files and updating local variables.
 * @version 1.0
 * @date 2026-05-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"

/**
 * @brief Updates local variable status and writes the target auto-brightness boolean configuration to filesystem.
 * @param new_value String representative ("true"/"false").
 */
void editAutoBrightness(const char new_value[]);

/**
 * @brief Updates local variable status and writes the target DST switch status to filesystem.
 * @param new_value String representative ("true"/"false").
 */
void editDaylightSavingEnabled(const char new_value[]);

/**
 * @brief Updates local variable status and writes the target manual brightness value to the filesystem.
 * @param new_value Integer value formatted as string.
 */
void editManualBrightness(const char new_value[]);

/**
 * @brief Modifies underlying XML configurations directly within flash storage strings dynamically.
 * @param new_value The modified payload array to merge inside open nodes.
 * @param tags_id Numerical ID identifier referencing specific matching XML target tags.
 */
void editSettingsFile(const char new_value[], uint8_t tags_id);

/**
 * @brief Switches time update mode between GPS or NTP server.
 * @param new_value Mode indicator keyword string ("gps" or "ntp").
 */
void editTimeSyncMode(const char new_value[]);

/**
 * @brief Updates local variable status and writes timer duration in the filesystem.
 * @param new_value Duration specified in elapsed integer seconds formatted as string.
 */
void editTimerDuration(const char new_value[]);

/**
 * @brief Updates local variable status and writes system timezone offset to the filesystem.
 * @param new_value Hourly offset value specified as string.
 */
void editTimezoneOffset(const char new_value[]);

/**
 * @brief Switches between timer and clock work modes.
 * @param new_value Task profile mode selection string ("timer" or "clock").
 */
void editWorkMode(const char new_value[]);