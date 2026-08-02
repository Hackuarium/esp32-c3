#ifndef _LORA_MESH_H
#define _LORA_MESH_H

#include <Arduino.h>
#include "lora/loraFrame.h"

/* The radio and the shared tables belong to TaskLoraMesh; anything reaching
   them from the serial task has to hold this first. The mutex is recursive, so
   nesting a send inside a held section is safe. */
boolean loraMeshTake(Print* output);
void loraMeshGive();

/* the address this node answers to, 1 to 254 */
uint8_t loraMeshAddress();

boolean loraMeshHasKey();

/* Queues a frame for transmission. Broadcasts and unanswered types leave right
   away; a type that expects an ACK (CMD, DATA_ACK) goes through the escalation
   ladder, which retries direct before it asks the mesh to relay. Only one
   acknowledged request is in flight at a time. */
boolean loraMeshSend(uint8_t destination,
                     uint8_t type,
                     const uint8_t* body,
                     uint8_t bodyLength,
                     Print* output);

/* Encodes count of this node's own parameters as a SET body. Used both by the
   copy command and by the answer to a remote read. Returns the body length, or
   0 if the range is invalid or will not fit. */
uint8_t loraMeshEncodeLocalParameters(uint8_t firstParameter,
                                      uint8_t count,
                                      uint8_t* out,
                                      uint8_t outSize);

/* Prints a SET-shaped body as "Node 42: C=10 D=20". */
void loraMeshReportParameters(uint8_t source,
                              const uint8_t* body,
                              uint8_t bodyLength);

/* Reads one parameter out of a SET-shaped body, or ERROR_VALUE when the block
   does not cover that index. */
int16_t loraMeshParameterFromBody(const uint8_t* body,
                                  uint8_t bodyLength,
                                  uint8_t number);

void loraMeshPrintInfo(Print* output);
void loraMeshPrintPeers(Print* output);

/* Applies a received CMD body. Returns a LORA_STATUS_/LORA_REASON_ code, which
   is what travels back in the ACK or NACK. */
uint8_t loraMeshApplyCommand(const uint8_t* body, uint8_t bodyLength);

/* Reports a received DATA body on the serial console. */
void loraMeshReportData(uint8_t source, const uint8_t* body, uint8_t bodyLength);

/* Broadcasts the parameter block selected by PARAM_LORA_BROADCAST_* as DATA.
   Called by the mesh task every PARAM_LORA_INTERVAL_SECONDS. */
void loraMeshBroadcastParameters();

/* re-reads the address and the group key from NVS after the serial menu has
   changed one of them, so a node can be provisioned without a reboot */
void loraMeshReloadIdentity();

void taskLoraMesh();
void processLoraCommand(char command, char* paramValue, Print* output);
void processLoraMeshSetCommand(char* paramValue, Print* output);
void processLoraMeshCopyCommand(char* paramValue, Print* output);

#endif
