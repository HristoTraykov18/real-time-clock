// GPSModule.cpp
#include "GPSModule.h"

static unsigned long gps_search_started_ms = 0; // millis() timestamp of the current acquisition window

/* ------------------------------------ Private helpers ------------------------------------ */

/**
 * @brief Sends a UBX-CFG-MSG frame setting how often the receiver emits one NMEA sentence.
 * @param msg_class Message class of the sentence, 0xF0 for the standard NMEA set.
 * @param msg_id Sentence identifier within the class.
 * @param rate Emissions per navigation fix, 0 disables the sentence.
 */
static void sendUbxCfgMsg(uint8_t msg_class, uint8_t msg_id, uint8_t rate) {
  uint8_t packet[] = {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, msg_class, msg_id, rate, 0x00, 0x00};
  uint8_t ck_a = 0, ck_b = 0;

  for (uint8_t i = 2; i < 9; i++) { // 8-bit Fletcher over class, id, length and payload
    ck_a += packet[i];
    ck_b += ck_a;
  }

  packet[9] = ck_a;
  packet[10] = ck_b;
  gpsSerial.write(packet, sizeof(packet));
}


/**
 * @brief Silences every NMEA sentence the clock does not read, leaving only RMC.
 * RMC alone carries the date, time, position and fix status.
 */
static void configureGpsSentences() {
  constexpr uint8_t NMEA_CLASS = 0xF0;
  constexpr uint8_t SENTENCE_RATES[][2] = { // Sentence id, emissions per fix
    {0x00, 0}, // GGA - fix data, satellite count, altitude
    {0x01, 0}, // GLL - position and time only
    {0x02, 0}, // GSA - active satellites and dilution of precision
    {0x03, 0}, // GSV - detail of every satellite in view
    {0x04, 1}, // RMC - date, time, position, fix status
    {0x05, 0}  // VTG - course and ground speed
  };

  for (const auto &sentence : SENTENCE_RATES) {
    sendUbxCfgMsg(NMEA_CLASS, sentence[0], sentence[1]);
    delay(10);
  }

  // GPS_TX is a boot strapping pin that has to read LOW at boot, but SoftwareSerial sets it
  // HIGH as the UART idle state, preserved on ESP.restart()
  pinMode(GPS_TX, INPUT);
}


/* ---------------------------------- Public functions ---------------------------------- */

void activateGPS() {
  static bool serial_started = false;

  if (!serial_started) {
    gpsSerial.begin(GPS_BAUD_RATE);
    configureGpsSentences();
    serial_started = true;
  }

  gps_state = GpsState::Searching;
  gps_search_started_ms = millis();

  if constexpr (DEBUG_MESSAGES)
    Serial.println(F("[activateGPS] GPS initialized. Searching for fix."));
}


bool gpsHasFreshFix() {
  return gps.date.isValid() && gps.location.isValid() && gps.location.age() < GPS_MAX_FIX_AGE;
}


GpsState gpsState() {
  return gps_state;
}


void serviceGPS() {
  static unsigned long last_report_ms = 0;
  static uint32_t bytes_seen = 0;

  while (gpsSerial.available()) {
    const char c = gpsSerial.read();
    bytes_seen++;
    Serial.write(c); // Raw passthrough, so a baud mismatch shows up as non-printable noise
    gps.encode(c);
  }

  if (millis() - last_report_ms < 5000) return;
  last_report_ms = millis();

  Serial.print(F("\n[GPS] bytes="));  Serial.print(bytes_seen);
  Serial.print(F(" ok="));            Serial.print(gps.passedChecksum());
  Serial.print(F(" bad="));           Serial.print(gps.failedChecksum());
  Serial.print(F(" withFix="));       Serial.print(gps.sentencesWithFix());
  Serial.print(F(" | date="));        Serial.print(gps.date.isValid());
  Serial.print(F(" time="));          Serial.print(gps.time.isValid());
  Serial.print(F(" loc="));           Serial.print(gps.location.isValid());
  Serial.print(F(" age="));           Serial.print(gps.location.age());
  Serial.print(F(" sats="));          Serial.print(gps.satellites.value());
  Serial.print(F(" state="));         Serial.println((int)gpsState());
}


bool updateTimeFromGPS() {
  if (!gpsHasFreshFix())
    return false;

  // The GPS reports UTC. Shift through the Unix timestamp so the timezone offset rolls the
  // calendar day, month and year over correctly
  DateTime utc(gps.date.year(), gps.date.month(), gps.date.day(),
               gps.time.hour(), gps.time.minute(), gps.time.second());
  applyTimeUpdate((time_t)utc.unixtime() + (timezone * 3600L), true);

  return true;
}
