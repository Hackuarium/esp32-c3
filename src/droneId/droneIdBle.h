#ifndef _DRONE_ID_BLE_H
#define _DRONE_ID_BLE_H

#include <Arduino.h>

/* Remote ID over Bluetooth: both of it.

   An aircraft broadcasts the same messages two ways, and in the United States
   it is required to do both at once. Bluetooth 4 legacy advertising carries
   exactly one 25-byte message per advertisement, because the service data
   structure fills all 31 bytes a legacy payload has. Bluetooth 5 Long Range
   uses extended advertising on the Coded PHY and carries a message pack of up
   to nine. The two advertising sets use different addresses, so the same
   aircraft arrives here as two transmitters - which is why the table keeps
   them apart and says which rows agree.

   Both are received by one scan: CONFIG_BT_NIMBLE_EXT_ADV makes NimBLE call
   ble_gap_ext_disc() with the same parameters for the uncoded and the coded
   PHY, so 1M and Coded are scanned alike and there is nothing to configure. */

void droneIdBleBegin();

/* Called every loop: starts the scan when this is the Bluetooth window and
   stops it when it is not, so that Wi-Fi gets a slice the coexistence arbiter
   cannot take back. Calling it repeatedly is the point - under extended
   advertising a scan started with duration 0 is not endless, it lasts about
   524 seconds and then completes, so something has to notice. */
void droneIdBleListen(boolean listening);

boolean droneIdBleScanning();

/* Extended advertisements the controller split and the library did not
   reassemble - a nine-message pack, which no transmitter is known to send.
   They never reach the decoder, so this is the only place they are counted. */
/* Every advertisement heard, whether or not it was Remote ID: the number
   that says the scanner is running when nothing is flying. */
uint32_t droneIdBleAdvertisements();

uint32_t droneIdBleTruncated();

#endif
