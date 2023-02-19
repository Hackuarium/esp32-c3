/*******************************************************************************
  This thread takes care of the logs and manage the time and its synchronisation
  The thread write the logs at a definite fixed interval of time in the
  SST25VF064 chip The time synchronization works through the NTP protocol and our server
*******************************************************************************/
#include <params.h>
#ifdef THR_LOGGER

void TaskLogger(void*);
void taskLogger();

#endif
