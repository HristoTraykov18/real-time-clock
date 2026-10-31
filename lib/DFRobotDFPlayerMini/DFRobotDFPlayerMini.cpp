/*!
 * @file DFRobotDFPlayerMini.cpp
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
 * MODIFIED for the Real Time Clock project (2026-10-25), see README.md.
 */

#include "DFRobotDFPlayerMini.h"

void DFRobotDFPlayerMini::uint16ToArray(uint16_t value, uint8_t *array){
  *array = (uint8_t)(value>>8);
  *(array+1) = (uint8_t)(value);
}

uint16_t DFRobotDFPlayerMini::calculateCheckSum(uint8_t *buffer){
  uint16_t sum = 0;
  for (int i=Stack_Version; i<Stack_CheckSum; i++) {
    sum += buffer[i];
  }
  return -sum;
}

void DFRobotDFPlayerMini::sendStack(){
  _serial->write(_sending, DFPLAYER_SEND_LENGTH); // MODIFIED: no ACK wait and no delay(10) afterwards
}

void DFRobotDFPlayerMini::sendStack(uint8_t command){
  sendStack(command, 0);
}

void DFRobotDFPlayerMini::sendStack(uint8_t command, uint16_t argument){
  _sending[Stack_Command] = command;
  uint16ToArray(argument, _sending+Stack_Parameter);
  uint16ToArray(calculateCheckSum(_sending), _sending+Stack_CheckSum);
  sendStack();
}

void DFRobotDFPlayerMini::disableACK(){
  _sending[Stack_ACK] = 0x00;
}

bool DFRobotDFPlayerMini::begin(Stream &stream){ // MODIFIED: no isACK and doReset parameters, sends nothing
  _serial = &stream;
  disableACK();

  return true;
}

void DFRobotDFPlayerMini::volume(uint8_t volume){
  sendStack(0x06, volume);
}

void DFRobotDFPlayerMini::stop(){
  sendStack(0x16);
}

void DFRobotDFPlayerMini::playMp3Folder(int fileNumber){
  sendStack(0x12, fileNumber);
}
