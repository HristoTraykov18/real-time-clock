# DFRobotDFPlayerMini (trimmed, MODIFIED)

DFRobot's library for the DFPlayer Mini, version V1.0.6 (commit `47c0149`, 2023-06-26):
https://github.com/DFRobot/DFRobotDFPlayerMini - GNU Lesser General Public License.

Used by the audio module, base version (`src/esp32/AudioPlayerBase.cpp`) for the MP3-TF-16P board.
Only compiled into audio builds (`lib_ignore_off` of `[audio_module_base]` in `platformio.ini`).

Changes against the original:
- Kept: `begin()`, `volume()` (0 to 30), `playMp3Folder()` (plays `/mp3/NNNN`), `stop()` and the frame and checksum code.
- Removed: receiving and parsing of replies, the query functions, EQ, devices, folders, loops, advertisements, sleep and reset.
  The JL clone chips answer queries unreliably (ACK before the data, docs/audio-module-base.spec 2.0).
- `begin(Stream&)` has no `isACK` / `doReset` parameters: commands are always sent without an acknowledgement
  request, and nothing is sent at start (the original reset blocks for up to 2.2 s).
- `sendStack()` no longer waits for an ACK and no longer calls `delay(10)` after every command: the caller spaces the
  commands without blocking.

The frames are byte-identical to the original's for every command used (volume 0-30, play 1-9999, stop).
