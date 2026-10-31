// AudioPlayerBase.cpp
#include "AudioModule.h"
#include <DFRobotDFPlayerMini.h> // Vendored and trimmed: sends without ACK requests, never reads, never blocks

/**
 * @brief Play or stop command waiting to be sent.
 */
enum class PendingCommand : uint8_t { None, Play, Stop };

constexpr uint32_t PLAYER_BAUD_RATE = 9600;
constexpr uint8_t PLAYER_VOLUME_STEP = 3;           // Volume 0 to 10 -> player volume 0 to 30
constexpr uint16_t MAX_TRACK_NUMBER = 9999;
constexpr unsigned long STARTUP_DELAY_MS = 2000;    // The player reads its SD card after power-on before it accepts commands
constexpr unsigned long COMMAND_SPACING_MS = 100;   // Commands sent closer together can be dropped by the player
constexpr unsigned long BUSY_GRACE_MS = 1000;       // BUSY goes LOW up to ~350 ms after a play command on clone chips

static char track_keys[MAX_AUDIO_TRACKS][5];        // "0003"
static char track_names[MAX_AUDIO_TRACKS][AUDIO_TRACK_NAME_SIZE];
static uint16_t track_numbers[MAX_AUDIO_TRACKS];
static uint8_t track_count = 0;

static DFRobotDFPlayerMini player;
static PendingCommand pending_command = PendingCommand::None;
static uint8_t pending_track = 0;                   // Track index of a pending play command
static int16_t pending_volume = -1;                 // Volume 0 to 10 waiting to be sent, -1 = none
static int8_t playing_track = -1;
static unsigned long last_command_ms = 0;
static unsigned long last_play_ms = 0;
static bool command_sent = false;

static bool parseTrackLine(char line[], uint16_t& number, char*& name);


/**
 * @brief Loads AUDIO_TRACKS_FILE into the track list. Lines with a bad number, a duplicate number or an empty
 * name are skipped, names longer than 32 bytes are shortened, lines after MAX_AUDIO_TRACKS are ignored.
 */
static void loadTrackList() {
  track_count = 0;

  if (!LittleFS.exists(AUDIO_TRACKS_FILE)) {
    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[loadTrackList] Missing "));
      Serial.println(AUDIO_TRACKS_FILE);
    }

    return;
  }

  File f = LittleFS.open(AUDIO_TRACKS_FILE, "r");
  char line[128];
  int16_t length;

  while (f && (length = readFileLine(f, line, sizeof(line))) != -1) { // Audio module function
    uint16_t number = 0;
    char* name = nullptr;

    if (length == 0)
      continue;

    if (track_count >= MAX_AUDIO_TRACKS) {
      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("[loadTrackList] Track limit reached, the remaining lines are ignored"));

      break;
    }

    bool duplicate = false;
    const bool valid = length > 0 && parseTrackLine(line, number, name);

    for (uint8_t i = 0; valid && i < track_count; i++)
      duplicate = duplicate || track_numbers[i] == number;

    if (!valid || duplicate) {
      if constexpr (DEBUG_MESSAGES) {
        Serial.print(F("[loadTrackList] Skipped line: "));
        Serial.println(length > 0 ? line : "(too long)");
      }

      continue;
    }

    track_numbers[track_count] = number;
    memcpy(track_keys[track_count], line, 4); // The four digits of the line
    track_keys[track_count][4] = '\0';
    strncpy(track_names[track_count], name, AUDIO_TRACK_NAME_SIZE - 1); // parseTrackLine() already shortened the name
    track_names[track_count][AUDIO_TRACK_NAME_SIZE - 1] = '\0';
    track_count++;
  }

  if (f)
    f.close();
}


/**
 * @brief Parses one AUDIO_TRACKS_FILE line: `NNNN|name`. Shortens a long name at a UTF-8 character boundary.
 * @param line Line without the line break, changed in place (BOM and spaces around the name removed).
 * @param number Set to the track number 1 to 9999.
 * @param name Set to the start of the name inside line.
 * @return bool true when the line is valid.
 */
static bool parseTrackLine(char line[], uint16_t& number, char*& name) {
  if ((uint8_t)line[0] == 0xEF && (uint8_t)line[1] == 0xBB && (uint8_t)line[2] == 0xBF) // UTF-8 BOM of Windows editors
    memmove(line, line + 3, strlen(line + 3) + 1);

  for (uint8_t i = 0; i < 4; i++) {
    if (line[i] < '0' || line[i] > '9')
      return false;
  }

  if (line[4] != '|')
    return false;

  number = (uint16_t)((line[0] - '0') * 1000 + (line[1] - '0') * 100 + (line[2] - '0') * 10 + (line[3] - '0'));
  name = line + 5;

  while (*name == ' ')
    name++;

  size_t length = strlen(name);

  while (length > 0 && name[length - 1] == ' ')
    name[--length] = '\0';

  if (number == 0 || number > MAX_TRACK_NUMBER || length == 0 || strchr(name, '|'))
    return false;

  if (length >= AUDIO_TRACK_NAME_SIZE) {
    size_t cut = AUDIO_TRACK_NAME_SIZE - 1;

    while (cut > 0 && ((uint8_t)name[cut] & 0xC0) == 0x80) // Do not split a multi-byte character
      cut--;

    name[cut] = '\0';
  }

  return true;
}


void beginAudioPlayer() {
  Serial1.begin(PLAYER_BAUD_RATE, SERIAL_8N1, AUDIO_RX, AUDIO_TX); // UART1 on explicit pins: its default pins belong to the flash
  player.begin(Serial1); // Sends nothing
  pinMode(AUDIO_BUSY, INPUT); // Input only pin without a pull-up. The player drives BUSY
  loadTrackList();
}


void playAudioTrack(uint8_t index) {
  if (index >= track_count)
    return;

  pending_command = PendingCommand::Play;
  pending_track = index;
}


int8_t readPlayingTrack() {
  if (pending_command == PendingCommand::Play)
    return (int8_t)pending_track;

  if (pending_command == PendingCommand::Stop || playing_track < 0)
    return -1;

  if (millis() - last_play_ms < BUSY_GRACE_MS || digitalRead(AUDIO_BUSY) == LOW) // BUSY is LOW while playing
    return playing_track;

  playing_track = -1; // Finished on its own

  return -1;
}


uint8_t readTrackCount() {
  return track_count;
}


const char* readTrackKey(uint8_t index) {
  return index < track_count ? track_keys[index] : "";
}


const char* readTrackName(uint8_t index) {
  return index < track_count ? track_names[index] : "";
}


void serviceAudioPlayer() {
  if (pending_command == PendingCommand::None && pending_volume < 0)
    return;

  const unsigned long now_ms = millis();

  if (now_ms < STARTUP_DELAY_MS || (command_sent && now_ms - last_command_ms < COMMAND_SPACING_MS))
    return;

  if (pending_volume >= 0) { // The volume goes first, so a track never starts at the player's default volume
    player.volume((uint8_t)(pending_volume * PLAYER_VOLUME_STEP));

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[serviceAudioPlayer] Volume "));
      Serial.println(pending_volume);
    }

    pending_volume = -1;
  }
  else if (pending_command == PendingCommand::Play) {
    player.playMp3Folder(track_numbers[pending_track]); // Plays /mp3/NNNN, independent of the copy order on the card
    playing_track = (int8_t)pending_track;
    last_play_ms = now_ms;
    pending_command = PendingCommand::None;

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[serviceAudioPlayer] Play track "));
      Serial.println(track_numbers[playing_track]);
    }
  }
  else {
    player.stop();
    pending_command = PendingCommand::None;

    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("[serviceAudioPlayer] Stop"));
  }

  last_command_ms = now_ms;
  command_sent = true;
}


void setAudioPlayerVolume(uint8_t level) {
  pending_volume = level > MAX_AUDIO_VOLUME ? MAX_AUDIO_VOLUME : level;
}


void stopAudioTrack() {
  pending_command = PendingCommand::Stop;
  playing_track = -1;
}
