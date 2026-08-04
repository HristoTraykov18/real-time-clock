// Trimmed copy of DallasTemperature 4.0.6 for the Real Time Clock project.
// Licensed under the MIT License - see LICENSE.

#include "DallasTemperature.h"

#if ARDUINO >= 100
#include "Arduino.h"
#else
extern "C" {
#include "WConstants.h"
}
#endif

// OneWire commands
#define STARTCONVO      0x44  // Tells device to take a temperature reading
#define READSCRATCH     0xBE  // Read from scratchpad
#define READPOWERSUPPLY 0xB4  // Determine if device needs parasite power

// Scratchpad locations
#define TEMP_LSB        0
#define TEMP_MSB        1
#define HIGH_ALARM_TEMP 2
#define CONFIGURATION   4
#define COUNT_REMAIN    6
#define COUNT_PER_C     7
#define SCRATCHPAD_CRC  8

// Device resolution
#define TEMP_9_BIT  0x1F
#define TEMP_10_BIT 0x3F
#define TEMP_11_BIT 0x5F
#define TEMP_12_BIT 0x7F

// DSROM FIELDS
#define DSROM_FAMILY    0
#define DSROM_CRC       7

DallasTemperature::DallasTemperature() {
    _wire = nullptr;
    devices = 0;
    ds18Count = 0;
    parasite = false;
    bitResolution = 9;
    waitForConversion = true;
    checkForConversion = true;
}

DallasTemperature::DallasTemperature(OneWire* _oneWire) : DallasTemperature() {
    setOneWire(_oneWire);
}

void DallasTemperature::setOneWire(OneWire* _oneWire) {
    _wire = _oneWire;
    devices = 0;
    ds18Count = 0;
    parasite = false;
    bitResolution = 9;
    waitForConversion = true;
    checkForConversion = true;
}

// Initialise the bus
void DallasTemperature::begin(void) {
    DeviceAddress deviceAddress;

    for (uint8_t retry = 0; retry < MAX_INITIALIZATION_RETRIES; retry++) {
        _wire->reset_search();
        devices = 0;
        ds18Count = 0;

        delay(INITIALIZATION_DELAY_MS);

        while (_wire->search(deviceAddress)) {
            if (validAddress(deviceAddress)) {
                devices++;

                if (validFamily(deviceAddress)) {
                    ds18Count++;

                    if (!parasite && readPowerSupply(deviceAddress)) {
                        parasite = true;
                    }

                    uint8_t b = getResolution(deviceAddress);
                    if (b > bitResolution) {
                        bitResolution = b;
                    }
                }
            }
        }

        if (devices > 0) break;
    }
}

bool DallasTemperature::validAddress(const uint8_t* deviceAddress) {
    return (_wire->crc8(const_cast<uint8_t*>(deviceAddress), 7) == deviceAddress[DSROM_CRC]);
}

bool DallasTemperature::validFamily(const uint8_t* deviceAddress) {
    switch (deviceAddress[DSROM_FAMILY]) {
        case DS18S20MODEL:
        case DS18B20MODEL:
        case DS1822MODEL:
        case DS1825MODEL:
        case DS28EA00MODEL:
            return true;
        default:
            return false;
    }
}

// Finds an address at a given index on the bus.
// NOTE: requires begin() to have enumerated the bus first.
bool DallasTemperature::getAddress(uint8_t* deviceAddress, uint8_t index) {
    if (index < devices) {
        uint8_t depth = 0;

        _wire->reset_search();

        while (depth <= index && _wire->search(deviceAddress)) {
            if (depth == index && validAddress(deviceAddress)) {
                return true;
            }
            depth++;
        }
    }
    return false;
}

bool DallasTemperature::isConnected(const uint8_t* deviceAddress, uint8_t* scratchPad) {
    bool b = readScratchPad(deviceAddress, scratchPad);
    return b && !isAllZeros(scratchPad) && (_wire->crc8(scratchPad, 8) == scratchPad[SCRATCHPAD_CRC]);
}

bool DallasTemperature::readScratchPad(const uint8_t* deviceAddress, uint8_t* scratchPad) {
    int b = _wire->reset();
    if (b == 0) return false;

    _wire->select(deviceAddress);
    _wire->write(READSCRATCH);

    for (uint8_t i = 0; i < 9; i++) {
        scratchPad[i] = _wire->read();
    }

    b = _wire->reset();
    return (b == 1);
}

// Returns true if parasite mode is used (2 wire), false if normal mode (3 wire).
// If no address is given (or nullptr) it checks if any device on the bus uses parasite mode.
bool DallasTemperature::readPowerSupply(const uint8_t* deviceAddress) {
    bool parasiteMode = false;
    _wire->reset();
    if (deviceAddress == nullptr) {
        _wire->skip();
    } else {
        _wire->select(deviceAddress);
    }

    _wire->write(READPOWERSUPPLY);
    if (_wire->read_bit() == 0) {
        parasiteMode = true;
    }
    _wire->reset();
    return parasiteMode;
}

// Returns the current resolution of the device (9-12), or 0 if the device was not found
uint8_t DallasTemperature::getResolution(const uint8_t* deviceAddress) {
    if (deviceAddress[DSROM_FAMILY] == DS18S20MODEL) return 12;

    ScratchPad scratchPad;
    if (isConnected(deviceAddress, scratchPad)) {
        if (deviceAddress[DSROM_FAMILY] == DS1825MODEL && scratchPad[CONFIGURATION] & 0x80) {
            return 12;
        }

        switch (scratchPad[CONFIGURATION]) {
            case TEMP_12_BIT: return 12;
            case TEMP_11_BIT: return 11;
            case TEMP_10_BIT: return 10;
            case TEMP_9_BIT: return 9;
        }
    }
    return 0;
}

// Sends command for all devices on the bus to perform a temperature conversion
DallasTemperature::request_t DallasTemperature::requestTemperatures() {
    request_t req = {};
    req.result = true;

    _wire->reset();
    _wire->skip();
    _wire->write(STARTCONVO, parasite);

    req.timestamp = millis();
    if (!waitForConversion) return req;

    blockTillConversionComplete(bitResolution, req.timestamp);
    return req;
}

// Continue to check if the IC has responded with a temperature
void DallasTemperature::blockTillConversionComplete(uint8_t bitResolution, unsigned long start) {
    if (checkForConversion && !parasite) {
        while (!isConversionComplete() && ((unsigned long)(millis() - start) < (unsigned long)MAX_CONVERSION_TIMEOUT)) {
            yield();
        }
    } else {
        delay(millisToWaitForConversion(bitResolution));
    }
}

bool DallasTemperature::isConversionComplete() {
    uint8_t b = _wire->read_bit();
    return (b == 1);
}

// Returns number of milliseconds to wait till conversion is complete (based on IC datasheet)
uint16_t DallasTemperature::millisToWaitForConversion(uint8_t bitResolution) {
    switch (bitResolution) {
        case 9:  return 94;
        case 10: return 188;
        case 11: return 375;
        default: return 750;
    }
}

// Fetch temperature for device index
float DallasTemperature::getTempCByIndex(uint8_t index) {
    DeviceAddress deviceAddress;
    if (!getAddress(deviceAddress, index)) {
        return DEVICE_DISCONNECTED_C;
    }
    return getTempC((uint8_t*)deviceAddress);
}

// Returns temperature in degrees C or DEVICE_DISCONNECTED_C if the
// device's scratch pad cannot be read successfully
float DallasTemperature::getTempC(const uint8_t* deviceAddress, byte retryCount) {
    return rawToCelsius(getTemp(deviceAddress, retryCount));
}

// Returns temperature in 1/128 degrees C or DEVICE_DISCONNECTED_RAW if the
// device's scratch pad cannot be read successfully
int32_t DallasTemperature::getTemp(const uint8_t* deviceAddress, byte retryCount) {
    ScratchPad scratchPad;
    byte retries = 0;

    while (retries++ <= retryCount) {
        if (isConnected(deviceAddress, scratchPad)) {
            return calculateTemperature(deviceAddress, scratchPad);
        }
    }

    return DEVICE_DISCONNECTED_RAW;
}

// Reads scratchpad and returns fixed-point temperature, scaling factor 2^-7
int32_t DallasTemperature::calculateTemperature(const uint8_t* deviceAddress, uint8_t* scratchPad) {
    int32_t fpTemperature = 0;

    // looking thru the spec sheets of all supported devices, bit 15 is always the signing bit
    int32_t neg = 0x0;
    if (scratchPad[TEMP_MSB] & 0x80)
        neg = 0xFFF80000;

    // detect MAX31850
    if (deviceAddress[DSROM_FAMILY] == DS1825MODEL && scratchPad[CONFIGURATION] & 0x80) {
        if (scratchPad[TEMP_LSB] & 1) { // Fault Detected
            if (scratchPad[HIGH_ALARM_TEMP] & 1) {
                return DEVICE_FAULT_OPEN_RAW;
            } else if (scratchPad[HIGH_ALARM_TEMP] >> 1 & 1) {
                return DEVICE_FAULT_SHORTGND_RAW;
            } else if (scratchPad[HIGH_ALARM_TEMP] >> 2 & 1) {
                return DEVICE_FAULT_SHORTVDD_RAW;
            } else {
                return DEVICE_DISCONNECTED_RAW;
            }
        }
        // We must mask out bit 1 (reserved) and 0 (fault) on TEMP_LSB
        fpTemperature = (((int32_t)scratchPad[TEMP_MSB]) << 11)
                       | (((int32_t)scratchPad[TEMP_LSB] & 0xFC) << 3)
                       | neg;
    } else {
        fpTemperature = (((int16_t)scratchPad[TEMP_MSB]) << 11)
                       | (((int16_t)scratchPad[TEMP_LSB]) << 3)
                       | neg;
    }

    // detect POR and insufficient power conditions
    if (deviceAddress[DSROM_FAMILY] == DS18B20MODEL) {
        if (scratchPad[0] == 0x50 && scratchPad[1] == 0x05 && scratchPad[6] == 0x0C) {
            return DEVICE_POWER_ON_RESET_RAW;
        } else if (scratchPad[0] == 0xFF && scratchPad[1] == 0x07) {
            return DEVICE_INSUFFICIENT_POWER_RAW;
        }
    }

    /*
     DS1820 and DS18S20 have a 9-bit temperature register.

     Resolutions greater than 9-bit can be calculated using the data from
     the temperature, and COUNT REMAIN and COUNT PER °C registers in the
     scratchpad.  The resolution of the calculation depends on the model.

     While the COUNT PER °C register is hard-wired to 16 (10h) in a
     DS18S20, it changes with temperature in DS1820.

     After reading the scratchpad, the TEMP_READ value is obtained by
     truncating the 0.5°C bit (bit 0) from the temperature data. The
     extended resolution temperature can then be calculated using the
     following equation:

                                      COUNT_PER_C - COUNT_REMAIN
     TEMPERATURE = TEMP_READ - 0.25 + --------------------------
                                             COUNT_PER_C

     Hagai Shatz simplified this to integer arithmetic for a 12 bits
     value for a DS18S20, and James Cameron added legacy DS1820 support.

     See - http://myarduinotoy.blogspot.co.uk/2013/02/12bit-result-from-ds18s20.html
     */

    if ((deviceAddress[DSROM_FAMILY] == DS18S20MODEL) && (scratchPad[COUNT_PER_C] != 0)) {
        fpTemperature = (((fpTemperature & 0xfff0) << 3) - 32
                        + (((scratchPad[COUNT_PER_C] - scratchPad[COUNT_REMAIN]) << 7)
                           / scratchPad[COUNT_PER_C])) | neg;
    }

    return fpTemperature;
}

// Convert from raw to Celsius
float DallasTemperature::rawToCelsius(int32_t raw) {
    if (raw <= DEVICE_DISCONNECTED_RAW)
        return DEVICE_DISCONNECTED_C;
    return (float)raw * 0.0078125f;  // 1/128
}

// Returns true if all bytes of scratchPad are '\0'
bool DallasTemperature::isAllZeros(const uint8_t* const scratchPad, const size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (scratchPad[i] != 0) return false;
    }
    return true;
}
