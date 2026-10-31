/**
 * @file AudioModule.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Audio module, common part of the base and the extended version: notifications, volume setting,
 * data response and validation of the web UI requests. See docs/audio-module-base.spec.
 * Included by Globals.h in every build (declarations only); the definitions exist in audio builds only.
 * @version 1.0
 * @date 2026-10-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include "Globals.h"
#include "AudioPlayer.h"

/**
 * @brief Outcome of an audio module request from the web UI.
 * text - Bulgarian message for the web UI.
 * ok   - true: the request was applied (HTTP 200). false: it was rejected and nothing changed (HTTP 400).
 */
struct AudioReply {
  const char* text;
  bool ok;
};

constexpr uint8_t MAX_AUDIO_TRACKS = 30;          // Upper limit of the track list (and size of its RAM array)
constexpr uint8_t MAX_NOTIFICATIONS = 30;
constexpr uint8_t MAX_AUDIO_VOLUME = 10;
constexpr size_t  AUDIO_TRACK_KEY_SIZE = 65;      // Track field of a notification: 64 bytes + NUL (file name in the extended version)
constexpr size_t  AUDIO_TRACK_NAME_SIZE = 33;     // Display name: 32 bytes of UTF-8 + NUL
// GET /audio response, worst case: V, P (longest track field), C, then every T and N line at its longest
constexpr size_t AUDIO_DATA_SIZE = (sizeof("V|10\n") - 1) + (sizeof("P|\n") - 1 + AUDIO_TRACK_KEY_SIZE - 1) + (sizeof("C|30\n") - 1)
                                 + MAX_AUDIO_TRACKS * (sizeof("T||\n") - 1 + AUDIO_TRACK_KEY_SIZE - 1 + AUDIO_TRACK_NAME_SIZE - 1)
                                 + MAX_NOTIFICATIONS * (sizeof("N|255||00:00|127\n") - 1 + AUDIO_TRACK_KEY_SIZE - 1)
                                 + 1; // NUL
constexpr const char* AUDIO_TRACKS_FILE = "/audio.csv";
constexpr const char* NOTIFICATIONS_FILE = "/notifications.csv";

/**
 * @brief One scheduled notification. weekdays is a 7-bit mask: bit0 = Monday ... bit6 = Sunday.
 */
struct AudioNotification {
  uint8_t id;
  uint8_t hour;
  uint8_t minute;
  uint8_t weekdays;
  char track[AUDIO_TRACK_KEY_SIZE];
};

/**
 * @brief Removes the listed notifications. Every id must exist and its stored track must match the track at
 * the same position, otherwise nothing is removed (the web UI showed outdated data).
 * @param ids Comma separated notification ids, e.g. "1,4,7".
 * @param tracks Comma separated track fields in the same order, e.g. "0002,0003,0003".
 * @return AudioReply Confirmation, or the reason of the rejection.
 */
AudioReply deleteNotifications(const char ids[], const char tracks[]);

/**
 * @brief Writes the notifications held in RAM to NOTIFICATIONS_FILE. The only function writing that file.
 */
void editNotificationsFile();

/**
 * @brief Finds a track of the player by its track field.
 * @param track Track field as stored in a notification ("0003" in the base version).
 * @return int8_t Index in the track list, or -1 when the track is not listed.
 */
int8_t findTrack(const char track[]);

/**
 * @brief Loads NOTIFICATIONS_FILE, applies the stored volume and starts the player.
 * Called once from setup(), after the settings are loaded.
 */
void initializeAudio();

/**
 * @brief Parses one NOTIFICATIONS_FILE line: `id|track|HH:MM|weekdays`.
 * @param line Line without the line break.
 * @param notification Filled when the line is valid.
 * @return true when every field is valid.
 */
bool parseNotificationLine(const char line[], AudioNotification& notification);

/**
 * @brief Parses a whole number written with digits only.
 * @param text Text to parse.
 * @param max_value Largest accepted value.
 * @param value Set to the number when the text is valid.
 * @return true for 1 to 5 digits with a value up to max_value.
 */
bool parseNumber(const char text[], uint16_t max_value, uint16_t& value);

/**
 * @brief Parses a time written as HH:MM (00:00 to 23:59).
 * @param text Time text, exactly 5 characters.
 * @param hour Set to the hour when the text is valid.
 * @param minute Set to the minute when the text is valid.
 * @return true when the time is valid.
 */
bool parseTime(const char text[], uint8_t& hour, uint8_t& minute);

/**
 * @brief Starts a listed track.
 * @param track Track field of the track ("0003" in the base version).
 * @return AudioReply Confirmation, or an error when the track is not listed.
 */
AudioReply playTrack(const char track[]);

/**
 * @brief Builds the GET /audio response, one record per line (see the spec, 3.5):
 * V|volume, P|playing track (0 = none), C|track count, T|track|name per track, N|id|track|HH:MM|weekdays per notification.
 * @param buffer Destination, NUL terminated. AUDIO_DATA_SIZE always fits the whole response.
 * @param size Size of the destination.
 */
void readAudioData(char buffer[], size_t size);

/**
 * @brief Reads one line of a text file into a buffer, without the line break ("\r\n" or "\n").
 * @param file Open file.
 * @param line Buffer for the line.
 * @param size Size of the buffer.
 * @return int16_t Length of the line, -1 at the end of the file, -2 for a line longer than the buffer (skipped).
 */
int16_t readFileLine(File& file, char line[], size_t size);

/**
 * @brief Creates or edits a notification after validating every field: time 00:00-23:59, at least one weekday,
 * the track is listed, no other notification at the same HH:MM on a shared weekday, at most MAX_NOTIFICATIONS.
 * @param id "0" for a new notification (the clock assigns the id), the id of the edited one otherwise.
 * @param track Track field ("0003" in the base version).
 * @param time HH:MM.
 * @param weekdays Weekday mask 1 to 127 (bit0 = Monday ... bit6 = Sunday).
 * @param edit "1" to edit the notification with this id, "0" to create one.
 * @return AudioReply Confirmation, or the reason of the rejection.
 */
AudioReply saveNotification(const char id[], const char track[], const char time[], const char weekdays[], const char edit[]);

/**
 * @brief Called from loop() on every pass. Returns at once when the player has nothing to do.
 */
void serviceAudio();

/**
 * @brief Called once per RTC second from loop(). On the first pass of every minute, plays the notification due
 * at this HH:MM and weekday, in every work mode. A minute that repeats within an hour (Daylight Saving Time
 * change, time synchronization) does not fire the same notification twice.
 */
void serviceNotifications();

/**
 * @brief Applies a new volume and stores it in the settings file.
 * @param level Volume "0" to "10".
 * @return AudioReply Confirmation, or an error for any other value.
 */
AudioReply setVolume(const char level[]);

/**
 * @brief Stops the playing track for good. The next play request starts a track from its beginning.
 * @return AudioReply Confirmation (also when nothing was playing).
 */
AudioReply stopTrack();
