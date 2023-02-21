// #define DEBUG_MEMORY    // takes huge amount of memory should not be
// activated !!

/*****************************************************************************************
  This thread takes care of the logs and manage the time and its synchronisation
  The thread write the logs at a definite fixed interval of time in the
SST25VF064 chip The time synchronization works through the NTP protocol and our
server
******************************************************************************************/
#include "common.h"


/*
 * EEPROM -> Preferences for ESP32 
 * Built-in Partition tables
 * (https://my-esp-idf.readthedocs.io/en/stable/api-guides/partition-tables.html)
 * Single factory app, no OTA:
 * nvs size 0x6000 - 24576 bytes (2^13(8 kB) + 2^14(16 kB))
 * Factory app, two OTA definitions:
 * nvs size 0x4000 - 16384 bytes (2^14(16 kB))
*/

#include <Preferences.h>

#include <toHex.h>
#include <TimeLib.h>

#include<Arduino.h>

// #include "Sem.h"
// SEMAPHORE_DECL(lockTimeCriticalZone, 1); // only one process in some specific
// zones

#define Max_Digits 10 // Max digits for ULong 0 to 4294967295

#ifndef NB_PARAMETERS_LINEAR_LOGS
  #define NB_PARAMETERS_LINEAR_LOGS 26
#endif

// Create object for NVS memory space
Preferences prefs;

// Or remove the one key only
//preferences.remove("one-key");

// Define structure to store logger
typedef struct {
  int32_t nextEntryID;
  int32_t timeNow;
  int16_t eventNumber;
  int16_t parameterValue;
  uint16_t params[NB_PARAMETERS_LINEAR_LOGS];
} sLogger_t;

// Define structure to test
typedef struct {
  uint8_t varA;
  uint8_t varB;
  uint8_t varC;
  uint8_t varD;
} sTest_t;

// Define structures with uint8_t vars to store data into NVS like Bytes
typedef struct {
  uint32_t nextEntryID;
} sNextEntryID_t;

typedef struct {
  uint32_t timeNow;
} sTimeNow_t;

typedef struct {
  int16_t params[NB_PARAMETERS_LINEAR_LOGS];
} sParams_t;

typedef struct {
  uint16_t eventNumber;
} sEventNumber_t;

typedef struct {
  uint16_t parameterValue;
} sParameterValue_t;



/******************************************
   DEFINE PARTITION TABLE (default is PARTITION_TABLE_SINGLE_APP)
 *****************************************/
//  THIS SHOULD BE AUTOMATIC !!!
#define PARTITION_TABLE_SINGLE_APP
// #define PARTITION_TABLE_TWO_OTA

#if defined(PARTITION_TABLE_SINGLE_APP) || defined(PARTITION_TABLE_TWO_OTA)

/*
 * Types of logs
 * nextEntryID: 4 bytes
 * timeNow: 4 bytes
 * params[A-Z]: 26 * 2 bytes = 52 bytes
 * eventNumber: 2 bytes
 * parameterValue: 2 bytes
 * TOTAL: 64 bytes
*/
#define ENTRY_SIZE_LINEAR_LOGS 64 // Actually 64
#define SIZE_COUNTER_ENTRY 4      // Entry var: nextEntryID
#define SIZE_TIMESTAMPS 4         // Time var: timeNow

// Definition of the log sectors in the flash for the logs
#if defined(PARTITION_TABLE_SINGLE_APP)  // 24576 bytes
#define ADDRESS_MAX \
  0xF000  // https://my-esp-idf.readthedocs.io/en/stable/api-guides/partition-tables.html: OFFSET + SIZE = 0x9000 + 0x6000
#elif defined(PARTITION_TABLE_TWO_OTA)  // 16384 bytes
#define ADDRESS_MAX (0XD000)
#endif

// #define ADDRESS_MAX   0xA000 // if we don't want to use all memory !!!!

#define ADDRESS_BEG 0x9000  // OFFSET
#define ADDRESS_SIZE (ADDRESS_MAX - ADDRESS_BEG)  // Size
#define ADDRESS_LAST (ADDRESS_MAX - ENTRY_SIZE_LINEAR_LOGS)
#define SECTOR_SIZE 4096
#define NB_ENTRIES_PER_SECTOR (SECTOR_SIZE / ENTRY_SIZE_LINEAR_LOGS)
// The number of entires by types of logs (seconds, minutes, hours,
// commands/events)
#define MAX_NB_ENTRIES (ADDRESS_SIZE / ENTRY_SIZE_LINEAR_LOGS - 2)  // Remove 2 slots for other data (params[A-Z, AA-AZ, BA-BP], epoch, qualifiers, ID, etc.)

#define MAX_MULTI_LOG 64  // Allows to display long log on serial

sLogger_t *pLogs = (sLogger_t  *)calloc(ENTRY_SIZE_LINEAR_LOGS, sizeof(sLogger_t));

// sNextEntryID_t *pLogsNextEntryID = (sNextEntryID_t *)calloc(1, sizeof(sNextEntryID_t));

// sTimeNow_t *pLogsTimeNow = (sTimeNow_t *)calloc(1, sizeof(sTimeNow_t));

// sEventNumber_t *pLogsEventNumber = (sEventNumber_t *)calloc(1, sizeof(sEventNumber_t));

// sParameterValue_t *pLogsParameterValue = (sParameterValue_t *)calloc(1, sizeof(sParameterValue_t));

// sParams_t *pLogsParams = (sParams_t *)calloc(1, sizeof(sParams_t));

// Declare function to obtain last EntryID
uint32_t getLastNextEntryID();
uint32_t nextEntryID = 1000;

char* j;
// Deactivate safeguard to store log into memory
bool logActive = true;

/******************************************************************************
  Returns the address corresponding to one log ID nilThdSleepMilliseconds(5);
nilThdSleepMilliseconds(5); entryNb:     Log ID return:      Address of the
first byte where the corresponding log is located
*******************************************************************************/
uint32_t findAddressOfEntryN(uint32_t entryN) { // SINGLE_APP, maximum address 0x0180 = 384
  uint32_t address =
      ((entryN % MAX_NB_ENTRIES) * ENTRY_SIZE_LINEAR_LOGS) % ADDRESS_SIZE +
      ADDRESS_BEG;
  return address;
}

char *struct2str_EntryID (sNextEntryID_t *ap, uint32_t sizeNextID)
{
  size_t len = Max_Digits * sizeNextID + sizeof(char);
  char *buffer = (char *)calloc(1, len);
  int cx = 0;
  uint16_t run = 0;

  while (run < (sizeNextID))
  {
    if (cx >= 0 && cx < len)      // check returned value
    cx += snprintf( buffer + cx, len - cx, "%lu,", ap[run].nextEntryID );
    ++run;
  }

  return buffer;
}

char *struct2str_TimeNow (sTimeNow_t *ap, uint32_t size)
{
  size_t len = Max_Digits * size + sizeof(char);
  char *buffer = (char *)calloc(1, len);
  int cx = 0;
  uint16_t run = 0;

  while (run < (size))
  {
    if (cx >= 0 && cx < len)      // check returned value
    cx += snprintf( buffer + cx, len - cx, "%lu,", ap[run].timeNow );
    ++run;
  }

  return buffer;
}

char *struct2str_EventNb (sEventNumber_t *ap, uint16_t size)
{
  size_t len = Max_Digits * size + sizeof(char);
  char *buffer = (char *)calloc(1, len);
  int cx = 0;
  uint16_t run = 0;

  while (run < (size))
  {
    if (cx >= 0 && cx < len)      // check returned value
    cx += snprintf( buffer + cx, len - cx, "%i,", ap[run].eventNumber );
    ++run;
  }

  return buffer;
}

char *struct2str_ParamValue (sParameterValue_t *ap, uint16_t size)
{
  size_t len = Max_Digits * size + sizeof(char);
  char *buffer = (char *)calloc(1, len);
  int cx = 0;
  uint16_t run = 0;

  while (run < (size))
  {
    if (cx >= 0 && cx < len)      // check returned value
    cx += snprintf( buffer + cx, len - cx, "%i,", ap[run].parameterValue );
    ++run;
  }

  return buffer;
}

char *struct2str_Params (sParams_t *ap, size_t size)
{
  size_t len = Max_Digits * size + sizeof(char);
  char *buffer = (char *)calloc(1, len);
  int cx = 0;
  uint16_t run = 0;
  uint16_t paramsLogs = size / NB_PARAMETERS_LINEAR_LOGS;

  while (run < paramsLogs)
  {
    for (uint16_t i = 0; i < NB_PARAMETERS_LINEAR_LOGS; i++)
    {
      if (cx >= 0 && cx < len) {  // check returned value
        cx += snprintf( buffer + cx, len - cx, "%d,", ap[run].params[i] );
      }
    }
    run++;
  }

  return buffer;
}

/***********************************************************************************
  Save logs in the Flash memory.
  event_number: If there is a command, then this parameter should be set with
the corresponding command/event number. Should be found in the define list of
  commands/errors
************************************************************************************/
void writeLog(uint16_t eventNumber, int parameterValue) {
  /********************************
             Safeguards
  ********************************/
  if (!logActive)
    return;
  
  // Serial.println(F("nextEntry"));
  // Serial.println(nextEntryID);

  if(nextEntryID >= 1000) {
    nextEntryID = getLastNextEntryID();
  }

  // Serial.println(F("Previous Open non-volatile storage"));
  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);
  
  // Open Preferences with my-app namespace. Each application module, library, 
  // etc has to use a namespace name to prevent key name collisions. We will 
  // open storage in RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  bool openNVS = prefs.begin("logger");

  if(openNVS) {
    Serial.println(F("Open non-volatile storage"));
  }

  /*****************************************************************************
    Check Free Memory
  *****************************************************************************/
  size_t freeMemory = prefs.freeEntries();

  /**
   * @brief Check memorie free space
   * Check free space for: params[A-Z, AA-AZ, BA-BP], epoch, qualifiers, ID, etc.
   */
  if (freeMemory < 16) 
  {
    // Clear memory and avoid overflow in flash memory NVS space
    prefs.clear();
    nextEntryID = 0;
  }

  if(nextEntryID == 0) prefs.clear();

  /*****************************************************************************
    Reading Old Sequence
  *****************************************************************************/
  
  // Obtain length and create buffer for nextEntryID & TimeNow
  size_t schLen32 = prefs.getBytesLength("nextEntryID");

  if(schLen32 == 0) {
    Serial.println(F("No bytes stored in nextEntryID"));
  }

  // Obtain length and create buffer for params
  size_t schLenParams = prefs.getBytesLength("params");

  if(schLenParams == 0) {
    Serial.println(F("No bytes stored in params"));
  }

  // Obtain length and create buffer for eventNumber & parameterValue
  size_t schLen16 = prefs.getBytesLength("eventNumber");

  if(schLen16 == 0) {
    Serial.println(F("No bytes stored in eventNumber"));
  }
  
  /*****************************************************************************
    Reallocating memory to store flash variables
  *****************************************************************************/
  
  // nextEntryID
  sNextEntryID_t *pLogsNextEntryID = (sNextEntryID_t *)calloc((schLen32 + 4) / 4, sizeof(sNextEntryID_t)); // (old data + new entry)

  // timeNow
  sTimeNow_t *pLogsTimeNow = (sTimeNow_t *)calloc((schLen32 + 4) / 4, sizeof(sTimeNow_t)); // (old data + new entry)

  if(schLen32 != 0) {
    size_t checkReadNextID = prefs.getBytes("nextEntryID", pLogsNextEntryID, schLen32);
    size_t checkReadTime = prefs.getBytes("timeNow", pLogsTimeNow, schLen32);
  }

  // params
  sParams_t *pLogsParams = (sParams_t *)calloc((schLenParams + sizeof(sParams_t)) / 2, sizeof(int16_t)); // (old data + new entry)

  if(schLenParams != 0) {
    size_t ckRdParams = prefs.getBytes("params", pLogsParams, schLenParams);
  }

  // eventNumber
  sEventNumber_t *pLogsEventNumber = (sEventNumber_t *)calloc((schLen16 + 2) / 2, sizeof(sParameterValue_t)); // (old data + new entry)

  // parameterValue
  sParameterValue_t *pLogsParameterValue = (sParameterValue_t *)calloc((schLen16 + 2) / 2, sizeof(sParameterValue_t)); // (old data + new entry)

  if(schLen16 != 0) {
    size_t ckRdEventN = prefs.getBytes("eventNumber", pLogsEventNumber, schLen16);
    size_t ckRdParamValue = prefs.getBytes("parameterValue", pLogsParameterValue, schLen16);
  }

  /*****************************************************************************
    Store new values
  *****************************************************************************/
  // nextEntryID
  pLogsNextEntryID[(schLen32) / 4].nextEntryID = nextEntryID;


  // timeNow
  uint32_t timeNow = (uint32_t)now();
  pLogsTimeNow[(schLen32) / 4].timeNow = timeNow;
  // pLogsTimeNow[nextEntryID].p0 = (timeNow >> 24) & 0xFF;
  // pLogsTimeNow[nextEntryID].p1 = (timeNow >> 16) & 0xFF;
  // pLogsTimeNow[nextEntryID].p2 = (timeNow >> 8) & 0xFF;
  // pLogsTimeNow[nextEntryID].p3 = (timeNow >> 0) & 0xFF;

  // params (A-Z)
  size_t pos = schLenParams / 2;
  for (size_t i = 0; i < NB_PARAMETERS_LINEAR_LOGS; i++) {
    // param = getParameter(i);  // 2 bytes per parameter
    // pLogsParams[nextEntryID].p0[i] = ((param >> 8) & 0xFF);
    // pLogsParams[nextEntryID].p1[i] = (param >> 0) & 0xFF;
    pLogsParams[pos].params[i] = 25 - i;
  }
  
  // eventNumber
  pLogsEventNumber[(schLen16) / 2].eventNumber = eventNumber;
  // pLogsEventNumber[nextEntryID].p0 = (eventNumber >> 8) & 0xFF;
  // pLogsEventNumber[nextEntryID].p1 = (eventNumber >> 0) & 0xFF;

  // parameterValue
  pLogsParameterValue[(schLen16) / 2].parameterValue = parameterValue;

  /*****************************************************************************
    Writing Sequence
  *****************************************************************************/
  // nextEntryID
  size_t lenNextEntryID = prefs.putBytes("nextEntryID", pLogsNextEntryID, (schLen32 + sizeof(sNextEntryID_t)));

  // timeNow
  size_t lenTimeNow = prefs.putBytes("timeNow", pLogsTimeNow, (schLen32 + sizeof(sTimeNow_t)));

  // params (A-Z)
  // size_t lenParams = prefs.putBytes("params", pLogsParams, (schLenParams + 1)*sizeof(sParams_t));
  size_t lenParams = prefs.putBytes("params", pLogsParams, (schLenParams + sizeof(sParams_t)));

  // eventNumber
  size_t lenEventNumber = prefs.putBytes("eventNumber", pLogsEventNumber, (schLen16 + sizeof(sEventNumber_t)));

  // parameterValue
  size_t lenParamValue = prefs.putBytes("parameterValue", pLogsParameterValue, (schLen16 + sizeof(sParameterValue_t)));

  prefs.end();  // Finish the writing process

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);
  Serial.println(F("Finish storage"));

  // Free memory
  // free(pLogsNextEntryID_2);
  // free(pLogsTimeNow);
  // free(pLogsParams);
  // free(pLogsEventNumber);
  // free(pLogsParameterValue);

  /*****************************************************************************
    Check saved information
    We assume that the logger is high priority
    And no other thread will change any of the values !!!!!!
  *****************************************************************************/
  bool isLogValid = true;

  // if (lenNextEntryID != (schLen32 + 1)*sizeof(sNextEntryID_t))
  if (lenNextEntryID != (schLen32 + sizeof(sNextEntryID_t)))
    isLogValid = false;
  if (lenTimeNow != (schLen32 + sizeof(sTimeNow_t)))
    isLogValid = false;
  if (lenParams != (schLenParams + sizeof(sParams_t)))
    isLogValid = false;
  if (lenEventNumber != (schLen16 + sizeof(sEventNumber_t)))
    isLogValid = false;
  if (lenParamValue != (schLen16 + sizeof(sParameterValue_t)))
    isLogValid = false;
  // if (lenEventNumber != (schLen16 + 1)*sizeof(sEventNumber_t))
  //   isLogValid = false;
  // if (lenParameterValue != (schLen16 + 1)*sizeof(sParameterValue_t))
    // isLogValid = false;

  if (isLogValid) {
    // Update the value of the next event log position in the memory
    nextEntryID++;
    Serial.println(F("Log success!"));
  } else {
    Serial.println(F("Log fail!"));
    Serial.println(nextEntryID);
    // if logger fails it is better to go back and erase the full sector
    // we can anyway not try to write if it was not erased !
    // and if we don't do this ... we will destroy the memory !
    // nextEntryID -= nextEntryID % NB_ENTRIES_PER_SECTOR;
  }

  Serial.println(F("Finish save data"));

  /*****************************************************************************
    Print saved information
  *****************************************************************************/

  char *charNextEntryID = struct2str_EntryID(pLogsNextEntryID, lenNextEntryID / 4);
  Serial.printf("\n nextEntryID as a string:\n\n  '%s'\n\n", charNextEntryID);
  if(charNextEntryID) free(charNextEntryID);
  
  char *charTimeNow = struct2str_TimeNow(pLogsTimeNow, lenTimeNow / 4);
  Serial.printf("\n timeNow as a string:\n\n  '%s'\n\n", charTimeNow);
  if(charTimeNow) free(charTimeNow);

  char *charParams = struct2str_Params(pLogsParams, lenParams / 2);
  Serial.printf("\n params as a string:\n\n  '%s'\n\n", charParams);
  if(charParams) free(charParams);

  char *charEventNb = struct2str_EventNb(pLogsEventNumber, lenEventNumber / 2);
  Serial.printf("\n eventNumber as a string:\n\n  '%s'\n\n", charEventNb);
  if(charEventNb) free(charEventNb);

  char *charParamValue = struct2str_ParamValue(pLogsParameterValue, lenParamValue / 2);
  Serial.printf("\n parameterValue as a string:\n\n  '%s'\n\n", charParamValue);
  /* free dynamically allocated memory */
  if(charParamValue) free(charParamValue);

  // Free pointers
  free(pLogsNextEntryID);
  free(pLogsTimeNow);
  free(pLogsParams);
  free(pLogsEventNumber);
  free(pLogsParameterValue);

  Serial.println(F("Free entries:"));
  Serial.println(freeMemory);

  /*****************************
         Out and Deselect
  ******************************/
  vTaskDelay(5);
}

/******************************************************************************************
  Read the corresponding logs in the flash memory of the entry number (ID).
  result: Array of uint8_t where the logs are stored. It should be a 32 bytes
 array for the 3 RRD logs and 12 bytes for the commands/events logs. #ifdef
 LOG_INTERVAL entryN: Log ID that will correspond to the logs address to be read
 and stored in result return:  Error flag: 0: no error occured
  EVENT_ERROR_NOT_FOUND_ENTRY_N: The log ID (entryN) was not found in the flash
 memory
 *****************************************************************************************/
uint32_t printLogN(Print* output, uint32_t entryN) {
  // Are we asking for a log entry that is not on the card anymore ? Then we
  // just start with the first that is on the card And we skip a sector ...
  if ((nextEntryID > MAX_NB_ENTRIES) &&
      (entryN < (nextEntryID - MAX_NB_ENTRIES + NB_ENTRIES_PER_SECTOR))) {
    entryN = nextEntryID - MAX_NB_ENTRIES + NB_ENTRIES_PER_SECTOR;
  }

  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);

  // Open namespace
  prefs.begin("logger");

  uint8_t checkDigit = 0;
  
  /*****************************************************************************
    Read and print parameters
  *****************************************************************************/
  // nextEntryID
  uint32_t index32 = entryN * sizeof(sNextEntryID_t);
  size_t schLen32 = prefs.getBytesLength("nextEntryID");
  uint8_t bufferNextEntryID[schLen32]; // prepare a buffer for the data
  prefs.getBytes("nextEntryID", bufferNextEntryID, schLen32);

  checkDigit ^= toHex(output, bufferNextEntryID[index32]);
  checkDigit ^= toHex(output, bufferNextEntryID[index32 + 1]);
  checkDigit ^= toHex(output, bufferNextEntryID[index32 + 2]);
  checkDigit ^= toHex(output, bufferNextEntryID[index32 + 3]);

  // timeNow
  uint8_t bufferTimeNow[schLen32]; // prepare a buffer for the data
  prefs.getBytes("timeNow", bufferTimeNow, schLen32);
  checkDigit ^= toHex(output, bufferTimeNow[index32]);
  checkDigit ^= toHex(output, bufferTimeNow[index32 + 1]);
  checkDigit ^= toHex(output, bufferTimeNow[index32 + 2]);
  checkDigit ^= toHex(output, bufferTimeNow[index32 + 3]);

  // params (A-Z)
  size_t schLenParams = prefs.getBytesLength("params");
  uint8_t bufferParams[schLenParams]; // prepare a buffer for the data
  prefs.getBytes("params", bufferParams, schLenParams);
  for (byte i = entryN*sizeof(sParams_t); i < schLenParams; i++) {
    checkDigit ^= toHex(output, bufferParams[i]);
  }

  // eventNumber
  uint32_t index16 = entryN * sizeof(sEventNumber_t);
  size_t schLen16 = prefs.getBytesLength("eventNumber");
  uint8_t bufferEventNumber[schLen16]; // prepare a buffer for the data
  prefs.getBytes("eventNumber", bufferEventNumber, schLen16);
  checkDigit ^= toHex(output, bufferEventNumber[index16]);
  checkDigit ^= toHex(output, bufferEventNumber[index16 + 1]);

  // parameterValue
  uint8_t bufferParamaterValue[schLen16]; // prepare a buffer for the data
  prefs.getBytes("parameterValue", bufferParamaterValue, schLen16);
  checkDigit ^= toHex(output, bufferParamaterValue[index16]);
  checkDigit ^= toHex(output, bufferParamaterValue[index16 + 1]);

  prefs.end();

  // checkDigit ^= toHex(output, getQualifier());
  toHex(output, checkDigit);
  output->println("");

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);

  return entryN;
}

// void Last_Log_To_SPI_buff(byte* buff) {
//   prefs.begin("logger");
//   sst.flashReadInit(findAddressOfEntryN(nextEntryID - 1));
//   for (byte i = 0; i < ENTRY_SIZE_LINEAR_LOGS; i++) {
//     byte oneByte = sst.flashReadNextInt8();
//     buff[i] = oneByte;
//   }
//   sst.flashReadFinish();
//   chSemSignal(&lockTimeCriticalZone);
// }

void loadLastEntryToParameters() {
  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);

  prefs.begin("logger");

  /*****************************************************************************
    Read and print parameters
  *****************************************************************************/
  // params
  size_t schLen = prefs.getBytesLength("params");
  uint8_t bufferParams[schLen]; // prepare a buffer for the data
  prefs.getBytes("params", bufferParams, schLen);
  prefs.end();

  for (uint16_t i = (schLen - 2*NB_PARAMETERS_LINEAR_LOGS); i < schLen  + NB_PARAMETERS_LINEAR_LOGS; i+2) {
    uint16_t parameter = (((uint16_t)bufferParams[i] << 8) && 0xFF00) || ((uint16_t)bufferParams[i + 1]);
    // setParameter(i, parameter);
  }

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);
}

/*****************************************************************************
  Returns the last log ID stored in the memory
  return: Last log ID stored in the memory corresponding to a log type
******************************************************************************/
void recoverLastEntryN() {
  uint32_t ID_temp = 0;
  uint32_t Time_temp = 0;
  uint32_t addressEntryN = ADDRESS_BEG;
  bool found = false;

  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);

  prefs.begin("logger");
  size_t schLen32 = prefs.getBytesLength("nextEntryID");
  uint8_t bufferNextEntryID[schLen32];

  uint8_t bufferTimeNow[schLen32];

  /*****************************************************************************
    Read values
  *****************************************************************************/
  // nextEntryID
  prefs.getBytes("nextEntryID", bufferNextEntryID, schLen32);

  // timeNow
  prefs.getBytes("timeNow", bufferTimeNow, schLen32);

  prefs.end();

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);

  ID_temp = (((uint32_t)bufferNextEntryID[schLen32 - 4] << 24) & 0xFF000000) | (((uint32_t)bufferNextEntryID[schLen32 - 3] << 16) & 0x00FF0000) | (((uint32_t)bufferNextEntryID[schLen32 - 2] << 8) & 0x0000FF00) | (((uint32_t)bufferNextEntryID[schLen32 - 1] << 0) & 0x000000FF);

  Time_temp = (((uint32_t)bufferTimeNow[schLen32 - 4] << 24) & 0xFF000000) | (((uint32_t)bufferTimeNow[schLen32 - 3] << 16) & 0x00FF0000) | (((uint32_t)bufferTimeNow[schLen32 - 2] << 8) & 0x0000FF00) | (((uint32_t)bufferTimeNow[schLen32 - 1] << 24) & 0x000000FF);

  /*****************************************************************************
    Store values
  *****************************************************************************/
  nextEntryID = ID_temp + 1;  // this will be the correct value in case of break
  setTime(Time_temp);

  logActive = true;
}

void printLastLog(Print* output) {
  printLogN(output, nextEntryID - 1);
}

void formatFlash(Print* output) {
  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);

  prefs.begin("logger");
  // Remove all preferences under the opened namespace
  if(prefs.clear()) {
    nextEntryID = 0;
    output->println(F("OK"));
  }
  else {
    output->println(F("Format flash ERROR!"));
  }

  prefs.end();

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);
}

void testFlash(Print* output) {
  output->println(F("W/R / validate"));
  // output->println(F("You need to format after test lf1234"));

  sTest_t *pTest = (sTest_t  *)calloc(3, sizeof(sTest_t));

  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);

  prefs.begin("test"); // use "test" namespace
  uint8_t content[] = {9, 30, 235, 255, 20, 15, 0, 1}; // two entries
  prefs.putBytes("test_A", content, sizeof(content));
  size_t schLen = prefs.getBytesLength("test_A");
  char buffer[schLen]; // prepare a buffer for the data
  prefs.getBytes("test_A", buffer, schLen);
  if (schLen % sizeof(sTest_t)) { // simple check that data fits
    log_e("Data is not correct size!");
    output->println(F("Data is not correct size!"));
    return;
  }
  pTest = (sTest_t *) buffer; // cast the bytes into a struct ptr
  output->printf("%02d:%02d %d/%d\n", 
    pTest[1].varA, pTest[1].varB,
    pTest[1].varC, pTest[1].varD);
  pTest[2] = {8, 30, 20, 21}; // add a third entry
  // force the struct array into a byte array
  prefs.putBytes("test_A", pTest, 3*sizeof(sTest_t)); 
  schLen = prefs.getBytesLength("test_A");
  char buffer2[schLen];
  prefs.getBytes("test_A", buffer2, schLen);
  for (int x = 0; x < schLen; x++) output->printf("%02X ", buffer2[x]);
  output->println(); 

  prefs.clear();
  prefs.remove("test_A");

  prefs.end();

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);
}

void readFlash(Print* output, long firstRecord) {
  output->println(F("Index / Address / ID / Epoch"));

  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);

  prefs.begin("logger");
  size_t schLen32 = prefs.getBytesLength("nextEntryID");
  uint8_t bufferNextEntryID[schLen32];

  uint8_t bufferTimeNow[schLen32];

  /*****************************************************************************
    Read values
  *****************************************************************************/
  // nextEntryID
  prefs.getBytes("nextEntryID", bufferNextEntryID, schLen32);

  // timeNow
  prefs.getBytes("timeNow", bufferTimeNow, schLen32);

  prefs.end();

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);

  uint32_t index = 0;
  uint32_t ID_temp = 0;
  uint32_t Time_temp = 0;

  for (int i = firstRecord; i < schLen32; i+4) {
    if (i == (firstRecord + 256))
      break;
    
    ID_temp = (((uint32_t)bufferNextEntryID[i] << 24) & 0xFF000000) | (((uint32_t)bufferNextEntryID[i + 1] << 16) & 0x00FF0000) | (((uint32_t)bufferNextEntryID[i + 2] << 8) & 0x0000FF00) | (((uint32_t)bufferNextEntryID[i + 3] << 0) & 0x000000FF);

    Time_temp = (((uint32_t)bufferTimeNow[i] << 24) & 0xFF000000) | (((uint32_t)bufferTimeNow[i + 1] << 16) & 0x00FF0000) | (((uint32_t)bufferTimeNow[i + 2] << 8) & 0x0000FF00) | (((uint32_t)bufferTimeNow[i + 3] << 24) & 0x000000FF);

    output->print(index);
    output->print(" ");
    output->print(i, HEX);
    output->print(" ");
    output->print(ID_temp, HEX);
    output->print(" ");
    output->print(Time_temp, HEX);
    output->println("");
  }  
}

/*
   We will check when we have a change to FF at the ID
*/

// NOT IMPLEMENTED

void debugFlash(Print* output) {
  output->print(F("Debug changes"));
  byte isFF = 2;
  for (long i = 0; i < MAX_NB_ENTRIES; i++) {
    long address = findAddressOfEntryN(i);
    // sst.flashReadInit(address);
    // long index = sst.flashReadNextInt32();
    uint32_t index = 0;
    if (index == 0xFFFFFFFF) {
      if (isFF != 1) {
        isFF = 1;
        output->println(F(""));
        output->print(i);
        output->print(F(" "));
        toHex(output, index);
      }
    } else {
      if (isFF != 0) {
        isFF = 0;
        output->println(F(""));
        output->print(i);
        output->print(F(" "));
        toHex(output, index);
      }
    }
    // sst.flashReadFinish();
    if (i % 1024 == 0) {
      output->print(F("."));
    }
  }
  output->println(F(""));
  output->println(F("Done"));
}

/*
   We will check when we have a change to FF at the ID
*/
// void checkNextID(Print* output) {
//   output->println(F("Check next ID"));
//   // we assume that the ID should always grow linearly. Just
//   // after it is not linear, we set the lastEntryID
//   sst.flashReadInit(0);
//   uint32_t expectedID = sst.flashReadNextInt32();
//   sst.flashReadFinish();
//   for (uint32_t i = 1; i < MAX_NB_ENTRIES; i++) {
//     expectedID++;
//     uint32_t address = findAddressOfEntryN(i);
//     sst.flashReadInit(address);
//     uint32_t currentID = sst.flashReadNextInt32();
//     sst.flashReadFinish();
//     if (currentID != expectedID && currentID != 4294967295) {
//       output->print(expectedID);
//       output->print(" ");
//       output->println(currentID);
//     }
//   }
//   output->println(F("Done"));
// }

void printLoggerHelp(Print* output) {
  output->println(F("Logger help"));
#ifdef DEBUG_MEMORY
  output->println(F("(lc) Check"));
  output->println(F("(ld) Debug"));
  output->println(F("(ln) Set nextID"));
#endif
  output->println(F("(lf) Format"));
  output->println(F("(li) Info"));
  output->println(F("(ll) Current log"));
  output->println(F("(lm) Multiple log"));
  output->println(F("(lr) Read (start record)"));
  output->println(F("(lt) Test"));
}

void processLoggerCommand(char command, char* data, Print* output) {
  switch (command) {
#ifdef DEBUG_MEMORY
    case 'c':
      checkNextID(output);
      break;
    case 'd':
      debugFlash(output);
      break;
#endif
    case 'f':
      if (data[0] == '\0' || atoi(data) != 1234) {
        output->println(F("To format flash enter lf1234"));
      } else {
        formatFlash(output);
      }
      break;
    case 'i':
      prefs.begin("logger");
      output->print(F("Free entries: "));
      output->println(prefs.freeEntries());
      prefs.end();
      break;
    case 'l':
      if (data[0] != '\0')
        printLogN(output, atol(data));
      else
        printLastLog(output);
      break;
    case 'm':
      if (data[0] != '\0') {
        long currentValueLong = atol(data);
        if ((currentValueLong - nextEntryID) < 0) {
          printLogN(output, currentValueLong);
        } else {
          byte endValue = MAX_MULTI_LOG;
          if ((uint32_t)currentValueLong > nextEntryID)
            endValue = 0;
          else if ((nextEntryID - currentValueLong) < MAX_MULTI_LOG)
            endValue = nextEntryID - currentValueLong;
          for (byte i = 0; i < endValue; i++) {
            currentValueLong = printLogN(output, currentValueLong) + 1;
            vTaskDelay(25);
          }
        }
      } else {
        output->println(nextEntryID - 1);
      }
      break;
#ifdef DEBUG_MEMORY
    case 'n':
      if (data[0] == '\0' || atoi(data) < NB_ENTRIES_PER_SECTOR ||
          atoi(data) % NB_ENTRIES_PER_SECTOR) {
        output->print(F("Must be a multiple of "));
        output->println(NB_ENTRIES_PER_SECTOR);
      } else {
        nextEntryID = atoi(data);
      }
      break;
#endif
    case 'r':
      if (data[0] == '\0') {
        readFlash(output, 0);
      } else {
        readFlash(output, atol(data));
      }
      break;
    case 't':
      if (data[0] == '\0' || atoi(data) != 1234) {
        output->println(F("YOU LOOSE ALL DATA! Use lt1234"));
      } else {
        testFlash(output);
      }
      break;
    default:
      printLoggerHelp(output);
  }
}

// void dumpLoggerFlash(Print* output, uint32_t fromAddress, uint32_t toAddress) {
//   int bytesPerRow = 16;
//   int j = 0;
//   char buf[4];
//   sst.flashReadInit(fromAddress);
//   // go from first to last eeprom address
//   for (uint32_t i = fromAddress; i <= toAddress; i++) {
//     if (j == 0) {
//       sprintf(buf, "%03X", i);
//       output->print(buf);
//       output->print(F(": "));
//     }
//     sprintf(buf, "%02X ", sst.flashReadNextInt8());
//     j++;
//     if (j == bytesPerRow) {
//       j = 0;
//       output->println(buf);
//       chThdSleep(25);
//     } else {
//       output->print(buf);
//     }
//   }
//   sst.flashReadFinish();
// }

// char *numberToLabel(byte number) {
//   /* get lenght of string required to hold struct values */
//   size_t len = 0;
//   char varA;
//   char varB = (char)(number - floor(number / 26) * 26 + 65);

//   if (number > 25) {
//     varA = (char)(floor(number / 26) + 64);
//     len = snprintf (NULL, len, "%s%s", varA, varB);
//   } else
//   {
//     len = snprintf (NULL, len, "%s", varB);
//   }
  
//   /* allocate/validate string to hold all values (+1 to null-terminate) */
//   char *apstr = (char *)calloc(1, sizeof *apstr * len + 1);
//   if (!apstr) {
//       Serial.printf("%s() error: virtual memory allocation failed.\n", __func__);
//   }

//   /* write/validate struct values to apstr */
//   if (number > 25) {
//     if (snprintf (apstr, len + 1, "%s%s", varA, varB) > len + 1)
//     {
//         Serial.printf("%s() error: snprintf returned truncated result (2 letters).\n", __func__);
//         return NULL;
//     }
//   } else
//   {
//     if (snprintf (apstr, len + 1, "%s", varB) > len + 1)
//     {
//         Serial.printf("%s() error: snprintf returned truncated result (1 letter).\n", __func__);
//         return NULL;
//     }
//   }

//   return apstr;
// }

// void checkParameters() {

// }

// void setupParameters() {
//   /*
//    * Enter Critical Zone
//    */
//   portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
//   taskENTER_CRITICAL(&myMutex);

//   bool openNVS = prefs.begin("logger");

//   if(openNVS) {
//     Serial.println(F("Open non-volatile storage"));
//   } else
//   {
//     Serial.println(F("setupParameters fail!"));
//   }
  
//   for (byte i = 0; i < MAX_PARAM; i++) {
//     parameters[i] = prefs.getInt(numberToLabel(i));
//   }
//   checkParameters();

//   /*
//    * Exit Critical Zone
//    */
//   taskEXIT_CRITICAL(&myMutex);

//   prefs.end();
// }

uint32_t getLastNextEntryID() {
  /*
   * Enter Critical Zone
   */
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  taskENTER_CRITICAL(&myMutex);

  bool openNVS = prefs.begin("logger");

  if(openNVS) {
    // Serial.println(F("Open non-volatile storage"));
  } else
  {
    Serial.println(F("getLastNextEntryID fail!"));
  }

  /*****************************************************************************
    Reading Old Sequence
  *****************************************************************************/
  // Obtain length and create buffer
  size_t schLen32 = prefs.getBytesLength("nextEntryID");
  if(schLen32 == 0) {
    Serial.println(F("No bytes stored in nextEntryID"));
  } else {
    // Serial.println(F("bytes stored in nextEntryID:"));
    // Serial.println(schLen32);
  }

  sNextEntryID_t *pLogsNextEntryID = (sNextEntryID_t *)calloc(schLen32, sizeof(sNextEntryID_t));

  size_t checkRead = prefs.getBytes("nextEntryID", pLogsNextEntryID, schLen32);

  /*
   * Exit Critical Zone
   */
  taskEXIT_CRITICAL(&myMutex);

  prefs.end();

  uint32_t entryID = pLogsNextEntryID[schLen32/4 - 1].nextEntryID;
  return entryID;
}

#endif