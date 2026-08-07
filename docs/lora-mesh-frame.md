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
 ctrl(1) src(1) dst(1) counter(3|4) | ciphertext(0..48) | mic(4) | route(2h) | trailer(1)
 \____________ authenticated ______/  \___ encrypted __/          \______ mutable ______/
```

| Field      | Bytes              | Notes                                                         |
| ---------- | ------------------ | ------------------------------------------------------------- |
| `ctrl`     | 1                  | `ver(1) type(3) cntsz(1) spare(3)`, bit 7 down to bit 0       |
| `src`      | 1                  | originator, 1–254                                             |
| `dst`      | 1                  | 1–254, or 255 for broadcast                                   |
| `counter`  | 3 or 4             | big-endian, `cntsz` says which                                |
| ciphertext | 0–48               | the body, same length as the plaintext (CCM is a stream mode) |
| `mic`      | 4                  | AES-128-CCM tag                                               |
| route      | 2 per recorded hop | `address(1) rssi(1)`, 0–4 entries                             |
| trailer    | 1                  | `budget(4) hops(4)`, high nibble first                        |

Minimum overhead is **11 bytes** (6-byte header + 4-byte tag + 1-byte trailer),
plus 2 for every recorded hop. `LORA_MAX_FRAME_SIZE` is **68**.

### `ctrl`

| Bit | Name    | Meaning                                |
| --- | ------- | -------------------------------------- |
| 7   | `ver`   | protocol version, currently always 0   |
| 6–4 | `type`  | frame type, see below                  |
| 3   | `cntsz` | 0 = 3-byte counter, 1 = 4-byte counter |
| 2–0 | spare   | transmitted as 0                       |

`ver` is decoded into `LoraFrame.version` but **no receiver checks it yet** — a
version-1 frame would be parsed as if it were version 0. Anything defining a
version 1 has to add that test first.

### Frame types

| Value | Name       | Body                      | Answered with                      |
| ----- | ---------- | ------------------------- | ---------------------------------- |
| 0     | `HELLO`    | empty                     | nothing                            |
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

The **nonce** is built from header fields, never transmitted separately:

```
nonce[0]   = ctrl
nonce[1]   = src
nonce[2]   = dst
nonce[3:7] = counter, zero-extended to 32 bit, big-endian
nonce[7:13]= 0
```

The counter is always zero-extended from 32 bits, so the permanent widening from
a 3- to a 4-byte transmitted counter (past `0xFFFFFF`) can never produce a nonce
that was already used.

The **additional authenticated data** is the header exactly as transmitted — all
6 or 7 bytes, passed to mbedTLS without a copy and with nothing masked out. That
works only because everything a relay rewrites lives in the trailer, past the
tag.

The price is that **the trailer is unauthenticated**. A route is metadata of the
same standing as an RSSI reading, not evidence: anyone replaying a captured
frame can claim `budget 15, hops 0`. `LORA_TTL_MAX_ACCEPT` is what actually caps
amplification, not the budget.

A relay cannot instead sign its passage inside the ciphertext. It holds the group
key and could re-encrypt, but the nonce comes from the origin's `src` and
`counter`, which it must not change, and re-encrypting under a spent nonce is
the one misuse CCM does not survive.

## The trailer, read from the end

The trailer is self-describing, which is why there is no length field and no
flag bit:

1. the **last** byte gives `budget` (high nibble) and `hops` (low nibble);
2. the number of stored route entries is `min(hops, 4)`, so the route occupies
   `2 × min(hops, 4)` bytes immediately before it;
3. everything before that is header, ciphertext and tag — and the header's first
   byte says whether it is 6 or 7 bytes long, which yields the body length.

A frame that outruns the 4-entry table keeps counting hops and stops recording
them, so `hops > 4` is how a truncated route announces itself.

A route entry is `address(1) rssi(1)`, the dBm at which **that** relay heard the
frame (clamped to int8), so one reception carries the margin of every hop it
crossed. The last hop is deliberately absent — the receiver measures that one
itself.

**The budget counts up, not down.** A frame travels while `hops < budget`, and a
`budget` of 0 means "do not relay". That, not the broadcast address, is the
direct-versus-flood switch. Escalating a retry from 0 to 2 hops rewrites one
trailer byte and reuses the ciphertext byte for byte.

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
per slot — while 43 bytes already cost ~1.8 s at SF12, so the answer is
truncated to one frame rather than paged over a channel with a duty cycle. The
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

A `DATA` body whose first byte is neither `0x01` nor `0x02` is reported as an
opaque opcode and length.

### HELLO

Empty body. It proves a direct link and nothing else, which is why it is never
relayed (`budget` 0) and why its cadence is measured in hours: the default `DI`
is **10800 s (3 h)**. One is also sent as soon as the task starts, so a node that
has just booted does not stay out of its neighbours' peer tables for a whole
period.

## Worked example

A `CMD` from node 42 to node 7, counter 1234, setting `DA` (index 104) to 1,
with a budget of 2 hops, under the key `000102…0f`:

```
ctrl     0x40           version 0, type 4 (CMD), cntsz 0
src      0x2A           42
dst      0x07           7
counter  0x0004D2       1234
nonce    402a07000004d2000000000000
aad      402a070004d2   the header, all 6 bytes
plain    01 68 01       SET_INT8, first = 104 (DA), value 1
frame    402a070004d2 8b1fa0 b7e01cbd 20
                       └ ct  └ mic    └ budget 2, hops 0
```

14 bytes on the air. After node 9 relays it, having heard it at −95 dBm
(`0xA1`):

```
frame    402a070004d2 8b1fa0 b7e01cbd 09a1 21
                                      │    └ budget 2, hops 1
                                      └ route: node 9 at −95 dBm
```

16 bytes. The header and tag are untouched — only the trailer grew.

## Sending, relaying, retrying

**Duplicate suppression and replay defence are the same test.** Each peer entry
holds `lastCounter` plus a 32-bit IPsec-style sliding window, because flooding
delivers the same frame by several paths and out of order — a plain
"greater than" test would drop legitimate frames. A frame that is not _fresh_ is
neither acted on nor relayed. A peer heard for the first time is accepted at
face value; that cold entry is the one hole in the design, and it closes with
the first frame recorded.

**A relay verifies the MIC before forwarding**, so only authentic group traffic
is ever amplified. It then requires all of: the frame is fresh, this node's role
is repeater (`DA` = 1), `remaining = budget − hops` is above 0 and not above
`LORA_TTL_MAX_ACCEPT` (3), and the frame is not addressed to this node. It waits
a random 0…3× airtime and **cancels its copy if it hears two other nodes relay
the same frame**. Skipping any of these turns a flood into an N² storm. A bridge
(`DA` = 2) does not relay.

**A retry reuses the same counter.** Incrementing it would make the receiver
execute the command twice, because it cannot tell a lost ACK from a second
command. The receiver remembers the last counter it answered per peer, so a
duplicate is acknowledged again without being applied again — while a duplicate
GET is simply answered again, since a read changes nothing.

The escalation ladder for a frame that expects an ACK (`CMD`, `DATA_ACK`, unicast
only — broadcasts are never acknowledged):

| Attempt | Budget     | Timeout                         |
| ------- | ---------- | ------------------------------- |
| 0, 1    | 0 (direct) | `2 × (2h+1) × airtime + 200 ms` |
| 2, 3    | 2 hops     | idem                            |
| 4       | 4 hops     | idem                            |

After five attempts the sender gives up and reports `noack`. If the destination
is not in the peer table, or was last heard over 30 minutes ago, the ladder
starts at attempt 2 and skips the two direct tries. Only one acknowledged
request is in flight at a time.

**The counter is both the CCM nonce and the anti-replay sequence, so it must
never go backwards.** `mesh.counter` in NVS therefore holds a _reservation_: a
promise that nothing above it was ever used. A node claims
`LORA_COUNTER_RESERVATION` (100) at a time and restarts at the bound, so a crash
mid-block skips forward over counters it may or may not have spent. That is the
flash-wear knob — one NVS write per 100 frames, paid for by burning 100 counters
on every boot.

## Radio and regulatory limits

| Setting          | Default        | Parameter                                    |
| ---------------- | -------------- | -------------------------------------------- |
| Carrier          | 869.525 MHz    | `DC`, 25 kHz steps over 400 MHz (`18781`)    |
| Bandwidth        | 250 kHz        | `DD`, one of 250 / 125 / 62 (= 62.5)         |
| Spreading factor | SF12           | `DE`, 7–12, anything else falls back to SF12 |
| Coding rate      | 4/5            | fixed                                        |
| Preamble         | 8 symbols      | fixed                                        |
| Sync word        | private (0x12) | fixed                                        |

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

869.525 is the centre of EN 300 220 sub-band P, the one place in the band that
allows 500 mW and a 10 % duty cycle, and the only carrier at which a 250 kHz
channel fills the sub-band exactly (869.400–869.650). It is not a quiet channel:
a LoRaWAN gateway sends its RX2 downlinks on the same frequency, at 27 dBm.
`18736` (868.4 MHz, sub-band M) is the alternative — 1 % and 14 dBm, but in the
gap between the mandatory LoRaWAN channels at 868.3 and 868.5, so it shares a
channel with nobody. All three settings are re-applied without a reboot when the
parameter changes.

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
437 frames of 823 ms at SF12/250 kHz — before it has to wait; at 1 % it is 36 s,
roughly 538 frames of 67 ms at SF7/125 kHz. A fixed post-transmission silence
would be far stricter than the regulation and would make the retry ladder
unusable. RadioLib enforces none of this outside LoRaWAN.

**Transmit power follows the carrier too**: 14 dBm (25 mW ERP) across 863–870,
22 dBm in sub-band P, which allows 500 mW — more than the SX1262 can produce, so
there the radio's own ceiling binds — and 10 dBm in 433.05–434.79, which allows
only 10 mW. It is deliberately **not** a parameter: there is no legitimate reason
to raise it, and a parameter is one typo away from transmitting illegally.

Every transmission is preceded by listen-before-talk (up to four channel scans
with a 20–60 ms backoff).

### Why the defaults are the slow ones

```
DC18781    carrier 869.525 MHz
DD250      bandwidth 250 kHz
DE12       spreading factor 12
```

The three settings are one decision. Sub-band P is the only part of the band
that allows 500 mW and a 10 % duty cycle, and the regulation lets its
869.4–869.65 MHz be used either as 25 kHz channels or **as one channel for high
speed data** — so 250 kHz is not a preference but the only wideband shape
allowed there, and 869.525 the only centre that fits it. SF12 then buys back the
3 dB the wider bandwidth costs, and 9.5 dB more on top.

The slots are adjacent, so one frame moves a node that is still on the old
settings: `ax42:DC18781,250,12`. Its ACK goes out on those old settings — the
radio is only retuned by the task loop, after the command has been handled — so
the sender hears the receipt and then follows. Move the sender last.
`dutyCycleDivisor()` and `maxTxPowerDbm()` both recognise 18776–18786, so the
10 % budget and the 22 dBm ceiling come with the frequency.

The comparison below is against 868.4 MHz / 125 kHz / SF7, which is what this
mesh ran on until 2026-08 and what `18736`, `DD125`, `DE7` still selects.

#### What it costs

From `airtimeMillis()` — CR 4/5, 8-symbol preamble, explicit header, CRC on. SF12
at 250 kHz has a 16.384 ms symbol, just past the 16 ms threshold, so the low data
rate optimisation is on and the frame is exactly half of SF12 at 125 kHz:

| Frame                       | Bytes | SF7 / 125 kHz | SF12 / 250 kHz |
| --------------------------- | ----- | ------------- | -------------- |
| HELLO                       | 11    | 41 ms         | 578 ms         |
| the `CMD` of the example    | 14    | 46 ms         | 578 ms         |
| GPS telemetry, 8 parameters | 29    | 67 ms         | 823 ms         |
| the largest frame           | 68    | 123 ms        | 1479 ms        |

Twelve times the airtime — against ten times the budget, so the number of frames
an hour holds barely moves:

|                         | SF7 / 125 kHz, 1 % | SF12 / 250 kHz, 10 % |
| ----------------------- | ------------------ | -------------------- |
| Transmit time per hour  | 36 s               | 360 s                |
| 29-byte frames per hour | 538                | 437                  |

Latency is where it is really paid. `ladderTimeout` is a multiple of airtime, so
for that 29-byte frame one direct attempt waits 1.8 s instead of 0.3 s and the
full escalation ladder takes 36 s instead of 3.8 s; relay jitter (0…3× airtime)
grows from 0.20 s to 2.5 s.

#### What it buys

|                | SF7 / 125 kHz | SF12 / 250 kHz | Gain         |
| -------------- | ------------- | -------------- | ------------ |
| Transmit power | 14 dBm        | 22 dBm         | +8 dB        |
| Sensitivity    | −124.5 dBm    | −134.0 dBm     | +9.5 dB      |
|                |               |                | **+17.5 dB** |

Sensitivity is −174 + 10 log₁₀(BW) + 6 dB noise figure + the demodulator floor
(−7.5 dB at SF7, −20 dB at SF12), which reproduces the SX1262 datasheet figures.
Widening to 250 kHz costs 3 dB of noise floor — SF12 at 125 kHz would be
−137 dBm — but that is not a shape the sub-band allows, and it would double the
airtime again.

17.5 dB is ×7.5 range in free space and ×2.7 to ×3.8 for a path loss exponent of
4 to 3, so **about three times the range** in real terrain, for a frame that
takes twelve times as long and an exchange that answers ten times slower. That
is the trade the defaults now take; a dense mesh of nodes that already hear each
other has nothing to gain from it and should go back to 868.4 / 125 / SF7.

## Decoding a captured frame

A bridge (`DA` = 2) emits one JSON object per line on Serial, including a `raw`
event with the whole packet as hex, **before** the key is consulted — so a
capture survives a node that cannot read what it heard. A host can decode those
bytes with the group key:

```js
import { createDecipheriv } from "node:crypto";

export function decodeMeshFrame(frame, key) {
  const trailer = frame[frame.length - 1];
  const budget = trailer >> 4;
  const hops = trailer & 0x0f;
  const stored = Math.min(hops, 4);
  const payloadLength = frame.length - (1 + stored * 2);

  const route = [];
  for (let i = 0; i < stored; i++) {
    const at = payloadLength + i * 2;
    route.push({ address: frame[at], rssi: frame.readInt8(at + 1) });
  }

  const ctrl = frame[0];
  const headerSize = ctrl & 0x08 ? 7 : 6;
  const header = frame.subarray(0, headerSize);
  const counter =
    ctrl & 0x08 ? header.readUInt32BE(3) : header.readUIntBE(3, 3);

  const ciphertext = frame.subarray(headerSize, payloadLength - 4);
  const mic = frame.subarray(payloadLength - 4, payloadLength);

  const nonce = Buffer.alloc(13);
  nonce[0] = ctrl;
  nonce[1] = header[1];
  nonce[2] = header[2];
  nonce.writeUInt32BE(counter, 3);

  const decipher = createDecipheriv("aes-128-ccm", key, nonce, {
    authTagLength: 4,
  });
  decipher.setAAD(header, { plaintextLength: ciphertext.length });
  decipher.setAuthTag(mic);
  const body = Buffer.concat([decipher.update(ciphertext), decipher.final()]);

  return {
    version: ctrl >> 7,
    type: (ctrl >> 4) & 0x07,
    source: header[1],
    destination: header[2],
    counter,
    budget,
    hops,
    route,
    body,
  };
}
```

`decipher.final()` throws when the tag does not verify, which is exactly the test
the firmware applies before it acts on — or relays — anything.

The other bridge events are listed in
[loraBridge.h](../src/lora/loraBridge.h) and in the project `CLAUDE.md`; `rx`
already carries the decrypted body as hex, so a host normally only needs the
decoder above for frames captured elsewhere or for a node without the key.
