#include <string.h>
#include <unity.h>

#include "loraRelayPolicy.h"

/* Which frames a bridge receipts, what a repeater waits for before carrying a
   frame further, and which replies it carries back. Getting any of these wrong
   does not break the mesh visibly - it only repeats frames the bridge already
   has, or floods replies - so each rule is pinned here.

   Runs on the host: `pio test -e native`. */

#define BRIDGE 3
#define WATCHER 7
#define RELAY 8

void setUp(void) {}

void tearDown(void) {}

static LoraFrame frameOf(uint8_t type,
                         uint8_t source,
                         uint8_t destination,
                         uint8_t budget,
                         uint8_t hops) {
  LoraFrame frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = type;
  frame.source = source;
  frame.destination = destination;
  frame.budget = budget;
  frame.hops = hops;
  frame.counter = 0x1234;
  return frame;
}

/* an ACK back to `destination` answering `counter` */
static LoraFrame replyOf(uint8_t type, uint8_t destination, uint32_t counter) {
  LoraFrame frame = frameOf(type, BRIDGE, destination, 1, 0);
  frame.body[0] = (uint8_t)(counter >> 16);
  frame.body[1] = (uint8_t)(counter >> 8);
  frame.body[2] = (uint8_t)counter;
  frame.body[3] = LORA_STATUS_OK;
  frame.bodyLength = 4;
  return frame;
}

static void test_a_drone_report_heard_direct_is_receipted(void) {
  LoraFrame report =
      frameOf(LORA_TYPE_DATA, WATCHER, LORA_ADDRESS_BROADCAST, 2, 0);
  TEST_ASSERT_TRUE(loraWantsReceipt(&report, BRIDGE));
  // one relay later it can still travel one more hop
  report.hops = 1;
  TEST_ASSERT_TRUE(loraWantsReceipt(&report, BRIDGE));
}

static void test_a_frame_that_cannot_travel_further_needs_no_receipt(void) {
  LoraFrame report =
      frameOf(LORA_TYPE_DATA, WATCHER, LORA_ADDRESS_BROADCAST, 2, 2);
  TEST_ASSERT_FALSE(loraWantsReceipt(&report, BRIDGE));
  // the bridge's own HELLO, and any other budget 0 frame
  LoraFrame hello =
      frameOf(LORA_TYPE_HELLO, WATCHER, LORA_ADDRESS_BROADCAST, 0, 0);
  TEST_ASSERT_FALSE(loraWantsReceipt(&hello, BRIDGE));
}

static void test_a_relayable_hello_is_receipted(void) {
  LoraFrame hello =
      frameOf(LORA_TYPE_HELLO, WATCHER, LORA_ADDRESS_BROADCAST, 2, 0);
  TEST_ASSERT_TRUE(loraWantsReceipt(&hello, BRIDGE));
}

static void test_only_what_is_meant_for_the_host_is_receipted(void) {
  // data for this bridge, yes; for another node, no
  LoraFrame toBridge = frameOf(LORA_TYPE_DATA, WATCHER, BRIDGE, 2, 0);
  TEST_ASSERT_TRUE(loraWantsReceipt(&toBridge, BRIDGE));
  LoraFrame toOther = frameOf(LORA_TYPE_DATA, WATCHER, 42, 2, 0);
  TEST_ASSERT_FALSE(loraWantsReceipt(&toOther, BRIDGE));
  // a broadcast command is meant for every node: a receipt would stop it
  LoraFrame command =
      frameOf(LORA_TYPE_CMD, WATCHER, LORA_ADDRESS_BROADCAST, 2, 0);
  TEST_ASSERT_FALSE(loraWantsReceipt(&command, BRIDGE));
  // replies are never answered
  LoraFrame ack = replyOf(LORA_TYPE_ACK, WATCHER, 0x1234);
  ack.budget = 2;
  TEST_ASSERT_FALSE(loraWantsReceipt(&ack, BRIDGE));
}

static void test_what_a_repeater_waits_for(void) {
  LoraFrame report =
      frameOf(LORA_TYPE_DATA, WATCHER, LORA_ADDRESS_BROADCAST, 2, 0);
  TEST_ASSERT_EQUAL(LORA_REPLY_ACK, loraExpectedReply(&report));
  LoraFrame hello =
      frameOf(LORA_TYPE_HELLO, WATCHER, LORA_ADDRESS_BROADCAST, 2, 0);
  TEST_ASSERT_EQUAL(LORA_REPLY_ACK, loraExpectedReply(&hello));

  LoraFrame set = frameOf(LORA_TYPE_CMD, BRIDGE, 42, 2, 0);
  set.body[0] = LORA_CMD_SET_PARAMETERS_INT8;
  set.bodyLength = 3;
  TEST_ASSERT_EQUAL(LORA_REPLY_ACK, loraExpectedReply(&set));

  LoraFrame get = frameOf(LORA_TYPE_CMD, BRIDGE, 42, 2, 0);
  get.body[0] = LORA_CMD_GET_PARAMETERS;
  get.bodyLength = 3;
  TEST_ASSERT_EQUAL(LORA_REPLY_RESP, loraExpectedReply(&get));

  LoraFrame broadcastSet =
      frameOf(LORA_TYPE_CMD, BRIDGE, LORA_ADDRESS_BROADCAST, 2, 0);
  TEST_ASSERT_EQUAL(LORA_REPLY_NONE, loraExpectedReply(&broadcastSet));

  LoraFrame confirmed = frameOf(LORA_TYPE_DATA_ACK, WATCHER, BRIDGE, 2, 0);
  TEST_ASSERT_EQUAL(LORA_REPLY_ACK, loraExpectedReply(&confirmed));
  confirmed.destination = LORA_ADDRESS_BROADCAST;
  TEST_ASSERT_EQUAL(LORA_REPLY_NONE, loraExpectedReply(&confirmed));

  LoraFrame ack = replyOf(LORA_TYPE_ACK, WATCHER, 0x1234);
  TEST_ASSERT_EQUAL(LORA_REPLY_NONE, loraExpectedReply(&ack));
}

static void test_a_reply_echoes_the_counter_it_answers(void) {
  LoraFrame ack = replyOf(LORA_TYPE_ACK, WATCHER, 0xABCDEF);
  uint32_t echoed = 0;
  TEST_ASSERT_TRUE(loraFrameIsReply(&ack));
  TEST_ASSERT_TRUE(loraReplyEcho(&ack, &echoed));
  TEST_ASSERT_EQUAL_HEX32(0xABCDEF, echoed);

  LoraFrame response = replyOf(LORA_TYPE_RESP, WATCHER, 0x000102);
  TEST_ASSERT_TRUE(loraReplyEcho(&response, &echoed));
  TEST_ASSERT_EQUAL_HEX32(0x000102, echoed);

  LoraFrame report =
      frameOf(LORA_TYPE_DATA, WATCHER, LORA_ADDRESS_BROADCAST, 2, 0);
  TEST_ASSERT_FALSE(loraFrameIsReply(&report));
  TEST_ASSERT_FALSE(loraReplyEcho(&report, &echoed));

  LoraFrame truncated = replyOf(LORA_TYPE_NACK, WATCHER, 0x1234);
  truncated.bodyLength = 2;
  TEST_ASSERT_FALSE(loraReplyEcho(&truncated, &echoed));
}

static void test_a_reply_goes_back_only_through_a_node_that_relayed(void) {
  LoraRelayedMemory memory;
  loraRelayedForget(&memory);
  LoraFrame receipt = replyOf(LORA_TYPE_ACK, WATCHER, 0x1234);
  TEST_ASSERT_FALSE(loraReplyOnPath(&memory, &receipt));

  loraRelayedRemember(&memory, WATCHER, 0x1234);
  TEST_ASSERT_TRUE(loraReplyOnPath(&memory, &receipt));

  // the same counter from another source is another message
  LoraFrame toRelay = replyOf(LORA_TYPE_ACK, RELAY, 0x1234);
  TEST_ASSERT_FALSE(loraReplyOnPath(&memory, &toRelay));
  // and something that is not a reply is never a reply on a path
  LoraFrame report =
      frameOf(LORA_TYPE_DATA, BRIDGE, WATCHER, 2, 0);
  report.body[0] = 0x00;
  report.body[1] = 0x12;
  report.body[2] = 0x34;
  report.bodyLength = 3;
  TEST_ASSERT_FALSE(loraReplyOnPath(&memory, &report));
}

static void test_the_path_matches_on_the_24_bits_a_reply_echoes(void) {
  LoraRelayedMemory memory;
  loraRelayedForget(&memory);
  // a counter past 2^24: only its low 24 bits come back in the reply
  loraRelayedRemember(&memory, WATCHER, 0x01ABCDEFul);
  LoraFrame receipt = replyOf(LORA_TYPE_ACK, WATCHER, 0xABCDEF);
  TEST_ASSERT_TRUE(loraReplyOnPath(&memory, &receipt));
}

static void test_the_memory_keeps_the_most_recent_messages(void) {
  LoraRelayedMemory memory;
  loraRelayedForget(&memory);
  for (uint32_t counter = 1; counter <= LORA_RELAYED_MEMORY_SIZE + 1;
       counter++) {
    loraRelayedRemember(&memory, WATCHER, counter);
  }
  LoraFrame oldest = replyOf(LORA_TYPE_ACK, WATCHER, 1);
  TEST_ASSERT_FALSE(loraReplyOnPath(&memory, &oldest));
  LoraFrame second = replyOf(LORA_TYPE_ACK, WATCHER, 2);
  TEST_ASSERT_TRUE(loraReplyOnPath(&memory, &second));
  LoraFrame newest =
      replyOf(LORA_TYPE_ACK, WATCHER, LORA_RELAYED_MEMORY_SIZE + 1);
  TEST_ASSERT_TRUE(loraReplyOnPath(&memory, &newest));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_a_drone_report_heard_direct_is_receipted);
  RUN_TEST(test_a_frame_that_cannot_travel_further_needs_no_receipt);
  RUN_TEST(test_a_relayable_hello_is_receipted);
  RUN_TEST(test_only_what_is_meant_for_the_host_is_receipted);
  RUN_TEST(test_what_a_repeater_waits_for);
  RUN_TEST(test_a_reply_echoes_the_counter_it_answers);
  RUN_TEST(test_a_reply_goes_back_only_through_a_node_that_relayed);
  RUN_TEST(test_the_path_matches_on_the_24_bits_a_reply_echoes);
  RUN_TEST(test_the_memory_keeps_the_most_recent_messages);
  return UNITY_END();
}
