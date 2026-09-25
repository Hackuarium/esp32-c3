#ifndef _DRONE_ID_MESH_H
#define _DRONE_ID_MESH_H

#include <Arduino.h>

/* What a watching post on a far fence says over LoRa: a summary of its drone
   table every (K) seconds, broadcast as DATA so the bridge prints it and the
   host stores it. A bridge sends none of it - it reports down its own cable
   at full precision (droneIdFeed.h), and spending a duty cycle to tell the
   host what the host already holds the other end of is waste.

   The table keeps one row per transmitter; the air gets one HANDLE per UAS ID.
   Up to four transmitters of one aircraft collapse into a record that carries
   the transports as a bitmask and a conflict flag when two of them disagree on
   where it is - so the merge the console refuses is made only here, announced
   in the record, and the evidence against it travels with it.

   Per tick, in this order:

     IDENT   a new handle, a newly arrived Operator ID or classification, a
             five minute keepalive, or (df) asking
     PILOT   the operator's first position, a move past (M), or a keepalive
     TRACK   every aircraft heard in the last 15 s, (L) to a frame - with the
             position bit clear for one that has sent no Location message

   An aircraft that has not yet sent its Basic ID has no UAS ID to key on and is
   not reported; its identity arrives within a second or two on every transport.

   Called from the mesh task, which already holds the radio. Every send goes
   through the governor, which drops what the budget cannot pay for - (dm) says
   how many. */

#if defined(THR_DRONE_ID) && defined(THR_LORA_MESH)

void droneIdMeshService();

/* (dm): the cadence, what it costs, what was sent; dm5 sets (K) and prices it */
void droneIdMeshCommand(const char* paramValue, Print* output);

/* (df3): re-send the IDENT of handle 3 on the next pass, for a host that holds
   a TRACK it cannot name */
void droneIdMeshRequestIdent(const char* paramValue, Print* output);

#endif

#endif
