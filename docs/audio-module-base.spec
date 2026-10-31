AUDIO MODULE BASE - SPECIFICATION (v3, as implemented, 2026-10-25)
=====================================================================

Status: implemented (2026-10-25), not tested on the hardware yet (see 6). Where the implementation
        differs from v2, the text says "(as built)".
Version: BASE. Plays audio files that are pre-loaded on the SD card, controls the volume and plays
         scheduled notifications. Managing the files from the clock (upload, delete, rename) is not
         part of this version; it is specified in audio-module-extended.spec, which builds on this one.
Platform: ESP32 only (esp32dev, WROOM-32). The module must not compile into ESP8266 builds.
Hardware: "MP3-TF-16P" board (DFPlayer Mini compatible, JieLi decoder), its micro SD card and a speaker.


1. WAY OF WORKING (project conventions to follow)
-------------------------------------------------
  - Module switch: `-D AUDIO_MODULE_BASE`. (as built) Globals.h reads the two flags once, into
    HAS_AUDIO_MODULE_BASE and HAS_AUDIO_MODULE_EXTENDED; HAS_AUDIO_MODULE is true for either version.
    Every call is guarded with `if constexpr (HAS_AUDIO_MODULE)`.
  - (as built) `static_assert` instead of `#error`: the audio module needs PLATFORM_IS_ESP32 (a constant
    of EspPlatform.h), and only one version may be selected.
  - Files (sources in the ESP32 folder; the headers are common, see 3.2):
      include/AudioModule.h, src/esp32/AudioModule.cpp
          Common part, shared with the extended version: notifications, volume setting, data
          response, request validation.
      include/AudioPlayer.h, src/esp32/AudioPlayerBase.cpp
          Base player: track list, track names, command queue; sends through the vendored and trimmed
          DFRobotDFPlayerMini library (lib/, see 7.1).
          The extended version provides the same functions in its own file (see 3.4), so the
          common part does not change. (as built) Tracks are addressed by their index in the track
          list; the text a notification stores for a track ("track field") is readTrackKey(index):
          "0003" in the base version, the file name in the extended one.
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
      d) Command support (0x12 play /mp3/NNNN, 0x16 stop, 0x06 volume) is not documented for JL
         chips. It has to be confirmed on the module (see 6, hardware checks). The track count
         query (0x48) is not used: the count comes from /audio.csv (3.3, decision Q5).

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
  - Constants go into include/esp32/EspPlatform.h (as built: in every ESP32 build, like GPS_RX/GPS_TX): AUDIO_RX = 16, AUDIO_TX = 17, AUDIO_BUSY = 34,
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
  - Every file on the card has a line in data/audio.csv with the same number (3.3).
  - Copy with a PC that leaves no hidden files on the card (macOS "._" files and "System Volume
    Information" can be played by clones). Formatting before copying is recommended.
  - Up to MAX_AUDIO_TRACKS = 30 tracks are managed by the clock (audio.csv lines above 30 are ignored).
  - Tracks are played with command 0x12 (play /mp3/NNNN), which does not depend on copy order.


3. FIRMWARE
-----------
3.1 Build configuration (platformio.ini + rtc_setup) - DONE (2026-10-21)
  - All modules use generated environments (see README "Build targets" and docs/rtc-firmware.md 2).
    The audio section already exists in platformio.ini:
        [audio_module_base]
        build_flags    = -D AUDIO_MODULE_BASE
        lib_deps       =                      (vendored library in lib/, see 7.1)
        src_filter     = +<esp32/AudioModule.cpp> +<esp32/AudioPlayerBase.cpp>
        lib_ignore_off = DFRobotDFPlayerMini  (also in the global lib_ignore of the plain environments)
  - The ESP32 source filter excludes the audio sources by default ([filters] esp32: -<esp32/Audio*.cpp>).
  - rtc_setup writes the environment of the selection (e.g. esp32_bright_temp_audio_base) into
    rtc_build.ini in the compile copy; platformio.ini loads it through extra_configs.
  - Building without rtc_setup: an environment like that in rtc_build_local.ini (see README).

3.2 Globals.h
  - (as built) `#ifdef`/`#else` pairs set HAS_AUDIO_MODULE_BASE / HAS_AUDIO_MODULE_EXTENDED (the only
    preprocessor use: a build flag that may be missing can only be read by the preprocessor);
    HAS_AUDIO_MODULE = their OR; the two `static_assert`s of section 1; DEFAULT_AUDIO_VOLUME = 5 and
    `audio_volume`.
  - (as built) AudioModule.h and AudioPlayer.h are in include/ and Globals.h includes AudioModule.h in
    every build: they hold declarations only and need no library, so no declared-only stubs are needed.
    `struct AudioReply { const char* text; bool ok; }` is in AudioModule.h.
  - New SettingId::AudioVolume + SETTING_TAGS entry `<audioVolume>` (value 0-10, default 5).
    Parsed in getInitialClockSettings() (only applied when HAS_AUDIO_MODULE).
  - data/espSettings.xml gets `<audioVolume>5</audioVolume>` (all builds; ignored without the module).

3.3 Data kept on the ESP32 (LittleFS)
  - /notifications.csv: `id|track|HH:MM|weekdays` lines; track = 4-digit track number (the extended
                        version stores the file name in this field); weekdays = 7-bit mask
                        (bit0 = Monday ... bit6 = Sunday); id 1-255, unique; max 30 lines.
                        The common part treats the track field as text (max 64 bytes), so the
                        extended version can reuse the parser.
  - /audio.csv        : required, read only. `NNNN|name` lines, one per file on the SD card
                        (NNNN = the 4-digit number of /mp3/NNNN.mp3), display name UTF-8, max 32 bytes,
                        no `|` or newline. Prepared on a PC as data/audio.csv and uploaded with the file
                        system. The names are fixed: the web UI cannot change them. The web UI shows
                        the name of the line whose NNNN matches the SD file (decision 7.5, Q6).
                        The track count is the number of valid lines, at most MAX_AUDIO_TRACKS = 30
                        (a constant in AudioModule.h, also the size of the RAM array); a track
                        exists when it has a line (decision Q5). Lines with a bad number, a
                        duplicate number or an empty name are skipped (debug message).
                        Missing or empty file: the audio tab shows "Липсва файл audio.csv" and no files.
  - Both are read once into fixed RAM arrays at boot (30 entries each, MAX_AUDIO_TRACKS for
    audio.csv); /notifications.csv is rewritten whole on change.
  - (as built) audio.csv accepts a UTF-8 BOM and CRLF line ends, trims spaces around the name, skips
    `0000`, and cuts a name longer than 32 bytes at a UTF-8 character boundary. A sample
    data/audio.csv is in the repository; rtc_setup copies it only into audio builds.
    /notifications.csv lines with a bad field or a repeated id are skipped at boot; a notification
    whose track is not listed is kept (the web UI greys it) but does not play.
  - A file system upload (uploadfs) replaces both files, as it erases the saved network today.

3.4 Functions (alphabetical definitions in each file)
  (as built) The request functions take the request texts as they arrive and validate them
  themselves; they return an AudioReply (message + ok), which ServerUtils sends as HTTP 200 or 400.
  AudioModule.cpp (common):
  - deleteNotifications(ids, tracks)   (as built: bulk, "1,4,7" and "0002,0003,0003")
                             every id exists and its stored track matches the track at the same
                             position -> remove all, rewrite file -> confirmation text. Any mismatch,
                             lists of different length or a bad id -> error text, nothing removed.
  - editNotificationsFile()  writes the RAM notification array to /notifications.csv (single writer).
  - findTrack(track)         index of the listed track whose track field is `track`, or -1.
  - initializeAudio()        loads /notifications.csv, applies the stored volume setting, calls
                             beginAudioPlayer().
  - parseNotificationLine(), parseNumber(), parseTime(), readFileLine()
                             (as built) shared parsing helpers, declared in AudioModule.h so the host
                             tests can call them.
  - playTrack(track)         track listed in /audio.csv -> playAudioTrack() -> confirmation text.
  - readAudioData(buffer, size) builds the data response (see 3.5): volume, playing track, track count,
                             track names, notifications. (as built) ServerUtils keeps a static buffer
                             of AUDIO_DATA_SIZE (the worst case, about 5.5 KB; a full base list is
                             about 3.6 KB) and sends it with sendWebpageResponse(), which now uses
                             send_P(): no heap copy of the text.
  - saveNotification(id, track, time, weekdays, edit)
                             time valid (00:00-23:59), weekdays mask non-zero, track exists,
                             no other notification at the same HH:MM on a shared weekday
                             (masks AND-ed non-zero; 07:00 Пн-Пт and 07:00 Сб-Нд are both allowed, Q1),
                             limit 30 (create only), on edit replace the entry with the same id,
                             rewrite file -> confirmation text. A new notification gets the smallest
                             free id (1-255).
  - serviceAudio()           called from loop() every pass; calls serviceAudioPlayer() (cheap: returns
                             at once when nothing is pending).
  - serviceNotifications()   called once per RTC second from loop(); compares HH:MM and weekday with
                             the table and plays the match in every clock mode, timer mode included
                             (Q3). It replaces any track that is playing; the interrupted track is
                             stopped for good, and the next Play press starts it from the beginning
                             (Q2). Non-blocking.
                             (as built) Each minute is evaluated once, on its first pass, not only at
                             second 0: a loop() blocked over hh:mm:00 (e.g. the 03:00 time sync) still
                             fires. A minute up to 60 minutes before or equal to the last fired one
                             (DST change back, time sync back) does not fire again; a larger step back
                             (a corrected date) does.
  - setVolume(level)         0..10 -> setAudioPlayerVolume(), store via editSettingsFile.
  - stopTrack()              stopAudioTrack() -> confirmation text. (as built) The stop command is sent
                             even when the player reports nothing playing (BUSY may lag); the text
                             says whether something was playing.

  AudioPlayerBase.cpp (base player, implements AudioPlayer.h):
  - readPlayingTrack()       (as built, replaces audioPlayerIsPlaying()) index of the playing track or
                             -1: a queued play counts as playing, a queued stop as not; otherwise BUSY
                             pin LOW, or a play command was sent less than 1 s ago (BUSY can lag the
                             command on clone chips). No serial round trip.
  - beginAudioPlayer()       Serial1 start, player.begin(Serial1), BUSY pin input, loads /audio.csv.
                             Sends nothing yet.
  - playAudioTrack(index)    queues command 0x12 (play /mp3/NNNN).
  - readTrackCount()         number of valid /audio.csv lines (max MAX_AUDIO_TRACKS), counted when the
                             file is loaded. No query to the module.
  - readTrackKey(index), readTrackName(index)
                             "NNNN" and the name from /audio.csv.
  - serviceAudioPlayer()     non-blocking:
                               - (as built) one queued volume and one queued play/stop; a newer
                                 request replaces the queued one of its kind. The volume goes first,
                                 so a track never starts at the player's default volume.
                               - not before 2 s after boot (the stored volume is queued at boot).
                               - at least 100 ms between two commands.
                               - never reads replies: the module's TX line is not used by the
                                 firmware (it stays wired for the hardware test sketch).
  - setAudioPlayerVolume(level)
                             0..10 -> DFPlayer 0..30 (x3), player.volume() (command 0x06).
  - stopAudioTrack()         player.stop() (command 0x16). Play: player.playMp3Folder() (command 0x12).
                             (as built) The queued play/stop is an `enum class PendingCommand
                             { None, Play, Stop }`; the protocol bytes live in the library only.

  All texts returned to the web UI are Bulgarian constants or snprintf into one static buffer
  (same pattern as ServerUtils' response_buf).

3.5 HTTP API (ServerUtils.cpp, routes registered only when HAS_AUDIO_MODULE)
  GET  /audio                        -> text/plain, one record per line:
                                        V|volume
                                        P|playing track (0 = none)
                                        C|track count
                                        T|track|name             (one per audio.csv line, file order)
                                        N|id|track|HH:MM|weekdays (one per notification)
  POST /audio-play       track=0003   (as built: the track field, as in the T lines)
  POST /audio-stop
  POST /audio-volume     volume=0..10
  POST /audio-notification         id=0|N&track=0003&time=HH:MM&weekdays=mask&edit=0|1
  POST /audio-notification-delete  ids=1,4,7&tracks=0002,0003,0003   (bulk delete; one id = single delete)
  - All answers HTTP 200 with a Bulgarian message on success; validation failures answer HTTP 400 so
    the web UI's confirm-then-apply logic keeps the controls unchanged. sendServerRequest() is changed
    to show the clock's own message for HTTP 400 too (today it shows a generic "Възникна грешка!").
  - Handlers refresh last_http_activity_ms like the other handlers.
  - (as built) The routes are lambdas inside `if constexpr (HAS_AUDIO_MODULE)` in initializeServers();
    sendAudioReply() turns an AudioReply into the HTTP 200/400 answer.

3.6 loop() / setup()
  - (as built) initializePeripherals() calls initializeAudio(). setup() now calls initializeFileSystem()
    before initializePeripherals(), because the audio module needs the settings and files (this also
    makes the GPS activation in initializePeripherals() effective; the repeat in setup() is removed).
  - loop(): serviceAudio() every pass; serviceNotifications() inside the existing once-per-second block.


4. WEB UI
---------
4.1 New files (data/)
  - audioAddonTabBtn.html : tab button "Аудио"; replaces <!-- PLACEHOLDER FOR audioAddonTabBtn.html -->
                            placed after #js-timer-menu-button in .option-buttons-container.
  - audioAddonTab.html    : the audio panel; replaces <!-- PLACEHOLDER FOR audioAddonTab.html -->
                            placed after the TIMER PANEL in .panels-wrapper.
  - audioAddonPopup.html  : (as built) the notification popup; replaces <!-- PLACEHOLDER FOR
                            audioAddonPopup.html --> after the last popup of body.html. Inside the panel
                            the popup would be clipped by .panels-wrapper (overflow hidden).
  - audioAddon.js         : appended to mainScript.js by rtc_setup.
  - audioAddon.css        : appended to mainStyle.css by rtc_setup.
  - All placeholders have empty default content (no audio markup in non-audio builds).
  - (as built) rtc_setup skips every `*Addon*.*` file when copying data/ (before, only `*Addon.*`, so
    audioAddonTab.html would have been copied to the file system).
  - The extended version adds its own addon files on top of these (see audio-module-extended.spec).

4.2 Tab button
  - Same look and behaviour as the existing tab buttons. Opens the audio panel.
  - The first opening requests GET /audio once; the loader is shown until the tables are filled;
    on failure the disconnected indicator and an error message are shown (same pattern as the side panels).
  - Data is kept in JS (tracks, notifications) and re-rendered locally after each confirmed change.
    (as built) Later openings and confirmed notification changes refresh GET /audio quietly (no
    loader), which also brings the id of a new notification and the playing state.
  - (as built) A delegated listener on .option-buttons-container calls TAB_OPEN_HANDLERS[name] after
    switching; the audio addon registers TAB_OPEN_HANDLERS.audio. Play, stop and volume show a popup
    only when they fail; notification save and delete use sendServerRequest() (loader + message).
  - Tab switching (decision 7.2): switchTab() works over all tab buttons and panels through data-tab
    attributes (today it knows only "rtc" and "timer"), so the audio addon only adds markup and does
    not override switchTab().

4.3 Volume control
  - "−" button, value (0-10), "+" button. Each change is sent to /audio-volume; the shown value changes
    only after the clock confirms (same rule as the other controls).

4.4 Tabbed table (100% of the form width, centred like the networks list)
  Header
    - Files view:         "Име на файл" sortable label with up/down arrows (sorts by name).
    - Notifications view: "Избери всички" checkbox, "Изтрий избраните" button (inactive until a row
                          is selected), "Файл" label (left), "Час" label (right), "Ново" button at the end.
                          (as built) Two lines, so it fits 360 px: checkbox, "Изтрий избраните" and
                          "Ново" on the first, the column labels "Файл" / "Час" on the second.
  Rows (zebra striping with the existing colour scheme)
    - Files:         [name, left, 70%, horizontal auto-scroll when too long] [Play button: rounded
                     triangle; turns into Stop (rounded square) while that track plays]
    - Notifications: [checkbox] [name, left, 40%, auto-scroll] [time HH:MM + weekday letters,
                     e.g. "07:30  Пн-Пт" (decision 7.4), left, 40%] [Edit button: paper with pen]
                     [Delete button: bin]. "Изтрий избраните" sends one bulk request (decision 7.3).
                     A notification whose track is missing on the card is shown greyed with a warning.
  Footer
    - Centre: two tab buttons - "Файлове" (left: table slides right, files view) and
      "Известия" (right: table slides left, notifications view).
    - Right: pagination - first, previous (SVG icons), "Страница # / #max", next, last.
      10 rows per page (one constant, ROWS_PER_PAGE = 10).
    - (as built) The view buttons take a centred line of their own and the pagination sits right-aligned
      below them: the form is only 448 px wide on a 1280 px desktop, too narrow for both in one line.
      Files are sorted by name (ascending first), notifications by time.
  Events
    - One delegated click listener on the table body, one on the header, one on the footer;
      rows carry data-* attributes (track / id). No per-row listeners.

4.5 Notification popup (create and edit)
  - Audio file list: scrollable list of the names; the selected item is marked in red
    (existing --red); creating without a selected file shows an error.
  - Time: HH:MM input with scrollable hours (00-23) and minutes (00-59), styled like the timer picker.
  - Weekdays: seven toggles Пн Вт Ср Чт Пт Сб Нд; at least one must be selected.
  - "Запази" / "Отказ". Edit opens the popup prefilled.
  - Client-side checks (limit 30, same time on a shared weekday, missing file, no weekday) before the
    request; the clock repeats the checks.

4.6 Play / Stop
  - Play sends /audio-play; on success that row's button becomes Stop and the others Play.
  - When playback ends on its own, the button returns to Play on the next data refresh
    (opening the tab or any confirmed action); no polling while idle.


5. rtc_setup - DONE (2026-10-21, file system part verified 2026-10-25)
------------
  - "Audio Module" checkbox below the module list, unchecked by default. Ticking it selects the
    ESP32, disables the ESP8266 choice and shows the "Base | Extended" toggle below the checkbox,
    set to "Base" and disabled until the extended version is implemented.
  - With the checkbox ticked, the generated environment adds the audio_module_base section and its
    name ends with "_audio_base"; its "audio" token makes the file system build inline the audio
    addon files of 4.1 (MODULE_TOKENS includes "audio").
  - Verified with the implementation: prepare_filesystem() with and without audio puts the audio markup,
    JS, CSS and audio.csv only into the audio build, leaves no placeholder comment, copies no addon
    file, and the minified mainScript.js passes node --check.


6. VERIFICATION PLAN
--------------------
  - Build all 8 ESP32 module combinations with and without the audio module, with the strict flags;
    ESP8266 builds must not change.
  - Host tests of the CSV parsing/writing, notification validation and the minute-matching logic
    (including DST jump and duplicate-minute protection).
  - jsdom tests of the audio tab: data rendering, sorting, pagination, popup validation, confirm-then-apply
    on success / HTTP 400 / no connection, single delegated listeners.
  - Before the implementation (by you, 10 minutes, recommended): run the DFPlayerAnalyzer sketch or a
    short test sketch I provide on the ESP32 with your module, to confirm on the JL chip that
    0x12 plays /mp3/0001.mp3, 0x16 stops, 0x06 changes the volume, and how long BUSY takes to go
    LOW. The results fix the open values in 2.0 d).
  - On hardware (by you): play/stop, volume, a notification firing while another track plays, BUSY pin.


7. DECISIONS (from the review answers)
-------------------------------------
7.1 Player driver (changed 2026-10-25): DFRobot's DFRobotDFPlayerMini V1.0.6 (LGPL), vendored in
    lib/ and trimmed to begin/volume/playMp3Folder/stop, MODIFIED to send without ACK requests and
    without its delay(10); receiving and the blocking reset are removed (lib/DFRobotDFPlayerMini/README.md).
    Its frames are byte-identical to the former own driver's (10,031 frames compared). The start-up
    wait, the command spacing, the queue and BUSY stay in AudioPlayerBase.cpp, because no library does
    them without blocking. Not used: enjoyneering/DFPlayer (GPL-3.0), DFPlayerMini_Fast (no licence),
    Makuna DFMiniMp3 (blocks up to 2.7 s per command). See "Audio player library research.md" (chat).
7.2 Generic tab switching through data-tab attributes (4.2).
7.3 Bulk delete of notifications in one request (3.5, 4.4).
7.4 Weekday letters in the time column (4.4).
7.5 Track names from the required data/audio.csv, fixed, no rename (3.3). Q6: no optional file.
Q1. The same HH:MM is forbidden only for notifications that share a weekday (3.4).
Q2. A notification stops the playing track for good; the next Play press restarts it (3.4).
Q3. Notifications play in every clock mode (3.4).
Q4. Default volume 5 of 10 (3.2).
N1. Player file names follow the versions: AudioPlayerBase.cpp and AudioPlayerExtended.cpp.
Q5. The track count is the number of valid lines in /audio.csv; MAX_AUDIO_TRACKS = 30 is only the
    upper limit. No 0x48 query, so the driver only sends commands (3.3, 3.4). The extended version
    counts the files it lists in /audio on its own SD card (audio-module-extended.spec 3.3, 3.4).
