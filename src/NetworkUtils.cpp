// NetworkUtils.cpp
#include "NetworkUtils.h"


void checkForUserConnection() {
  bool counted = WiFi.softAPgetStationNum() > 0;
  bool http_live = (millis() - last_http_activity_ms) < AP_CONNECTION_TIMEOUT;
  bool present = ap_station_associated && counted && http_live;

  if (present && !active_connection) {
    someone_just_connected = true;
    active_connection = true;
  }
  else if (!present && active_connection) {
    someone_just_connected = false;
    active_connection = false;

    if (software_update_server_active) {
      softwareUpdateServer.stop();
      software_update_server_active = false;

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("OTA update server stopped"));
    }

    if (counted)
      evictStaleStations();
  }
}


wl_status_t connectClockToNetwork(const char* ssid, const char* pass, bool is_hidden,
                                  uint8_t channel, const uint8_t* bssid) {
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED && currentSsidEquals(ssid)) {
    if constexpr (DEBUG_MESSAGES) {
      char ssid_buf[33]; // Max SSID len (32) + NUL = 33
      currentSsid(ssid_buf);
      Serial.print(F("Already connected to "));
      Serial.println(ssid_buf);
    }

    return status;
  }

  WiFi.begin(ssid, pass, channel, bssid);
  yield();

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("Trying to connect to "));
    Serial.print(ssid);

    if (bssid) {
        Serial.print(F(" | Saved MAC: "));
        Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                      bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
        Serial.print(F(" (fast-connect)"));
    }

    Serial.println();
  }

  const uint16_t CONNECT_ATTEMPT_DELAY = bssid ? 80 : 130;

  for (uint16_t i = 0; i < CONNECT_ATTEMPT_DELAY; i++) {
    status = WiFi.status();

    if (status == WL_CONNECTED) {
      saveNetworkInfo(ssid, pass, is_hidden ? "true" : "false", WiFi.channel(), WiFi.BSSID());

      if constexpr (DEBUG_MESSAGES) {
        Serial.print(F("\nConnected to "));
        Serial.println(ssid);
      }

      break;
    }

    // Unrecoverable statuses
    if (status == WL_WRONG_PASSWORD || status == WL_NO_SSID_AVAIL) {
      if constexpr (DEBUG_MESSAGES) {
        Serial.print(F("\nConnection attempt stopped. Wi-Fi status "));
        Serial.println(status);
      }

      break;
    }

    if constexpr (DEBUG_MESSAGES) {
      if (i % 10 == 0)
        Serial.print(F("."));
    }

    delay(CONNECT_ATTEMPT_DELAY);
    yield();
  }

  if constexpr (DEBUG_MESSAGES)
    Serial.println();

  return status;
}


void currentSsid(char* buf) {
  struct station_config conf;
  wifi_station_get_config(&conf);
  memcpy(buf, conf.ssid, 32);
  buf[32] = '\0';
}


bool currentSsidEquals(const char* target) {
  if (!target)
    return false;

  size_t target_len = strlen(target);

  if (target_len > 32)
    return false;

  struct station_config conf;
  wifi_station_get_config(&conf);

  if (memcmp(conf.ssid, target, target_len) != 0)
    return false;

  return target_len == 32 || conf.ssid[target_len] == 0;
}


void evictStaleStations() {
  struct station_info *si = wifi_softap_get_station_info();

  while (si != NULL) {
    char mac_addr[18];
    snprintf(mac_addr, sizeof(mac_addr), "%02X:%02X:%02X:%02X:%02X:%02X",
            si->bssid[0], si->bssid[1], si->bssid[2],
            si->bssid[3], si->bssid[4], si->bssid[5]);
    wifi_softap_deauth(si->bssid);
    si = STAILQ_NEXT(si, next);

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(mac_addr);
      Serial.println(F(" disconnected from softAP due to inactivity"));
    }
  }

  wifi_softap_free_station_info();
}


bool fastConnectCacheIsValid(uint8_t channel, const uint8_t* bssid) {
  if (!channel || !bssid)
    return false;

  for (uint8_t scan_attempts = 5; scan_attempts > 0; scan_attempts--) {
    int8_t number_of_networks = WiFi.scanNetworks(false, true, channel);

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("scanNetworks: "));
      Serial.println(number_of_networks);
    }

    if (number_of_networks > 0) {
      bool is_valid = false;

      for (uint8_t i = 0; i < number_of_networks; i++) {
        const bss_info* info = WiFi.getScanInfoByIndex(i);

        if (info && memcmp(info->bssid, bssid, 6) == 0) {
          is_valid = true;

          break;
        }
      }

      WiFi.scanDelete();
      yield();

      return is_valid;
    }

    WiFi.scanDelete();
    delay(500);
  }

  return false;
}


int getPacketLength(IPAddress& address) {
  sendPacket(address);

  unsigned long start_millis = millis();
  unsigned long last_millis = start_millis;
  unsigned long packet_length = 0;

  while (millis() - start_millis < 1500 && packet_length == 0) {
    packet_length = udp.parsePacket();

    if (millis() != last_millis && ((millis() - start_millis) % 100) == 0) {
      if constexpr (DEBUG_MESSAGES)
        Serial.print(F("."));

      last_millis = millis();
    }
  }

  return packet_length;
}


void initializeSoftAP() {
  char buf[128];
  const char* network_data[5];
  loadNetworkInfo(buf, sizeof(buf), network_data);
  WiFi.softAP(ESP_SSID, ESP_PASS, (network_data[3] ? atoi(network_data[3]) : 1), 0, 1); // Set ESP access point
}


bool loadNetworkInfo(char* buf, size_t buf_size, const char* fields[5]) {
  for (uint8_t i = 0; i < 5; i++)
    fields[i] = nullptr;

  if (!LittleFS.exists("creds.txt")) return false;

  File f = LittleFS.open("creds.txt", "r");

  if (!f) return false;

  int n = f.read((uint8_t*)buf, buf_size - 1);
  f.close();

  if (n <= 0) return false;

  buf[n] = '\0';

  // Walk buf, replacing '\n' with '\0' to terminate each field in place.
  fields[0] = buf;
  uint8_t j = 1;

  for (int i = 0; i < n && j < 5; i++) {
    if (buf[i] == '\n') {
      buf[i] = '\0';
      fields[j++] = &buf[i + 1];
    }
  }

  return true;
}


void manageStaReconnect() {
  static uint8_t retry_count = 2; // Attempt only 1 reconnect on boot, thus the value is 2
  static bool back_off_active = false;
  static uint8_t back_off_hour = 0;

  if (!LittleFS.exists("creds.txt") ||
      ap_station_associated ||
      millis() < radio_settle_until_ms)
      return;

  if (WiFi.status() == WL_CONNECTED) {
    if (back_off_active) {
      back_off_active = false;

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("Back off disabled "));
    }

    retry_count = 0;
    return;
  }

  if (back_off_active) {
    if (rtc.now().hour() != back_off_hour) {
      back_off_active = false;
      retry_count = 0;
    }

    return;
  }

  if (retry_count < 3) {
    if (networkReconnect() == WL_CONNECTED) {
      autoUpdateTime(true);
      retry_count = 0;
    }
    else if (++retry_count >= 3) {
      back_off_active = true;
      back_off_hour = rtc.now().hour();

      if constexpr (DEBUG_MESSAGES) {
        Serial.print(F("Back off enabled for "));
        Serial.print(back_off_hour + 1);
        Serial.println(F(":00"));
      }
    }
  }
}


bool networkIsInRange(const char* ssid) {
  if (!ssid)
    return false;

  const size_t ssid_len = strlen(ssid);

  if (ssid_len == 0 || ssid_len > 32)
    return false;

  if constexpr (DEBUG_MESSAGES) {
    Serial.print(F("Requested: "));
    Serial.println(ssid);
  }

  int8_t number_of_networks = WiFi.scanNetworks(false, true);

  bool in_range = false;

  for (uint8_t i = 0; i < number_of_networks; i++) {
    const bss_info* info = WiFi.getScanInfoByIndex(i);

    if (!info)
      continue;

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("In range: "));

      for (uint8_t k = 0; k < 32 && info->ssid[k]; k++)
        Serial.write(info->ssid[k]);

      Serial.print(F(" | Channel: "));
      Serial.println(info->channel);
    }

    if (memcmp(info->ssid, ssid, ssid_len) == 0 && (ssid_len == 32 || info->ssid[ssid_len] == 0)) {
      in_range = true;

      break;
    }
  }

  WiFi.scanDelete();
  yield();

  return in_range;
}


wl_status_t networkReconnect() {
  wl_status_t connection_status = WiFi.status();

  if (connection_status != WL_CONNECTED) {
    char buf[128]; // SSID (32) + pass (64) + is_hidden (5) + channel (2) + BSSID (17) + 4 linesep = 120
    const char* network_data[5];

    if (!loadNetworkInfo(buf, sizeof(buf), network_data)) {
      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("No creds.txt file"));

      return connection_status;
    }

    int32_t channel = 0;
    uint8_t bssid[6];
    const uint8_t* bssid_ptr = nullptr;

    if (network_data[3] && network_data[4] && parseBssid(network_data[4], bssid)) {
      channel = atoi(network_data[3]);
      bssid_ptr = bssid;
    }

    bool is_hidden = strcmp(network_data[2], "true") == 0;

    if (bssid_ptr && fastConnectCacheIsValid(channel, bssid_ptr))
      connection_status = connectClockToNetwork(network_data[0], network_data[1], is_hidden, channel, bssid_ptr);

    if (connection_status != WL_CONNECTED) {
      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("Running full Wi-Fi scan"));

      connection_status = connectClockToNetwork(network_data[0], network_data[1], is_hidden);
    }
  }

  return connection_status;
}


bool parseBssid(const char* s, uint8_t* bssid) {
  if (!s || strlen(s) != 17) // "XX:XX:XX:XX:XX:XX"
    return false;

  auto hexVal = [](char c) -> int8_t {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  };

  for (uint8_t i = 0; i < 6; i++) {
    int8_t h = hexVal(s[i * 3]);
    int8_t l = hexVal(s[i * 3 + 1]);

    if (h < 0 || l < 0)
      return false;

    bssid[i] = (h << 4) | l;
  }

  return true;
}


void saveNetworkInfo(const char *network_name, const char* network_pass, const char* is_hidden,
                     uint8_t channel, const uint8_t* bssid) {

  char buf[128]; // SSID (32) + pass (64) + is_hidden (5) + channel (2) + BSSID (17) + 4 linesep = 124
  int n = snprintf(buf, sizeof(buf),
                   "%s\n%s\n%s\n%u\n%02X:%02X:%02X:%02X:%02X:%02X",
                   network_name, network_pass, is_hidden, channel,
                   bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

  if (n <= 0 || n >= (int)sizeof(buf)) {
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("\nSave Network Info: snprintf overflow"));

    return;
  }

  File f = LittleFS.open("creds.txt", "w");

  if (!f) {
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("\nSave Network Info: Failed to open creds.txt"));

    return;
  }

  f.write((const uint8_t*)buf, (size_t)n);
  f.close();
}


void sendPacket(IPAddress& address) {
  if constexpr (DEBUG_MESSAGES)
    Serial.println(F("Preparing NTP packet"));

  const uint8_t NTP_PACKET_SIZE = 48;
  byte packet_buffer[NTP_PACKET_SIZE];

  // Initialize values needed to form NTP request (see URL above for details on the packets)
  packet_buffer[0] = 0b11100011; // LI, Version, Mode
  packet_buffer[1] = 0; // Stratum, or type of clock
  packet_buffer[2] = 6; // Polling Interval
  packet_buffer[3] = 0xEC; // Peer Clock Precision
  packet_buffer[12]  = 49;
  packet_buffer[13]  = 0x4E;
  packet_buffer[14]  = 49;
  packet_buffer[15]  = 52;

  // All NTP fields have values, send a packet requesting a timestamp
  udp.beginPacket(address, 123); // NTP requests are to port 123
  udp.write(packet_buffer, NTP_PACKET_SIZE);
  udp.endPacket();

  if constexpr (DEBUG_MESSAGES)
    Serial.print(F("NTP packet sent. Waiting response"));
}


bool updateTimeFromNTP() {
  if (!WiFi.hostByName(EU_NTP_SERVER_1, time_server_ip)) {
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("NTP DNS resolution failed"));

    return false;
  }

  while (udp.parsePacket() > 0)
    while (udp.available()) udp.read();

  if (!getPacketLength(time_server_ip)) return false;

  if (udp.remoteIP() != time_server_ip) {
    if constexpr (DEBUG_MESSAGES)
      Serial.println(F("NTP response from unexpected source discarded"));

    return false;
  }

  if constexpr (DEBUG_MESSAGES)
    Serial.println(F("\nNTP response received"));

  const uint8_t NTP_PACKET_SIZE = 48;
  byte packet_buffer[NTP_PACKET_SIZE];
  int read_count = udp.read(packet_buffer, NTP_PACKET_SIZE);

  if (read_count != NTP_PACKET_SIZE) return false;

  // Validate protocol network_data
  uint8_t li = (packet_buffer[0] >> 6) & 0x03;
  uint8_t mode = packet_buffer[0] & 0x07;
  uint8_t stratum = packet_buffer[1];

  if (li == 3 || mode != 4 || stratum == 0 || stratum > 15) {
    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("NTP packet rejected: LI="));
      Serial.print(li);
      Serial.print(F(" | mode="));
      Serial.print(mode);
      Serial.print(F(" | stratum="));
      Serial.println(stratum);
    }

    return false;
  }

  unsigned long secs_since_1900 =
    ((unsigned long) packet_buffer[40] << 24) |
    ((unsigned long) packet_buffer[41] << 16) |
    ((unsigned long) packet_buffer[42] << 8)  |
     (unsigned long) packet_buffer[43];

  if (secs_since_1900 == 0) return false;
  if (secs_since_1900 < 2208988800UL + 1577836800UL) return false;

  // Unix time starts on Jan 1 1970. In seconds, 70 years is 2208988800. Add one second to compensate calculation delay
  time_t epoch = secs_since_1900 - 2208988800UL + (timezone * 3600L) + 1;

  // DST offset
  const bool dst_applied = daylight_saving_enabled && isDaylightSavingPeriod(epoch);

  if (dst_applied)
    epoch += 3600;

  struct tm *current_time = gmtime(&epoch); // The offset is already contained in the timestamp
  current_time->tm_year += 1900; // Year is calculated from 1900 to now, so set to current year

  rtc.adjust(DateTime(current_time->tm_year, current_time->tm_mon + 1, current_time->tm_mday,
                      current_time->tm_hour, current_time->tm_min, current_time->tm_sec));

  daylight_saving_active = dst_applied;
  connected_to_ntp = true;

  return true;
}
