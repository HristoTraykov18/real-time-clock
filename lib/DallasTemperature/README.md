# DallasTemperature (trimmed for the Real Time Clock project)

This is a trimmed copy of the
[Arduino Library for Maxim Temperature Integrated Circuits](https://github.com/milesburton/Arduino-Temperature-Control-Library)
**4.0.6** by Miles Burton et al. It is vendored in `lib/` and reworked to contain only
the code Real Time Clock software actually uses: reading the temperature
in degrees Celsius from a DS18B20-family sensor by device index.

## Supported devices

DS18B20, DS18S20, DS1822, DS1820, MAX31820, DS1825/MAX31850 (family detection is kept intact).

## Wiring

You will need a pull-up resistor of about 5 KOhm between the 1-Wire data line
and your 5V power. If you are using the DS18B20, ground pins 1 and 3. The
centre pin is the data line '1-wire'.

## Usage in this project

```cpp
temperatureSensor.begin();              // Required - enumerates the bus
temperatureSensor.requestTemperatures();
current_temperature = temperatureSensor.getTempCByIndex(0);
```

`begin()` is mandatory in 4.x: `getAddress()` only searches indexes below the
device count discovered by `begin()`, so reads by index return
`DEVICE_DISCONNECTED_C` (-127) until the bus has been enumerated.

## What is included

- `DallasTemperature()` / `DallasTemperature(OneWire*)` constructors and `setOneWire()`
- `begin()` — bus enumeration with retries, parasite power detection, resolution detection
- `requestTemperatures()` — global conversion command with blocking wait, returns `request_t`
- `getTempCByIndex()`, `getTempC()`, `getTemp()`, `rawToCelsius()`
- Supporting internals: `getAddress()`, `validAddress()`, `validFamily()`,
  `isConnected()`, `readScratchPad()`, `readPowerSupply()`, `getResolution(address)`,
  `isConversionComplete()`, `millisToWaitForConversion()`, `blockTillConversionComplete()`
- Fault/status raw codes used by `calculateTemperature()`: disconnected, MAX31850
  open / short-to-GND / short-to-VDD, power-on-reset and insufficient-power detection

## What was removed (compared to upstream 4.0.6)

- All alarm support (`REQUIRESALARMS` section: alarm search, handlers, high/low alarm temps)
- Fahrenheit conversions and constants (`getTempF*`, `toFahrenheit`, `toCelsius`,
  `rawToFahrenheit`, `celsiusToRaw`, `DEVICE_*_F`)
- Per-device conversion requests (`requestTemperaturesByAddress` / `ByIndex`)
- Resolution setters and the global resolution getter (`setResolution`, `getResolution()`)
- Scratchpad writing and EEPROM save/recall (`writeScratchPad`, `saveScratchPad*`,
  `recallScratchPad*`, auto-save flag)
- User data storage in the alarm registers (`setUserData*` / `getUserData*`)
- External strong pull-up support (`DallasTemperature(OneWire*, uint8_t)`, `setPullupPin`)
- Conversion flag setters/getters, device counters (`getDeviceCount`, `getDS18Count`,
  `verifyDeviceCount`), `isParasitePowerMode()`, `isConnected(address)` single-argument
  overload, the extra `blockTillConversionComplete` / `millisToWaitForConversion`
  overloads, custom `new` / `delete` operators (`REQUIRESNEW`) and the `__STM32F1__` include branch

## Changes inherited from the 3.9.0 → 4.0.6 update

- Power-on-reset filtering: an unconverted DS18B20 no longer reports a bogus **+85 °C**;
  it returns `DEVICE_POWER_ON_RESET_RAW`, which reads back as -127 °C
- Insufficient-power detection (`0xFF 0x07` scratchpad pattern)
- Explicit sign extension for negative temperatures, raw values widened to `int32_t`
- MAX31850 thermocouple fault codes and resolution handling
- `begin()` retries the bus search up to 3 times with a 50 ms settle delay
- Conversion wait uses a captured timestamp (race-condition fix)
- `getTemp()` / `getTempC()` accept an optional retry count (default 0)

## Dependencies

Requires [OneWire](https://github.com/PaulStoffregen/OneWire) `^2.3.5` from
Paul Stoffregen, exactly like upstream.

## Examples

The `examples/` folder is kept unchanged from upstream 4.0.6 for reference. The `Simple`
and `Single` examples map closest to the retained API; examples using alarms, user data,
Fahrenheit output or scratchpad writing rely on removed code and will not compile against
this copy. They can be built against the original library.

## License and credits

Original library by Miles Burton, Tim Newsome, Guil Barros and Rob Tillaart,
licensed under the MIT License. This trimmed copy keeps the same license — see `LICENSE`.
