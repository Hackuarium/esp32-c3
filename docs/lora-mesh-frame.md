# The LoRa mesh frame

Wire format and behaviour of the private mesh built in `src/lora/` and
`src/taskLoraMesh.cpp`. It is not LoRaWAN: there is no network server, no join
procedure and no device registry — one group, one AES-128 key, flooding with a
hop budget.

The implementation is the reference; this document describes it. The canonical
sources are [loraFrame.h](../src/lora/loraFrame.h) (constants and layout),
[loraFrame.cpp](../src/lora/loraFrame.cpp) (codec),
[taskLoraMesh.cpp](../src/taskLoraMesh.cpp) (radio, relaying, retries) and
[loraMeshParameters.cpp](../src/lora/loraMeshParameters.cpp) (bodies).

## Layout

```
 from(1) seal(3|4) | E( ctrl(1) src(1) dst(1) counter(3|4) body(0..48) route(2h) trailer(1) ) | mic(4)
 \__ clear, AAD __/     \_________________________ encrypted _________________________/
```

| Field     | Bytes              | Notes                                                       |
| --------- | ------------------ | ----------------------------------------------------------- |
| `from`    | 1                  | the node that transmitted **this copy**, 1–254              |
| `seal`    | 3 or 4             | a counter `from` never used for any other transmission      |
| `ctrl`    | 1                  | `ver(1) type(3) spare(4)`, bit 7 down to bit 0               |
| `src`     | 1                  | originator, 1–254                                           |
| `dst`     | 1                  | 1–254, or 255 for broadcast                                 |
| `counter` | 3 or 4             | the message, the same in every copy and every retry         |
| body      | 0–48               |                                                             |
| route     | 2 per recorded hop | `address(1) rssi(1)`, 0–4 entries                           |
| trailer   | 1                  | `budget(4) hops(4)`, high nibble first                      |
| `mic`     | 4                  | AES-128-CCM tag over everything before it                   |

Everything from `ctrl` to the trailer is one CCM plaintext, so the ciphertext
has the same length. Only `from` and `seal` can be read without the key.

Minimum overhead is **15 bytes** (4-byte seal + 6-byte header + 1-byte
trailer + 4-byte tag), plus 2 for every recorded hop. `LORA_MAX_FRAME_SIZE` is
**73**.

### Counters

`seal` and `counter` share one encoding: big-endian, **3 bytes while the value
fits in 23 bits, 4 bytes with bit 7 of the first byte set once it does not**.
The remaining 31 bits are the value. A node draws both from one sequence, so
the widening happens once, for good, after about 8.4 million transmissions.

### `ctrl`

| Bit | Name   | Meaning                                  |
| --- | ------ | ---------------------------------------- |
| 7   | `ver`  | protocol version, **1**                  |
| 6–4 | `type` | frame type, see below                    |
| 3–0 | spare  | transmitted as 0                         |

A frame whose `ver` is not 1 is refused even when its tag verifies. Version 0
was the previous envelope, with the header in clear; its frames do not decode
here at all, and the reverse is equally true.

### Frame types

| Value | Name       | Body                      | Answered with                      |
| ----- | ---------- | ------------------------- | ---------------------------------- |
| 0     | `HELLO`    | flags [+ position]        | nothing                            |
| 1     | `DATA`     | SET-shaped                | nothing                            |
| 2     | `DATA_ACK` | SET-shaped                | `ACK` (unicast only)               |
| 3     | `ACK`      | counter echo + status     | —                                  |
| 4     | `CMD`      | opcode-led                | `ACK`/`NACK`, `RESP` for a read, both for a console command |
| 5     | `RESP`     | counter echo + SET-shaped or console text | —                  |
| 6     | `NACK`     | counter echo + status     | —                                  |
| 7     | `EXT`      | reserved                  | —                                  |

### Addresses

`0` means unset — a node with address 0 refuses to transmit. `255`
(`LORA_ADDRESS_BROADCAST`) is the broadcast address, so a node owns **1 to 254**.
Both address and group key live in NVS (`mesh.address`, `mesh.key`) and are set
from the serial menu with `(an)` and `(ak)`; changing either takes effect
without a reboot.

## Cryptography

AES-128-CCM with a 13-byte nonce and a 4-byte tag, from mbedTLS. One primitive
gives confidentiality and authenticity in a single pass — there is no second key
and no separate CMAC.

The **nonce** is built from the two clear fields, never transmitted separately:

```
nonce[0]    = 0x01
nonce[1]    = from
nonce[2:6]  = seal, the full 32-bit value, big-endian
nonce[6:13] = 0
```

**Every transmission is sealed afresh by whoever makes it** — the origin, each
retry, each relay — under its own address and a counter it has never used. That
is what lets a relay record its passage and a retry raise the budget: both
change the plaintext, and encrypting two plaintexts under one nonce is the one
misuse CCM does not survive. Nonces stay unique across the mesh because
addresses are, and within a node because `seal` never repeats (see the counter
reservation below).

The first byte keeps this format's nonces apart from the previous one's, which
began with a `ctrl` byte whose three low bits were always clear. A mesh that
keeps its key and its counters across the upgrade therefore never meets a nonce
twice. The seal is always put in the nonce as the full 32-bit value, so its
widening cannot collide with a nonce already used either.

The **additional authenticated data** is `from` and `seal` exactly as
transmitted. The nonce already holds their values; binding their encoding too
leaves no second spelling of a counter that would still verify.

So **nothing on the air is unauthenticated**: the route and the budget are
inside the ciphertext, and a relay can only change them by sealing a copy of
its own. A key holder can still claim anything, which is why
`LORA_TTL_MAX_ACCEPT`, not the budget, is what caps amplification.

What stays readable without the key is which node transmitted, how often, how
long its frames are and how strongly they arrive. A frame's type, its source
and destination, the message counter and the path are not.

## The plaintext, read from both ends

The plaintext is self-describing, which is why there is no length field and no
flag bit:

1. `ctrl`, `src` and `dst` are the first three bytes, and the counter's first
   byte says whether it takes 3 or 4 more;
2. the **last** byte gives `budget` (high nibble) and `hops` (low nibble);
3. the number of stored route entries is `min(hops, 4)`, so the route occupies
   `2 × min(hops, 4)` bytes immediately before it;
4. the body is whatever lies between.

A frame that outruns the 4-entry table keeps counting hops and stops recording
them, so `hops > 4` is how a truncated route announces itself — and `from` is
then the only record of the relay that transmitted the copy heard.

A route entry is `address(1) rssi(1)`, the dBm at which **that** relay heard the
frame (clamped to int8), so one reception carries the margin of every hop it
crossed. The last hop is deliberately absent — the receiver measures that one
itself, from `from`.

**The budget counts up, not down.** A frame travels while `hops < budget`, and a
`budget` of 0 means "do not relay". That, not the broadcast address, is the
direct-versus-flood switch. Escalating a retry from 0 to 2 hops changes the
plaintext, so the retry is sealed again under a new seal, keeping its counter.

## Bodies

Note the endianness split: **counter echoes are big-endian, parameter int16
values are little-endian.**

### CMD — set parameters

```
opcode(1) first(1) values(n)
```

| Opcode | Meaning                                                         |
| ------ | --------------------------------------------------------------- |
| `0x01` | `SET_PARAMETERS_INT8` — one signed byte per parameter           |
| `0x02` | `SET_PARAMETERS_INT16` — two bytes per parameter, little-endian |

The sender picks int8 when every value fits in `-128..127`, so the common case of
small settings costs one byte per parameter instead of two. `first` is the index
of the first parameter, in the same `A`=0, `C`=2, `AA`=26, `DA`=104 scheme the
serial console uses. A block is applied with `setAndSaveParameter`, so it
persists.

At most `LORA_MAX_PARAMETERS_PER_FRAME` = **20** parameters per frame — which is
exactly what fits a RESP: 3 echoed counter bytes + 2 header + 20 × 2 = 45 of the
48-byte body.

### CMD — get parameters

```
opcode(1)=0x03 first(1) count(1)
```

Answered with a `RESP`, not an `ACK`, because the caller wants the values rather
than a receipt. A GET addressed to the broadcast address is ignored — every node
answering at once is a response storm.

### CMD — console

```
opcode(1)=0x04 text(n)
```

The one opcode that does not name a parameter: `text` is the command an operator
would have typed on that node's own port — printable ASCII, lowercase verb
first, no terminator, at most `LORA_CONSOLE_MAX_TEXT` = **47** bytes. It is what
`ar42:pr1234` puts on the air, and the only way to reach a *verb* — a reboot, a
peer dump, a wifi scan — on a node nobody can plug a cable into.

It is answered **twice**: an `ACK` the moment it is queued, then a `RESP`
carrying what the command printed. Two answers rather than one because the
command runs *after* the receipt, not before — a node told to reboot never gets
to send a `RESP`, so the `ACK` is the only thing that can prove the frame
arrived at all. It also runs outside the receive path: a console command is a
whole console command, several of them block for seconds, and a few transmit.

Only one is queued at a time; a second arriving before the first has answered is
`NACK`ed with `0x04`, because the slot holds the request the `RESP` is addressed
to. A console command sent to the broadcast address is dropped without queueing.

### RESP

```
requestCounter low 24 bits(3, big-endian) | opcode(1) first(1) values(n)
requestCounter low 24 bits(3, big-endian) | opcode(1)=0x04 flags(1) text(n)
```

The counter echo lets the requester close its pending request, the same trick
`ACK` uses. The opcode says which of the two follows: a SET-shaped block for a
parameter read, or the console output for `0x04`.

A console reply is capped at `LORA_CONSOLE_MAX_REPLY` = **43** bytes, and
`flags` carries one bit, `LORA_CONSOLE_TRUNCATED` = `0x01`, saying it was cut
there. What a node prints is unbounded — the parameter dump alone is one line
per slot — while such a frame already costs 196 ms on the default profile and
1.4 s at SF12, so the answer is truncated to one frame rather than paged over a
channel with a duty cycle. The
bit exists because a reply cut at the frame boundary and a command that simply
had little to say are otherwise the same bytes, and reading the first as the
second is how an operator concludes a node answered when it only started to.

### ACK / NACK

```
requestCounter low 24 bits(3, big-endian) status(1)
```

| Status | Meaning                       |
| ------ | ----------------------------- |
| `0x00` | OK                                        |
| `0x01` | unknown command                           |
| `0x02` | bad body                                  |
| `0x03` | parameter range out of bounds             |
| `0x04` | busy — a console command is still queued  |

An `ACK` or `RESP` goes back with a budget equal to the `hops` the request
actually took — a measurement, rather than the guess a countdown would give.

### DATA

A `DATA` body **is a SET body byte for byte** — same opcode, same first index,
same values. The frame type is the whole difference: a `CMD` is applied by the
receiver, a `DATA` is only reported. That is why a tracker broadcasts its fix as
`DATA`; a broadcast SET would have every neighbour overwrite its own `G` with
the tracker's latitude.

Telemetry has therefore no frame type of its own: a node broadcasts the
parameter window `DG … DG+DH` every `DF` seconds. A GPS tracker is just
`taskGPS` writing the fix into `G`…`N` plus `DF20 DG6 DH8`. Adjacency is load
bearing — an int32 spread over two int16 slots only survives the trip because
both halves sit in the same run.

The window ends with satellites (`L`), HDOP × 100 (`M`) and the GGA fix quality
(`N`), because a coordinate alone cannot be weighted by whoever collects it:
nothing else in the block distinguishes a 4-satellite 2D fix from a
12-satellite one.

A board built with `BLE_SCAN` extends the same run by one: the median RSSI of
the Bluetooth device it was told to watch sits at `O`, immediately after the fix
quality, and the window becomes `DG6 DH9`. That placement is the whole reason it
is at 14 and not somewhere more convenient — a signal strength in a slot that
does not touch the fix would need a frame of its own, and the two readings only
mean something together. It costs 2 bytes a frame and, at SF9, no airtime at
all: 33 and 35 bytes fall in the same block of symbols.

**The broadcast is held while the fix is not current.** The coordinates keep
their last good value for ever once the receiver stops solving — `publishFix()`
writes only a location TinyGPSPlus calls valid — so a tracker taken indoors would
repeat the last place it saw the sky, every `DF` seconds, looking exactly like a
node standing there. `loraMeshBroadcastParameters()` therefore checks
`gpsHasCurrentFix()`: a position older than `GPS_FIX_MAX_AGE_MS` (30 s, about
thirty missed GGA sentences), or a GGA reporting quality 0, holds the frame and
prints `No GPS fix, holding the broadcast` once. It applies only when the window
covers `G`…`J`, so a board broadcasting something other than a fix is untouched,
and HELLO is never suppressed — a node with no position stays in the peer tables.

A drone watcher that is not a bridge adds three DATA bodies of its own —
`0x10` `TRACK`, `0x11` `PILOT` and `0x12` `IDENT` — specified in
[drone-mesh-forwarding.md](drone-mesh-forwarding.md). The firmware only encodes
them.

A `DATA` body whose first byte is neither `0x01` nor `0x02` is reported as an
opaque opcode and length — so a bridge prints a `data` line for a drone record,
and its `rx` line carries the whole body in hex for the host to decode.

### HELLO

```
flags(1) [latitude(4) longitude(4)]
```

| Bit | Meaning                                                   |
| --- | --------------------------------------------------------- |
| 0   | a position follows                                        |
| 1   | it is a current GPS fix; clear when placed by hand (`al`) |
| 2   | this node watches for drones (built with `THR_DRONE_ID`)  |
| 3   | this node relays what it hears (role `DA1`)               |
| 4   | this node is a bridge (role `DA2`)                        |
| 5–7 | reserved, sent as 0                                       |

Bit 4 is the only way a host learns of a bridge other than its own, which it
knows from `ai`. It matters beyond the label: a repeater drops its copy on any
bridge's receipt, so a second bridge feeding another host, or none, can keep
a report from ever reaching this one.

`latitude` and `longitude` are **int32, little-endian, degrees × 1e6** — the
scale of the fix in `G`…`J`, so a tracker announces its position exactly as
`taskGPS` stored it. The body is 1 byte without a position and 9 with one.

Its cadence is measured in hours — the default `DI` is **10800 s (3 h)** —
because a peer table costs airtime to maintain. One is also sent as soon as the
task starts, so a node that has just booted does not stay out of its
neighbours' peer tables for a whole period.

It travels with the default budget `DB`, like a report, so a node the bridge
does not hear directly still gets its position to the host; a bridge's
[receipt](#receipts-a-frame-the-bridge-has-is-not-repeated) keeps it from being
repeated when the bridge did hear it. A bridge's own HELLO keeps budget 0:
nobody receipts it, so every repeater around would carry it for nothing.

It also says where the node is, because a drone watcher on a fence has no GPS
and the host still has to place it — and to rebuild the `TRACK` coordinates it
relays, which travel modulo a window and are put back against the position of
the post that sent them. The position is, in this order:

1. a current GPS fix, with bit 1 set — only while `gpsHasCurrentFix()` says so,
   the test that also holds the telemetry broadcast, since the coordinates in
   `G`…`J` keep their last value for ever once the receiver stops solving;
2. a position placed by hand with `al46.5191,6.5668`, kept in NVS as `mesh.lat`
   and `mesh.lon`, with bit 1 clear;
3. none — the flags alone.

Bit 3 is the role, because nothing else on the air says it: a repeater only
shows up in a route once it has carried somebody's frame, and a host drawing
the network has to know which nodes extend it before any of them has. Older
firmware sends it clear, so a node that relays but has not been reflashed reads
as an endpoint until it is.

```
05 3cd3c502 90336400
│  │        └ longitude 6566800 = 6.566800
│  └ latitude 46519100 = 46.519100
└ position, placed by hand, drone watcher
```

**An empty body is what older firmware sends, and it stays legal.** No receiver
reads a HELLO body — the frame updates the peer table and, off a bridge, prints
one line — so an older node treats a 9-byte HELLO exactly like an empty one, and
a bridge of any version prints the decrypted body as hex on its `rx` line like
every other body. Only the host decodes it (see the decoder below).

A node can also be asked where it stands: `ar42:al` brings back the line `al` prints —
`Location: 46.519100,6.566800 (fixed)`, `(gps)` for a live fix, or
`Location: not set` — which is at most 40 characters and so fits the 43 bytes of
one console reply. `ai` prints the same line after `Role:`, which is how a host
places the bridge itself.

## Worked example

A `CMD` from node 42 to node 7, message 1234, setting `DA` (index 104) to 1,
with a budget of 2 hops, under the key `000102…0f`. Its first transmission is
sealed with the counter it was just given:

```
from     0x2A           42
seal     0x0004D2       1234
nonce    012a000004d200000000000000
aad      2a0004d2       from and seal, as transmitted
plain    c0 2a 07 0004d2 016801 20
         │  │  │  │      │      └ budget 2, hops 0
         │  │  │  │      └ SET_INT8, first = 104 (DA), value 1
         │  │  │  └ counter 1234
         │  │  └ dst 7
         │  └ src 42
         └ ctrl: version 1, type 4 (CMD)
frame    2a0004d2 3bfee7348323dd0df668 7b3962a4
         │        └ ciphertext          └ mic
         └ from, seal
```

18 bytes on the air. Node 9 relays it, having heard it at −95 dBm (`0xA1`), and
seals its copy with its own counter, 5000:

```
from     0x09           9
seal     0x001388       5000
nonce    01090000138800000000000000
plain    c0 2a 07 0004d2 016801 09a1 21
                                │    └ budget 2, hops 1
                                └ route: node 9 at −95 dBm
frame    09001388 bfc94163e8b320b1771a457c 3f3a5348
```

20 bytes. The two copies share no byte on the air, yet both name message 1234
from node 42, which is what a receiver deduplicates on. Both frames are checked
byte for byte by `test/test_lora_frame`.

## Sending, relaying, retrying

**Duplicate suppression and replay defence are the same test**, and it is keyed
on the message — `src` and `counter` — never on the seal, which differs in every
copy. Each peer entry holds `lastCounter` plus a 32-bit IPsec-style sliding
window, because flooding delivers the same frame by several paths and out of
order — a plain "greater than" test would drop legitimate frames. A frame that
is not _fresh_ is neither acted on nor relayed. A peer heard for the first time
is accepted at face value; that cold entry is the one hole in the design, and it
closes with the first frame recorded.

A node's messages are not numbered consecutively: its retries and the frames it
relays draw from the same sequence. The window is 32 values wide, so two
messages from one node that arrive out of order are both accepted only while
fewer than 32 of its transmissions lie between them.

**A relay verifies the MIC before forwarding**, so only authentic group traffic
is ever amplified. It then requires all of: the frame is fresh, this node's role
is repeater (`DA` = 1), `remaining = budget − hops` is above 0 and not above
`LORA_TTL_MAX_ACCEPT` (3), and the frame is not addressed to this node. A reply
(`ACK`, `NACK`, `RESP`) must also answer a message this node relayed itself. It
records itself in the route, **waits for the frame's answer**, then a random
0…3× airtime, and **cancels its copy if it hears that answer or two other nodes
relay the same message**. Skipping any of these turns a flood into an N² storm.
A copy that does go out is sealed under the relay's own address and a counter it
draws at that moment, so a cancelled copy costs no counter. A bridge (`DA` = 2)
does not relay.

### Receipts: a frame the bridge has is not repeated

A bridge answers every `HELLO` and `DATA` meant for the host — sent to everyone
or to the bridge — that could still travel (`budget > hops`) with a **receipt**,
before it prints anything. A receipt is an ordinary `ACK`: addressed to the
source, echoing its counter, status 0, budget the `hops` the copy took, so a
receipt for a direct reception is heard only around the bridge.

A repeater holding a copy waits for the answer the frame can get before its
jitter: `LORA_REPLY_TURNAROUND_MS` (100) plus the answer's airtime — a short
`ACK`, or a whole frame for the `RESP` to a read. That is about 0.2 s at the
defaults. Nothing is waited for a reply, or for a broadcast command, which
nobody answers. Hearing any copy of an answer — `ACK`, `NACK` or `RESP`
addressed to the source and echoing the counter — cancels the copy. So only a
repeater that did not hear the bridge carries the frame on, and a repeater out
of the bridge's range relays exactly as before: nothing is lost.

**Replies retrace their request.** Every reply begins with the low 24 bits of
the counter it answers, so a repeater carries one on only if it relayed that
message itself, from a memory of its last 16 relays. No field is added to the
wire, and a node without the rule floods replies up to their budget as before,
so both coexist on one mesh.

The bridge keeps a fifth of its airtime back and sends no receipt below it.
Without receipts the mesh relays as if no bridge were in range, so running short
degrades into repetition, never into loss. A receipt is ~93 ms at the defaults:
a drone watcher reporting every 5 s costs the bridge about 19 % of its hour,
where two relayed copies of each report cost every repeater about 37 %.

The rules live in `src/lora/loraRelayPolicy.h`, tested on the host by
`test/test_lora_relay`.

**A retry keeps the message counter.** Incrementing it would make the receiver
execute the command twice, because it cannot tell a lost ACK from a second
command. The receiver remembers the last counter it answered per peer, so a
duplicate is acknowledged again without being applied again — while a duplicate
GET is simply answered again, since a read changes nothing. Each attempt is
sealed again under a new seal, because the budget it carries has changed.

The escalation ladder for a frame that expects an ACK (`CMD`, `DATA_ACK`, unicast
only — broadcasts are never acknowledged by the nodes they reach). `wait` is
what each relay spends waiting for the answer on the way out, turnaround plus a
full frame's airtime. The top rung is 3 hops because that is the most a relay
accepts — at 4 the first relay refused it:

| Attempt | Budget     | Timeout                         |
| ------- | ---------- | ------------------------------- |
| 0       | 0 (direct) | `2 × (2h+1) × airtime + h × wait + 200 ms` |
| 1       | 2 hops     | idem                            |
| 2       | 3 hops     | idem                            |

After three attempts the sender gives up and reports `noack`. If the destination
is not in the peer table, or was last heard directly over 30 minutes ago, the ladder
starts at attempt 1 and skips the direct try. Only one acknowledged request is
in flight at a time.

**One counter sequence feeds both the seals and the messages, and it must never
go backwards**: a seal used twice is a nonce used twice. A message's first
transmission is sealed with the counter it was just given; every later
transmission of this node — a retry, a relay — draws a new one. `mesh.counter`
in NVS therefore holds a _reservation_: a promise that nothing above it was ever
used. A node claims `LORA_COUNTER_RESERVATION` (100) at a time and restarts at
the bound, so a crash mid-block skips forward over counters it may or may not
have spent. That is the flash-wear knob — one NVS write per 100 transmissions,
paid for by burning 100 counters on every boot. A busy repeater spends it
faster than before, since each copy it relays now costs one.

## Radio and regulatory limits

| Setting          | Default        | Parameter                                   |
| ---------------- | -------------- | ------------------------------------------- |
| Carrier          | 869.525 MHz    | `DC`, 25 kHz steps over 400 MHz (`18781`)   |
| Bandwidth        | 250 kHz        | `DD`, one of 250 / 125 / 62 (= 62.5)        |
| Spreading factor | SF9            | `DE`, 7–12, anything else falls back to SF9 |
| Coding rate      | 4/5            | fixed                                       |
| Preamble         | 8 symbols      | fixed                                       |
| Sync word        | private (0x12) | fixed                                       |

`DC` counts 25 kHz steps because that is the channel raster of sub-band P, and
it divides every EU868 and US915 channel — a 0.1 MHz step could not express
869.525, the centre of the one wide channel the regulation grants 500 mW.
Counting from 400 MHz keeps the SX1262's whole 150–960 MHz range inside a signed
int16, so the carrier stays an ordinary parameter needing no unsigned accessor.
Firmware before 2026-08 counted 0.1 MHz in this slot. A stored value between
1500 and 9600 is therefore an old one — unambiguously, because no band this
radio uses lands in that window under the new encoding — and is **refused, not
converted**: a node that was never reset falls back to the default rather than
coming up on 617.1 MHz. Nothing is written back, so setting `DC` yourself is
still the only thing that changes it.

869.525 MHz is the centre of EN 300 220 sub-band P, the one place in the band
that allows 500 mW and a 10 % duty cycle, and the only carrier at which a 250 kHz
channel fills the sub-band exactly (869.400–869.650) — which is why the bandwidth
is not a separate decision. It costs the quiet: a LoRaWAN gateway sends its RX2
downlinks on the same frequency, at 27 dBm. `18736` (868.4 MHz) is the
alternative, in the gap between the mandatory LoRaWAN channels at 868.3 and
868.5, where the mesh shares the channel with nobody and 125 kHz is the widest
that fits without spilling into either — for 1 % of the hour and 14 dBm. All
three settings are re-applied without a reboot when the parameter changes.

**The duty cycle follows the carrier** and is not a constant; anything
unrecognised falls back to the strictest value:

| Sub-band            | Range               | Duty cycle |
| ------------------- | ------------------- | ---------- |
| —                   | 433.05 – 434.79 MHz | 10 %       |
| L / M               | 865 – 868.6 MHz     | 1 %        |
| P                   | 869.4 – 869.65 MHz  | 10 %       |
| Q                   | 869.7 – 870 MHz     | 1 %        |
| K, N, and every gap | —                   | 0.1 %      |

**It is a budget, not a delay.** EN 300 220 defines the duty cycle as transmit
time within a one-hour observation window, so the governor is a token bucket:
`airtimeBudgetMillis` holds the transmit time still available, it is credited
back at 1/N of real time, and each transmission spends what it costs. At the
default 10 % that is 360 s per hour, which the node may burst through — roughly
2903 frames of 124 ms at SF9/250 kHz — before it has to wait; on a 1 % carrier it
is 36 s, roughly 145 frames of 247 ms at SF9/125 kHz. A fixed post-transmission
silence would be far stricter than the regulation and would make the retry
ladder unusable. RadioLib enforces none of this outside LoRaWAN.

**Transmit power follows the carrier too**: 14 dBm (25 mW ERP) across 863–870,
22 dBm in sub-band P, which allows 500 mW — more than the SX1262 can produce, so
there the radio's own ceiling binds — and 10 dBm in 433.05–434.79, which allows
only 10 mW. On the default carrier the node therefore transmits at **22 dBm, and
there is nothing above it**: `SX1262::checkOutputPower` refuses anything over 22,
so the legal 500 mW is out of reach of the part whatever is asked of it. It is
deliberately **not** a parameter: there is no legitimate reason to raise it, and
a parameter is one typo away from transmitting illegally.

Every transmission is preceded by listen-before-talk (up to four channel scans
with a 20–60 ms backoff).

### Why these three, and what the alternatives buy

```
DC18781    carrier 869.525 MHz
DD250      bandwidth 250 kHz
DE9        spreading factor 9
```

The three settings are one decision, taken from the duty cycle backwards.
869.525 MHz is the centre of sub-band P, which allows 10 % and 500 mW — 360 s of
airtime an hour, and the radio's own 22 dBm rather than the 14 dBm the rest of
the band permits. That also fixes the bandwidth: the regulation allows P as
25 kHz channels or as one wideband channel, and 869.400–869.650 is exactly
250 kHz, so the channel fills the sub-band edge to edge. SF9 is then what the
budget can afford without spending it: a 33-byte telemetry frame costs 124 ms, so
the hour pays for nearly three thousand of them.

**Airtime, not link budget, is what this mesh runs out of first.** For the
35-byte frame that carries a fix and a beacon RSSI, at 124 ms:

| `gt` | Frames per hour | Airtime | Against the 360 s of sub-band P | On 868.4 MHz, against 36 s |
| ---- | --------------- | ------- | ------------------------------- | -------------------------- |
| 60   | 60              | 7.4 s   | 2 %                             | 41 %                       |
| 30   | 120             | 14.9 s  | 4 %                             | 82 % — no room to relay    |
| 25   | 144             | 17.9 s  | 5 %                             | 99 % — the practical floor |
| 10   | 360             | 44.6 s  | 12 %                            | **247 % — two frames in three are dropped** |
| 5    | 720             | 89.3 s  | 25 %                            | —                          |
| 2    | 1800            | 223.2 s | 62 %                            | —                          |

The governor drops what it cannot pay for rather than sending it late, so past
the floor a faster cadence does not degrade, it goes missing — and the loss is
silent unless a bridge is counting. That right-hand column is the whole argument
for the default: a tracker reporting every 10 s does not fit in a 1 % sub-band at
any spreading factor, and fits four times over in P.

The cost is the company. 869.525 **is** the RX2 downlink of every LoRaWAN gateway
in range, sending at 27 dBm; the mesh's private sync word means neither side
decodes the other, but a busy gateway is still a channel busy. When the quiet is
worth more than the allowance —

```
DC18736    carrier 868.4 MHz
DD125      bandwidth 125 kHz
DE9        spreading factor 9
```

— 868.4 MHz falls in the gap between the mandatory LoRaWAN channels at 868.3 and
868.5, where nothing else transmits and 125 kHz is the widest channel that fits
without spilling into either. It costs 8 dB of transmit power and nine tenths of
the airtime, and buys 3 dB of sensitivity back from the narrower channel: **−5 dB
net**, and a cadence no faster than about 25 s.

The other direction is `DE12` on the same carrier, when a link will not close at
all: +7.5 dB, for a frame that takes 906 ms instead of 124 and an exchange that
answers six times slower.

The slots are adjacent, so one frame moves a node: `ax42:DC18736,125,9`. Its ACK
goes out on the old settings — the radio is only retuned by the task loop, after
the command has been handled — so the sender hears the receipt and then follows.
Move the sender last. `dutyCycleDivisor()` and `maxTxPowerDbm()` both recognise
18776–18786, so the 10 % budget and the 22 dBm ceiling come with the frequency,
and leaving that window takes both away.

#### What each one costs

From `airtimeMillis()` — CR 4/5, 8-symbol preamble, explicit header, CRC on. SF12
at 250 kHz has a 16.384 ms symbol, just past the 16 ms threshold, so the low data
rate optimisation is on and the frame is exactly half of SF12 at 125 kHz:

| Frame                       | Bytes | **SF9 / 250 kHz** | SF9 / 125 kHz | SF12 / 250 kHz |
| --------------------------- | ----- | ----------------- | ------------- | -------------- |
| HELLO, no position          | 16    | 83 ms             | 165 ms        | 660 ms         |
| HELLO with a position       | 24    | 103 ms            | 206 ms        | 742 ms         |
| the `CMD` of the example    | 18    | 93 ms             | 186 ms        | 660 ms         |
| GPS telemetry, 8 parameters | 33    | 124 ms            | 247 ms        | 906 ms         |
| the same plus a beacon RSSI | 35    | 124 ms            | 247 ms        | 906 ms         |
| the largest frame           | 73    | 216 ms            | 432 ms        | 1561 ms        |

Against the budget each carrier grants, for that 33-byte frame:

|                         | **SF9 / 250 kHz, 10 %** | SF9 / 125 kHz, 1 % | SF12 / 250 kHz, 10 % |
| ----------------------- | ----------------------- | ------------------ | -------------------- |
| Transmit time per hour  | 360 s                   | 36 s               | 360 s                |
| 33-byte frames per hour | 2903                    | 145                | 397                  |

Latency follows airtime the same way. `ladderTimeout` is
`2 × (2 × hops + 1) × airtime + hops × wait + 200 ms` over a 0/2/3-hop ladder,
where `wait` is the relay's wait for an answer. Relay jitter (0…3× airtime) is
0…0.37 s at the default and 0…2.7 s at SF12, after that wait.

#### What each one buys

|                | **SF9 / 250 kHz** | SF9 / 125 kHz | SF12 / 250 kHz |
| -------------- | ----------------- | ------------- | -------------- |
| Transmit power | 22 dBm            | 14 dBm        | 22 dBm         |
| Sensitivity    | −126.5 dBm        | −129.5 dBm    | −134.0 dBm     |
| Against the default | —            | **−5 dB**     | **+7.5 dB**    |

Sensitivity is −174 + 10 log₁₀(BW) + 6 dB noise figure + the demodulator floor
(−12.5 dB at SF9, −20 dB at SF12), which reproduces the SX1262 datasheet
figures. Widening to 250 kHz costs 3 dB of noise floor — SF12 at 125 kHz would be
−137 dBm — but that is not a shape sub-band P allows, and it would double the
airtime again.

7.5 dB is ×2.4 range in free space and ×1.5 to ×1.8 for a path loss exponent of 4
to 3, so the SF12 end is worth reaching for only when a link actually will not
close: it is 7.3 times the airtime per frame for range a mesh whose nodes already
hear each other does not need. Read a node's reported margin against the row it
is running before spending any of it.

## Decoding a captured frame

A bridge (`DA` = 2) emits one JSON object per line on Serial, including a `raw`
event with the whole packet as hex, **before** the key is consulted — so a
capture survives a node that cannot read what it heard. A host can decode those
bytes with the group key:

```js
import { createDecipheriv } from "node:crypto";

function readCounter(bytes, at) {
  // bit 7 of the first byte set: 4 bytes holding 31 bits, otherwise 3 bytes
  if (bytes[at] & 0x80) {
    return { value: bytes.readUInt32BE(at) & 0x7fffffff, size: 4 };
  }
  return { value: bytes.readUIntBE(at, 3), size: 3 };
}

export function decodeMeshFrame(frame, key) {
  const seal = readCounter(frame, 1);
  const clear = frame.subarray(0, 1 + seal.size);
  const ciphertext = frame.subarray(clear.length, frame.length - 4);
  const mic = frame.subarray(frame.length - 4);

  const nonce = Buffer.alloc(13);
  nonce[0] = 0x01;
  nonce[1] = frame[0];
  nonce.writeUInt32BE(seal.value, 2);

  const decipher = createDecipheriv("aes-128-ccm", key, nonce, {
    authTagLength: 4,
  });
  decipher.setAAD(clear, { plaintextLength: ciphertext.length });
  decipher.setAuthTag(mic);
  const plain = Buffer.concat([decipher.update(ciphertext), decipher.final()]);

  const ctrl = plain[0];
  if (ctrl >> 7 !== 1) throw new Error(`frame version ${ctrl >> 7}`);
  const counter = readCounter(plain, 3);
  const trailer = plain[plain.length - 1];
  const hops = trailer & 0x0f;
  const stored = Math.min(hops, 4);
  const routeStart = plain.length - 1 - stored * 2;

  const route = [];
  for (let i = 0; i < stored; i++) {
    const at = routeStart + i * 2;
    route.push({ address: plain[at], rssi: plain.readInt8(at + 1) });
  }

  return {
    from: frame[0],
    seal: seal.value,
    type: (ctrl >> 4) & 0x07,
    source: plain[1],
    destination: plain[2],
    counter: counter.value,
    budget: trailer >> 4,
    hops,
    route,
    body: plain.subarray(3 + counter.size, routeStart),
  };
}
```

`decipher.final()` throws when the tag does not verify, which is exactly the test
the firmware applies before it acts on — or relays — anything. Without the key,
`from` and `seal` are all a capture can say about a frame.

The other bridge events are listed in
[loraBridge.h](../src/lora/loraBridge.h) and in the project `CLAUDE.md`; `rx`
already carries the decrypted body as hex, so a host normally only needs the
decoder above for frames captured elsewhere or for a node without the key.

A `HELLO` body (type 0), whichever of the two it came from:

```js
export function decodeHelloBody(body) {
  // empty: a node on firmware older than the position, which says nothing
  if (body.length === 0) return null;
  const flags = body[0];
  const hello = {
    droneWatcher: (flags & 0x04) !== 0,
    repeater: (flags & 0x08) !== 0,
    bridge: (flags & 0x10) !== 0,
    position: null,
  };
  if (flags & 0x01 && body.length >= 9) {
    hello.position = {
      latitude: body.readInt32LE(1) / 1e6,
      longitude: body.readInt32LE(5) / 1e6,
      source: flags & 0x02 ? "gps" : "fixed",
    };
  }
  return hello;
}
```
