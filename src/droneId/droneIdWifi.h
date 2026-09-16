#ifndef _DRONE_ID_WIFI_H
#define _DRONE_ID_WIFI_H

#include <Arduino.h>

/* Remote ID over Wi-Fi: both of it.

   The Beacon method puts a message pack in a vendor specific information
   element of an ordinary 802.11 beacon, under the ASD-STAN identifier
   FA:0B:BC. The NAN method sends a Neighbor Awareness Networking service
   discovery frame - an action frame to the NAN network address, carrying the
   Open Drone ID service. An aircraft may use either, and DJI's standards mode
   uses the beacon.

   The radio is put in promiscuous mode and never associates with anything, so
   this board cannot also be a Wi-Fi station: the station role pins the channel
   to its access point's. That is why the drone tracker is a board kind of its
   own rather than a flag on an existing one.

   What it cannot hear is 5 GHz. Channel 149 is the other social channel and
   Beacon on 5 GHz is a mandatory-alternative transport in Europe, so an
   aircraft that chose it is invisible here - not because of this code but
   because the ESP32-S3 has a 2.4 GHz radio. It takes a second receiver, and
   the console says so rather than leaving it to be discovered. */

void droneIdWifiBegin();

/* Called every loop: opens the receiver when this is the Wi-Fi window and
   closes it when it is not. With the mode left at none and the receiver shut,
   Wi-Fi reports itself idle to the coexistence arbiter, which then gives the
   radio to Bluetooth - which is the whole mechanism of the time slice. */
void droneIdWifiListen(boolean listening);

boolean droneIdWifiListening();
uint8_t droneIdWifiChannel();

/* Every 802.11 management frame the sniffer has been handed: the number that
   says the receiver is running when no drone is up. */
uint32_t droneIdWifiFrames();

/* True when the radio came up at all. A board whose Wi-Fi refused to start
   still watches Bluetooth, and (di) says which half is running. */
boolean droneIdWifiReady();

#endif
