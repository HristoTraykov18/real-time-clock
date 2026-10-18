# Real Time Clock

Personal clock project for **ESP8266** (NodeMCU v2) and **ESP32** (ESP32 DevKit) boards. A DS3231 real-time clock keeps the time, a 4-digit TM1637 display shows it, and a web interface served by the board is used for all settings.

## Features

- Time synchronization over Wi-Fi (NTP, `0.europe.pool.ntp.org`), daily at 03:00 and on request, with a GPS receiver as an optional source
- Manual time setting from the phone's clock when no network is available
- Timezone offset (whole hours, -12 to +12) and automatic EU daylight saving time
- Clock and countdown timer modes (timer up to 99:59:59)
- Web interface (in Bulgarian) through the clock's own access point at `192.168.4.1`, or its IP address in the connected network
- Password protected over-the-air firmware update
- Optional modules, selected at build time:
  - **Temperature** - DS18B20 sensor, alternates time and temperature on the display
  - **Brightness** - light sensor for automatic display brightness
  - **GPS** - u-blox receiver as a time source (not fully tested)

## Build targets

`platformio.ini` defines one environment per board and module combination:

| Environment | Modules |
|---|---|
| `esp8266_only`, `esp32_only` | none |
| `*_temp`, `*_bright`, `*_gps` | one module |
| `*_temp_bright`, `*_gps_temp`, `*_gps_bright` | two modules |
| `*_gps_temp_bright` | all modules |

Pin assignments are in `include/esp8266/EspPlatform.h` and `include/esp32/EspPlatform.h`.

## Building and uploading

Requires [PlatformIO Core](https://platformio.org/install/cli) (or the PlatformIO IDE) and Python 3 with Tkinter and pip. The first file system build installs the web page minifiers `rjsmin` and `rcssmin` with pip; if that install fails, the build stops with pip's error.

The easiest way is the setup launcher, started from the project root:

```
pythonw rtc_setup.pyw
```

Select the environment, then use **Build and Upload Firmware** and **Build and Upload File System** (a new board needs both). The launcher builds from a prepared copy in `Real_Time_Clock_Compile/`, where it sets the debug messages, the clock's access point name (`NBG_CLOCK_<ID>`) and password, the software update login, the timezone and the module parts of the web interface. The repository sources are never changed.

Without the launcher, PlatformIO can build the repository directly. The launcher's edits are then missing: the access point is `Test` / `Test1234`, the software update login is the `admin` / `ChangeMe1234` placeholder, debug messages are on, the timezone is 0 and the web interface has no GPS switch.

```
pio run -e esp8266_temp_bright -t upload      # firmware
pio run -e esp8266_temp_bright -t uploadfs    # web interface and default settings (LittleFS)
pio device monitor -e esp8266_temp_bright     # serial monitor, 115200 baud
```

Uploading the file system replaces the whole LittleFS partition, including the saved network and any settings changed from the web interface.

### Credentials

The access point password of numbered clocks and the login of the software update page are not stored in the repository. Copy `rtc_secrets.example.ini` to `rtc_secrets.ini` (ignored by git) and set your own values. The launcher refuses to prepare a firmware build without this file.

## Usage

Step-by-step instructions for connecting to the clock and using its web interface:

- [Български](instructions/BG.txt)

In short:

1. Connect a phone or computer to the clock's Wi-Fi access point.
2. Open `http://192.168.4.1`. While no saved network is reachable, the clock takes the time from the device on every visit.
3. Pick a network from the list (or enter a hidden one) to enable automatic time synchronization. Leave the password empty for an open network.

## Project structure

```
src/, include/   Firmware sources and headers (platform specific code in esp32/ and esp8266/ subfolders)
lib/             Vendored libraries (trimmed or modified)
data/            Web interface and default settings, uploaded to LittleFS
instructions/    User instructions (BG.txt in Bulgarian)
rtc_setup.pyw    Build and upload launcher (with rtc_setup_utils.py and rtc_secrets.example.ini)
```

## Libraries

| Library | Version | Notes |
|---|---|---|
| ESP8266 Arduino core | 3.1.2 | PlatformIO `platformio/espressif8266 @ 4.2.1` |
| ESP32 Arduino core | 3.3.11 | pioarduino `platform-espressif32` 55.03.311 |
| [RTClib](https://github.com/adafruit/RTClib) | 2.1.4 | Vendored in `lib/`, trimmed to the DS3231; needs Adafruit BusIO ^1.14.1 |
| [TM1637](https://github.com/avishorp/TM1637) | - | Vendored in `lib/`, modified (reversed digit logic, leading zero handling) |
| [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library) | 4.0.6 | Vendored in `lib/`, trimmed; temperature builds only |
| [OneWire](https://github.com/PaulStoffregen/OneWire) | ^2.3.8 | Temperature builds only |
| [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus) | 1.0.3 | GPS builds only |
| [EspSoftwareSerial](https://github.com/plerup/espsoftwareserial) | - | ESP8266 GPS builds only |

## License

MIT, see [LICENSE](LICENSE).
