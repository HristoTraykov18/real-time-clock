/**************************************************************************/
/*!
  @file     RTClib.cpp

  @mainpage Adafruit RTClib (trimmed)

  @section intro Introduction

  This is a fork of JeeLab's fantastic real time clock library for Arduino,
  trimmed for the NEON.BG Real Time Clock project.

  @section classes Available classes

  This trimmed library provides the following classes:

  - DateTime represents a specific point in time; this is the data
    type used for setting and reading the RTC
  - RTC_DS3231 interfaces the DS3231 chip

  @section license License

  Original library by JeeLabs https://jeelabs.org/pub/docs/rtclib/, released to
  the public domain.

  This version: MIT (see LICENSE)
*/
/**************************************************************************/

#include "RTClib.h"

#ifdef __AVR__
#include <avr/pgmspace.h>
#elif defined(ESP8266)
#include <pgmspace.h>
#elif defined(ARDUINO_ARCH_SAMD)
// nothing special needed
#elif defined(ARDUINO_SAM_DUE)
#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

/**************************************************************************/
/*!
    @brief Write value to register.
    @param reg register address
    @param val value to write
*/
/**************************************************************************/
void RTC_I2C::write_register(uint8_t reg, uint8_t val) {
  uint8_t buffer[2] = {reg, val};
  i2c_dev->write(buffer, 2);
}

/**************************************************************************/
/*!
    @brief Read value from register.
    @param reg register address
    @return value of register
*/
/**************************************************************************/
uint8_t RTC_I2C::read_register(uint8_t reg) {
  uint8_t buffer[1];
  i2c_dev->write(&reg, 1);
  i2c_dev->read(buffer, 1);
  return buffer[0];
}

/**************************************************************************/
// utility code, some of this could be exposed in the DateTime API if needed
/**************************************************************************/

/**
  Number of days in each month, from January to November. December is not
  needed. Omitting it avoids an incompatibility with Paul Stoffregen's Time
  library. C.f. https://github.com/adafruit/RTClib/issues/114
*/
const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30};

/** Unixtime for 2000-01-01 00:00:00, the base of the internal representation */
constexpr uint32_t SECONDS_FROM_1970_TO_2000 = 946684800;

/**************************************************************************/
/*!
    @brief  Given a date, return number of days since 2000/01/01,
            valid for 2000--2099
    @param y Year
    @param m Month
    @param d Day
    @return Number of days
*/
/**************************************************************************/
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
  if (y >= 2000U)
    y -= 2000U;
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += pgm_read_byte(daysInMonth + i - 1);
  if (m > 2 && y % 4 == 0)
    ++days;
  return days + 365 * y + (y + 3) / 4 - 1;
}

/**************************************************************************/
/*!
    @brief  Given a number of days, hours, minutes, and seconds, return the
   total seconds
    @param days Days
    @param h Hours
    @param m Minutes
    @param s Seconds
    @return Number of seconds total
*/
/**************************************************************************/
static uint32_t time2ulong(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
  return ((days * 24UL + h) * 60 + m) * 60 + s;
}

/**************************************************************************/
/*!
    @brief  Constructor from
        [Unix time](https://en.wikipedia.org/wiki/Unix_time).

    This builds a DateTime from an integer specifying the number of seconds
    elapsed since the epoch: 1970-01-01 00:00:00.

    @see The `unixtime()` method is the converse of this constructor.

    @param t Time elapsed in seconds since 1970-01-01 00:00:00.
*/
/**************************************************************************/
DateTime::DateTime(uint32_t t) {
  t -= SECONDS_FROM_1970_TO_2000; // bring to 2000 timestamp from 1970

  ss = t % 60;
  t /= 60;
  mm = t % 60;
  t /= 60;
  hh = t % 24;
  uint16_t days = t / 24;
  uint8_t leap;
  for (yOff = 0;; ++yOff) {
    leap = yOff % 4 == 0;
    if (days < 365U + leap)
      break;
    days -= 365 + leap;
  }
  for (m = 1; m < 12; ++m) {
    uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
    if (leap && m == 2)
      ++daysPerMonth;
    if (days < daysPerMonth)
      break;
    days -= daysPerMonth;
  }
  d = days + 1;
}

/**************************************************************************/
/*!
    @brief  Constructor from (year, month, day, hour, minute, second).
    @warning If the provided parameters are not valid (e.g. 31 February),
           the constructed DateTime will be invalid.
    @param year Either the full year (range: 2000--2099) or the offset from
        year 2000 (range: 0--99).
    @param month Month number (1--12).
    @param day Day of the month (1--31).
    @param hour,min,sec Hour (0--23), minute (0--59) and second (0--59).
*/
/**************************************************************************/
DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                   uint8_t min, uint8_t sec) {

  if (year >= 2000U)
    year -= 2000U;
  yOff = year;
  m = month;
  d = day;
  hh = hour;
  mm = min;
  ss = sec;
}

/**************************************************************************/
/*!
    @brief  Return the day of the week.
    @return Day of week as an integer from 0 (Sunday) to 6 (Saturday).
*/
/**************************************************************************/
uint8_t DateTime::dayOfTheWeek() const {
  uint16_t day = date2days(yOff, m, d);
  return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

/**************************************************************************/
/*!
    @brief  Return Unix time: seconds since 1 Jan 1970.

    @see The `DateTime::DateTime(uint32_t)` constructor is the converse of
        this method.

    @return Number of seconds since 1970-01-01 00:00:00.
*/
/**************************************************************************/
uint32_t DateTime::unixtime(void) const {
  uint32_t t;
  uint16_t days = date2days(yOff, m, d);
  t = time2ulong(days, hh, mm, ss);
  t += SECONDS_FROM_1970_TO_2000; // seconds from 1970 to 2000

  return t;
}
