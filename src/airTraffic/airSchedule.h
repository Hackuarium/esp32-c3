#ifndef _AIR_SCHEDULE_H
#define _AIR_SCHEDULE_H

#include <stdint.h>

#include "airProtocols.h"

/* Which setting one radio listens with, and when.

   It cannot listen to everything at once: FLARM, OGN and the ADS-L M-band are
   one modulation on two channels with three sync words, FANET is LoRa, and the
   ADS-L O-band is a third frequency. What makes one radio worth having is that
   in Europe the M-band is a timetable rather than a lottery. Every second, from
   the UTC second, FLARM and OGN transmit in two slots:

     slot 0   400 to  800 ms   FLARM on 868.2, OGN on 868.4
     slot 1   800 to 1200 ms   FLARM on 868.4, OGN on 868.2

   (the European plan of the OGN tracker's frequency hopping: channel = slot
   XOR protocol, where the rest of the world hops pseudo-randomly). So with the
   time known, one radio can follow FLARM through both slots, and FANET - which
   transmits whenever the channel is free - gets the 200 to 400 ms nobody
   slotted uses. ADS-L M-band frames are heard wherever the radio happens to be
   on the M-band: the sync word it listens with there catches both. That the
   timetable holds on the air is what the first capture has to confirm, which
   is why every frame is reported with the millisecond it arrived at.

   What one radio cannot do is follow FLARM and OGN in the same slot - they are
   on opposite channels - so when both are asked for, OGN gets one second in
   AIR_SHARE_TURN and FLARM the rest; the O-band takes the gap from FANET the
   same way. FLARM leads because it is what most gliders, and many helicopters
   and light aircraft, carry in the Alps. A second radio on the same board is
   what ends the compromise, and the schedule is per radio so that it can be
   added.

   Without the time there is no timetable to follow, so every enabled setting
   takes turns of equal length. */

/* The windows, in milliseconds after the UTC second. */
#define AIR_GAP_START_MS 200
#define AIR_SLOT0_START_MS 400
#define AIR_SLOT1_START_MS 800

/* One cycle in this many goes to the minor of two settings that want the same
   window: OGN against FLARM in the slots, the O-band against FANET in the
   gap. */
#define AIR_SHARE_TURN 4

/* The setting for the moment millisInSecond after the start of the UTC second
   secondOfDay. The cycle of a second runs from 200 ms into it to 200 ms into
   the next, because slot 1 straddles the boundary. */
AirListen airListenAtUtc(uint8_t mask, uint32_t secondOfDay,
                         uint16_t millisInSecond);

/* The setting for turn number `turn` when the time is unknown: the enabled
   settings in table order, round and round. */
AirListen airListenRotating(uint8_t mask, uint32_t turn);

#endif
