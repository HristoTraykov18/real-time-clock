
//  Author: avishorp@gmail.com
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

extern "C" {
  #include <stdlib.h>
  #include <string.h>
  #include <inttypes.h>
}

#include <TM1637Display.h>
#include <Arduino.h>

#define TM1637_I2C_COMM1    0x40
#define TM1637_I2C_COMM2    0xC0
#define TM1637_I2C_COMM3    0x80

//
//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---
//      D
uint8_t digit_to_segment[] = {
 // XGFEDCBA
  0b00111111,    // 0
  0b00000110,    // 1
  0b01011011,    // 2
  0b01001111,    // 3
  0b01100110,    // 4
  0b01101101,    // 5
  0b01111101,    // 6
  0b00000111,    // 7
  0b01111111,    // 8
  0b01101111,    // 9
  0b01110111,    // A
  0b01111100,    // b
  0b00111001,    // C
  0b01011110,    // d
  0b01111001,    // E
  0b01110001     // F
};

const uint8_t digitToSegmentReversed[] = {
 // XGFEDCBA
  0b01000000,    // Reversed 0
  0b01111001,    // Reversed 1
  0b00100100,    // Reversed 2
  0b00110000,    // Reversed 3
  0b00011001,    // Reversed 4
  0b00010010,    // Reversed 5
  0b00000010,    // Reversed 6
  0b01111000,    // Reversed 7
  0b00000000,    // Nothing
  0b00010000,    // Reversed 9/Segment E
  0b00001000,    // Segment D
  0b00000011,    // Segments A & B
  0b01000110,    // Segments B, C & G
  0b00100001,    // Segments A & F
  0b00000110,    // 1
  0b00001110     // Segments B, C & D
};

TM1637Display::TM1637Display(uint8_t pin_CLK, uint8_t pin_DIO, unsigned int bitDelay, bool reverse_digit_logic)
{
  uint16_t digitToSegmentSize = sizeof(digit_to_segment);

  if (reverse_digit_logic) {
    for(uint8_t i = 0; i < digitToSegmentSize; i++) {
      digit_to_segment[i] = digitToSegmentReversed[i];
    }
  }

	m_pin_CLK = pin_CLK;
	m_pin_DIO = pin_DIO;
	m_bitDelay = bitDelay;
  m_reverse_digit_logic = reverse_digit_logic;

	// Set the pin direction and default value.
	// Both pins are set as inputs, allowing the pull-up resistors to pull them up
  pinMode(m_pin_CLK, INPUT);
  pinMode(m_pin_DIO, INPUT);
	digitalWrite(m_pin_CLK, LOW);
	digitalWrite(m_pin_DIO, LOW);
}

void TM1637Display::setBrightness(uint8_t brightness, bool on)
{
	m_brightness = (brightness & 0x7) | (on? 0x08 : 0x00);
}

void TM1637Display::setSegments(const uint8_t segments[], uint8_t length, uint8_t pos)
{
  // Write COMM1
	start();
	writeByte(TM1637_I2C_COMM1);
	stop();

	// Write COMM2 + first digit address
	start();
	writeByte(TM1637_I2C_COMM2 + (pos & 0x03));

	// Write the data bytes
	for (uint8_t k=0; k < length; k++)
	  writeByte(segments[k]);

	stop();

	// Write COMM3 + brightness
	start();
	writeByte(TM1637_I2C_COMM3 + (m_brightness & 0x0f));
	stop();
}

void TM1637Display::clear()
{
  uint8_t data[] = { 0, 0, 0, 0 };
	setSegments(data);
}

void TM1637Display::showNumber(uint16_t num, uint8_t dots, bool leading_zero, uint8_t length, uint8_t pos)
{
  uint8_t digits[4];
	const static int divisors[] = { 1, 10, 100, 1000 };

	for(int8_t k = 0; k < 4; k++) {
	  int divisor = divisors[4 - 1 - k];
		int d = num / divisor;
    uint8_t digit = 0;

		if (d == 0) {
		  if (!leading_zero && k > 0)
        digit = encodeDigit(d);
      else
        if (!m_reverse_digit_logic)
          digit = 0;
        else
          digit = 127; // Each bit is set to 1 and reversed - nothing is displayed
		}
		else {
			digit = encodeDigit(d);
			num -= d * divisor;
		}

    // Add the decimal point/colon to the digit
    digit |= (dots & 0x80);
    dots <<= 1;

    digits[k] = digit;
	}

	setSegments(digits + (4 - length), length, pos);
}

void TM1637Display::bitDelay()
{
	delayMicroseconds(m_bitDelay);
}

void TM1637Display::start()
{
  pinMode(m_pin_DIO, OUTPUT);
  bitDelay();
}

void TM1637Display::stop()
{
	pinMode(m_pin_DIO, OUTPUT);
	bitDelay();
	pinMode(m_pin_CLK, INPUT);
	bitDelay();
	pinMode(m_pin_DIO, INPUT);
	bitDelay();
}

bool TM1637Display::writeByte(uint8_t b)
{
  uint8_t data = b;

  // 8 Data Bits
  for(uint8_t i = 0; i < 8; i++) {
    // CLK low
    pinMode(m_pin_CLK, OUTPUT);
    bitDelay();

	  // Set data bit
    if (data & 0x01)
      pinMode(m_pin_DIO, INPUT);
    else
      pinMode(m_pin_DIO, OUTPUT);

    bitDelay();

	  // CLK high
    pinMode(m_pin_CLK, INPUT);
    bitDelay();
    data = data >> 1;
  }

  // Wait for acknowledge
  // CLK to zero
  pinMode(m_pin_CLK, OUTPUT);
  pinMode(m_pin_DIO, INPUT);
  bitDelay();

  // CLK to high
  pinMode(m_pin_CLK, INPUT);
  bitDelay();

  uint8_t ack = digitalRead(m_pin_DIO);

  if (ack == 0)
    pinMode(m_pin_DIO, OUTPUT);

  bitDelay();
  pinMode(m_pin_CLK, OUTPUT);
  bitDelay();

  return ack;
}

uint8_t TM1637Display::encodeDigit(uint8_t digit)
{
	return digit_to_segment[digit & 0x0f];
}
