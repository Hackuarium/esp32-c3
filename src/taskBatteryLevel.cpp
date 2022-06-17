#include "./common.h"
#include "./params.h"

void TaskBatteryLevel(void* pvParameters) {
  (void)pvParameters;

  pinMode(0, INPUT);

  while (true) {
    int raw_level = analogRead(0);  // GPIO0 is used to read the battery level.
    // A voltage divider is used with the following connections: BAT+/VBUS -> R1
    // -> GPIO0 and GND/BAT- -> R2 -> GPIO0 with R1=R2=220kOhm A first test
    // indicates that a full NCR18650B battery (4.12V) gives a value of 2895,
    // and empty (2.615V) gives a value of 1869. The discharge of the NCR18650B
    // with WiFi connected without sleep lasted 39h42min. The discharge
    // decreased almost linearly except for the last 2h (from value 2281 until
    // 1869) where the voltage drop is faster.
    vTaskDelay(100);
    /* Emergency sleep to allow the battery to charge enough to connect to WiFi
    (below this value WiFi will reset the board without allowing deepsleep) if
    (raw_level < 1600 && raw_level > 1300){ Serial.println("EMERGENCY deep
    sleep"); goToDeepSleep();
    }
    */

    setParameter(PARAM_BATTERY, raw_level);
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
