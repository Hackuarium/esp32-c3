#include "./common.h"
#include "./params.h"

char wifiTemp[40];

void printWifiHelp(Print* output) {
  output->println(F("(wi) Wifi info"));
  output->println(F("(ws) Wifi SSID"));
  output->println(F("(wp) Wifi password"));
  output->println(F("(wm) Wifi MDNS"));
  output->println(F("(wq) MQTT broker"));
  output->println(F("(wh) MQTT publish topic"));
  output->println(F("(wt) MQTT subscribe topic"));
}

void processWifiCommand(char command,
                        char* paramValue,
                        Print* output) {  // char and char* ??
  switch (command) {
    case 'i':
      output->println("Wifi information");
      output->print("SSID: ");
      getParameter("wifi.ssid", wifiTemp);
      output->println(wifiTemp);
      output->print("Password: ");
      getParameter("wifi.password", wifiTemp);
      output->println(wifiTemp);
      output->print("MDNS: ");
      getParameter("wifi.mdns", wifiTemp);
      output->println(wifiTemp);
      output->print("MQTT broker: ");
      getParameter("mqtt.broker", wifiTemp);
      output->println(wifiTemp);
      output->print("MQTT subscribe topic: ");
      getParameter("mqtt.subscribe", wifiTemp);
      output->println(wifiTemp);
      output->print("MQTT publish topic: ");
      getParameter("mqtt.publish", wifiTemp);
      output->println(wifiTemp);

      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        output->println("Failed to obtain time");
        return;
      }
      output->println(&timeinfo, "Time: %A, %B %d %Y %H:%M:%S");

      break;
    case 'm':
      setParameter("wifi.mdns", paramValue);
      output->println(paramValue);
      break;
    case 'p':
      setParameter("wifi.password", paramValue);
      output->println(paramValue);
      break;
    case 'q':  // MQTT server
      setParameter("mqtt.broker", paramValue);
      output->println(paramValue);
      break;
    case 't':  // MQTT subscribe topic
      setParameter("mqtt.subscribe", paramValue);
      output->println(paramValue);
      break;
    case 'h':  // MQTT publish topic
      setParameter("mqtt.publish", paramValue);
      output->println(paramValue);
      break;
    case 's':
      if (paramValue[0] == '\0') {
        output->println("Please enter the WIFI SSID");
      } else {
        setParameter("wifi.ssid", paramValue);
        output->println(paramValue);
      }
      break;
    default:
      printWifiHelp(output);
  }
}
