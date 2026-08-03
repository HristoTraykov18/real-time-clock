/**
 * @file DisplayUtils.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Utility functions for managing the TM1637 display of the Smart Clock, including update animations, 
 * brightness control, and time/temperature visualization.
 * @version 1.0
 * @date 2026-05-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"

/**
 * @brief Executes a sweeping structural segment transition layout representing successful synchronization.
 * @param updated_from_gps Reverses animation, if updated via GPS module.
 */
void displayClockJustUpdated(bool updated_from_gps = false);

/**
 * @brief Flashes the TM1637 display segments when user connects to the ESP access point.
 */
void flashDisplay();

/**
 * @brief Displays formatted hour/minute representations to TM1637 segments while in clock mode.
 */
void printCurrentTime();

/**
 * @brief Displays hours, minutes, or cascading remaining seconds to TM1637 segments while in timer mode.
 */
void printRemainingTime();

/**
 * @brief Displays time and temperature or (only time for clocks without temperature sensor) on the TM1637.
 */
void visualizeOnDisplay();