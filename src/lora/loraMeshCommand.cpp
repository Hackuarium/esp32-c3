#include "config.h"
#ifdef THR_LORA_MESH
#include <string.h>

#include "lora/loraBridge.h"
#include "lora/loraMesh.h"
#include "lora/loraPeers.h"
#include "params.h"

void loraMeshReloadIdentity();

void loraMeshReportData(uint8_t source,
                        const uint8_t* body,
                        uint8_t bodyLength) {
  if (bodyLength < 1) {
    return;
  }
  /* a DATA body is a SET body that is reported instead of applied */
  if (body[0] == LORA_CMD_SET_PARAMETERS_INT8 ||
      body[0] == LORA_CMD_SET_PARAMETERS_INT16) {
    loraMeshReportParameters(source, body, bodyLength);
    return;
  }

  Print* json = loraBridgeBegin("data");
  loraBridgeInt(json, "src", source);
  loraBridgeInt(json, "opcode", body[0]);
  loraBridgeInt(json, "length", bodyLength - 1);
  loraBridgeEnd(json);
  if (json != NULL) {
    return;
  }

  Serial.print(F("DATA from "));
  Serial.print(source);
  Serial.print(F(", opcode "));
  Serial.print(body[0]);
  Serial.print(F(", "));
  Serial.print(bodyLength - 1);
  Serial.println(F(" byte(s)"));
}

/* The periodic broadcast: the same block a manual (ac) would copy, sent as DATA
   so every neighbour prints it rather than overwriting its own parameters. */
void loraMeshBroadcastParameters() {
  int16_t firstParameter = getParameter(PARAM_LORA_BROADCAST_FIRST_PARAMETER);
  int16_t count = getParameter(PARAM_LORA_BROADCAST_NB_PARAMETERS);
  if (firstParameter < 0 || firstParameter >= MAX_PARAM || count < 1 ||
      count > LORA_MAX_PARAMETERS_PER_FRAME) {
    return;
  }

  uint8_t body[LORA_MAX_BODY_SIZE];
  uint8_t length = loraMeshEncodeLocalParameters(
      (uint8_t)firstParameter, (uint8_t)count, body, LORA_MAX_BODY_SIZE);
  if (length == 0) {
    Serial.println(F("Broadcast window out of bounds, check AE and AF"));
    return;
  }

  loraMeshSend(LORA_ADDRESS_BROADCAST, LORA_TYPE_DATA, body, length,
               loraMeshSilent());
}

static void printLoraMeshHelp(Print* output) {
  output->println(F("(ai) info - address, key, counter, peers"));
  output->println(F("(ak) set the AES128 group key (hex 32 chars)"));
  output->print(F("(an) set this node address, 1 to "));
  output->println(LORA_ADDRESS_MAX);
  output->println(F("(ap) peer table"));
  output->println(F("(ah) broadcast a HELLO now"));
  output->println(F("(az) forget every peer"));
  output->println(F("(ax) set or read a parameter over the mesh"));
  output->println(F("    axA123     set A to 123 on every node"));
  output->println(F("    ax42:A123  set A on node 42, wait for an ACK"));
  output->println(F("    ax42:A     read A from node 42"));
  output->println(F("    axC10,20   set C and D on every node"));
  output->println(F("(ag) read a block of parameters back in one exchange"));
  output->println(F("    ag42:DA8   read DA to DH from node 42, one answer"));
  output->println(F("(ac) copy this node's own parameters outwards"));
  output->println(F("    ac42:C6    copy C to H to node 42"));
  output->println(F("    ac:C6      copy C to H to every node"));
  output->println(F("settings, read with (ai), written as e.g. DE9"));
  printParameterHelp(output, PARAM_LORA_ROLE,
                     F("0 = endpoint, 1 = repeater, 2 = bridge (JSON)"));
  printParameterHelp(output, PARAM_LORA_TTL,
                     F("hops allowed for frames sent from here, 0 to 7"));
  printParameterHelp(output, PARAM_LORA_SPREADING_FACTOR,
                     F("spreading factor 7-12, other value = SF12"));
  printParameterHelp(output, PARAM_LORA_FREQUENCY,
                     F("carrier in 25 kHz over 400 MHz, 18781 = 869.525"));
  printParameterHelp(output, PARAM_LORA_BANDWIDTH,
                     F("bandwidth kHz: 250, 125 or 62 (= 62.5)"));
  printParameterHelp(output, PARAM_LORA_HELLO_SECONDS,
                     F("seconds between HELLOs, 0 = never, 10800 = 3 h"));
  printParameterHelp(output, PARAM_LORA_INTERVAL_SECONDS,
                     F("seconds between parameter broadcasts, 0 = never"));
  printParameterHelp(output, PARAM_LORA_BROADCAST_FIRST_PARAMETER,
                     F("first parameter of the broadcast block"));
  printParameterHelp(output, PARAM_LORA_BROADCAST_NB_PARAMETERS,
                     F("how many parameters it carries, e.g. 6 from G"));
  output->println(F("    the duty cycle follows the frequency: 1% in"));
  output->println(F("    865-868.6 and 869.7-870, 10% in 869.4-869.65,"));
  output->println(F("    0.1% everywhere else in the 863-870 band"));
}

void processLoraCommand(char command, char* paramValue, Print* output) {
  /* the whole command runs under the mutex: it touches the radio, the peer
     table and the shared blob buffer in params.cpp */
  if (!loraMeshTake(output)) {
    return;
  }

  switch (command) {
    case 'i':
      loraMeshPrintInfo(output);
      break;
    case 'k':
      if (!checkParameterLength(paramValue, 32, output)) {
        deleteParameter("mesh.key");
        loraMeshReloadIdentity();
        output->println(F("Key deleted"));
        break;
      }
      setBlobParameterFromHex("mesh.key", paramValue);
      loraMeshReloadIdentity();
      output->println(paramValue);
      break;
    case 'n': {
      int32_t address = atol(paramValue);
      if (address < 1 || address > LORA_ADDRESS_MAX) {
        output->print(F("Address must be 1 to "));
        output->println(LORA_ADDRESS_MAX);
        break;
      }
      setNVSParameterInt32("mesh.address", address);
      loraMeshReloadIdentity();
      output->println(address);
      break;
    }
    case 'p':
      loraMeshPrintPeers(output);
      break;
    case 'h':
      loraMeshSend(LORA_ADDRESS_BROADCAST, LORA_TYPE_HELLO, NULL, 0, output);
      break;
    case 'z':
      loraPeerClear();
      output->println(F("Peer table cleared"));
      break;
    case 'x':
      processLoraMeshSetCommand(paramValue, output);
      break;
    case 'g':
      processLoraMeshGetCommand(paramValue, output);
      break;
    case 'c':
      processLoraMeshCopyCommand(paramValue, output);
      break;
    default:
      printLoraMeshHelp(output);
      break;
  }

  loraMeshGive();
}

#endif
