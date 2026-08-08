#ifndef _CONFIG_LORA_MESH_PARAMS_H
#define _CONFIG_LORA_MESH_PARAMS_H

/* Everything a board needs to join the private LoRa mesh, in one include.

   The mesh code never refers to a parameter by number, only by name, so any
   board could in principle pick its own slots - but then every board picks
   different ones and a block copied from one config collides with the outputs
   or the gates of the next. This header reserves a range instead: 104 to 113
   (DA to DJ) is free in every config in this repository, so a board joins by
   including this file and calling taskLoraMesh(), with nothing to renumber.

     #include "./configLoraMeshParams.h"

   Include it before the board declares its own MAX_PARAM and it supplies 114.
   Include it after, and a board whose MAX_PARAM is too small to hold the block
   fails to compile rather than writing past parameters[].

   A slot is left spare so the mesh can grow without moving what is already
   stored in NVS: a parameter is persisted under its letter, so renumbering one
   silently hands a node the value of something else. */

/* enables the (a) serial menu, implemented by src/lora/loraMeshCommand.cpp */
#ifndef THR_LORA
#define THR_LORA 1
#endif
/* enables the mesh itself: the task, the codec and the (ax)/(ac) commands */
#ifndef THR_LORA_MESH
#define THR_LORA_MESH 1
#endif

/* 0 = endpoint (originates and consumes frames), 1 = repeater (does that and
   also forwards frames that are not addressed to it), 2 = bridge (an endpoint
   whose serial port is a machine readable feed - see src/lora/loraBridge.h) */
#define PARAM_LORA_ROLE 104  // DA
/* hops allowed for the frames this node originates, 0 = never relayed */
#define PARAM_LORA_TTL 105  // DB
/* carrier as a count of 25 kHz steps above 400 MHz, so 18736 is 868.4 MHz -
   the default - and 18781 is 869.525. 25 kHz is the raster of sub-band P and
   divides every EU868 and US915 channel; counting from 400 MHz keeps the
   SX1262's whole 150-960 MHz range inside a signed int16, so the slot stays an
   ordinary parameter. Firmware before 2026-08 counted 0.1 MHz here; those
   values, 1500 to 9600, are refused rather than converted, so a node that was
   never reset runs on the default instead of on 617.1 MHz. */
#define PARAM_LORA_FREQUENCY 106  // DC
/* bandwidth in kHz: 250, 125 or 62 (meaning 62.5) - 125 on the default carrier,
   the widest that fits between the LoRaWAN channels at 868.3 and 868.5 */
#define PARAM_LORA_BANDWIDTH 107  // DD
/* 7 to 12, anything else falls back to SF9 */
#define PARAM_LORA_SPREADING_FACTOR 108  // DE
/* seconds between two periodic parameter broadcasts, 0 = never */
#define PARAM_LORA_INTERVAL_SECONDS 109  // DF
/* Which block of parameters that broadcast carries, the same first/count shape
   as the logging window and as the (ac) command: it starts at
   PARAM_LORA_BROADCAST_FIRST_PARAMETER and covers
   PARAM_LORA_BROADCAST_NB_PARAMETERS slots. */
#define PARAM_LORA_BROADCAST_FIRST_PARAMETER 110  // DG
#define PARAM_LORA_BROADCAST_NB_PARAMETERS 111    // DH
/* seconds between two automatic HELLOs, 0 = never. A HELLO only proves a direct
   link, so it is worth little airtime: three hours keeps the peer table alive
   without competing with whatever the node actually has to say */
#define PARAM_LORA_HELLO_SECONDS 112  // DI
#define LORA_HELLO_SECONDS_DEFAULT 10800

/* 113 (DJ) is reserved for the mesh */
#define LORA_MESH_MAX_PARAM 114

#define LORA_ROLE_ENDPOINT 0
#define LORA_ROLE_REPEATER 1
#define LORA_ROLE_BRIDGE 2

#ifdef MAX_PARAM
#if MAX_PARAM < LORA_MESH_MAX_PARAM
#error "Raise MAX_PARAM to 114: the LoRa mesh reserves parameters 104 to 113"
#endif
#else
#define MAX_PARAM LORA_MESH_MAX_PARAM
#endif

#endif
