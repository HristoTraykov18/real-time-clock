#ifndef DallasTemperature_h
#define DallasTemperature_h

#define DALLASTEMPLIBVERSION "4.0.6"

// Trimmed copy of DallasTemperature 4.0.6 for the Real Time Clock project.
// It contains only the code required by the clock software: bus initialization
// and Celsius temperature reads by device index.
// See README.md for details about what was removed compared to upstream.
//
// Licensed under the MIT License - see LICENSE.

// Includes
#include <inttypes.h>
#include <Arduino.h>
#include <OneWire.h>

// Constants for device models
#define DS18S20MODEL 0x10  // also DS1820
#define DS18B20MODEL 0x28  // also MAX31820
#define DS1822MODEL  0x22
#define DS1825MODEL  0x3B  // also MAX31850
#define DS28EA00MODEL 0x42

// Error Codes
#define DEVICE_DISCONNECTED_C -127
#define DEVICE_DISCONNECTED_RAW -7040

#define DEVICE_FAULT_OPEN_C -254
#define DEVICE_FAULT_OPEN_RAW -32512

#define DEVICE_FAULT_SHORTGND_C -253
#define DEVICE_FAULT_SHORTGND_RAW -32384

#define DEVICE_FAULT_SHORTVDD_C -252
#define DEVICE_FAULT_SHORTVDD_RAW -32256

#define DEVICE_POWER_ON_RESET_C -251
#define DEVICE_POWER_ON_RESET_RAW -32128

#define DEVICE_INSUFFICIENT_POWER_C -250
#define DEVICE_INSUFFICIENT_POWER_RAW -32000

// Configuration Constants
#define MAX_CONVERSION_TIMEOUT 750
#define MAX_INITIALIZATION_RETRIES 3
#define INITIALIZATION_DELAY_MS 50

typedef uint8_t DeviceAddress[8];

class DallasTemperature {
public:
    struct request_t {
        bool result;
        unsigned long timestamp;
        operator bool() { return result; }
    };

    // Constructors
    DallasTemperature();
    DallasTemperature(OneWire*);

    // Setup & Configuration
    void setOneWire(OneWire*);
    void begin(void);

    // Device Information
    bool validAddress(const uint8_t*);
    bool validFamily(const uint8_t* deviceAddress);
    bool getAddress(uint8_t*, uint8_t);
    bool isConnected(const uint8_t*, uint8_t*);

    // Scratchpad Operations
    bool readScratchPad(const uint8_t*, uint8_t*);
    bool readPowerSupply(const uint8_t* deviceAddress = nullptr);

    // Resolution Control
    uint8_t getResolution(const uint8_t*);

    // Temperature Operations
    request_t requestTemperatures(void);
    int32_t getTemp(const uint8_t*, byte retryCount = 0);
    float getTempC(const uint8_t*, byte retryCount = 0);
    float getTempCByIndex(uint8_t);

    // Conversion Status
    bool isConversionComplete(void);
    static uint16_t millisToWaitForConversion(uint8_t);

    // Temperature Conversion Utilities
    static float rawToCelsius(int32_t);

    // Conversion Completion Methods
    void blockTillConversionComplete(uint8_t, unsigned long);

private:
    typedef uint8_t ScratchPad[9];

    // Internal State
    bool parasite;
    uint8_t bitResolution;
    bool waitForConversion;
    bool checkForConversion;
    uint8_t devices;
    uint8_t ds18Count;
    OneWire* _wire;

    // Internal Methods
    int32_t calculateTemperature(const uint8_t*, uint8_t*);
    bool isAllZeros(const uint8_t* const scratchPad, const size_t length = 9);
};

#endif // DallasTemperature_h
