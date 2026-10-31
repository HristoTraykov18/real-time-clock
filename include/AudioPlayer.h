/**
 * @file AudioPlayer.h
 * @author Hristo Traykov (hristotraykov98@gmail.com)
 * @brief Player interface of the audio module. AudioPlayerBase.cpp implements it for the MP3-TF-16P board
 * (base version), AudioPlayerExtended.cpp for the VS1053 board (extended version), so the common part in
 * AudioModule.cpp does not depend on the hardware. Tracks are addressed by their index in the track list.
 * @version 1.0
 * @date 2026-10-25
 * @copyright Copyright (c) 2026. Licensed under the MIT License.
 * See LICENSE file in the project root for full license details.
 */

#pragma once
#include <stdint.h>

/**
 * @brief Starts the player hardware and loads the track list. Sends nothing to the player yet.
 */
void beginAudioPlayer();

/**
 * @brief Starts a track. A playing track is replaced.
 * @param index Index in the track list (0 to readTrackCount() - 1).
 */
void playAudioTrack(uint8_t index);

/**
 * @brief Tells which track is playing.
 * @return int8_t Index of the playing track, or -1 when nothing plays.
 */
int8_t readPlayingTrack();

/**
 * @brief Number of tracks in the track list.
 * @return uint8_t 0 to MAX_AUDIO_TRACKS.
 */
uint8_t readTrackCount();

/**
 * @brief Track field of a track: the text a notification stores to refer to it ("0003" in the base version).
 * @param index Index in the track list.
 * @return const char* Text kept by the player, valid until the track list changes.
 */
const char* readTrackKey(uint8_t index);

/**
 * @brief Display name of a track.
 * @param index Index in the track list.
 * @return const char* UTF-8 text kept by the player, valid until the track list changes.
 */
const char* readTrackName(uint8_t index);

/**
 * @brief Does the pending player work without blocking. Called from loop() on every pass.
 */
void serviceAudioPlayer();

/**
 * @brief Applies a volume.
 * @param level 0 to MAX_AUDIO_VOLUME.
 */
void setAudioPlayerVolume(uint8_t level);

/**
 * @brief Stops the playing track.
 */
void stopAudioTrack();
