// GPSModule.cpp
#include "GPSModule.h"

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
  gpsSerial.begin(GPS_BAUD_RATE);
  configureGpsSentences();
  gps_state = GpsState::Searching;
  gps_search_started_ms = millis();

  if constexpr (DEBUG_MESSAGES)
    Serial.println(F("GPS initialized. Searching for fix."));
}


bool gpsHasFreshFix() {
  return gps.date.isValid() && gps.location.isValid() && gps.location.age() < GPS_MAX_FIX_AGE;
}


GpsState gpsState() {
  return gps_state;
}


void serviceGPS() {
  if (!set_time_with_gps) {
    gps_state = GpsState::Disabled;

    return;
  }

  while (gpsSerial.available())
    gps.encode(gpsSerial.read());

  if (gpsHasFreshFix()) {
    if (gps_state != GpsState::Locked) {
      gps_state = GpsState::Locked;

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("GPS fix acquired"));
    }

    return;
  }

  if (gps_state == GpsState::Locked) {
    gps_state = GpsState::Searching;
    gps_search_started_ms = millis();

    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("GPS fix lost, searching again"));
  }
  else if (gps_state == GpsState::Searching &&
           millis() - gps_search_started_ms > GPS_ACQUISITION_TIMEOUT) {
    gps_state = GpsState::TimedOut;

    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("GPS unreachable, falling back to manual updates"));
  }
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
