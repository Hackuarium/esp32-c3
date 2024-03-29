#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <StringStream.h>
#include <WiFi.h>
#include "./SerialUtilities.h"
#include "./pixels/function.h"
#include "./pixels/gif.h"
#include "./taskSerial.h"
#include "config.h"

AsyncWebServer server(80);
AsyncEventSource events("/events");

void notFound(AsyncWebServerRequest* request) {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (request->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += request->args();
  message += "\n";
  for (uint8_t i = 0; i < request->args(); i++) {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
  }
  request->send(404, "text/plain", message);
}

#ifdef MAX_LED
void handleFunction(AsyncWebServerRequest* request) {
  String action = request->arg("value");
  setFunction(action.c_str());
  request->send(200, "text/plain", "Function changed");
}
void handleGIF(AsyncWebServerRequest* request) {
  String action = request->arg("value");
  setGIF(action.c_str());
  request->send(200, "text/plain", "GIF changed");
}
#endif

void handleListLogs(AsyncWebServerRequest* request) {
  AsyncResponseStream* response = request->beginResponseStream("text/plain");

  File root = SPIFFS.open("/logs");

  File file = root.openNextFile();
  while (file) {
    response->printf(file.name());
    file = root.openNextFile();
  }

  request->send(response);
}

void handleCommand(AsyncWebServerRequest* request) {
  String action = request->arg("value");
  if (action == "") {
    action = "h";
  }
  String message;
  StringStream stream((String&)message);
  printResult(&action[0], &stream);

  request->send(200, "text/plain", message);
}

void TaskWebserver(void* pvParameters) {
  //  vTaskDelay(1212);
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  while ((WiFi.status() != WL_CONNECTED || WiFi.localIP() == INADDR_NONE) &&
         WiFi.softAPIP() == INADDR_NONE) {
    vTaskDelay(5000);
  }

  (void)pvParameters;

  server.on("/command", handleCommand);
  server.on("/listLogs", handleListLogs);
#ifdef MAX_LED
  server.on("/function", handleFunction);
  server.on("/gif", handleGIF);
#endif

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.serveStatic("/test.jpg", SPIFFS, "/test.jpg");
  server.onNotFound(notFound);
  server.addHandler(&events);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
  Serial.println("HTTP server started");

  while (true) {
    vTaskDelay(1000);
  }
}

void sendEventSource(char* event, char* data) {
  events.send(data, event, millis());
}

void taskWebserver() {
  vTaskDelay(4351);
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskWebserver, "TaskWebserver",
                          20000,  // This stack size can be checked &
                                  // adjusted by reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
