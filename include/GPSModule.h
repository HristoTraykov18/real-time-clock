/**
 * @file GPSModule.h
 * @brief Interfaces for configuring a hardware GPS module and syncing the RTC.
 */

#pragma once
#include "Globals.h"
#include "DisplayUtils.h"

/**
 * @brief Allocates connection windows and initializes the serial line interface for the GPS unit.
 */
void activateGPS();

/**
 * @brief Attempts to parse satellite telemetry strings to pull down automated atomic clock updates.
 * @return true if spatial coordinates were acquired and the RTC was successfully adjusted.
 */
bool updateTimeFromGPS(TinyGPSDate &d, TinyGPSTime &t);