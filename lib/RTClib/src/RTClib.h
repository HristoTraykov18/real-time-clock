/**************************************************************************/
/*!
  @file     RTClib.h

  Original library by JeeLabs http://news.jeelabs.org/code/, released to the
  public domain

  License: MIT (see LICENSE)

  This is a fork of JeeLab's fantastic real time clock library for Arduino,
  trimmed for the NEON.BG Real Time Clock project. It contains only the
  code required by the clock software: the DateTime class and the DS3231
  driver (begin / adjust / now). See README.md for details about what was
  removed compared to the upstream Adafruit RTClib 2.1.4.
*/
/**************************************************************************/

#ifndef _RTCLIB_H_
#define _RTCLIB_H_

#include <Adafruit_I2CDevice.h>
#include <Arduino.h>

/**************************************************************************/
/*!
    @brief  Simple general-purpose date/time class (no TZ / DST / leap
            seconds).

    This class stores date and time information in a broken-down form, as a
    tuple (year, month, day, hour, minute, second). The day of the week is
    not stored, but computed on request. The class has no notion of time
    zones, daylight saving time, or
    [leap seconds](http://en.wikipedia.org/wiki/Leap_second): time is stored
    in whatever time zone the user chooses to use.

    The class supports dates in the range from 1 Jan 2000 to 31 Dec 2099
    inclusive.
*/
/**************************************************************************/
class DateTime {
public:
  DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0,
           uint8_t min = 0, uint8_t sec = 0);
  DateTime(const DateTime &copy);

  /*!
      @brief  Return the year.
      @return Year (range: 2000--2099).
  */
  uint16_t year() const { return 2000U + yOff; }
  /*!
      @brief  Return the month.
      @return Month number (1--12).
  */
  uint8_t month() const { return m; }
  /*!
      @brief  Return the day of the month.
      @return Day of the month (1--31).
  */
  uint8_t day() const { return d; }
  /*!
      @brief  Return the hour
      @return Hour (0--23).
  */
  uint8_t hour() const { return hh; }
  /*!
      @brief  Return the minute.
      @return Minute (0--59).
  */
  uint8_t minute() const { return mm; }
  /*!
      @brief  Return the second.
      @return Second (0--59).
  */
  uint8_t second() const { return ss; }

  uint8_t dayOfTheWeek() const;

protected:
  uint8_t yOff; ///< Year offset from 2000
  uint8_t m;    ///< Month 1-12
  uint8_t d;    ///< Day 1-31
  uint8_t hh;   ///< Hours 0-23
  uint8_t mm;   ///< Minutes 0-59
  uint8_t ss;   ///< Seconds 0-59
};

/**************************************************************************/
/*!
    @brief  A generic I2C RTC base class. DO NOT USE DIRECTLY
*/
/**************************************************************************/
class RTC_I2C {
protected:
  /*!
      @brief  Convert a binary coded decimal value to binary. RTC stores
    time/date values as BCD.
      @param val BCD value
      @return Binary value
  */
  static uint8_t bcd2bin(uint8_t val) { return val - 6 * (val >> 4); }
  /*!
      @brief  Convert a binary value to BCD format for the RTC registers
      @param val Binary value
      @return BCD value
  */
  static uint8_t bin2bcd(uint8_t val) { return val + 6 * (val / 10); }
  Adafruit_I2CDevice *i2c_dev = NULL; ///< Pointer to I2C bus interface
  uint8_t read_register(uint8_t reg);
  void write_register(uint8_t reg, uint8_t val);
};

/**************************************************************************/
/*!
    @brief  RTC based on the DS3231 chip connected via I2C and the Wire library
*/
/**************************************************************************/
class RTC_DS3231 : RTC_I2C {
public:
  bool begin(TwoWire *wireInstance = &Wire);
  void adjust(const DateTime &dt);
  DateTime now();
  /*!
      @brief  Convert the day of the week to a representation suitable for
              storing in the DS3231: from 1 (Monday) to 7 (Sunday).
      @param  d Day of the week as represented by the library:
              from 0 (Sunday) to 6 (Saturday).
      @return the converted value
  */
  static uint8_t dowToDS3231(uint8_t d) { return d == 0 ? 7 : d; }
};

#endif // _RTCLIB_H_
