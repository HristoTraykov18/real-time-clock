---
name: rtc-setup-launcher
description: Reference for rtc_setup.pyw and rtc_setup_utils.py, the Tkinter launcher that prepares Real_Time_Clock_Compile and runs PlatformIO builds, uploads and the serial monitor for the real-time-clock ESP8266/ESP32 firmware. Use when building or flashing a clock, changing the launcher, or debugging why a build or the web files came out wrong.
---

# rtc_setup — build and upload launcher

`rtc_setup.pyw` is a small Tkinter GUI in the root of the real-time-clock repo. It never builds the repo in place: it copies the sources into `Real_Time_Clock_Compile/`, edits the **copies** (debug flag, AP name and password, software update login, timezone, web-page addons, minification) and then runs PlatformIO Core against that folder in a separate terminal window. The repository sources stay untouched.

Files:
- `rtc_setup.pyw` — window (`SetupApp`), PlatformIO discovery, terminal spawning and command chaining.
- `rtc_setup_utils.py` — `CompileFolder` (all file preparation) and `StatusArea` (message block widget). Exports `PROJECT_FILE = "platformio.ini"`, `SECRETS_FILE = "rtc_secrets.ini"` and `SECRETS_EXAMPLE_FILE = "rtc_secrets.example.ini"`.
- `last_id.txt` — next clock ID to assign (plain integer, gitignored).
- `rtc_secrets.ini` — local credentials (gitignored): `[secrets]` with `ap_password`, `ota_user`, `ota_password`. `rtc_secrets.example.ini` is the committed template with placeholders (`ChangeMe1234`, `admin`).
- `Real_Time_Clock_Compile/` — generated build copy (gitignored).

For the firmware itself see `docs/rtc-firmware.md` (also used as the `real-time-clock-firmware` skill).

## 1. Requirements and launch

- Python 3 with Tkinter and pip. The web-page minifiers `rjsmin` and `rcssmin` (`MINIFIER_PACKAGES`) are installed automatically on the first filesystem build (see step 4 below).
- `rtc_secrets.ini` in the repo root; a firmware preparation fails without it ("rtc_secrets.ini not found. Copy rtc_secrets.example.ini and set the values"). Filesystem builds do not read it.
- PlatformIO Core, found in this order (`find_platformio()`): `platformio` or `pio` on PATH → `%PLATFORMIO_CORE_DIR%` or `~/.platformio/penv/Scripts/platformio.exe` (`bin/platformio` on Linux/macOS), the PlatformIO IDE venv → `python -m platformio` if installed in the running interpreter (swaps `pythonw.exe` for `python.exe` so output is visible). The result is cached after the first success.
- Run from the repo root: `pythonw rtc_setup.pyw` (or double-click). **All paths are relative to the current working directory** (`platformio.ini`, `data/`, `last_id.txt`, the compile folder), so launching from elsewhere shows "No environments found".
- Designed for Windows (author's machine); Linux/macOS paths exist but are secondary.

## 2. Window controls

| Control | Effect |
|---|---|
| **Test Environment** checkbox (called "RTC Info Messages" before Oct 2026) | Inverted variable: checked → `production=False` → debug serial output stays on, and in the copied `src/CoreUtils.cpp` and `src/DisplayUtils.cpp` the second `tm1637.showNumber()` argument 32 becomes 64 (test display hardware). Unchecked → `DEBUG_MESSAGES` set to `false` in the copied `Globals.h`, `showNumber()` untouched. Checked by default. |
| **Edit Clock ID** checkbox | Reads `last_id.txt`; shows a `Clock ID: - 00068 +` stepper (1–99999). Fails with a status message if the file is missing or empty. |
| **Timezone offset** stepper | −12…+12, wraps around, default **+2**. Written only into the copied `espSettings.xml` (filesystem build). |
| Environment combobox | All `[env:...]` names parsed from the repo `platformio.ini` by regex, in file order; first one selected. |
| **Auto-Upload** checkbox (above the build buttons) | Default on: the upload starts right after a successful build, without a prompt. Off: "Upload now? [Y/n]" is asked first. |
| **Build and Upload Firmware** | Prepare firmware folders → `pio run` → upload (auto or after the prompt) `pio run -t upload` → optional monitor. |
| **Build and Upload File System** | Prepare `data/` → `pio run -t buildfs` → upload (auto or after the prompt) `pio run -t uploadfs` → optional monitor. A failed upload (either button) asks "Upload failed. Retry? [Y/n]" until it succeeds or N is answered; a failed build ends the task without a retry prompt. |
| **Auto-Monitor** checkbox | Appends `pio device monitor` to the same terminal after a successful upload. Default on. |
| **Serial Monitor** | Opens a monitor only, against the repo root `platformio.ini` (no preparation). |
| Status area | Green progress lines or red error, auto-cleared after 5 s. |

The three action buttons are disabled while the spawned terminal is open (polled every 500 ms) and re-enabled when it closes.

## 3. What preparation does (`CompileFolder`)

`CompileFolder(environment, timezone, clock_id=None, production=False)`; each step appends a line to `report`. Each build only refreshes the folders it owns, so a firmware build never wipes `data/` and vice versa. `Real_Time_Clock_Compile/.pio` (PlatformIO cache) is always kept. `platformio.ini` is copied in on every preparation.

### `prepare_firmware()` — folders `include`, `lib`, `src`
1. `reset_folder` deletes only those three folders in the compile copy, `copy_sources` copies them fresh plus `platformio.ini`.
2. `set_debug_messages`: when production, regex `DEBUG_MESSAGES = true` → `false` in `include/Globals.h`.
3. `set_test_display`: when not production, in `TEST_DISPLAY_SOURCES` (`src/CoreUtils.cpp`, `src/DisplayUtils.cpp`) regex `(?m)^([^/\n]*?\btm1637\.showNumber\([^,()]*,\s*)32\b` → `\g<1>64`. Only the value 32 changes, one call per line, and calls after a `//` comment (the commented OSRAM variants) are left alone. Report line `Test display: showNumber() colon value 32 changed to 64`.
4. `set_credentials`: reads `rtc_secrets.ini` through `read_secrets()` and writes into the copied `include/Globals.h`:
   - always `OTA_USER` and `OTA_PASS` (the repo holds the placeholders `admin` / `ChangeMe1234`);
   - with a clock ID also `ESP_SSID` → `"NBG_CLOCK_%05d"` and `ESP_PASS` → `ap_password`. Without an ID the repo access point values (`Test`/`Test1234`) ship.
   - `read_secrets()` uses `ConfigParser(interpolation=None)` (passwords may contain `%`) and rejects missing values, quotes, backslashes, non-printable or non-ASCII characters, and an `ap_password` outside 8–63 characters (WPA2 limits). Values are inserted with a callable `re.sub` replacement, so no regex escaping applies.

### `prepare_filesystem()` — folder `data`
1. Reset and copy `data/`, skipping `*.old.*` and `*Addon.*` files.
2. `edit_timezone`: regex replaces the `<timezoneHoursOffset>` value in `data/espSettings.xml`.
3. `add_addon_content` for `body.html`, `mainScript.js`, `mainStyle.css` (`WEBPAGE_MAIN_FILES`):
   - Active modules = tokens of the env name that are in `MODULE_TOKENS = ["bright", "gps", "temp"]` (e.g. `esp32_gps_temp_bright` → `gps`, `temp`, `bright`). Addon files are named after the token: `gpsAddon.html`, `brightAddon.js`, ...
   - HTML: each `<!-- PLACEHOLDER FOR <name> -->…<!-- END OF PLACEHOLDER FOR <name> -->` block is replaced by the file `data/<name>` when the module (`<name>` text before `Addon`, lower-cased) is active and the file exists; otherwise the default content between the markers stays and only the marker comments are removed.
   - JS/CSS: everything after `/* END OF MAIN <filename> FILE */` is dropped (the marker too), then `data/<module>Addon.js|.css` of each active module is appended.
   - Today only `gpsAddon.html` exists: GPS builds get a working GPS/Wi-Fi toggle, other builds keep a disabled Wi-Fi-only toggle.
4. `minify_webpage_files`: every `.js` file goes through `rjsmin.jsmin` and every `.css` file through `rcssmin.cssmin` (real tokenizers: strings, template literals, regex literals and newlines needed for automatic semicolon insertion are preserved). The packages come from `import_minifiers()`: it imports them, and when that fails runs `<sys.executable> -m pip install rjsmin rcssmin` (no console window on Windows), adds the per-user site folder to `sys.path` if pip fell back to a user install and that folder did not exist at start, imports again and reports "Installed rjsmin and rcssmin". When pip fails, it raises `RuntimeError` with the last 3 lines of pip's output, so the preparation stops and no unminified files are uploaded. The Tk window does not respond while pip runs (usually a few seconds). Linux system Pythons that are "externally managed" (PEP 668) refuse the install; use a venv there. `index.html` and `body.html` are not minified (`index.html` is already minified in the repo). Sizes in Oct 2026: `mainScript.js` 32.9 → 26.1 KB, `mainStyle.css` 19.9 → 13.9 KB.

Failures raise; the launcher shows "Failed to prepare the compile folder" with the exception text and does not start PlatformIO. Already-written lines remain in the status area.

### Clock ID bookkeeping
After a **successful firmware preparation** with Edit Clock ID ticked, `save_and_increment_clock_id()` increments the ID and writes it to `last_id.txt`. Filesystem builds never consume an ID.

## 4. Terminal command chain

Since Oct 2026 the terminal runs a Python step runner instead of a cmd/bash one-liner (`command_chain()` and the `choice` prompts were removed):
- `launch_terminal_task(title, steps)` takes dicts `{"arguments": [...], "prompt": str|None, "retry_prompt": str|None}`, prefixes the PlatformIO command and calls `spawn_terminal()`.
- `spawn_terminal()` writes `{"title", "steps": [{"command", "prompt", "retry_prompt"}]}` to a temp JSON file (`rtc_task_*.json`) and starts `[console_python(), TASK_RUNNER, "--run-task", <file>]`. `TASK_RUNNER` is `rtc_setup_utils.py` itself (`__main__` block). Windows: new console (`CREATE_NEW_CONSOLE`). Linux/macOS: first of `x-terminal-emulator -e`, `gnome-terminal --`, `xterm -e`; without one the runner starts detached and every prompt counts as no.
- `console_python()` returns `python.exe` next to `pythonw.exe` (a .pyw has no console); also used by `find_platformio()`.
- `run_terminal_task(path)` (in `rtc_setup_utils`): reads and deletes the task file, sets the window title (`SetConsoleTitleW` / OSC escape), runs `run_step()` per step and stops at the first failed or declined step, catches Ctrl+C (`Stopped`, e.g. leaving the monitor), then waits for "Press Enter to close".
- `run_step(step)`: asks `prompt` first if set; runs the command; on a non-zero exit asks `retry_prompt` (if set) and repeats until success or N.
- `ask()`: `[Y/n]` prompt; Enter, Y or anything except n/N is yes; EOF is no. (The old Windows `choice /c YN` ignored Enter, which is why Enter did not work as Y.)
- Steps built by `run_build_and_upload`: build (no prompt, no retry) → upload (`prompt` = None with Auto-Upload, else "Upload now?"; `retry_prompt` = "Upload failed. Retry?") → monitor if Auto-Monitor. `run_monitor` sends one monitor step.

Equivalent manual commands (from the repo root, env `esp8266_temp_bright` as an example):
```
pio run -d Real_Time_Clock_Compile -e esp8266_temp_bright
pio run -d Real_Time_Clock_Compile -e esp8266_temp_bright -t upload
pio run -d Real_Time_Clock_Compile -e esp8266_temp_bright -t buildfs
pio run -d Real_Time_Clock_Compile -e esp8266_temp_bright -t uploadfs
pio device monitor -d . -e esp8266_temp_bright
```
(Preparation must have been run first, or the compile folder holds stale sources.)

## 5. Typical workflows

- **First use on a machine**: copy `rtc_secrets.example.ini` to `rtc_secrets.ini` and set the access point password and the software update login. The minifiers install themselves on the first filesystem build (internet access needed once).
- **New clock**: tick Edit Clock ID (check the ID), untick Test Environment for production, set timezone, select the env matching the fitted modules, Build and Upload Firmware, then Build and Upload File System. Both are needed on a blank board.
- **Firmware-only update**: firmware button only; settings and saved Wi-Fi on the device survive. Over the air is also possible: build, then upload `Real_Time_Clock_Compile/.pio/build/<env>/firmware.bin` through the clock's update page (port 1394, `/sourceControl`).
- **Web UI or default-settings change**: filesystem button.
- **Debugging**: keep Test Environment ticked (also switches the display colon value to 64) and Auto-Monitor on; the monitor uses 115200 baud with the platform exception decoder.

## 6. Extending the launcher

- New PlatformIO env: nothing to do, the combobox reads `platformio.ini`.
- New module: add its env-name token to `MODULE_TOKENS`; name addon files `<token>Addon.html/.js/.css` so `<name>.split("Addon")[0].lower()` equals the token; add a placeholder block in `body.html` for HTML addons.
- New web file needing addons: add it to `WEBPAGE_MAIN_FILES` and end it with `/* END OF MAIN <filename> FILE */`.
- New substitution in a copied source: add a method using `apply_substitutions(path, (pattern, replacement), ...)`, call it from `prepare_firmware()` or `prepare_filesystem()`, append a `report` line.
- New secret (a value that must not be committed): add its key to the tuple in `read_secrets()`, a placeholder constant in `Globals.h`, a substitution in `set_credentials()`, and the key to `rtc_secrets.example.ini`.
- Keep writes inside `COMPILE_FOLDER`; that invariant is what keeps the repo clean.
- Planned (audio module, not implemented): an "Audio Module" checkbox below the environment dropdown (unchecked by default) that limits the list to ESP32 environments and shows a disabled "Base | Extended" toggle; the build then uses a derived environment written into the compile copy of `platformio.ini`. See section 5 of `docs/audio-module-base.spec`; the mechanism is still under discussion.

## 7. Known behaviour and limitations (Oct 2026 review)

Accepted as is (the launcher is a convenience tool):
- **The clock ID is consumed when preparation succeeds**, before PlatformIO runs. A failed build or a declined upload still increments `last_id.txt`.
- **Working-directory dependence**: launching from another folder finds no environments and writes the compile folder in the wrong place.
- **Timezone only reaches the device through the filesystem upload**; a firmware-only flash keeps the old timezone.
- **`uploadfs` replaces the whole LittleFS partition**: the device loses `/creds.txt` (saved Wi-Fi) and any settings changed from the web UI, which reset to `data/espSettings.xml` plus the chosen timezone. A full/partial erase option is planned (TODO.md).
- The Serial Monitor button uses the repo root `platformio.ini`, not the compile copy; fine unless the two diverge.

Open:
- [Certain] The access point password and the software update login used before Oct 2026 are in the public git history. Clocks flashed before the move to `rtc_secrets.ini` keep them until reflashed; new values only take effect for new builds.
