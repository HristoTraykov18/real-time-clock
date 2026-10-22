---
name: real-time-clock-firmware
description: Reference for the real-time-clock firmware (ESP8266/ESP32 + DS3231 RTC + TM1637 display, optional temperature, brightness and GPS modules, LittleFS web UI). Use when reading, debugging, extending or reviewing code in the real-time-clock repo (src, include, lib, data, platformio.ini).
---

# Real Time Clock firmware

Arduino-framework clock firmware built with PlatformIO for **ESP8266 (NodeMCU v2)** and **ESP32 (esp32dev)**. A DS3231 RTC keeps time, a 4-digit TM1637 display shows it, and the board runs a soft access point plus a station connection so a phone can configure it through a web page served from LittleFS. Time is synced from NTP (or GPS on GPS builds). Optional hardware modules are selected at compile time.

Author: Hristo Traykov (NEON.BG). Repo: `github.com/HristoTraykov18/real-time-clock`. MIT licence. Firmware comment says version 1.12.x. User-facing strings (web UI and HTTP replies) are **Bulgarian**.

Building/uploading is normally done through `rtc_setup.pyw` (see `docs/rtc-setup-launcher.md`, also used as the `rtc-setup-launcher` skill), which builds from a copy in `Real_Time_Clock_Compile/`, never from the repo root.

---

## 1. Repository layout

```
platformio.ini          Plain environments esp32 / esp8266 + one section per module; module builds use a generated
                        environment (rtc_build*.ini through extra_configs), see section 2
include/
  Globals.h             Central include: libraries, constants, inline globals, module #ifdef blocks
  CoreUtils.h           Time sync, DST, RTC init/reset, timer, settings load
  ConfigUtils.h         Setting setters that also rewrite espSettings.xml
  DisplayUtils.h        TM1637 rendering, animations, connect-flash
  NetworkUtils.h        Wi-Fi STA/AP management, saved credentials, NTP
  ServerUtils.h         HTTP handlers and route registration
  EspUtils.h            Platform abstraction API (implemented twice, see below)
  BrightnessModule.h / TemperatureModule.h / GPSModule.h
  esp32/EspPlatform.h   ESP32 libs, pinout, LED polarity, I2C bus-recovery primitives
  esp8266/EspPlatform.h ESP8266 equivalents
src/
  main.cpp              setup() / loop()
  CoreUtils.cpp ConfigUtils.cpp DisplayUtils.cpp NetworkUtils.cpp ServerUtils.cpp
  BrightnessModule.cpp TemperatureModule.cpp GPSModule.cpp
  esp32/EspUtils.cpp    ESP32 implementation of EspUtils.h
  esp8266/EspUtils.cpp  ESP8266 implementation of EspUtils.h
lib/                    Vendored, trimmed libraries
  RTClib/               Adafruit RTClib 2.1.4 cut down to DateTime + RTC_DS3231 (begin/adjust/now)
  TM1637/               avishorp TM1637 library, MODIFIED (see section 8)
  DallasTemperature/    Trimmed DallasTemperature (only compiled for temperature builds)
data/                   LittleFS image contents (web UI + settings)
  index.html            Tiny loader page (minified, one line)
  body.html             Page markup, contains <!-- PLACEHOLDER FOR gpsAddon.html --> block
  mainScript.js         All UI logic; ends with /* END OF MAIN mainScript.js FILE */
  mainStyle.css         Styling (CSS vars on :root, desktop media query at 1000px)
  espSettings.xml       Default settings
  gpsAddon.html         GPS/Wi-Fi toggle, inlined into body.html only for GPS builds
  neonLogoIcon.ico      Favicon (also used as the rtc_setup window icon)
rtc_setup.pyw, rtc_setup_utils.py, last_id.txt   Build launcher (see docs/rtc-setup-launcher.md)
README.md               Overview, build targets, build/upload commands, credentials, usage, library versions
docs/                   rtc-firmware.md (this document) and rtc-setup-launcher.md (references, also used as skills),
                        audio-module-base.spec and audio-module-extended.spec (audio module drafts)
  instructions/         End-user instructions, UTF-8 with CRLF: BG.txt (linked from the README "Usage" section) and
                        EN.txt (English translation, gitignored and not linked until an English UI exists).
                        Keep both in step with web UI changes; quote UI labels exactly as the page shows them
rtc_secrets.example.ini Committed template of rtc_secrets.ini (access point password, software update login)
rtc_secrets.ini         Local credentials read by rtc_setup (gitignored)
Real_Time_Clock_Compile/  Generated build copy (gitignored)
```

Keep README.md and docs/ in step with changes to features, build targets or library versions. The TODO list is kept outside the repository.

## 2. Build system and compile-time modules

### Targets (generated environments, since Oct 2026)
`platformio.ini` has only two build environments, `env:esp32` and `env:esp8266` (no modules; `default_envs` = both), plus one section per module. Every module section has the same four options:

| Section | `build_flags` | `lib_deps` | `src_filter` | `lib_ignore_off` |
|---|---|---|---|---|
| `brightness_module` | `-D BRIGHTNESS_MODULE` | – | `+<BrightnessModule.cpp>` | – |
| `gps_module` | `-D GPS_MODULE` | TinyGPSPlus 1.0.3 | `+<GPSModule.cpp>` | – |
| `temperature_module` | `-D TEMPERATURE_MODULE` | OneWire ^2.3.8 | `+<TemperatureModule.cpp>` | `DallasTemperature` |
| `audio_module_base` (ESP32) | `-D AUDIO_MODULE_BASE` | – | `+<esp32/AudioModule.cpp> +<esp32/AudioPlayerDfPlayer.cpp>` | – |
| `audio_module_extended` (ESP32) | `-D AUDIO_MODULE_EXTENDED` | (VS1053 driver later) | `+<esp32/AudioModule.cpp> +<esp32/AudioPlayerVs1053.cpp> +<esp32/AudioFiles.cpp>` | – |

A build with modules uses an environment that `extends = env:<platform>` and lists, as multi-line options: `build_flags`, `lib_deps` and `build_src_filter` = the platform value followed by the selected sections' `build_flags` / `lib_deps` / `src_filter`, and `lib_ignore` = the `lib_ignore_off` of every module that is NOT selected. rtc_setup writes it to `rtc_build.ini` in the compile copy for every build; `platformio.ini` loads `rtc_build*.ini` through `extra_configs` (a missing file is ignored). Name: `<platform>_<tokens>` with the module tokens in alphabetical module order (`bright`, `gps`, `temp`, then `audio_base` / `audio_extended`), or `<platform>_only`; e.g. `esp32_bright_gps_temp`. By hand: put such an environment in `rtc_build_local.ini` in the project root (gitignored), see README.
- Validated Oct 2026: all 8 ESP32 combinations built from generated environments give the same firmware sizes as the former fixed environments; `DallasTemperature` is linked only with the temperature module. Letting the LDF evaluate `#ifdef`s instead of `lib_ignore` (`lib_ldf_mode = chain+` / `deep+`) does not work with ESP32 core 3.3.11 (`Network.h` not found in the WiFi library).

- `[esp32_base]`: pioarduino platform-espressif32 release `55.03.311` (installs as espressif32 55.3.311, Arduino core 3.3.11), `board = esp32dev`, `default.csv` partitions, upload 921600, `-D PLATFORM_ESP32 -Iinclude/esp32`, exception decoder.
- `[esp8266_base]`: `platform = platformio/espressif8266 @ 4.2.1` (pinned; framework-arduinoespressif8266 3.30102.0 = Arduino core 3.1.2), `board = nodemcuv2`, upload 256000, `-D PLATFORM_ESP8266 -Iinclude/esp8266`, adds `EspSoftwareSerial`.
- `[filters]` exclude the other platform's `src/esp*/` folder and every module source (`BrightnessModule.cpp`, `GPSModule.cpp`, `TemperatureModule.cpp`, ESP32 `esp32/Audio*.cpp`); the module sections add theirs back.
- `lib_ignore = DallasTemperature` globally (plain environments); generated environments replace it with the `lib_ignore_off` of the unselected modules. All environments pull `Adafruit BusIO` (needed by vendored RTClib).
- `[diagnostics]` applies strict warnings to project sources only (`build_src_flags`): `-Wall -Wextra -Wdouble-promotion` and `-Werror` for return-type, uninitialized, implicit-fallthrough, format, write-strings, sign-compare. New code must compile clean under these.
- `pio check` is configured with cppcheck over `src/` and `include/`.
- Filesystem is LittleFS (`board_build.filesystem = littlefs`), monitor 115200.

### How a module is switched on/off in code
`Globals.h` has one block per module:
```cpp
#ifdef TEMPERATURE_MODULE
  #include ... ; inline objects ...; constexpr bool HAS_TEMPERATURE_MODULE = true;
#else
  void initializeTemperatureSensor();  // declared only, never defined
  void printCurrentTimeOrTemperature();
  constexpr bool HAS_TEMPERATURE_MODULE = false;
#endif
```
Call sites are always wrapped in `if constexpr (HAS_X_MODULE) { ... }`. The declared-only stubs let the code compile when the module is absent; the discarded `if constexpr` branch means they are never linked. **Any new call to a module function outside an `if constexpr (HAS_...)` guard will fail to link on builds without that module.**

`set_time_with_gps` exists in both branches (always `false` without GPS). `GpsState` enum is defined unconditionally.

### Platform abstraction
- `#include "EspPlatform.h"` resolves to `include/esp32/` or `include/esp8266/` through the `-I` flag. It provides libraries, `using WebServerClass = WebServer | ESP8266WebServer`, pin constants, `LIGHT_LEVELS`, `LED_ON/LED_OFF`, I2C bus-recovery primitives (`SDA_LOW/HIGH`, `SCL_LOW/HIGH`, `SDA_READ`, `I2C_RESTART`) and, for GPS builds, `gpsSerial`.
- `EspUtils.h` declares `esp*` wrappers implemented separately in `src/esp32/EspUtils.cpp` and `src/esp8266/EspUtils.cpp`: Wi-Fi event subscriptions, softAP station eviction, `LittleFS.begin` (ESP32 passes `formatOnFail=true`), heap stats, auth-failure detection, EU country code, modem sleep, HTTP updater setup, station SSID read, scan helpers (`espScanNetworks/Ssid/Bssid/Channel/Delete`), `espGpsSerialBegin`.
- **Rule:** platform-specific API calls go into both `EspUtils.cpp` files behind a wrapper; shared code calls only `esp*` functions.

Notable platform differences:
- Auth failure: both platforms set a `sta_auth_failed` flag from the STA-disconnect reason (`AUTH_FAIL` 202, `HANDSHAKE_TIMEOUT` 204, `4WAY_HANDSHAKE_TIMEOUT` 15); both also count `AUTH_EXPIRE` 2 (measured on ESP8266, Oct 2026: a wrong WPA2 password gives reason 2 about 5.5 s after `WiFi.begin()`; an empty password on a protected network gives 202). ESP8266 also accepts `WL_WRONG_PASSWORD`. AUTH_EXPIRE can also come from a weak signal, so a barely reachable network may be reported as a wrong password. The `Wi-Fi status N` debug line prints the status after the rewrite to `WL_NO_SSID_AVAIL` (1), not the raw value. A second flag `sta_assoc_failed` is set for `ASSOC_FAIL` 203 (seen on ESP8266 with a WPA3-only network) and read with `espIsAssocFailure()`. `connectClockToNetwork()` calls `espClearConnectFailures()` (clears both flags) before `WiFi.begin()`, so both only report the current attempt. Before Oct 2026 the ESP8266 relied on `WL_WRONG_PASSWORD` alone, but at the first disconnect event the status still reads `WL_DISCONNECTED` (SDK still connecting), so a wrong password was reported as "не е в обхват"; the flag also used to survive into later attempts, so a timeout after a wrong-password attempt could answer "Грешна парола!". Debug builds print the disconnect reason on both platforms.
- Station eviction: ESP8266 deauths each station via `wifi_softap_deauth`; ESP32 calls `esp_wifi_deauth_sta(0)` (all).
- I2C recovery: ESP8266 toggles output-enable registers (`GPES/GPEC`), no restart needed; ESP32 uses `pinMode` and then `Wire.end(); Wire.begin();`.

## 3. Hardware and pinout

| Signal | ESP8266 (NodeMCU) | ESP32 (esp32dev) |
|---|---|---|
| TM1637 CLK | D4 | GPIO14 |
| TM1637 DIO | D5 | GPIO13 |
| DS3231 SDA / SCL | D2 (GPIO4) / D1 (GPIO5) default | GPIO21 / GPIO22 default |
| Status LED | GPIO16, active LOW | GPIO2, active HIGH |
| Light sensor (brightness module) | A0, 10-bit | GPIO35 (ADC1), 12-bit |
| DS18B20 1-Wire (temperature module) | D3 (GPIO0) | GPIO32 (static_assert ≤ 33) |
| GPS RX / TX (GPS module) | D7 / D8 via SoftwareSerial | GPIO27 / GPIO33 via UART2 (`Serial2`) |

`LIGHT_LEVELS` = `{sensor ceiling, display brightness}` rows: ESP8266 `{75,0},{200,2},{500,3},{1023,7}`; ESP32 `{300,0},{700,2},{1500,3},{4095,7}`.

ESP32 pin rules: GPIO34–39 are input-only (fine for the light sensor, never for a TX, CLK or 1-Wire line); OneWire cannot drive pins above GPIO33 (enforced by `static_assert`); only ADC1 pins (32–39) read while Wi-Fi is on. GPIO14 is taken by the TM1637 CLK, so it cannot be reused.

## 4. Runtime flow

### setup() (`main.cpp`)
1. `Serial.begin(115200)` only when `DEBUG_MESSAGES`.
2. LED off; `initializePeripherals()`: display shows `88:88`, `rtc.begin()` retried forever with `resetRTC()` (I2C bus recovery: clock SCL until SDA is released, LED lit during reset), then retried while the RTC returns impossible hour/min/sec; init temperature sensor if built.
3. `initializeFileSystem()`: mount LittleFS (loops until success) then `getInitialClockSettings()` parses `/espSettings.xml` into globals.
4. Apply brightness, compute `daylight_saving_active`, `activateGPS()` if GPS is the saved source. (The GPS activation inside `initializePeripherals` never fires because settings are not loaded yet; the one in setup is the effective one.)
5. Wi-Fi: `persistent(false)`, `setAutoReconnect(false)`, `WIFI_AP_STA`, attach event handlers, `initializeSoftAP()`, disconnect STA, modem sleep off, EU country, start 5 s radio-settle window.
6. `initializeServers()`: UDP 2390 for NTP, register HTTP routes, configure (not start) OTA server.
7. `delay(5000)`, then if `networkReconnect()` connects, `autoUpdateTime(true)`.

### loop()
- `server.handleClient()`; OTA server `handleClient()` only while active; `serviceGPS()` on GPS builds (must run every iteration).
- Once per RTC second (`rtc.now().second()` changed): timer mode → `timerCountdown()`, clock mode → `autoUpdateTime()` (fires at 03:00:00); `visualizeOnDisplay()`; debug status line; `checkForClientConnection()`; `manageStaReconnect()`.

Many paths block (Wi-Fi connect up to 18 s, scans, NTP wait 1.5 s x up to 5, update animation ~0.7 s). The 03:00:00 sync only runs if loop() sees that exact second.

## 5. Timekeeping

- RTC stores **local wall-clock time including DST**. `daylight_saving_active` records whether the +1 h is currently applied.
- `updateTime()`: GPS if selected and not `TimedOut`, otherwise `networkReconnect()` + `updateTimeFromNTP()`.
- `autoUpdateTime(force)`: at 03:00:00 (`UPDATE_HOUR`) or when forced, tries `updateTime()` up to 5 times. If all fail on the scheduled run and DST is enabled, `daylightSavingChange()` shifts the RTC by ±1 h through the Unix timestamp.
- NTP: DNS for `0.europe.pool.ntp.org` (any `hostByName()` result other than `1` is a failure: ESP8266 returns `0`, the ESP32 core returns the negative error code, e.g. `-54`, so never test it with `!`), flush stale UDP, send 48-byte request, wait ≤ 1.5 s, reject wrong source IP, LI=3, mode≠4, stratum 0 or >15, or timestamps before 2020. Result: `ntp - 2208988800 + timezone_offset*3600 + 1` passed to `applyTimeUpdate()`. Sets `connected_to_ntp = true`.
- GPS: UTC from TinyGPS++ RMC + `timezone_offset*3600` → `applyTimeUpdate(..., true)` (animation plays reversed).
- `applyTimeUpdate(standard_epoch, from_gps)`: adds 3600 if DST enabled and in period, `rtc.adjust`, updates `daylight_saving_active`, plays the 9-frame segment-fill animation.
- Manual (phone) time: `timeSyncMode=js&currentTime=Y,M,D,h,m,s` where **M is JavaScript's 0-based month**; firmware adds 1. Used when no saved network connects.
- DST rule is **EU only**: April–September always; March from 03:00 local standard on the last Sunday; October until 02:00 local standard on the last Sunday (`getLastSundayDate`). These hours match the EU 01:00 UTC switch only for UTC+2 (Bulgaria); other offsets switch an hour off.
- Timezone: whole hours, −12…+12 (`timezoneHoursOffset`). No half-hour zones.

## 6. Networking

- **SoftAP**: SSID/password `ESP_SSID`/`ESP_PASS` in `Globals.h` (repo defaults `Test`/`Test1234`; for numbered clocks rtc_setup rewrites them to `NBG_CLOCK_<5-digit id>` and `ap_password` from `rtc_secrets.ini`). Channel = saved STA channel or 1. **Max 1 client.** Web UI at `192.168.4.1`.
- **Saved network** `/creds.txt` (plain text, 5 lines): `SSID\nPASSWORD\ntrue|false (hidden)\nCHANNEL\nAA:BB:CC:DD:EE:FF`. Written by `saveNetworkInfo()` after a successful connect (skips the write when unchanged). Read with `loadNetworkInfo()` into a 128-byte buffer.
- `networkReconnect()`: if a cached channel+BSSID still answers a single-channel scan (`fastConnectCacheIsValid`, 5 tries), fast-connect (7.5 s timeout); otherwise full connect (18 s). `connectClockToNetwork()` aborts early when an STA-disconnect event arrives, returning the auth-failure status or `WL_NO_SSID_AVAIL`.
- `manageStaReconnect()` (every second): only when creds exist, no phone is associated to the AP and the radio-settle window has passed. Up to 3 attempts, then backs off until the RTC hour changes.
- **Phone session**: `checkForClientConnection()` treats a client as present while associated **and** an HTTP request arrived in the last `AP_CONNECTION_TIMEOUT` (610 s). On leave: stop OTA server, evict stations. The page (only when opened at 192.168.4.1) counts 600 s of inactivity, warns for the last 30 s, then calls `/timeout` (server deauths) or `/extend`.
- On AP association the display flashes 6 times (`flashDisplay`); phase differs depending on whether an NTP sync has happened.

## 7. Settings file and HTTP API

### `/espSettings.xml`
```xml
<daylightSavingEnabled>true|false</daylightSavingEnabled>
<timeSyncMode>wifi|gps</timeSyncMode>
<autoBrightnessControl>true|false</autoBrightnessControl>
<manualBrightnessLevel>0-3 (LIGHT_LEVELS row index)</manualBrightnessLevel>
<timezoneHoursOffset>-12..12</timezoneHoursOffset>
<workMode>rtc|timer</workMode>
<timerDuration>seconds, 0-359999</timerDuration>
```
- Order of `enum class SettingId` must match `SETTING_TAGS[]` (enforced by `static_assert`).
- Parsing and editing use `strstr` on a 512-byte buffer; values are cut at 15 chars. `editSettingsFile()` replaces the text between existing tags; **it cannot add a tag that is missing** from the file on the device. New settings therefore need a filesystem upload (which also wipes `/creds.txt`).
- `editSettingsFile()` compares the stored text with the new value and skips the flash write when they are equal. `editManualBrightness()` relies on this alone (the level on screen may come from the light sensor, so it is not compared against); it first validates the level with `lightLevelIndex()` (exactly one digit `0`…`LIGHT_LEVELS_COUNT - 1`, otherwise −1), then sets `display_brightness` and `last_display_brightness` to `LIGHT_LEVELS[level][1]` and returns true. At boot an invalid stored level (e.g. `6` from a file of the old 0–6 slider) is ignored and `DEFAULT_BRIGHTNESS` stays. The other setters also compare with their global first; they then update the global (and side effects: DST change → `daylightSavingChange()`, GPS selected → `activateGPS()`, timer start → reset timer state).

### Routes (`initializeServers()`, port 80)
| Route | Response / effect |
|---|---|
| `/` no args | streams `/index.html` |
| `/` with args | dispatch below, plain-text reply in Bulgarian |
| `/info` | `ssid|rssi|ip|mac|HH:MM:SS` (`-|-0|-|...` when not connected; rssi without sign) |
| `/additional-settings` | `timezone|true/false` (creds file exists) |
| `/networks` | chunked lines `ssid|rssi\n` (reuses a running/finished scan, else blocking scan) |
| `/delete-creds` | removes `/creds.txt`, `WiFi.disconnect(true)` |
| `/timeout`, `/extend` | end session (deauth) / refresh activity timestamp |
| `/activate-update` | starts OTA server, replies `OK` |
| `/settings` | raw `espSettings.xml` |
| `/m` | heap: max block, free heap, fragmentation % |
| `/body.html`, `/mainStyle.css`, `/mainScript.js`, `/neonLogoIcon.ico` | static, with HTTP Range support |

`handleWebInterface()` argument dispatch (order matters, `else if` chain):
1. `workMode=rtc|timer` is applied first if present.
2. `timeSyncMode=wifi` (+`ssid`, `pass` empty for an open network or ≥ 8 chars, `isHiddenNetwork`) → `handleWifiTimeSync` → `joinRequestedNetwork()`: for a visible network `networkIsInRange(ssid, &needs_password)` also reads the scan's auth mode (`espScanNeedsPassword()`: not open, and on ESP32 not OWE); an empty password for a protected network is answered at once with `Мрежата <ssid> изисква парола!` without a connect attempt. A rejected login with an empty password (e.g. a hidden network, no scan) gives the same text; with a password it is `Грешна парола!`. An association refusal (203) answers `Часовникът не може да се свърже с мрежа <ssid>.` + `Видът защита на мрежата (напр. WPA3) не се поддържа.` (203 can also mean MAC filtering or a full client list, hence the "напр."). (Renamed from `validateNetworkInput()` in Oct 2026; it picks a format string per outcome and ends with one `snprintf`, success goes through `createReply()`. Do not enable `-Wformat-nonliteral`: with `-Werror=format` it turns into an error, flags this function and also `ESP8266WebServer-impl.h:544` in the core, so every ESP8266 build fails.); `=gps` → `handleGPSTimeSync` (20 attempts × 250 ms); `=js` (+`currentTime`) → `handleManualTimeSync`.
3. else `daylightSavingEnabled=true|false`.
4. else `autoBrightnessControl=true|false` (+`manualBrightnessLevel` 0–3, used only with `false`). Any other `autoBrightnessControl` text or an invalid level is answered with HTTP 400 `Невалидна настройка за яркост!` (`sendWebpageResponse(text, 400)`) and changes nothing; `handleBrightnessControl()` then returns nullptr.
5. else `status=start|pause|resume` (+`duration` seconds), only when in timer mode.
6. else serve index.html.
7. Independently afterwards: `timezoneHoursOffset=<int>`.

Responses for long-running operations are produced only after the blocking work finishes; the UI uses a 20 s fetch timeout for POSTs.

### OTA
Separate `WebServerClass` on **port 1394**, path `/sourceControl`, HTTP authentication with `OTA_USER` / `OTA_PASS` from `Globals.h`. The repository holds the placeholders `admin` / `ChangeMe1234`; rtc_setup writes the real login from the gitignored `rtc_secrets.ini` into the compile copy. Never put real credentials in the repository sources. Started by `/activate-update`; the UI then navigates to `http://<host>:1394/sourceControl`. Stopped when the phone session ends.

## 8. Display (TM1637)

- Brightness 0–7 (`DEFAULT_BRIGHTNESS = 2`). Applied every second in `flashDisplay()`; the connect/NTP flash toggles between 0 and the top `LIGHT_LEVELS` brightness (`LIGHT_LEVELS[LIGHT_LEVELS_COUNT - 1][1]`, 7 on both platforms).
- **Vendored library is modified**: constructor has a 4th parameter `reverse_digit_logic` (inverted segment table for OSRAM-based units NBG_CLOCK_00001/00002; see commented lines in `printCurrentTime`), and `showNumber()` leading-zero behaviour differs from upstream: with `leading_zero=false` only the first digit is blanked when zero, later zeros are drawn (05:07 renders ` 5:07`, 00:05 renders ` 0:05`). `dots=32` turns the colon on with this hardware, `0` off. Do not replace with the upstream library.
- Clock mode: `printCurrentTime()` H:MM with colon blinking each second. Timer mode: `printRemainingTime()` shows HH:MM, or MM:SS under 1 h; colon blinks while running, steady while paused.
- Timer state: `timer_duration` is an `int32_t` count of remaining seconds (UI maximum 99:59:59 = 359999 s, which displays as `99:59`). It was `int16_t` before Oct 2026 and wrapped above 32767 s (9:06:07). `timer_status` 0 paused / 1 running; `timer_millis` and `timer_millis_offset` keep sub-second progress across pause/resume. `timerCountdown()` runs once per RTC second and stops at 0.
- Raw glyphs are bytes `0bGFEDCBA` (e.g. `0x63` degree, `0x39` C, `0x40` minus).
- `animateTimeUpdate()` 9 frames × 75 ms, reversed for GPS.

## 9. Modules

**Temperature** (DS18B20 via OneWire + trimmed DallasTemperature): display alternates 7 s time / 4 s temperature. First reading blocking at boot, then non-blocking (`setWaitForConversion(false)`, request one tick, read the next). `getTempC()` returns a float that is stored in an `int8_t`, so the value is truncated toward zero (23.9 → 23, −4.9 → −4). This is the intended behaviour, not a bug. −127 means no answer. Valid range −79…99; three consecutive bad readings show `----`. Formats: ` 5°C`, `-5°C`, `-12°`, `23°C`.

**Brightness** (photoresistor on ADC): when auto is on and no flash is running, maps the reading to a `LIGHT_LEVELS` row. A one-step change must persist 3 seconds; a jump of 2+ steps applies immediately. Manual brightness (all builds, not only `_bright`): the web slider has 4 positions, 0–3 (`body.html` `max="3"` must stay `LIGHT_LEVELS_COUNT - 1`); the position is the stored value and selects a `LIGHT_LEVELS` row, so the manual levels are the same 4 brightnesses the sensor uses (0, 2, 3, 7). `LIGHT_LEVELS_COUNT` lives in `Globals.h` with a `static_assert` of 1–10 rows (one-digit level).

**GPS** (u-blox, 9600 baud, TinyGPS++): on activation sends UBX-CFG-MSG frames leaving only RMC enabled, then sets `GPS_TX` to INPUT (boot strapping pin). States `Disabled → Searching → Locked`, `TimedOut` after 180 s without a fix (NTP is then used). Fix is fresh if date+location valid and age < 2 s. The README calls the GPS module not fully tested. Web UI shows an enabled GPS/Wi-Fi toggle only in GPS builds (via `gpsAddon.html`).

**Audio** (planned, ESP32 only, not implemented yet): two versions specified in `docs/audio-module-base.spec` (MP3-TF-16P / DFPlayer compatible board, files pre-loaded on its SD card, notifications and volume; switch `-D AUDIO_MODULE_BASE`) and `docs/audio-module-extended.spec` (VS1053 board with SD card, adds upload, delete and rename from the web UI; switch `-D AUDIO_MODULE_EXTENDED`). Both set `HAS_AUDIO_MODULE`. Their `platformio.ini` sections exist (`audio_module_base`, `audio_module_extended`); the module code does not exist yet.

## 10. Web UI (`data/`)

- `index.html` shows a placeholder, then fetches `mainStyle.css`, `body.html`, `mainScript.js` in parallel with `fetchWithRangeRetry` (resumes partial downloads with `Range`, up to 30 tries) and injects them. This exists because large responses over the softAP drop; keep `streamFileToClient()` Range support intact.
- Two tabs: Clock (network list, hidden-network manual entry, sync method, DST and auto-brightness checkboxes, brightness slider) and Timer (HH:MM:SS picker up to 99 h, start, pause/resume). Side panels: info (`/info`, ticking clock) and additional settings (timezone ±, delete saved network, software update).
- On load: `/settings` + `/networks` in parallel; in clock mode it immediately posts the phone's time (`timeSyncMode=js`). Clicking the Clock tab posts it again (which triggers a reconnect attempt on the device).
- Settings controls change only after the clock confirms the request. `sendServerRequest()` returns `true` only for an HTTP 200 answer. The daylight-saving and auto-brightness checkboxes and the Wi-Fi/GPS switch (`toggleDaylightSaving`, `toggleBrightnessControl`, `toggleTimeSyncMode`) call `event.preventDefault()` in their `click` handler, which keeps the box unchanged, and set `checked` (and the slider visibility) after a successful answer. The brightness slider keeps the last accepted level in `dataset.confirmedValue` and moves back when saving fails; the timezone number goes back to `confirmedTimezone`. New controls should follow the same pattern.
- All POSTs are `application/x-www-form-urlencoded` bodies built by string concatenation. Free-text values (`ssid`, `pass`) are wrapped in `encodeURIComponent()` in `connectToSelectedNetwork()` and `submitNetworkRequest()`; the ESP8266 and ESP32 WebServer libraries split the body on `&`/`=` and then decode `%XX` and `+`, so the handlers receive the original text. Numbers, booleans and `currentTime` (digits and commas) are sent raw. **Any new free-text field must be encoded the same way**, or `&`, `=`, `+` and `%` in it will corrupt the request.
- `rtc_setup` minifies the JS/CSS (comment stripping + whitespace collapse) and inlines addons, so: keep statements semicolon-terminated, keep the `/* END OF MAIN <file> FILE */` marker as the last line of `mainScript.js`/`mainStyle.css`, and keep placeholder comments exact.

## 11. Code conventions

- Headers in `include/` carry Doxygen file blocks (author, version, date, MIT) and `@brief/@param/@return` per function; `.cpp` files define functions roughly in alphabetical order.
- Functions camelCase, variables snake_case, constants UPPER_SNAKE, globals are C++17 `inline` variables in `Globals.h`.
- Every serial print is inside `if constexpr (DEBUG_MESSAGES)` and uses `F()` strings with a `[functionName]` prefix. rtc_setup flips `DEBUG_MESSAGES` to false for production builds.
- Prefer fixed `char` buffers and `snprintf` over `String`; SSID buffers are 33 bytes.
- Cross-module calls get a trailing comment naming the module (`// Network Utils`, `// Esp Utils`, `// GPS module function`).

## 12. Recipes

**Add a persisted setting**
1. Append to `SettingId` (before `Count`) and `SETTING_TAGS` in `Globals.h`; add a global.
2. Add the tag with a default to `data/espSettings.xml` (keep the file under 512 bytes, value under 16 chars).
3. Parse it in `getInitialClockSettings()` (`CoreUtils.cpp`).
4. Add `editX()` in `ConfigUtils.h/.cpp`.
5. Dispatch it in `handleWebInterface()` (mind the `else if` order) and read/send it in `mainScript.js` (`requestConfig()` and a sender).
6. Devices need a filesystem upload to get the new tag.

**Add an HTTP endpoint**: declare/define the handler in `ServerUtils`, `server.on(...)` in `initializeServers()`, reply via `sendWebpageResponse()` (refreshes the session timestamp) or set `last_http_activity_ms` yourself.

**Add a hardware module**
1. `[x_module]` section in `platformio.ini` with all four options (`build_flags = -D X_MODULE`, `lib_deps`, `src_filter = +<XModule.cpp>`, `lib_ignore_off`, empty values allowed), and `-<XModule.cpp>` in the `[filters]` line of each platform it supports. Add it to `MODULES` in `rtc_setup_utils.py` (label → token, section); the launcher then shows it in the module list. No new environments.
2. `#ifdef X_MODULE` block in `Globals.h` with includes/objects and `HAS_X_MODULE = true`, `#else` block with declared-only stubs and `false`.
3. Guard every call with `if constexpr (HAS_X_MODULE)`.
4. Pins in both `EspPlatform.h` files; platform-specific calls via `EspUtils`.
5. For web UI parts, add `<token>Addon.html/.js/.css` and a placeholder, where `<token>` is the module's env-name token (`bright`, `gps`, `temp`, ...); register the token in `rtc_setup_utils.MODULE_TOKENS`.

**Add platform-specific behaviour**: declare in `EspUtils.h`, implement in both `src/esp32/EspUtils.cpp` and `src/esp8266/EspUtils.cpp`.

**Check a change before flashing**
- Firmware: build every module combination the change can reach (generated environments; all 8 per platform when in doubt). Project sources compile under the strict `[diagnostics]` flags, so a clean build means no new warnings. On ESP8266 `int32_t` is `int`; on ESP32 (IDF 5) it is `long` — avoid `%d`/`%ld` format assumptions.
- Web UI: run `CompileFolder(platform, modules, tz).prepare_filesystem()` from `rtc_setup_utils` and `node --check Real_Time_Clock_Compile/data/mainScript.js`, because the shipped file is the minified one.
- Request handling: the device sees `server.arg()` values after `urlDecode`, so test free-text input with `& = + % space` and non-ASCII (Cyrillic) characters.

## 13. Known issues and risks (as of Oct 2026 review)

- [Certain] The access point password and OTA login used before Oct 2026 remain in the public git history. Clocks flashed before the move to `rtc_secrets.ini` keep them until they are reflashed with new values.
- [Certain] UART pins on ESP32 must be output-capable for TX: in Arduino core 3.3.11 `uartBegin()` installs no driver at all (RX included) when TX is GPIO34–39.
- [Certain] `sendPacket()` does not zero its 48-byte NTP buffer; bytes 4–11 and 16–47 are stack garbage. [Likely] harmless with public pools, but not spec-clean.
- [Certain] Saved Wi-Fi password is stored in plain text in `/creds.txt` (hashing options are on the TODO list).
- [Certain] Daylight saving is EU-only.
- [Likely] If loop() is blocked through 03:00:00 (e.g. a reconnect attempt), that day's scheduled sync and DST fallback are skipped.
- [Certain] Joining a new network from the web page can outlast the page's 20 s request timeout (`NETWORK_REQUEST_TIMEOUT_MS`) in rare cases. Accepted as is (Oct 2026).
  - Measured (ESP32, Oct 2026): on a network without Internet the router answers the DNS query with an error at once (`hostByName(): DNS Failed ... error '-54'`) and all 5 NTP attempts were over in about 6–7 s. Until the fix of `readTimestamp()` each failed lookup was treated as a success and spent 1.5 s waiting for a reply from `0.0.0.0`; now a fast DNS failure ends each attempt immediately. A healthy connect takes about 5–11 s.
  - Worst case, only when the DNS server never answers: blocking scan (ESP8266 about 1.6 s, ESP32 ≤ about 4.3 s) + 18.0 s `connectClockToNetwork()` timeout + 5 DNS timeouts. Each lookup gives up after 7 lwIP timer ticks (1+1+2+3 s) = 6–7 s **per DNS server** the router hands out; ESP8266 caps `hostByName()` at 10.0 s, ESP32 has no cap. Totals: ESP8266 ≈ 54.6 s (1 server) / ≈ 69.6 s (2); ESP32 ≈ 57.3 s (1) / ≈ 92.3 s (2) / ≈ 127 s (3).
  - DNS works but NTP does not answer: 5 × 1.5 s = 7.5 s more. If the link drops again during the NTP attempts, each attempt also runs `networkReconnect()` (up to about 26 s more each).
  - The softAP also moves to the router's channel when the station connects (one radio, one channel), which drops the phone's connection mid-request on a first connect to a router not on the AP's channel. The page can then report a failure although the clock connected. Proposed remedies if needed: stop the 5 attempts after a DNS failure, raise the page timeout for this request, or reply at once and let the page poll for the result.
- [Certain] The timer duration is range-checked only by the web page (99:59:59); a direct request can store larger or negative values.
