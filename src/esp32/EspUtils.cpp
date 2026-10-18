// esp32/EspUtils.cpp
#include "EspUtils.h"
#include "NetworkUtils.h"

static HTTPUpdateServer httpUpdater;

static char scan_ssid[33];

static volatile bool sta_auth_failed = false;


void espAttachWifiEventHandlers() {
  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) {
      ap_station_associated = true;
      last_http_activity_ms = millis();

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("\n=== Device connected to softAP ==="));
  }, ARDUINO_EVENT_WIFI_AP_STACONNECTED);

  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) {
      espEvictStations();
      ap_station_associated = false;
      radio_settle_until_ms = millis() + RADIO_SETTLE_PERIOD;

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("\n=== Device disconnected from softAP ==="));
  }, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
      const uint8_t reason = info.wifi_sta_disconnected.reason;

      sta_auth_failed = reason == WIFI_REASON_AUTH_FAIL
                     || reason == WIFI_REASON_HANDSHAKE_TIMEOUT
                     || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT;

      sta_disconnect_count += 1;

      if constexpr (DEBUG_MESSAGES) {
        Serial.print(F("\n=== ESP STA disconnected from "));
        Serial.write(info.wifi_sta_disconnected.ssid, info.wifi_sta_disconnected.ssid_len);
        Serial.print(F(" | Reason: "));
        Serial.print(WiFi.disconnectReasonName((wifi_err_reason_t)reason));
        Serial.println(F(" ==="));
      }
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}


void espEvictStations() {
  if constexpr (DEBUG_MESSAGES) {
    wifi_sta_list_t stations;

    if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK) {
      for (int i = 0; i < stations.num; i++) {
        char mac_addr[18];
        snprintf(mac_addr, sizeof(mac_addr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 stations.sta[i].mac[0], stations.sta[i].mac[1], stations.sta[i].mac[2],
                 stations.sta[i].mac[3], stations.sta[i].mac[4], stations.sta[i].mac[5]);
        Serial.print(F("[espEvictStations] "));
        Serial.print(mac_addr);
        Serial.println(F(" disconnected from softAP due to inactivity"));
      }
    }
  }

  esp_wifi_deauth_sta(0);
}


bool espFsBegin() {
  return LittleFS.begin(true);
}


#ifdef  GPS_MODULE
void espGpsSerialBegin(unsigned long baud) {
  gpsSerial.begin(baud, SERIAL_8N1, GPS_RX, GPS_TX); // UART2, pins given so the board cannot drift
}
#endif


uint8_t espHeapFragmentation() {
  const uint32_t free_heap = ESP.getFreeHeap();

  if (!free_heap)
    return 0;

  return (uint8_t)(100 - ((uint64_t)ESP.getMaxAllocHeap() * 100 / free_heap));
}


bool espIsAuthFailure(int) {
  return sta_auth_failed;
}


uint32_t espMaxAllocHeap() {
  return ESP.getMaxAllocHeap();
}


const uint8_t* espScanBssid(int index) {
  const wifi_ap_record_t* info = (const wifi_ap_record_t*)WiFi.getScanInfoByIndex(index);

  return info ? info->bssid : nullptr;
}


uint8_t espScanChannel(int index) {
  const wifi_ap_record_t* info = (const wifi_ap_record_t*)WiFi.getScanInfoByIndex(index);

  return info ? info->primary : 0;
}


void espScanDelete() {
  WiFi.scanDelete();
}


int espScanNetworks(uint8_t channel) {
  return WiFi.scanNetworks(false, true, false, 300, channel);
}


const char* espScanSsid(int index) {
  const wifi_ap_record_t* info = (const wifi_ap_record_t*)WiFi.getScanInfoByIndex(index);

  if (!info) {
    scan_ssid[0] = '\0';

    return scan_ssid;
  }

  memcpy(scan_ssid, info->ssid, sizeof(scan_ssid) - 1);
  scan_ssid[sizeof(scan_ssid) - 1] = '\0';

  return scan_ssid;
}


void espSetCountry() {
  const esp_err_t result = esp_wifi_set_country_code("EU", false);

  if constexpr (DEBUG_MESSAGES) {
    if (result != ESP_OK) {
      Serial.print(F("[espSetCountry] esp_wifi_set_country_code failed: "));
      Serial.println(esp_err_to_name(result));
    }
  }
}


void espSetWifiSleep(bool enabled) {
  WiFi.setSleep(enabled);
}


void espSetupHttpUpdater(WebServerClass* update_server, const char* path,
                         const char* user, const char* pass) {
  httpUpdater.setup(update_server, path, user, pass);
}


void espStationSsid(char* buf) {
  wifi_config_t conf = {};
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  memcpy(buf, conf.sta.ssid, 32);
  buf[32] = '\0';
}
