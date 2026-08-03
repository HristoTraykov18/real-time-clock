TM1637
======
Changed version of the Arduino library for TM1637 (LED Driver)


Description
-----------
An Arduino library for 7-segment display modules based on the TM1637 chip

Hardware Connection
-------------------
The display modules has two signal connection (and two power connections) which are CLK and DIO. These pins can be connected to any pair of digital pins on the Arduino. When an object is created, the pins should be configured. There is no limitation on the number of instances used concurrently (as long as each instance has a pin pair of its own).

Installation
------------
The library is installed as any Arduino library, by copying the files into a directory on the library search path of the Arduino IDE

Usage
-----
The library provides a single class named TM1637Display. An instance of this class provides the following functions:

* `setSegments`   - Set the raw value of the segments of each digit
* `showNumber`    - Display a decimal number
* `setBrightness` - Sets the brightness of the display

The information given above is only a summary. Please refer to TM1637Display.h for more information.
