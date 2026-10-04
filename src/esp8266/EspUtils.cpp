// esp8266/EspUtils.cpp
#include "EspUtils.h"
#include "NetworkUtils.h"

static WiFiEventHandler ap_connect_handler;
static WiFiEventHandler ap_disconnect_handler;
static WiFiEventHandler sta_disconnect_handler;

static ESP8266HTTPUpdateServer http_updater;

static char scan_ssid[33];


void espAttachWifiEventHandlers() {
  ap_connect_handler = WiFi.onSoftAPModeStationConnected(
    [](const WiFiEventSoftAPModeStationConnected&) {
      ap_station_associated = true;
      last_http_activity_ms = millis();

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("\n=== Device connected to softAP ==="));
  });

  ap_disconnect_handler = WiFi.onSoftAPModeStationDisconnected(
    [](const WiFiEventSoftAPModeStationDisconnected&) {
      espEvictStations(); // Network Utils
      ap_station_associated = false;
      radio_settle_until_ms = millis() + RADIO_SETTLE_PERIOD;

      if constexpr (DEBUG_MESSAGES)
        Serial.println(F("\n=== Device disconnected from softAP ==="));
  });

  sta_disconnect_handler = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected& event) {
      sta_disconnect_count++;

      if constexpr (DEBUG_MESSAGES) {
        Serial.print(F("\n=== ESP STA disconnected from "));
        Serial.print(event.ssid);
        Serial.println(F(" ==="));
      }
  });
}


void espEvictStations() {
  struct station_info *si = wifi_softap_get_station_info();

  while (si != NULL) {
    char mac_addr[18];
    snprintf(mac_addr, sizeof(mac_addr), "%02X:%02X:%02X:%02X:%02X:%02X",
            si->bssid[0], si->bssid[1], si->bssid[2],
            si->bssid[3], si->bssid[4], si->bssid[5]);
    wifi_softap_deauth(si->bssid);
    si = STAILQ_NEXT(si, next);

    if constexpr (DEBUG_MESSAGES) {
      Serial.print(F("[espEvictStations] "));
      Serial.print(mac_addr);
      Serial.println(F(" disconnected from softAP due to inactivity"));
    }
  }

  wifi_softap_free_station_info();
}


bool espFsBegin() {
  return LittleFS.begin(); // Formats on a missing or unreadable filesystem by default
}


#ifdef  GPS_MODULE
void espGpsSerialBegin(unsigned long baud) {
  gpsSerial.begin(baud); // The pins come from the SoftwareSerial constructor in Globals.h
}
#endif


uint8_t espHeapFragmentation() {
  return ESP.getHeapFragmentation();
}


bool espIsAuthFailure(int status) {
  return status == WL_WRONG_PASSWORD;
}


uint32_t espMaxAllocHeap() {
  return ESP.getMaxFreeBlockSize();
}


const uint8_t* espScanBssid(int index) {
  const bss_info* info = WiFi.getScanInfoByIndex(index);

  return info ? info->bssid : nullptr;
}


uint8_t espScanChannel(int index) {
  const bss_info* info = WiFi.getScanInfoByIndex(index);

  return info ? info->channel : 0;
}


void espScanDelete() {
  WiFi.scanDelete();
}


int espScanNetworks(uint8_t channel) {
  return WiFi.scanNetworks(false, true, channel);
}


const char* espScanSsid(int index) {
  const bss_info* info = WiFi.getScanInfoByIndex(index);
  const size_t length = info ? (info->ssid_len > 32 ? 32 : info->ssid_len) : 0;

  if (length)
    memcpy(scan_ssid, info->ssid, length);

  scan_ssid[length] = '\0';

  return scan_ssid;
}


void espSetCountry() {
  wifi_country_t country_settings = { "EU", 1, 13, WIFI_COUNTRY_POLICY_MANUAL };
  wifi_set_country(&country_settings);
}


void espSetupHttpUpdater(WebServerClass* update_server, const char* path,
                         const char* user, const char* pass) {
  http_updater.setup(update_server, path, user, pass);
}


void espSetWifiSleep(bool enabled) {
  WiFi.setSleepMode(enabled ? WIFI_MODEM_SLEEP : WIFI_NONE_SLEEP);
}


void espStationSsid(char* buf) {
  struct station_config conf;
  wifi_station_get_config(&conf);
  memcpy(buf, conf.ssid, 32);
  buf[32] = '\0';
}
