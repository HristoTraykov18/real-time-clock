# RTClib (trimmed for the Real Time Clock project)

This is a trimmed copy of [Adafruit RTClib](https://github.com/adafruit/RTClib) **2.1.4**,
a fork of JeeLab's real time clock library for Arduino. It is vendored in `lib/` and
reworked to contain only the code the NEON.BG Real Time Clock software actually uses.

## What is included

- `DateTime` — broken-down date/time value (range: 1 Jan 2000 – 31 Dec 2099)
  - `DateTime(year, month, day, hour, minute, second)` constructor and copy constructor
  - `year()`, `month()`, `day()`, `hour()`, `minute()`, `second()`
  - `dayOfTheWeek()` — ranges from 0 to 6 inclusive with 0 being 'Sunday'
- `RTC_I2C` — internal I2C base class (BCD helpers, register read/write)
- `RTC_DS3231` — the only supported chip
  - `begin()`, `adjust()`, `now()`, `dowToDS3231()`

## What was removed (compared to upstream 2.1.4)

- All other RTC drivers: `RTC_DS1307`, `RTC_PCF8523`, `RTC_PCF8563`, `RTC_Millis`, `RTC_Micros`
- The `TimeSpan` class and all `DateTime` arithmetic / comparison operators
- `DateTime` string handling: `toString()`, `timestamp()`, ISO 8601 and
  `__DATE__` / `__TIME__` constructors, `unixtime()`, `secondstime()`, `isValid()`,
  `twelveHour()`, `isPM()`
- DS3231 extras: alarms, square wave / 32K output control, `getTemperature()`, `lostPower()`

## Dependencies

The library still requires [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO)
(`Adafruit_I2CDevice`) and the `Wire` library, exactly like upstream.

## Examples

The `examples/` folder is kept unchanged from upstream for reference. Only the
`ds3231` example is compatible with this trimmed version as-is; the remaining examples
use classes and methods that were removed and will not compile against this copy.
They can be built against the original Adafruit RTClib.

## License and credits

Original library by JeeLabs (public domain), forked and maintained by
[Adafruit](https://github.com/adafruit/RTClib). This trimmed copy keeps the original
MIT license — see `license.txt`. Adafruit invests time and resources providing open
source code, please support Adafruit and open-source hardware by purchasing products
from Adafruit!
