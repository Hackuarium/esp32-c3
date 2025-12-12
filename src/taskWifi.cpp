#include <WiFi.h>
#include "esp_wpa2.h"
#include "params.h"

void taskWifiAP();

char ssid[30];
char password[30];
char username[30];
char identity[30];

void retrieveWifiParameters() {
  getParameter("wifi.ssid", ssid);
  getParameter("wifi.password", password);
  getParameter("wifi.username", username);
  getParameter("wifi.identity", identity);
}

// Function to find and connect to the strongest AP with matching SSID
bool connectToStrongestAP(const char* targetSSID, bool isEnterprise) {
  Serial.println("Scanning for networks...");
  int numNetworks = WiFi.scanNetworks();

  if (numNetworks == 0) {
    Serial.println("No networks found");
    return false;
  }

  int8_t bestRSSI = -127;
  int bestIndex = -1;
  uint8_t bestBSSID[6];
  int32_t bestChannel = 0;

  // Find the strongest network with matching SSID
  for (int i = 0; i < numNetworks; i++) {
    if (strcmp(WiFi.SSID(i).c_str(), targetSSID) == 0) {
      int8_t rssi = WiFi.RSSI(i);
      Serial.printf("Found %s with RSSI: %d dBm (Channel: %d)\n", targetSSID,
                    rssi, WiFi.channel(i));

      if (rssi > bestRSSI) {
        bestRSSI = rssi;
        bestIndex = i;
        memcpy(bestBSSID, WiFi.BSSID(i), 6);
        bestChannel = WiFi.channel(i);
      }
    }
  }

  WiFi.scanDelete();

  if (bestIndex == -1) {
    Serial.printf("SSID '%s' not found in scan\n", targetSSID);
    return false;
  }

  Serial.printf("Connecting to strongest AP: %s (RSSI: %d dBm, Channel: %d)\n",
                targetSSID, bestRSSI, bestChannel);
  Serial.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X\n", bestBSSID[0],
                bestBSSID[1], bestBSSID[2], bestBSSID[3], bestBSSID[4],
                bestBSSID[5]);

  // Connect to specific BSSID (strongest AP)
  if (isEnterprise) {
    // For Enterprise, credentials are already set via
    // esp_wifi_sta_wpa2_ent_set_* WiFi.begin(ssid, passphrase, channel, bssid)
    WiFi.begin(targetSSID, NULL, bestChannel, bestBSSID);
  } else {
    WiFi.begin(targetSSID, password, bestChannel, bestBSSID);
  }

  return true;
}

void TaskWifi(void* pvParameters) {
  int16_t wifiTimeout = -1;

  while (getParameter(PARAM_WIFI_MODE) == 1) {
    vTaskDelay(1000);
  }

  if (getParameter(PARAM_WIFI_MODE) == 2) {
    wifiTimeout = 30;
  }

  vTaskDelay(2500);

  unsigned long start = millis();

  retrieveWifiParameters();

  if (strlen(ssid) < 2 || strlen(password) < 2) {
    Serial.println("No wifi ssid or password defined");
    while ((strlen(ssid) < 2 || strlen(password) < 2) && wifiTimeout > 0 &&
           (millis() - start) < wifiTimeout * 1000) {
      vTaskDelay(1000);
      retrieveWifiParameters();
    }
  }

  Serial.print("Trying to connect to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);

  if (strlen(identity) == 0) {
    strcpy(identity, username);
  }

  bool wifi_Enterprise_type = false;

  if (strlen(identity) == 0 && strlen(username) == 0) {
    Serial.println("Using domestic WPA2");
    wifi_Enterprise_type = false;
  } else {
    Serial.println("Using WPA2 Enterprise");
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)identity, strlen(identity));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t*)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t*)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();
    wifi_Enterprise_type = true;
  }

  uint8_t counter = 0;
  uint8_t reconnectAttempts = 0;

  while ((wifiTimeout <= 0 || (millis() - start) < wifiTimeout * 1000) &&
         counter++ < 30) {
    vTaskDelay(2000);

    if (WiFi.status() != WL_CONNECTED) {
      setParameter(PARAM_WIFI_RSSI, -1);
      Serial.println("WIFI not connected, scanning for strongest AP");

      // Scan and connect to strongest AP
      if (connectToStrongestAP(ssid, wifi_Enterprise_type)) {
        // Wait for connection
        int waitCounter = 0;
        while (WiFi.status() != WL_CONNECTED && waitCounter++ < 20) {
          vTaskDelay(500);
          Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("WIFI connected successfully");
          Serial.print("IP address: ");
          Serial.println(WiFi.localIP());
          Serial.print("Connected RSSI: ");
          Serial.println(WiFi.RSSI());
        }
      }
    }

    if (WiFi.status() != WL_CONNECTED) {
      setParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_WIFI);
      reconnectAttempts++;
    } else {
      clearParameterBit(PARAM_STATUS, PARAM_STATUS_FLAG_NO_WIFI);
      setParameter(PARAM_WIFI_RSSI, WiFi.RSSI());
      counter = 0;
      reconnectAttempts = 0;
      wifiTimeout = -1;

      // Monitor signal strength and rescan if it drops significantly
      static unsigned long lastScanTime = 0;
      static int8_t lastRSSI = 0;
      int8_t currentRSSI = WiFi.RSSI();

      /*
      // Rescan every 5 minutes or if signal drops by more than 10 dBm
      if (millis() - lastScanTime > 300000 ||
          (lastRSSI - currentRSSI > 10 && currentRSSI < -70)) {
        Serial.println("Signal degraded or periodic check, rescanning...");
        WiFi.disconnect();
        lastScanTime = millis();
        counter = 29;  // Force reconnection in next iteration
      }
      */
      lastRSSI = currentRSSI;
    }
  }

  if (wifiTimeout > 0) {
    Serial.println("Could not connect to wifi, starting AP");
    WiFi.disconnect(true);
    setParameter(PARAM_WIFI_MODE, 1);
    while (true) {
      vTaskDelay(10000);
    }
  }
  Serial.println("Wifi process crashed");
}

void taskWifi() {
  xTaskCreatePinnedToCore(TaskWifi, "TaskWifi", 12000, NULL, 0, NULL, 1);
}