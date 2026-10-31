/*!
 * @file DFRobotDFPlayerMini.h
 * @brief DFPlayer - An Arduino Mini MP3 Player From DFRobot
 * @n Header file for DFRobot's DFPlayer
 *
 * @copyright	[DFRobot]( http://www.dfrobot.com ), 2016
 * @copyright	GNU Lesser General Public License
 *
 * @author [Angelo](Angelo.qiao@dfrobot.com)
 * @version  V1.0.6
 * @date  2016-12-07
 *
 * MODIFIED for the Real Time Clock project (2026-10-25), see README.md: trimmed to setting the volume,
 * playing a track of the "mp3" folder and stopping. Commands are always sent without an acknowledgement
 * request and nothing is received; the reset and the 10 ms delay after every command are removed.
 */

#include "Arduino.h"

#ifndef DFRobotDFPlayerMini_cpp
    #define DFRobotDFPlayerMini_cpp

#define DFPLAYER_SEND_LENGTH 10

#define Stack_Header 0
#define Stack_Version 1
#define Stack_Length 2
#define Stack_Command 3
#define Stack_ACK 4
#define Stack_Parameter 5
#define Stack_CheckSum 7
#define Stack_End 9

class DFRobotDFPlayerMini {
  Stream* _serial;

  uint8_t _sending[DFPLAYER_SEND_LENGTH] = {0x7E, 0xFF, 06, 00, 01, 00, 00, 00, 00, 0xEF};

  void sendStack();
  void sendStack(uint8_t command);
  void sendStack(uint8_t command, uint16_t argument);

  void disableACK();

  void uint16ToArray(uint16_t value,uint8_t *array);

  uint16_t calculateCheckSum(uint8_t *buffer);

  public:

  bool begin(Stream& stream);

  void volume(uint8_t volume);

  void stop();

  void playMp3Folder(int fileNumber);
};

#endif
