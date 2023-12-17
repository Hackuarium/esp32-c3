#include "config.h"

#ifdef PARAM_BATTERY1

#include "params.h"
void TaskBatteryLevel(void* pvParameters) {
  (void)pvParameters;

  pinMode(2, INPUT);
  pinMode(3, INPUT);

  while (true) {
    // internal reference is 3.3v
    // because we have a divider we need to multiply by 2
    int rawLevel1 = (double)analogRead(2) / 4096 * 3.3 * 2 * 1000;
    Serial.print("Battery level1: ");
    Serial.println(rawLevel1);
    int rawLevel2 = (double)analogRead(3) / 4096 * 3.3 * 2 * 1000;
    Serial.print("Battery level2: ");
    Serial.println(rawLevel2);

    // GPIO0 is used to read the battery level.
    // A voltage divider is used with the following connections: BAT+/VBUS -> R1
    // -> GPIO0 and GND/BAT- -> R2 -> GPIO0 with R1=R2=220kOhm A first test
    // indicates that a full NCR18650B battery (4.12V) gives a value of 2895,
    // and empty (2.615V) gives a value of 1869. The discharge of the NCR18650B
    // with WiFi connected without sleep lasted 39h42min. The discharge
    // decreased almost linearly except for the last 2h (from value 2281 until
    // 1869) where the voltage drop is faster.
    vTaskDelay(1000);
    /* Emergency sleep to allow the battery to charge enough to connect to WiFi
    (below this value WiFi will reset the board without allowing deepsleep) if
    (raw_level < 1600 && raw_level > 1300){ Serial.println("EMERGENCY deep
    sleep"); goToDeepSleep();
    }
    */

    setParameter(PARAM_BATTERY1, rawLevel1);
    setParameter(PARAM_BATTERY2, rawLevel2);
    vTaskDelay(1000);
  }
}

void taskBatteryLevel() {
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskBatteryLevel, "TaskBatteryLevel",
                          1048,  // This stack size can be checked & adjusted by
                                 // reading the Stack Highwater
                          NULL,
                          0,  // Priority, with 3 (configMAX_PRIORITIES - 1)
                              // being the highest, and 0 being the lowest.
                          NULL, 1);
}
#endif