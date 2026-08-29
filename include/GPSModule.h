/**
 * @file GPSModule.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief GPS module functions.
 * @version 1.0
 * @date 2026-08-11
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"
#include "CoreUtils.h"

/**
 * @brief Starts the GPS serial link and opens a fresh acquisition window.
 */
void activateGPS();

/**
 * @brief Reports whether a usable satellite fix is currently available.
 * @return bool true if the date, time and location fields are valid and were refreshed less than
 * GPS_MAX_FIX_AGE milliseconds ago.
 */
bool gpsHasFreshFix();

/**
 * @brief Current acquisition state of the GPS module.
 * @return GpsState The state the module is in, used to decide which time source to use.
 */
GpsState gpsState();

/**
 * @brief Feeds buffered NMEA bytes to the parser and advances the acquisition state.
 * Must be called on every loop iteration, otherwise the serial buffer overflows and sentences
 * are lost.
 */
void serviceGPS();

/**
 * @brief Writes the current GPS time to the RTC, applying timezone and daylight saving offsets.
 * @return bool true if a fresh fix was available and the RTC was updated.
 */
bool updateTimeFromGPS();
