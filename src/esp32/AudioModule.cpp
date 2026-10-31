// AudioModule.cpp
#include "AudioModule.h"
#include "ConfigUtils.h"

constexpr uint8_t MAX_NOTIFICATION_ID = 255;
constexpr uint8_t ALL_WEEKDAYS = 0x7F;
constexpr uint32_t REPEATED_MINUTES_WINDOW = 60; // A Daylight Saving Time change repeats up to one hour

static AudioNotification notifications[MAX_NOTIFICATIONS];
static uint8_t notification_count = 0;
static uint32_t last_checked_minute = 0; // Minute (local Unix time / 60) handled last by serviceNotifications()
static uint32_t last_fired_minute = 0;   // Minute of the last notification that played, 0 = none yet
static char reply_buf[160];              // Formatted replies, valid until the next one is built


/**
 * @brief Copies the text between two pointers into a buffer.
 * @param start First character of the field.
 * @param end Character after the field.
 * @param buffer Destination, NUL terminated.
 * @param size Size of the destination.
 * @return bool false for an empty field or one that does not fit.
 */
static bool copyField(const char* start, const char* end, char buffer[], size_t size) {
  const size_t length = (size_t)(end - start);

  if (length == 0 || length >= size)
    return false;

  memcpy(buffer, start, length);
  buffer[length] = '\0';

  return true;
}


/**
 * @brief Finds a notification by its id.
 * @param id Notification id.
 * @return int8_t Index in the notification array, or -1.
 */
static int8_t findNotification(uint16_t id) {
  for (uint8_t i = 0; i < notification_count; i++) {
    if (notifications[i].id == id)
      return (int8_t)i;
  }

  return -1;
}


/**
 * @brief Writes one notification as `id|track|HH:MM|weekdays` followed by a line break.
 * @param notification Notification to write.
 * @param prefix Text put before the record ("" for the file, "N|" for the data response).
 * @param buffer Destination.
 * @param size Size of the destination.
 */
static void formatNotification(const AudioNotification& notification, const char prefix[], char buffer[], size_t size) {
  snprintf(buffer, size, "%s%u|%s|%02u:%02u|%u\n", prefix,
           (unsigned)notification.id, notification.track,
           (unsigned)notification.hour, (unsigned)notification.minute, (unsigned)notification.weekdays);
}


/**
 * @brief Reports a rejected request.
 * @param text Reason of the rejection.
 * @return AudioReply Reply with ok = false.
 */
static AudioReply rejected(const char text[]) {
  return { text, false };
}


AudioReply deleteNotifications(const char ids[], const char tracks[]) {
  bool marked[MAX_NOTIFICATIONS] = {};
  uint8_t marked_count = 0;
  const char* id_start = ids;
  const char* track_start = tracks;

  while (true) {
    const char* id_end = strchr(id_start, ',');
    const char* track_end = strchr(track_start, ',');

    if (!id_end) id_end = id_start + strlen(id_start);
    if (!track_end) track_end = track_start + strlen(track_start);

    char id_text[6];
    uint16_t id;

    if (!copyField(id_start, id_end, id_text, sizeof(id_text)) || !parseNumber(id_text, MAX_NOTIFICATION_ID, id))
      return rejected("Невалидна заявка за изтриване!");

    const int8_t index = findNotification(id);
    const size_t track_length = (size_t)(track_end - track_start);

    // The stored track must match too: otherwise the web UI shows outdated data
    if (index < 0 || strlen(notifications[index].track) != track_length ||
        memcmp(notifications[index].track, track_start, track_length) != 0)
      return rejected("Някои от избраните известия не са намерени!\nМоля отворете раздела отново.");

    if (!marked[index]) {
      marked[index] = true;
      marked_count++;
    }

    if (*id_end == '\0' || *track_end == '\0') {
      if (*id_end != *track_end) // One list is longer than the other
        return rejected("Невалидна заявка за изтриване!");

      break;
    }

    id_start = id_end + 1;
    track_start = track_end + 1;
  }

  uint8_t kept = 0;

  for (uint8_t i = 0; i < notification_count; i++) {
    if (!marked[i])
      notifications[kept++] = notifications[i];
  }

  notification_count = kept;
  editNotificationsFile();

  return { marked_count == 1 ? "Известието е изтрито!" : "Известията са изтрити!", true };
}


void editNotificationsFile() {
  File f = LittleFS.open(NOTIFICATIONS_FILE, "w");

  if (!f) {
    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[editNotificationsFile] Could not open "));
      Serial.println(NOTIFICATIONS_FILE);
    }

    return;
  }

  char line[AUDIO_TRACK_KEY_SIZE + 24];

  for (uint8_t i = 0; i < notification_count; i++) {
    formatNotification(notifications[i], "", line, sizeof(line));
    f.write((const uint8_t*)line, strlen(line));
  }

  f.close();
}


int8_t findTrack(const char track[]) {
  const uint8_t count = readTrackCount(); // Audio player function

  for (uint8_t i = 0; i < count; i++) {
    if (strcmp(readTrackKey(i), track) == 0) // Audio player function
      return (int8_t)i;
  }

  return -1;
}


void initializeAudio() {
  if (LittleFS.exists(NOTIFICATIONS_FILE)) {
    File f = LittleFS.open(NOTIFICATIONS_FILE, "r");
    char line[AUDIO_TRACK_KEY_SIZE + 24];
    int16_t length;

    while (f && (length = readFileLine(f, line, sizeof(line))) != -1) {
      AudioNotification notification;

      if (length == 0)
        continue;

      if (notification_count < MAX_NOTIFICATIONS && length > 0 && parseNotificationLine(line, notification) &&
          findNotification(notification.id) < 0)
        notifications[notification_count++] = notification;
      else if constexpr (DEBUG_MESSAGES) {
        Serial.print(F("[initializeAudio] Skipped notification line: "));
        Serial.println(length > 0 ? line : "(too long)");
      }
    }

    if (f)
      f.close();
  }

  beginAudioPlayer(); // Audio player function
  setAudioPlayerVolume(audio_volume); // Audio player function | Sent once the player is ready

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("[initializeAudio] Tracks: "));
    Serial.print(readTrackCount());
    Serial.print(F(", notifications: "));
    Serial.println(notification_count);
  }
}


bool parseNotificationLine(const char line[], AudioNotification& notification) {
  const char* first = strchr(line, '|');
  const char* second = first ? strchr(first + 1, '|') : nullptr;
  const char* third = second ? strchr(second + 1, '|') : nullptr;

  if (!third || strchr(third + 1, '|'))
    return false;

  char id_text[6];
  char time_text[6];
  char weekdays_text[5];
  uint16_t id;
  uint16_t weekdays;

  if (!copyField(line, first, id_text, sizeof(id_text)) || !parseNumber(id_text, MAX_NOTIFICATION_ID, id) || id == 0 ||
      !copyField(first + 1, second, notification.track, sizeof(notification.track)) ||
      !copyField(second + 1, third, time_text, sizeof(time_text)) ||
      !parseTime(time_text, notification.hour, notification.minute) ||
      !copyField(third + 1, third + 1 + strlen(third + 1), weekdays_text, sizeof(weekdays_text)) ||
      !parseNumber(weekdays_text, ALL_WEEKDAYS, weekdays) || weekdays == 0)
    return false;

  notification.id = (uint8_t)id;
  notification.weekdays = (uint8_t)weekdays;

  return true;
}


bool parseNumber(const char text[], uint16_t max_value, uint16_t& value) {
  uint32_t result = 0;
  size_t length = 0;

  for (; text[length] != '\0'; length++) {
    if (text[length] < '0' || text[length] > '9' || length >= 5)
      return false;

    result = result * 10 + (uint32_t)(text[length] - '0');
  }

  if (length == 0 || result > max_value)
    return false;

  value = (uint16_t)result;

  return true;
}


bool parseTime(const char text[], uint8_t& hour, uint8_t& minute) {
  auto isDigit = [](char c) { return c >= '0' && c <= '9'; };

  if (strlen(text) != 5 || !isDigit(text[0]) || !isDigit(text[1]) || text[2] != ':' || !isDigit(text[3]) || !isDigit(text[4]))
    return false;

  const uint8_t h = (uint8_t)((text[0] - '0') * 10 + (text[1] - '0'));
  const uint8_t m = (uint8_t)((text[3] - '0') * 10 + (text[4] - '0'));

  if (h > 23 || m > 59)
    return false;

  hour = h;
  minute = m;

  return true;
}


AudioReply playTrack(const char track[]) {
  const int8_t index = findTrack(track);

  if (index < 0)
    return rejected("Файлът не е намерен!\nМоля отворете раздела отново.");

  playAudioTrack((uint8_t)index); // Audio player function
  snprintf(reply_buf, sizeof(reply_buf), "Възпроизвеждане: %s", readTrackName((uint8_t)index)); // Audio player function

  return { reply_buf, true };
}


void readAudioData(char buffer[], size_t size) {
  char line[AUDIO_TRACK_KEY_SIZE + AUDIO_TRACK_NAME_SIZE + 16];
  const int8_t playing = readPlayingTrack(); // Audio player function
  const uint8_t count = readTrackCount(); // Audio player function
  size_t used = 0;

  auto write_line = [&] (const char text[]) { // A line that does not fit is left out (AUDIO_DATA_SIZE always fits)
    const size_t length = strlen(text);

    if (used + length < size) {
      memcpy(buffer + used, text, length + 1);
      used += length;
    }
  };

  buffer[0] = '\0';

  snprintf(line, sizeof(line), "V|%u\n", (unsigned)audio_volume);
  write_line(line);
  snprintf(line, sizeof(line), "P|%s\n", playing >= 0 ? readTrackKey((uint8_t)playing) : "0");
  write_line(line);
  snprintf(line, sizeof(line), "C|%u\n", (unsigned)count);
  write_line(line);

  for (uint8_t i = 0; i < count; i++) {
    snprintf(line, sizeof(line), "T|%s|%s\n", readTrackKey(i), readTrackName(i)); // Audio player functions
    write_line(line);
  }

  for (uint8_t i = 0; i < notification_count; i++) {
    formatNotification(notifications[i], "N|", line, sizeof(line));
    write_line(line);
  }
}


int16_t readFileLine(File& file, char line[], size_t size) {
  if (!file.available())
    return -1;

  size_t length = 0;
  bool too_long = false;
  int c;

  while ((c = file.read()) >= 0 && c != '\n') {
    if (length + 1 < size)
      line[length++] = (char)c;
    else
      too_long = true;
  }

  if (length > 0 && line[length - 1] == '\r')
    length--;

  line[length] = '\0';

  return too_long ? -2 : (int16_t)length;
}


AudioReply saveNotification(const char id[], const char track[], const char time[], const char weekdays[], const char edit[]) {
  const bool is_edit = strcmp(edit, "1") == 0;
  uint16_t id_value;
  uint16_t weekdays_value;
  uint8_t hour;
  uint8_t minute;

  if ((!is_edit && strcmp(edit, "0") != 0) || !parseNumber(id, MAX_NOTIFICATION_ID, id_value) || is_edit != (id_value != 0))
    return rejected("Невалидно известие!");

  if (!parseTime(time, hour, minute))
    return rejected("Невалиден час!");

  if (!parseNumber(weekdays, ALL_WEEKDAYS, weekdays_value) || weekdays_value == 0)
    return rejected("Изберете поне един ден от седмицата!");

  if (strlen(track) >= AUDIO_TRACK_KEY_SIZE || findTrack(track) < 0)
    return rejected("Избраният файл липсва!");

  int8_t index = -1;

  if (is_edit) {
    index = findNotification(id_value);

    if (index < 0)
      return rejected("Известието не е намерено!\nМоля отворете раздела отново.");
  }
  else if (notification_count >= MAX_NOTIFICATIONS) {
    snprintf(reply_buf, sizeof(reply_buf), "Достигнат е максималният брой известия (%u)!", (unsigned)MAX_NOTIFICATIONS);

    return rejected(reply_buf);
  }

  for (uint8_t i = 0; i < notification_count; i++) {
    const AudioNotification& other = notifications[i];

    if ((int8_t)i != index && other.hour == hour && other.minute == minute && (other.weekdays & weekdays_value) != 0) {
      snprintf(reply_buf, sizeof(reply_buf), "Вече има известие в %02u:%02u за някой от избраните дни!",
               (unsigned)hour, (unsigned)minute);

      return rejected(reply_buf);
    }
  }

  if (!is_edit) {
    uint8_t new_id = 1;

    while (findNotification(new_id) >= 0) // At most MAX_NOTIFICATIONS ids are taken, so a free one is always found
      new_id++;

    index = (int8_t)notification_count++;
    notifications[index].id = new_id;
  }

  AudioNotification& notification = notifications[index];
  notification.hour = hour;
  notification.minute = minute;
  notification.weekdays = (uint8_t)weekdays_value;
  snprintf(notification.track, sizeof(notification.track), "%s", track);
  editNotificationsFile();

  return { is_edit ? "Известието е променено!" : "Известието е създадено!", true };
}


void serviceAudio() {
  serviceAudioPlayer(); // Audio player function
}


void serviceNotifications() {
  const DateTime now = rtc.now();
  const uint32_t minute_key = now.unixtime() / 60;

  if (minute_key == last_checked_minute)
    return;

  last_checked_minute = minute_key;

  // The RTC keeps local time, so a minute played before a clock change back has the same key again
  if (last_fired_minute != 0 && minute_key <= last_fired_minute && last_fired_minute - minute_key <= REPEATED_MINUTES_WINDOW)
    return;

  const uint8_t weekday_bit = (uint8_t)(1 << ((now.dayOfTheWeek() + 6) % 7)); // dayOfTheWeek(): 0 = Sunday

  for (uint8_t i = 0; i < notification_count; i++) {
    const AudioNotification& notification = notifications[i];

    if (notification.hour != now.hour() || notification.minute != now.minute() || (notification.weekdays & weekday_bit) == 0)
      continue;

    const int8_t index = findTrack(notification.track);

    if (index >= 0) {
      playAudioTrack((uint8_t)index); // Audio player function
      last_fired_minute = minute_key;
    }

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[serviceNotifications] Notification "));
      Serial.print(notification.id);
      Serial.println(index >= 0 ? F(" played") : F(" skipped, its track is not listed"));
    }

    return; // Saving rejects a second notification at the same time and weekday
  }
}


AudioReply setVolume(const char level[]) {
  uint16_t volume;

  if (!parseNumber(level, MAX_AUDIO_VOLUME, volume))
    return rejected("Невалидна сила на звука!");

  char value[6];
  snprintf(value, sizeof(value), "%u", (unsigned)volume);

  audio_volume = (uint8_t)volume;
  setAudioPlayerVolume(audio_volume); // Audio player function
  editSettingsFile(value, SettingId::AudioVolume); // Config Utils

  return { "Силата на звука е запазена!", true };
}


AudioReply stopTrack() {
  const bool was_playing = readPlayingTrack() >= 0; // Audio player function

  stopAudioTrack(); // Audio player function | Sent in any case, the BUSY pin may lag

  return { was_playing ? "Възпроизвеждането е спряно!" : "Няма файл, който се възпроизвежда.", true };
}
