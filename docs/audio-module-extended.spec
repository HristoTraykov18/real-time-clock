AUDIO MODULE EXTENDED - SPECIFICATION (v1, 2026-10-21)
======================================================

Status: draft for approval. Nothing is implemented yet; the base version comes first.
Version: EXTENDED. Everything of the base version plus full management of the audio files from the
         web UI: upload, delete, rename, real file names, file sizes, free space, duplicate check.
Platform: ESP32 only (esp32dev, WROOM-32). The module must not compile into ESP8266 builds.
Hardware: VS1053 decoder board with a micro SD slot + a small audio amplifier + speaker
          (option B of "Audio module hardware research"). The DFPlayer board is not used.

Relation to audio-module-base.spec ("BASE"): this spec only lists what differs. Every section of
BASE that is not mentioned here applies unchanged (conventions, notifications, volume setting,
loop/setup integration, web UI structure, verification approach). Where this spec says "replaces",
the BASE text does not apply to the extended version.


1. WAY OF WORKING
-----------------
  - Same as BASE 1, with the module switch `-D AUDIO_MODULE_EXTENDED` (HAS_AUDIO_MODULE = true).
  - Files:
      include/esp32/AudioModule.h, src/esp32/AudioModule.cpp
          Common part from BASE, reused (generalized where 3.3 says so).
      include/esp32/AudioPlayer.h, src/esp32/AudioPlayerVs1053.cpp
          Extended player: VS1053 driver, SD card, playback streaming. Implements the same
          AudioPlayer.h functions as BASE's AudioPlayerDfPlayer.cpp.
      include/esp32/AudioFiles.h, src/esp32/AudioFiles.cpp
          File management: list, upload, delete, rename, sizes, free space, duplicate check.
  - The SD card and the VS1053 share one SPI bus. Every access goes through SPI transactions, and
    SD file operations and playback never run at the same time (see 3.4).


2. HARDWARE (replaces BASE 2)
-----------------------------
2.1 Parts
  - VS1053 MP3 decoder board with micro SD slot (5 V input with on-board 3.3 V regulator, 3.3 V logic).
    Decodes MP3, WAV, AAC, Ogg Vorbis (and FLAC with a plugin). Many cheap boards start in MIDI mode
    and need a register write at start-up to switch to MP3 mode (see 3.4).
  - Amplifier: the VS1053 output is line/headphone level. One mono amplifier fed from one channel
    (e.g. PAM8302, 2.5 W) or a PAM8403 with one channel used. 5 V supply.
  - Speaker 4-8 Ohm, rated above the amplifier output (the speaker of the base build can be reused).
  - 100-470 uF capacitor across 5 V/GND at the amplifier.

2.2 Pinout (existing pins unchanged; the base version's GPIO16/17/34 are free again)

  Signal                         ESP32 pin   Peripheral pin
  ----------------------------   ---------   ---------------------------------
  (existing pins as in BASE 2.1: 2, 13, 14, 21, 22, 27, 32, 33, 35)
  SPI SCK           NEW          GPIO18      VS1053 board SCK  (shared by VS1053 and SD)
  SPI MISO          NEW          GPIO19      VS1053 board MISO (shared)
  SPI MOSI          NEW          GPIO23      VS1053 board MOSI (shared)
  VS1053 XCS        NEW          GPIO5       VS1053 command chip select (boot strapping pin; idle high, OK)
  VS1053 XDCS       NEW          GPIO16      VS1053 data chip select
  VS1053 DREQ       NEW          GPIO4       VS1053 data request (input)
  VS1053 XRESET     NEW          GPIO17      VS1053 reset
  SD CS             NEW          GPIO26      SD slot chip select on the VS1053 board
  Power             NEW          5V (VIN)    VS1053 board 5V, amplifier VCC
  Ground            NEW          GND         VS1053 board GND, amplifier GND
  Audio             NEW          -           VS1053 LOUT (or ROUT) + AGND -> amplifier input

  Notes:
  - VSPI default pins (18/19/23) are used through SPI.begin(18, 19, 23).
  - Constants in include/esp32/EspPlatform.h: AUDIO_SCK, AUDIO_MISO, AUDIO_MOSI, AUDIO_XCS, AUDIO_XDCS,
    AUDIO_DREQ, AUDIO_XRESET, AUDIO_SD_CS (inside the extended block, so the base build keeps its
    AUDIO_RX/TX/BUSY constants).
  - The VS1053 audio ground (AGND / GBUF) must not be tied to the main ground on some boards when a
    headphone output with GBUF is used; take the line output to the amplifier. To be checked on the
    board you buy.

2.3 Connection diagram

                         +-----------------------------+
                         |      ESP32 DevKit (WROOM)   |
   (existing peripherals | as in BASE 2.2)        5V   | ------+--------------> VS1053 board 5V
    unchanged)           |                       GND   | ----+-|--------------> VS1053 board GND
                         |                    GPIO18   | ----|-|--------------> SCK
                         |                    GPIO19   | <---|-|--------------- MISO
                         |                    GPIO23   | ----|-|--------------> MOSI
                         |                     GPIO5   | ----|-|--------------> XCS
                         |                    GPIO16   | ----|-|--------------> XDCS
                         |                     GPIO4   | <---|-|--------------- DREQ
                         |                    GPIO17   | ----|-|--------------> XRESET
                         |                    GPIO26   | ----|-|--------------> SD CS
                         +-----------------------------+     | |
                                                             | +--> amplifier VCC   === 100-470 uF
                                                             +----> amplifier GND
                          VS1053 LOUT ---> amplifier IN+ ; VS1053 AGND ---> amplifier IN-
                          amplifier OUT+ / OUT- ---> speaker (BTL: neither speaker wire to GND)

2.4 SD card (replaces BASE 2.3)
  - FAT32, up to 32 GB, inserted in the VS1053 board's slot.
  - Audio files live in /audio with their real names (UTF-8 long file names are enabled in the
    Arduino ESP32 libraries). No number prefix is needed.
  - Formats: MP3 and WAV (see Q2 for AAC/Ogg).
  - Up to 30 files are managed (same RAM arrays as BASE; see Q1).
  - "Default files": the files copied to the card before the clock is delivered. They are ordinary
    files (no special markers, as agreed for BASE); see Q3 for protection from deletion.


3. FIRMWARE
-----------
3.1 Build configuration (extends BASE 3.1)
  - The section exists in platformio.ini; lib_deps gets the VS1053 driver with the implementation (7.1):
        [audio_module_extended]
        build_flags    = -D AUDIO_MODULE_EXTENDED
        lib_deps       = (the VS1053 driver, see 7.1)
        src_filter     = +<esp32/AudioModule.cpp> +<esp32/AudioPlayerVs1053.cpp> +<esp32/AudioFiles.cpp>
        lib_ignore_off =
  - Generated environment: <platform>_<modules>_audio_extended, written by rtc_setup like BASE 3.1.
  - SD support comes from the ESP32 core (SD, FS, SPI libraries); no extra lib_deps.
  - Flash budget: about 205 KB are free in the largest ESP32 build today. SD + VS1053 driver +
    file management are estimated at 40-80 KB; to be measured in the first build.

3.2 Globals.h
  - As BASE 3.2. `<audioVolume>` is shared by both versions.

3.3 Data (replaces BASE 3.3)
  - Track identity is the file name (e.g. "Morning bell.mp3") instead of the track number.
  - /notifications.csv: `id|file name|HH:MM|weekdays` (same format as BASE; the track field holds the
    file name, max 64 bytes). The common parser already treats the field as text.
  - Renaming a file rewrites the notifications that use it; deleting a file removes them.
  - /audioTracks.csv is not used (real names are read from the card).
  - The file list (name, size) is read from /audio at boot and after every upload, delete or rename,
    into a fixed RAM array (30 entries).

3.4 Functions
  AudioPlayerVs1053.cpp (implements AudioPlayer.h; replaces BASE's player functions):
  - beginAudioPlayer()       SPI.begin, SD.begin(AUDIO_SD_CS), VS1053 hardware reset, clock setup,
                             switch from MIDI to MP3 mode when needed, stored volume.
  - audioPlayerIsPlaying()   a file is open and not finished.
  - playAudioTrack(name)     opens /audio/<name>, starts streaming (stops a playing file first).
  - readTrackCount(), readTrackName(index, buffer, size)
                             from the RAM file list.
  - serviceAudioPlayer()     feeds the VS1053: while DREQ is high, send 32-byte blocks from a buffer
                             that is refilled from the SD card. Non-blocking. See 7.2 for a dedicated
                             task, which avoids gaps while loop() is blocked (Wi-Fi connect, NTP).
  - setAudioPlayerVolume(level)
                             0..10 -> VS1053 SCI_VOL attenuation (10 = 0 dB, each step about 3 dB
                             less, 0 = mute). Exact table to be set by ear.
  - stopAudioTrack()         VS1053 cancel sequence, close the file.

  AudioFiles.cpp:
  - deleteAudioFiles(names)  each file exists -> SD.remove -> remove its notifications -> refresh list.
  - readFreeSpace(), readTotalSpace()
                             SD.totalBytes() / SD.usedBytes().
  - receiveUpload(...)       WebServer upload callback (see 3.5): checks before the first byte
                             (playback stopped, name valid and unique, extension .mp3/.wav, declared
                             size <= 10 MB and <= free space - 1 MB reserve, file count < 30), writes
                             chunks to /audio/.upload.tmp, checks the content type from the first
                             bytes (MP3: "ID3" or frame sync 0xFFEx; WAV: "RIFF....WAVE"), and at the
                             end runs the duplicate check before renaming the temp file to its name.
                             Any failure removes the temp file.
  - isDuplicate(temp)        another file with the same size whose SHA-256 (mbedTLS, hardware
                             accelerated) matches the upload. Only same-size files are hashed.
  - renameAudioFile(old, new)
                             name valid and unique, extension kept -> SD.rename -> rewrite the
                             notifications that use it -> refresh list.
  - validFileName(name)      UTF-8, 1-64 bytes, no / \ | : * ? " < > or control characters,
                             not starting with ".".

  While an upload, delete or rename runs, playback is stopped first (one SPI bus, one owner).

3.5 HTTP API (extends BASE 3.5)
  - Track parameters carry the file name (URL-encoded) instead of the number.
  GET  /audio                        -> as BASE, with:
                                        T|file name|size in bytes   (replaces T|track|name)
                                        S|free bytes|total bytes
  POST /audio-upload                 multipart/form-data, one file per request (10 MB max)
  POST /audio-delete                 names=<name1>/<name2>/...   (bulk; "/" cannot occur in names)
  POST /audio-rename                 name=<old>&new=<new>
  - Same rules as BASE: HTTP 400 with a Bulgarian message on validation failure.
  - A notification due while an upload runs is played right after the upload when it is less than
    60 s late (see Q4).


4. WEB UI (extends BASE 4)
--------------------------
4.1 Files
  - BASE addon files stay (audioAddonTabBtn.html, audioAddonTab.html, audioAddon.js, audioAddon.css).
  - New: audioExtendedAddon.html (extra markup inside the audio panel, through placeholders in
    audioAddonTab.html), audioExtendedAddon.js and audioExtendedAddon.css (appended after the BASE
    addon code; they extend or override BASE functions).
  - rtc_setup must resolve placeholders that appear inside inserted addon content (one more pass of
    the placeholder replacement; today a single pass is made).

4.2 Files view (replaces the BASE files view)
  Header
    - "Избери всички" checkbox, "Изтрий избраните" button (inactive until a row is selected),
      "Име на файл" sortable label, "Размер" label, "Качи" (upload) button at the end.
  Rows
    - [checkbox] [file name, left, auto-scroll; click -> inline rename of the real file, extension
      shown but not editable] [size, e.g. "3.4 MB"] [Play / Stop button]
  Footer
    - As BASE, plus "Свободно: X MB от Y MB" on the left.
  Delete
    - Confirmation popup listing the files and the number of notifications that will be removed.

4.3 Upload
  - "Качи" opens a file picker limited to .mp3 and .wav. Client-side checks before sending:
    extension, size <= 10 MB, size <= free space, name rules, no file with the same name.
  - Sent with XMLHttpRequest (fetch has no upload progress) to /audio-upload; a progress bar is shown;
    timeout 120 s. On success the list and free space are refreshed.
  - The upload button is disabled while a track plays (playback must be stopped first) and while
    another upload runs.

4.4 Notifications
  - As BASE; the file list in the popup shows real file names.


5. rtc_setup (extends BASE 5)
-----------------------------
  - The "Base | Extended" toggle is enabled (remove the DISABLED state set in set_app_widgets()).
    "Extended" adds the audio_module_extended section to the generated environment (3.1).
  - File system builds with "Extended" add the module names "audio" and "audioextended", so both the
    BASE addon files and audioExtendedAddon.* are inlined.


6. VERIFICATION PLAN (extends BASE 6)
-------------------------------------
  - Build all 8 ESP32 module combinations with the extended audio module; measure the flash use.
  - Host tests: file name validation, upload checks (size, free space, count, extension, magic
    bytes), duplicate detection, notification rewrite on rename/delete.
  - jsdom tests: upload flow with progress and errors, bulk delete, inline rename, free space display.
  - On hardware: playback of MP3 and WAV, MIDI-mode switch on the board you buy, upload of a 10 MB
    file (time and stability), playback stability while the web UI is used.


7. PROPOSALS
------------
7.1 VS1053 driver: ESP_VS1053_Library (baldram), pinned to a release tag in lib_deps through its
    GitHub URL (not in the PlatformIO registry), or a minimal own driver (reset, clock, mode switch,
    volume, data feed; about 150-200 lines). Proposal: the library, because the start-up sequence
    and the MIDI-mode fix are already handled there.
7.2 Playback in a FreeRTOS task (core 1, 4-8 KB ring buffer) instead of serviceAudioPlayer() in
    loop(), so blocking network calls do not cause gaps. Needs a mutex around SD access.
7.3 Keep the 30-file limit and the fixed RAM arrays of BASE for the first version.


8. OPEN QUESTIONS
-----------------
Q1. Is 30 files enough, or should the limit be higher (more RAM for the file list)?
Q2. Allow AAC and Ogg Vorbis as well (the VS1053 decodes them), or only MP3 and WAV like BASE?
Q3. Should the default files be protected from deletion, or behave like any other file?
Q4. A notification due during an upload: play it right after the upload (spec), or skip it?
Q5. "Total size check" of the original requirements: is the free space check (file <= free space
    - 1 MB) enough, or should the audio folder also have its own size limit?
Q6. Amplifier: single-channel PAM8302 or PAM8403 with one channel used?
