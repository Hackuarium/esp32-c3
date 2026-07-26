
// need to use native code:
// https://github.com/espressif/esp-idf/blob/5c1044d84d625219eafa18c24758d9f0e4006b2c/examples/protocols/esp_http_client/main/esp_http_client_example.c

#include <WiFi.h>
#include "config.h"
#include "esp_http_client.h"

#define MAX_HTTP_BUFFER 1000
/* Longest URL we keep open a connection for. */
#define MAX_HTTP_URL 160

esp_err_t httpEventHandler(esp_http_client_event_t* evt);

char httpBuffer[MAX_HTTP_BUFFER] = {0};
esp_http_client_handle_t httpClientHandle = NULL;
esp_err_t httpError;

/* Bytes written into httpBuffer for the response being received. */
static int httpOutputLen = 0;
/* The URL the currently-open connection was made for ("" when none is open). */
static char httpOpenUrl[MAX_HTTP_URL] = {0};
/* Set by httpPerform when it had to open a new connection (i.e. pay a handshake). */
static bool httpDidConnect = false;
/* Requests the connection being replaced managed to serve. */
static uint32_t httpFetchesOnConnection = 0;

/*
  Plain HTTP only. A TLS handshake needs a ~40 KB contiguous heap block that this
  build (~55 KB free) cannot spare, so https:// URLs fail on this hardware — the
  energy-flow and weather endpoints are both served over HTTP for that reason.
*/
static esp_http_client_config_t httpConfig = {
    .method = HTTP_METHOD_GET,
    .timeout_ms = 10000,
    .event_handler = httpEventHandler,
    .user_data = httpBuffer,
};

static void httpDropConnection() {
  if (httpClientHandle != NULL) {
    esp_http_client_cleanup(httpClientHandle);
    httpClientHandle = NULL;
  }
  httpOpenUrl[0] = 0;
}

/*
  Perform the request, keeping the connection open between calls to the same URL.

  Measured against the backend (Apache, KeepAliveTimeout 5 s): a request within
  the window reuses the connection and skips the TCP setup, ~3 ms instead of
  ~13 ms — so polling FASTER than the keep-alive window is cheaper per fetch than
  polling slower, which always pays a fresh connection. Switching URL (the
  forecast lives on another host) closes the connection first.
*/
static esp_err_t httpPerform(char* url) {
  httpDidConnect = false;
  if (httpClientHandle != NULL && strcmp(httpOpenUrl, url) != 0) {
    httpDropConnection();
  }
  if (httpClientHandle == NULL) {
    httpConfig.url = url;
    httpClientHandle = esp_http_client_init(&httpConfig);
    if (httpClientHandle == NULL) {
      return ESP_FAIL;
    }
    strncpy(httpOpenUrl, url, MAX_HTTP_URL - 1);
    httpOpenUrl[MAX_HTTP_URL - 1] = 0;
    httpDidConnect = true;
  }
  httpOutputLen = 0;
  // Clear the shared buffer. The original `httpBuffer[MAX_HTTP_BUFFER] = {0}`
  // wrote one past the end of the array and cleared nothing, so a short response
  // left the tail of a previous, longer one behind — the energy-flow payload is
  // ~110 bytes and shares this buffer with the much longer forecast one.
  memset(httpBuffer, 0, MAX_HTTP_BUFFER);
  return esp_http_client_perform(httpClientHandle);
}

char* fetch(char* url) {
  uint32_t startedAt = millis();
  httpError = httpPerform(url);
  if (httpError != ESP_OK) {
    // A kept-alive socket the server has since closed fails on first use. Drop
    // it and retry once on a fresh connection, so an expired keep-alive costs a
    // handshake rather than a missed sample.
    httpDropConnection();
    httpError = httpPerform(url);
  }

  // Report only when a connection had to be opened, so the log tells you at a
  // glance whether reuse is working on this device: silence means it is, a line
  // every poll means it is not, and the figure is the real handshake cost here.
  if (httpDidConnect) {
    Serial.printf("[http] opened a connection in %lu ms (previous one served %lu requests)\n",
                  (unsigned long)(millis() - startedAt),
                  (unsigned long)httpFetchesOnConnection);
    httpFetchesOnConnection = 0;
  } else {
    httpFetchesOnConnection++;
  }

  if (httpError == ESP_OK) {
    if (strlen(httpBuffer) == 0) {
      Serial.println("No data");
    } else if (strlen(httpBuffer) > 900) {
      Serial.println("Data too long");
    }
  } else {
    Serial.println("Failing to retrieve http info");
    httpDropConnection();
  }
  return httpBuffer;
}

esp_err_t httpEventHandler(esp_http_client_event_t* evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
      // Restart the write cursor. A redirect (or an aborted transfer) connects
      // again without ever reaching ON_FINISH, and carrying the old offset over
      // would append the second response after the first. Note this does NOT
      // fire on a reused connection, which is why httpPerform resets it too.
      httpOutputLen = 0;
      break;
    case HTTP_EVENT_ON_DATA:
      /*
       *  Check for chunked encoding is added as the URL for chunked encoding
       * used in this example returns binary data. However, event handler can
       * also be used in case chunked encoding is used.
       */
      if (evt->user_data) {
        if (httpOutputLen + evt->data_len < MAX_HTTP_BUFFER) {
          memcpy((char*)evt->user_data + httpOutputLen, evt->data,
                 evt->data_len);
        }
      } else {
        Serial.println("Need to define user_data");
        return ESP_FAIL;
      }
      httpOutputLen += evt->data_len;
      break;
    case HTTP_EVENT_ON_FINISH:
      httpOutputLen = 0;
      break;
  }
  return ESP_OK;
}
