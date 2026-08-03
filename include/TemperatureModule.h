/**
 * @file TemperatureModule.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Handles reading and formatting temperature values for display outputs.
 * @version 1.0
 * @date 2026-05-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"
#include "DisplayUtils.h"

/**
 * @brief Decodes and prints the current temperature reading to the TM1637 display segments.
 */
void printCurrentTemperature();

/**
 * @brief Handles switching display states between time and temperature.
 */
void printCurrentTimeOrTemperature();