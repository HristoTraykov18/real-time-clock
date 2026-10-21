AUDIO MODULE BASE - SPECIFICATION (v1, after review answers, 2026-10-21)
=====================================================================

Status: draft for approval. Nothing is implemented yet.
Version: BASE. Plays audio files that are pre-loaded on the SD card, controls the volume and plays
         scheduled notifications. Managing the files from the clock (upload, delete, rename) is not
         part of this version; it is specified in audio-module-extended.spec, which builds on this one.
Platform: ESP32 only (esp32dev, WROOM-32). The module must not compile into ESP8266 builds.
Hardware: "MP3-TF-16P" board (DFPlayer Mini compatible, JieLi decoder), its micro SD card and a speaker.


1. WAY OF WORKING (project conventions to follow)
-------------------------------------------------
  - Module switch: `-D AUDIO_MODULE_BASE`. Globals.h derives `HAS_AUDIO_MODULE` (true for the base and
    the extended version) and keeps declared-only stubs in the `#else` branch; every call is guarded
    with `if constexpr (HAS_AUDIO_MODULE)`.
  - `#error` when AUDIO_MODULE_BASE or AUDIO_MODULE_EXTENDED is set without PLATFORM_ESP32, and when
    both versions are set at the same time.
  - Files (ESP32 folders, already on the ESP32 include path):
      include/esp32/AudioModule.h, src/esp32/AudioModule.cpp
          Common part, shared with the extended version: notifications, volume setting, data
          response, request validation.
      include/esp32/AudioPlayer.h, src/esp32/AudioPlayerDfPlayer.cpp
          Base player: DFPlayer driver, track count, track names. The extended version provides
          the same functions in its own file (see 3.4), so the common part does not change.
    ESP32 builds without the module exclude all audio .cpp files in build_src_filter (see 3.1).
  - Doxygen file header + @brief/@param/@return per function in the header; function definitions in
    alphabetical order in every .cpp touched; trailing "// Audio module function" comments on calls.
  - No `String` for stored or built texts: fixed `char` buffers + snprintf; parsing in place.
  - Every serial print inside `if constexpr (DEBUG_MESSAGES)` with F() strings and a [functionName] prefix.
  - Strict build flags of [diagnostics] must stay warning-free.
  - Web: Bulgarian UI texts; free-text values sent with encodeURIComponent; controls change only after
    the clock confirms (sendServerRequest() == true); one delegated event listener per table area.
  - Settings that must survive a reboot live in espSettings.xml (new tag needs a file system upload).


2. HARDWARE
-----------
2.0 The module (identified from photos)
  - Board: "MP3-TF-16P" (no version marking), 16 pins, DFPlayer Mini pinout and size.
  - Decoder: 24-pin SOP chip with the "JL" (Zhuhai JieLi) logo; part number not readable in the photo.
    Not the original YX5200-24SS. JL AB23xx-based boards are reported to work at 3.3 V and 5 V.
  - Amplifier: 8-pin "8002"-type mono bridge-tied (BTL) amplifier, about 2-3 W into 4-8 Ohm at 5 V.
  - Consequences for this spec (JL / clone behaviour):
      a) Replies to queries are not reliable: newer/clone chips send the ACK before the data, some
         answer 0 or nothing to the first query. The firmware sends commands with ACK disabled and
         never waits for a reply (see 3.4, 7.1).
      b) The BUSY pin can go LOW only ~350 ms after a play command (seen on AA19/YX clones), so
         "is playing" uses a short grace time after a play command (see 3.4).
      c) The chip needs time after power-on to read the SD card before it accepts commands, so the
         start-up commands are sent from loop() after a delay, not from setup() (see 3.4).
      d) Command support (0x12 play /mp3/NNNN, 0x16 stop, 0x06 volume, 0x48 track count) is not
         documented for JL chips. It has to be confirmed on the module (see 6, hardware checks).

2.1 Pinout (ESP32 DevKit, WROOM-32). Existing pins unchanged.

  Signal                         ESP32 pin   Peripheral pin
  ----------------------------   ---------   ---------------------------------
  TM1637 CLK (existing)          GPIO14      TM1637 CLK
  TM1637 DIO (existing)          GPIO13      TM1637 DIO
  DS3231 SDA (existing)          GPIO21      DS3231 SDA
  DS3231 SCL (existing)          GPIO22      DS3231 SCL
  Status LED (existing)          GPIO2       on-board LED
  Light sensor (existing)        GPIO35      light sensor divider output (ADC1)
  DS18B20 1-Wire (existing)      GPIO32      DS18B20 DQ
  GPS RX (existing)              GPIO27      GPS TX
  GPS TX (existing)              GPIO33      GPS RX
  DFPlayer RX       NEW          GPIO17      DFPlayer pin 2 (RX) through a 1 kOhm series resistor
  DFPlayer TX       NEW          GPIO16      DFPlayer pin 3 (TX)
  DFPlayer BUSY     NEW          GPIO34      DFPlayer pin 16 (BUSY, LOW while playing); input-only pin, OK
  DFPlayer power    NEW          5V (VIN)    DFPlayer pin 1 (VCC)
  DFPlayer ground   NEW          GND         DFPlayer pins 7 and 10 (GND)
  Speaker           NEW          -           DFPlayer pin 6 (SPK_1) and pin 8 (SPK_2), speaker < 3 W

  Notes:
  - UART: Serial1 (UART1) with explicit pins: Serial1.begin(9600, SERIAL_8N1, 16, 17).
    UART1's default pins (GPIO9/10) belong to the flash chip and must not be used. UART2 stays with the GPS.
  - GPIO16/17 are free on WROOM-32 modules (esp32dev). On WROVER modules they are used by PSRAM.
  - Add a 100-470 uF capacitor across the DFPlayer's VCC/GND close to the module (speaker current
    peaks can otherwise reset the board or cause clicks).
  - Before connecting DFPlayer TX to GPIO16: with the module on 5 V, measure TX against GND while idle.
    Up to 3.6 V: connect directly. Higher: add a divider (e.g. 1 kOhm series + 2 kOhm to GND).
  - The speaker connects only between SPK_1 and SPK_2 (BTL output). Neither speaker wire goes to GND.
  - Keep the 1 kOhm resistor on DFPlayer RX; it reduces the hum many boards pick up from the data line.
  - Constants go into include/esp32/EspPlatform.h: AUDIO_RX = 16, AUDIO_TX = 17, AUDIO_BUSY = 34,
    with a static_assert that AUDIO_TX < 34 (output-capable), like the existing OneWire check.

2.2 Connection diagram

                         +-----------------------------+
                         |      ESP32 DevKit (WROOM)   |
   TM1637 CLK  <-------- | GPIO14                  5V  | --------+----------------> DFPlayer VCC (1)
   TM1637 DIO  <-------> | GPIO13                 GND  | ----+---|----------------> DFPlayer GND (7, 10)
   DS3231 SDA  <-------> | GPIO21                      |     |  === 100-470 uF (VCC-GND, at the DFPlayer)
   DS3231 SCL  <-------- | GPIO22                      |     |
   Light sensor -------> | GPIO35                      |     |
   DS18B20 DQ  <-------> | GPIO32                      |     |
   GPS TX      -------> | GPIO27                      |     |
   GPS RX      <-------- | GPIO33                      |     |
   (on-board LED)        | GPIO2                       |     |
                         |                     GPIO17  | --[1k]--------------------> DFPlayer RX (2)
                         |                     GPIO16  | <-------------------------- DFPlayer TX (3)
                         |                     GPIO34  | <-------------------------- DFPlayer BUSY (16)
                         +-----------------------------+
                                                         DFPlayer SPK_1 (6) ---(+) Speaker
                                                         DFPlayer SPK_2 (8) ---(-) Speaker  (< 3 W)
                                                         micro SD card in the DFPlayer slot

   Power of the existing peripherals stays as currently wired.

2.3 SD card layout (prepared on a PC)
  - FAT16/FAT32, up to 32 GB.
  - Files in folder /mp3, named 0001.mp3 ... 0030.mp3 (4-digit prefix is what the DFPlayer uses;
    the rest of the name is ignored by the player, e.g. "0003_Morning bell.mp3" is allowed).
  - Formats: those the DFPlayer decodes - MP3 and WAV, 8-48 kHz sample rate.
  - No other audio files on the card (the track count query counts every file on the card).
  - Copy with a PC that leaves no hidden files on the card (macOS "._" files and "System Volume
    Information" can be counted or played by clones). Formatting before copying is recommended.
  - Up to 30 tracks are managed by the clock (tracks above 30 are ignored).
  - Tracks are played with command 0x12 (play /mp3/NNNN), which does not depend on copy order.


3. FIRMWARE
-----------
3.1 Build configuration (platformio.ini + rtc_setup)
  STATUS: proposal under discussion (2026-10-21). The mechanism was validated with PlatformIO; the
  decision between "audio only" and "all modules" is still open (see the discussion). This section
  describes the audio-only variant.
  - New configuration section, like the other modules:
        [audio_module_base]
        build_flags      = -D AUDIO_MODULE_BASE
        build_src_filter = +<esp32/AudioModule.cpp> +<esp32/AudioPlayerDfPlayer.cpp>
    No lib_deps (own DFPlayer driver, see 7.1).
  - The ESP32 source filter excludes the audio sources by default:
        [filters] esp32 = +<*> -<esp8266/> -<esp32/Audio*.cpp>
  - No new build targets. rtc_setup builds the audio firmware from the selected ESP32 environment by
    writing a derived environment into the compile copy of platformio.ini (see 5), e.g.:
        [env:esp32_temp_bright_audio_base]
        extends          = env:esp32_temp_bright
        build_flags      = ${env:esp32_temp_bright.build_flags} ${audio_module_base.build_flags}
        build_src_filter = ${env:esp32_temp_bright.build_src_filter} ${audio_module_base.build_src_filter}
  - Building without rtc_setup: add the same derived environment to platformio.ini locally.

3.2 Globals.h
  - `#if defined(AUDIO_MODULE_BASE) || defined(AUDIO_MODULE_EXTENDED)` block: includes AudioModule.h,
    HAS_AUDIO_MODULE = true. `#else`: declared-only stubs, HAS_AUDIO_MODULE = false.
  - The checks of section 1 (`#error`) sit at the top of that block.
  - New SettingId::AudioVolume + SETTING_TAGS entry `<audioVolume>` (value 0-10, default 5).
    Parsed in getInitialClockSettings() (only applied when HAS_AUDIO_MODULE).
  - data/espSettings.xml gets `<audioVolume>5</audioVolume>` (all builds; ignored without the module).

3.3 Data kept on the ESP32 (LittleFS)
  - /notifications.csv: `id|track|HH:MM|weekdays` lines; track = 4-digit track number (the extended
                        version stores the file name in this field); weekdays = 7-bit mask
                        (bit0 = Monday ... bit6 = Sunday); id 1-255, unique; max 30 lines.
                        The common part treats the track field as text (max 64 bytes), so the
                        extended version can reuse the parser.
  - /audioTracks.csv  : optional, read only. `track|name` lines with display names, UTF-8, max 32 bytes,
                        no `|` or newline. Prepared on a PC as data/audioTracks.csv and uploaded with
                        the file system (see 7.5, Q6). Tracks without an entry show "Файл NNNN".
  - Both are read once into fixed RAM arrays at boot (30 entries each); /notifications.csv is
    rewritten whole on change.
  - A file system upload (uploadfs) replaces both files, as it erases the saved network today.

3.4 Functions (alphabetical definitions in each file)
  AudioModule.cpp (common):
  - deleteNotification(id, track)
                             notification exists (id) and its stored track matches -> remove, rewrite
                             file -> confirmation text. Mismatch / missing -> error text.
  - editNotificationsFile()  writes the RAM notification array to /notifications.csv (single writer).
  - initializeAudio()        loads /notifications.csv, applies the stored volume setting, calls
                             beginAudioPlayer().
  - playTrack(track)         track exists (readTrackCount()) -> playAudioTrack() -> confirmation text.
  - readAudioData(buffer)    builds the data response (see 3.5): volume, playing track, track count,
                             track names, notifications. No confirmation message.
  - saveNotification(id, track, time, weekdays, is_edit)
                             time valid (00:00-23:59), weekdays mask non-zero, track exists,
                             no other notification at the same HH:MM (see open question Q1),
                             limit 30 (create only), on edit replace the entry with the same id,
                             rewrite file -> confirmation text.
  - serviceAudio()           called from loop() every pass; calls serviceAudioPlayer() (cheap: returns
                             at once when nothing is pending).
  - serviceNotifications()   called once per RTC second from loop(); at second 0 compares HH:MM and
                             weekday with the table and plays the match (this replaces any track that
                             is playing; the interrupted track is not resumed - see Q2). Remembers the
                             last fired minute so a repeated minute (DST change, time sync) does not
                             fire twice. Non-blocking.
  - setVolume(level)         0..10 -> setAudioPlayerVolume(), store via editSettingsFile.
  - stopTrack()              audioPlayerIsPlaying() -> stopAudioTrack() -> confirmation text.

  AudioPlayerDfPlayer.cpp (base player, implements AudioPlayer.h):
  - audioPlayerIsPlaying()   BUSY pin LOW, or a play command was sent less than 1 s ago
                             (BUSY can lag the command on clone chips). No serial round trip.
  - beginAudioPlayer()       Serial1 start, BUSY pin input, loads /audioTracks.csv. Sends nothing yet.
  - playAudioTrack(track)    command 0x12 (play /mp3/NNNN).
  - readTrackCount()         track count from the 0x48 reply, or the fallback (see serviceAudioPlayer).
  - readTrackName(track, buffer, size)
                             name from /audioTracks.csv, else "Файл NNNN".
  - serviceAudioPlayer()     non-blocking:
                               - 2 s after boot: send the stored volume, then (next pass) the track
                                 count query 0x48; at least 100 ms between two commands.
                               - reads any reply bytes that arrived (10-byte frames, checksum checked);
                                 a valid 0x48 reply sets the track count; other frames are dropped.
                               - no valid count within 3 s: track count from the fallback (see Q5).
  - setAudioPlayerVolume(level)
                             0..10 -> DFPlayer 0..30 (x3), command 0x06.
  - stopAudioTrack()         command 0x16.

  All texts returned to the web UI are Bulgarian constants or snprintf into one static buffer
  (same pattern as ServerUtils' response_buf).

3.5 HTTP API (ServerUtils.cpp, routes registered only when HAS_AUDIO_MODULE)
  GET  /audio                        -> text/plain, one record per line:
                                        V|volume
                                        P|playing track (0 = none)
                                        C|track count
                                        T|track|name             (one per track, 1..count)
                                        N|id|track|HH:MM|weekdays (one per notification)
  POST /audio-play       track=N
  POST /audio-stop
  POST /audio-volume     volume=0..10
  POST /audio-notification         id=0|N&track=N&time=HH:MM&weekdays=mask&edit=0|1
  POST /audio-notification-delete  ids=1,4,7&tracks=2,3,3   (bulk delete; one id = single delete)
  - All answers HTTP 200 with a Bulgarian message on success; validation failures answer HTTP 400 so
    the web UI's confirm-then-apply logic keeps the controls unchanged. sendServerRequest() is changed
    to show the clock's own message for HTTP 400 too (today it shows a generic "Възникна грешка!").
  - Handlers refresh last_http_activity_ms like the other handlers.

3.6 loop() / setup()
  - setup(): initializeAudio() after initializeFileSystem() (settings must be loaded first).
  - loop(): serviceAudio() every pass; serviceNotifications() inside the existing once-per-second block.


4. WEB UI
---------
4.1 New files (data/)
  - audioAddonTabBtn.html : tab button "Аудио"; replaces <!-- PLACEHOLDER FOR audioAddonTabBtn.html -->
                            placed after #js-timer-menu-button in .option-buttons-container.
  - audioAddonTab.html    : the audio panel; replaces <!-- PLACEHOLDER FOR audioAddonTab.html -->
                            placed after the TIMER PANEL in .panels-wrapper.
  - audioAddon.js         : appended to mainScript.js by rtc_setup.
  - audioAddon.css        : appended to mainStyle.css by rtc_setup.
  - Both placeholders have empty default content (no audio markup in non-audio builds).
  - The extended version adds its own addon files on top of these (see audio-module-extended.spec).

4.2 Tab button
  - Same look and behaviour as the existing tab buttons. Opens the audio panel.
  - The first opening requests GET /audio once; the loader is shown until the tables are filled;
    on failure the disconnected indicator and an error message are shown (same pattern as the side panels).
  - Data is kept in JS (tracks, notifications) and re-rendered locally after each confirmed change.

4.3 Volume control
  - "−" button, value (0-10), "+" button. Each change is sent to /audio-volume; the shown value changes
    only after the clock confirms (same rule as the other controls).

4.4 Tabbed table (100% of the form width, centred like the networks list)
  Header
    - Files view:         "Име на файл" sortable label with up/down arrows (sorts by name).
    - Notifications view: "Избери всички" checkbox, "Изтрий избраните" button (inactive until a row
                          is selected), "Файл" label (left), "Час" label (right), "Ново" button at the end.
  Rows (zebra striping with the existing colour scheme)
    - Files:         [name, left, 70%, horizontal auto-scroll when too long] [Play button: rounded
                     triangle; turns into Stop (rounded square) while that track plays]
    - Notifications: [checkbox] [name, left, 40%, auto-scroll] [time HH:MM + weekday letters,
                     left, 40%] [Edit button: paper with pen] [Delete button: bin]
                     A notification whose track is missing on the card is shown greyed with a warning.
  Footer
    - Centre: two tab buttons - "Файлове" (left: table slides right, files view) and
      "Известия" (right: table slides left, notifications view).
    - Right: pagination - first, previous (SVG icons), "Страница # / #max", next, last.
      10 rows per page (one constant, ROWS_PER_PAGE = 10).
  Events
    - One delegated click listener on the table body, one on the header, one on the footer;
      rows carry data-* attributes (track / id). No per-row listeners.

4.5 Notification popup (create and edit)
  - Audio file list: scrollable list of the names; the selected item is marked in red
    (existing --red); creating without a selected file shows an error.
  - Time: HH:MM input with scrollable hours (00-23) and minutes (00-59), styled like the timer picker.
  - Weekdays: seven toggles Пн Вт Ср Чт Пт Сб Нд; at least one must be selected.
  - "Запази" / "Отказ". Edit opens the popup prefilled.
  - Client-side checks (limit 30, duplicate time, missing file, no weekday) before the request;
    the clock repeats the checks.

4.6 Play / Stop
  - Play sends /audio-play; on success that row's button becomes Stop and the others Play.
  - When playback ends on its own, the button returns to Play on the next data refresh
    (opening the tab or any confirmed action); no polling while idle.


5. rtc_setup
------------
  STATUS: requirements agreed; the build mechanism is part of the discussion of 2026-10-21 (see 3.1).
  - New "Audio Module" checkbox below the environment dropdown, unchecked by default.
  - When checked: the dropdown lists only ESP32 environments, and a toggle appears below the checkbox
    with "Base" on the left and "Extended" on the right. The toggle is set to "Base" and disabled
    until the extended version is implemented.
  - Firmware build with the checkbox ticked: the derived environment of 3.1 is written into the
    compile copy of platformio.ini (name: <selected env>_audio_base) and built and uploaded instead
    of the selected environment. The serial monitor uses the selected environment (same board).
  - File system build with the checkbox ticked: the audio addon files of 4.1 are inlined. CompileFolder
    gets the audio version as a parameter and adds "audio" to its module list, so the addon logic
    (placeholders and appended .js/.css) works unchanged.
  - Verification: prepare_filesystem() with and without audio, check that the audio markup/JS/CSS
    appear only in the audio build, and node --check the minified mainScript.js.


6. VERIFICATION PLAN
--------------------
  - Build every ESP32 environment with and without the base configuration, with the strict flags;
    ESP8266 builds must not change.
  - Host tests of the CSV parsing/writing, notification validation and the minute-matching logic
    (including DST jump and duplicate-minute protection).
  - jsdom tests of the audio tab: data rendering, sorting, pagination, popup validation, confirm-then-apply
    on success / HTTP 400 / no connection, single delegated listeners.
  - Before the implementation (by you, 10 minutes, recommended): run the DFPlayerAnalyzer sketch or a
    short test sketch I provide on the ESP32 with your module, to confirm on the JL chip that
    0x12 plays /mp3/0001.mp3, 0x16 stops, 0x06 changes the volume, 0x48 returns the right count,
    and how long BUSY takes to go LOW. The results fix the open values in 2.0 d) and Q5.
  - On hardware (by you): play/stop, volume, a notification firing while another track plays, BUSY pin.


7. PROPOSALS (better ways, need your OK)
----------------------------------------
7.1 DFPlayer driver: own minimal driver inside AudioPlayerDfPlayer.cpp (~60-80 lines: frame build with
    checksum, ACK flag 0, commands 0x06/0x12/0x16/0x48, non-blocking reply parser). With the JL chip
    this is the stronger choice: DFRobotDFPlayerMini's read functions assume the reply order of the
    original chip (data before ACK) and block while waiting, which is the known failure on newer
    and clone chips. No lib_deps entry is needed then (3.1).
7.2 Generic tab switching: switchTab() today knows only "rtc" and "timer". Proposal: make it work over
    all tab buttons/panels via data-tab attributes, so the audio addon only adds markup. Otherwise
    audioAddon.js has to override switchTab().
7.3 Bulk delete of notifications in one request (ids list) instead of one request per selected row.
7.4 Weekday letters in the time column (e.g. "07:30  Пн-Пт") so rows stay readable without a popup.
7.5 Track names from an optional data/audioTracks.csv uploaded with the file system (3.3), so the
    list can show real names without a rename function. Without the file the list shows "Файл NNNN".


8. OPEN QUESTIONS
-----------------
Q1. Duplicate time rule with weekdays: forbid the same HH:MM in any two notifications, or only when
    their weekdays overlap (07:00 Пн-Пт and 07:00 Сб-Нд allowed)?
Q2. Interrupted audio: stop it for good (spec above), or resume it after the notification? Resuming is
    possible only with the DFPlayer's ADVERT feature, which needs a copy of each notification file in an
    /ADVERT folder and works only while something is playing.
Q3. Should notifications also play while the clock is in timer mode? (Spec above: yes, in every mode.)
Q4. Default volume for new clocks: 5 of 10?
Q5. Track count when the 0x48 query gives no valid answer:
    a) a "Брой файлове" number (1-30) set in the audio tab and stored in espSettings.xml as
       <audioTrackCount> - used as fallback, or always if you prefer (no query at all);
    b) assume 30 and let a play request for a missing track fail silently on the player.
    Proposal: a) as fallback only.
Q6. Keep the optional names file of 7.5, or show only "Файл NNNN"?
