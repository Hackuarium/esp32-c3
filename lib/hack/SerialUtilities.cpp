#include "./common.h"
#include "./params.h"
#include "./taskNTPD.h"
#include "./taskOneWire.h"

void resetParameters();

static String mac2String(uint64_t mac) {
  byte* ar = (byte*)&mac;
  String s;
  for (byte i = 0; i < 6; ++i) {
    char buf[3];
    sprintf(buf, "%02X", ar[i]);  // J-M-L: slight modification, added the 0 in
                                  // the format for padding
    s += buf;
    if (i < 5)
      s += ':';
  }
  return s;
}

static const char* getFlashModeStr() {
  switch (ESP.getFlashChipMode()) {
    case FM_DIO:
      return "DIO";
    case FM_DOUT:
      return "DOUT";
    case FM_FAST_READ:
      return "FAST READ";
    case FM_QIO:
      return "QIO";
    case FM_QOUT:
      return "QOUT";
    case FM_SLOW_READ:
      return "SLOW READ";
    case FM_UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

static const char* getResetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_BROWNOUT:
      return "Brownout reset (software or hardware)";
    case ESP_RST_DEEPSLEEP:
      return "Reset after exiting deep sleep mode";
    case ESP_RST_EXT:
      return "Reset by external pin (not applicable for ESP32)";
    case ESP_RST_INT_WDT:
      return "Reset (software or hardware) due to interrupt watchdog";
    case ESP_RST_PANIC:
      return "Software reset due to exception/panic";
    case ESP_RST_POWERON:
      return "Reset due to power-on event";
    case ESP_RST_SDIO:
      return "Reset over SDIO";
    case ESP_RST_SW:
      return "Software reset via esp_restart";
    case ESP_RST_TASK_WDT:
      return "Reset due to task watchdog";
    case ESP_RST_WDT:
      return "ESP_RST_WDT";

    case ESP_RST_UNKNOWN:
    default:
      return "Unknown";
  }
}

void printUtilitiesHelp(Print* output) {
  output->println(F("(uc) Compact settings"));
  output->println(F("(ue) Epoch"));
  output->println(F("(uf) Free"));
  output->println(F("(ui) Info"));
  output->println(F("(uq) Qualifier"));
  output->println(F("(ur) Reset"));
  output->println(F("(uz) eeprom"));
}

void printHelp(Print* output) {
  output->println(F("(h)elp"));
#ifdef THR_WIRE_MASTER
  output->println(F("(i)2c"));
#endif
#ifdef THR_EEPROM_LOGGER
  output->println(F("(l)og"));
#endif
  output->println(F("(o)neWire"));
  output->println(F("(s)ettings"));
  output->println(F("(u)tilities"));
  output->println(F("(w)ifi"));

  // todo printSpecificHelp(output);
}

char ptrTaskList[600];

static void printFreeMemory(Print* output) {
  output->print(F("Number of tasks: "));
  uint8_t nbTasks = uxTaskGetNumberOfTasks();
  output->println(nbTasks);

  output->println(xPortGetFreeHeapSize());
  output->println(xPortGetMinimumEverFreeHeapSize());

  for (uint8_t i = 0; i < nbTasks; i++) {
  }

  TaskHandle_t taskBlinkHandle = xTaskGetHandle("TaskBlink");
  output->print("Task blink: ");
  output->println(uxTaskGetStackHighWaterMark(taskBlinkHandle));
  TaskHandle_t taskMqttHandle = xTaskGetHandle("TaskMQTT");
  output->print("Task MQTT: ");
  output->println(uxTaskGetStackHighWaterMark(taskMqttHandle));

  //  vTaskList(buffer);

  /*
  vTaskList(ptrTaskList);
  output->println(F("Task  State   Prio    Stack    Num"));
  output->println(F("----------------------------------"));
  output->print(ptrTaskList);
  //  output->println(uxTaskGetSystemState()),
  */

  output->print(F("Free heap: "));
  output->println(ESP.getFreeHeap());
}

static void printInfo(Print* output) {
  esp_chip_info_t info;
  esp_chip_info(&info);

  output->printf("ESP-IDF Version: %s\n", ESP.getSdkVersion());
  printf("\n");
  printf("Chip info:\n");
  printf("\tModel: %s\n", ESP.getChipModel());
  printf("\tRevison number: %d\n", ESP.getChipRevision());
  printf("\tCores: %d\n", ESP.getChipCores());
  printf("\tClock: %d MHz\n", ESP.getCpuFreqMHz());
  printf("\tFeatures:%s%s%s%s%s\r\n",
         info.features & CHIP_FEATURE_WIFI_BGN ? " 802.11bgn " : "",
         info.features & CHIP_FEATURE_BLE ? " BLE " : "",
         info.features & CHIP_FEATURE_BT ? " BT " : "",
         info.features & CHIP_FEATURE_EMB_FLASH ? " Embedded-Flash "
                                                : " External-Flash ",
         info.features & CHIP_FEATURE_EMB_PSRAM ? " Embedded-PSRAM" : "");

  printf("EFuse MAC: %s\n", mac2String(ESP.getEfuseMac()).c_str());

  printf("Flash size: %d MB (mode: %s, speed: %d MHz)\n",
         ESP.getFlashChipSize() / (1024 * 1024), getFlashModeStr(),
         ESP.getFlashChipSpeed() / (1024 * 1024));
  printf("PSRAM size: %d MB\n", ESP.getPsramSize() / (1024 * 1024));

  printf("Sketch size: %d KB\n", ESP.getSketchSize() / (1024));
  printf("Sketch MD5: %s\n", ESP.getSketchMD5().c_str());
}

void processUtilitiesCommand(char command,
                             char* paramValue,
                             Print* output) {  // char and char* ??
  switch (command) {
    case 'c':
      if (paramValue[0] != '\0') {
        printCompactParameters(output, atoi(paramValue));
      } else {
        printCompactParameters(output);
      }
      break;
    case 'e':
      if (paramValue[0] != '\0') {
        setEpoch(atol(paramValue));
        output->println(getEpoch());
      } else {
        output->println(getEpoch());
      }
      break;
    case 'f':
      printFreeMemory(output);
      break;
    case 'i':
      printInfo(output);
      break;
    case 'q':
      if (paramValue[0] != '\0') {
        setQualifier(atoi(paramValue));
      }
      {
        uint16_t a = getQualifier();
        output->println(a);
      }
      break;
    case 'r':
      if (paramValue[0] != '\0') {
        if (atol(paramValue) == 1234) {
          resetParameters();
          output->println(F("Reset done"));
        }
      } else {
        output->println(F("To reset enter ur1234"));
      }
      break;

    case 'z':
      // todo  getStatusEEPROM(output);
      break;
    default:
      printUtilitiesHelp(output);
      break;
  }
}
