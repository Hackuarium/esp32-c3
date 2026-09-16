# Drone Remote ID

A drone in Europe or the United States is required to say who it is. ASTM
F3411 — and ASD-STAN prEN 4709-002, which is the same wire format under a
different cover — makes an aircraft broadcast its identity, its position, its
heading and speed, and **the position of the operator standing on the ground**,
in the clear, unencrypted, continuously, so that anybody underneath can read
them with an ordinary radio.

`[env:droneTracker]` is that radio. It listens on both radios, decodes with the
reference library, and prints what it hears.

    ~/.platformio/penv/bin/pio run -e droneTracker -t upload

## What it listens to

There is one message format and four ways to carry it. An aircraft picks at
least one; a compliant American one transmits both Bluetooth methods at once,
and the same aircraft may also be on Wi-Fi. **All four are received here.**

| Transport | How it is recognised | Carries |
|---|---|---|
| Bluetooth 4 legacy advertising | AD type `0x16`, UUID `0xFFFA`, app code `0x0D` | exactly one 25-byte message |
| Bluetooth 5 Long Range | the same, on an extended advertisement (coded PHY) | a message pack, up to 9 |
| Wi-Fi Beacon | vendor element `0xDD`, OUI `FA:0B:BC`, type `0x0D` | a pack, or one bare message |
| Wi-Fi NAN | an action frame to `51:6F:9A:01:00:00`, Open Drone ID service | a pack |

Both Bluetooth methods arrive through one NimBLE scan:
`CONFIG_BT_NIMBLE_EXT_ADV` makes it call `ble_gap_ext_disc()` with the same
parameters for the uncoded and the coded PHY, so 1M and Coded are scanned alike
and there is nothing to configure.

### What it cannot hear, and why the console says so

- **5 GHz.** Channel 149 is the other social channel, and Wi-Fi Beacon on
  5 GHz is a mandatory-alternative transport in Europe. An aircraft that chose
  it is legal, conformant and completely inaudible here — the ESP32-S3 has a
  2.4 GHz radio. It takes a second receiver, and `di` says so rather than
  leaving it to be discovered.
- **DJI's own DroneID**, which is a different protocol on a different physical
  layer and is not 802.11 at all. See *Coverage and residual risk* below; it is
  the gap that most needs to be understood before this is relied on.
- **A nine-message pack over Bluetooth 5.** An extended advertisement past 229
  bytes is split into two HCI reports and NimBLE 1.4.3 does not reassemble
  them, so neither fragment is a valid AD structure and both are dropped before
  any decoding. `di` counts them separately as *split too long*, since they
  never reach the decoder to be counted as refused. Packs of one to eight
  messages — which is what transmitters actually send — are unaffected.

## The two radios are one radio

The ESP32-S3 has a single 2.4 GHz front end. Espressif's own coexistence table
rates Wi-Fi promiscuous receive alongside Bluetooth as *supported but
unstable*: a sniffer is not one of the four Wi-Fi states the arbiter reserves
time for, and with Wi-Fi idle "the RF module is controlled by Bluetooth".

So rather than let that be decided implicitly, the two take turns. `A` seconds
of Bluetooth, then `B` seconds of Wi-Fi, with the scan stopped and the receiver
closed at each handover so the slice is one the arbiter cannot take back.

**Seven and three, because the aircraft decides and not us.** A position goes
out at least once a second on every transport, so three seconds of Wi-Fi is
three chances at a beacon and five or six NAN discovery windows, and seven of
Bluetooth is seven advertisements from each of the two Bluetooth sets. Missing
one is not missing an aircraft.

Set either to 0 and the other gets the whole radio — `B0` for a Bluetooth-only
receiver, `A0` for a Wi-Fi-only one.

## Channels

`C` pins the Wi-Fi channel, and it is **6** by default. Channel 6 is the social
channel: an aircraft that uses it is allowed to transmit once a second, and one
that uses any other channel must transmit five times a second. So parking on 6
is what hears the slow transmitters, and wandering is what hears the fast ones
badly. NAN settles it — its discovery windows are 16 ms out of every 524 ms, on
channel 6, and time spent elsewhere is discovery windows missed.

`C0` hops instead, over **6, 1, 6, 11**, two seconds each — so channel 6 still
gets half the time. Two seconds because a beacon on channel 6 arrives once a
second and a shorter dwell catches nothing; and the step is on a dwell timer of
its own rather than on the window handover, because with `A0` the Wi-Fi window
never ends and a hop tied to the handover would freeze on its first channel.
`esp_wifi_set_channel` is only called when the channel actually changes — it is
the call this ESP-IDF has a reported deadlock against when made rapidly from an
application task.

## Reading the console

A whole block is printed when there is genuinely something new: a first
sighting, or a message type this transmitter had not sent before. That is not a
detail — **identity, position, the operator's position and the registration are
separate messages sent at their own rates**, so the picture assembles over the
first minute of a flight and the console shows each piece as it lands.

```
=== Drone heard, BT5 ===
Address: 1c:2f:33:44:55:66, -71 dBm
UAS ID: 1581F5559000000ABCD (serial number), multirotor
State: airborne
Position: 46.5197123, 6.6323011 (<30 m)
Altitude: 512.0 m geodetic, 120.0 m above ground (<10 m)
Movement: 12.50 m/s, heading 203, up 1.5 m/s (<1 m/s)

=== Drone, more from BT5 ===
...
Operator: 46.5190000, 6.6320000, 480 m (live GNSS)
Class: EU open C1
Operator ID: FIN87astrdge12k8
```

In between, one line per aircraft, no more often than `D` seconds — an aircraft
sends its position once a second, and a console that repeats itself sixty times
a minute is one nobody reads:

```
[drone] 1581F5559000000ABCD 46.5197123, 6.6323011 512 m 12.5 m/s -71 dBm BT5
```

| Command | |
|---|---|
| `di` | the radios, the channel, the table, the frames refused |
| `dl` | one line per transmitter heard |
| `dd3` | everything transmitter 3 has said |
| `dh` | the last payload accepted, and the last refused, in hex |
| `dc` | clear the list |

## Several watchers

The board is a XIAO ESP32S3 with a Wio-SX1262, so it is a mesh node as well.
Three radios, and only two of them compete: LoRa is a separate chip at 868 MHz
and neither shares the 2.4 GHz front end nor the coexistence arbiter that
Bluetooth and Wi-Fi take turns over.

What that buys is `ar`, which runs a console command on another node — and the
`(d)` menu is a console command like any other:

```
ar42:dl        what node 42 can see from where it is standing
ar42:di        whether its radios are running
ar42:C0        put node 42 on the hopping channel plan
```

One reply is one frame, truncated at 43 bytes, so `dl` from across the mesh
returns the beginning of the list and says it was cut. Three boards around a
field triangulate by signal strength in a way one board cannot.

## One row is one transmitter, not one aircraft

A drone broadcasting on both radios uses a **different address on each**, and
the reference transmitter uses a different random static address again for its
Bluetooth 4 and Bluetooth 5 advertising sets. So the same aircraft can occupy
four rows.

They are not merged, and that is a decision. Merging would mean trusting the
UAS ID — the one field a receiver is least entitled to assume is unique, since
it is the one a spoofer picks. What is actually on the air is four
transmitters; `dl` says so, marks the rows that agree on a UAS ID, and lets the
operator draw the conclusion:

```
0 1581F5559000000ABCD BT4 -79 dBm 46.5197123, 6.6323011, 2 s ago
1 1581F5559000000ABCD BT5 -71 dBm 46.5197123, 6.6323011, 1 s ago, same UAS ID as 0
2 1581F5559000000ABCD beacon ch6 -83 dBm 46.5197123, 6.6323011, 3 s ago, same UAS ID as 0
```

It also keeps a signal strength per transport, which is the number that answers
whether Bluetooth or Wi-Fi is reaching further today.

## Parameters

| | | Default |
|---|---|---|
| `A` | seconds per cycle on Bluetooth, 0 = never | 7 |
| `B` | seconds per cycle on Wi-Fi, 0 = never | 3 |
| `C` | Wi-Fi channel, 0 = hop over 6, 1, 6, 11 | 6 |
| `D` | quiet seconds between two lines on one aircraft, 0 = every frame | 5 |
| `E` | seconds of silence before a transmitter is dropped | 300 |
| `F` | transmitters currently in the table, written by the task | — |
| `M` | metres the operator must move before the feed reports them again | 25 |
| `Q` | seconds between two `drone` lines about one transmitter, on a bridge | 1 |

A freshly flashed board has an empty NVS partition, where every slot reads 0 —
not "unset". Since 0 is a legitimate `A` and a legitimate `B`, a board coming up
on those would listen to neither radio and look exactly like a quiet sky, so the
task writes the whole block when `E` reads 0: forgetting an aircraft after no
seconds at all is not a choice anybody made. Same tell as the mesh's radio
triple.

## Into a database: `DA2` makes this board a feed

The board is a mesh node, so `DA2` makes it a **bridge** — and a bridge's
console is a data feed rather than a log. What it hears on 2.4 GHz then leaves
the serial port as JSON, one object per line, next to the lines the mesh already
produces:

```json
{"event":"ident","addr":"1c:2f:33:44:55:66","via":"BT5","uas":"1581F5559000000ABCD","idType":1,"uaType":2,"operator":"FIN87astrdge12k8","version":2}
{"event":"pilot","addr":"1c:2f:33:44:55:66","via":"BT5","uas":"1581F5559000000ABCD","lat":46.5190000,"lon":6.6320000,"source":1,"category":1,"class":3,"rssi":-71}
{"event":"drone","addr":"1c:2f:33:44:55:66","via":"BT5","uas":"1581F5559000000ABCD","status":2,"lat":46.5197123,"lon":6.6323011,"alt":512.0,"height":120.0,"heightRef":1,"speed":12.50,"heading":203.0,"hacc":3,"rssi":-71,"ch":6}
{"event":"lost","addr":"1c:2f:33:44:55:66","via":"BT5","uas":"1581F5559000000ABCD","seen":312,"silent":300,"messages":842,"best":-64}
```

This is the whole path to `lpatiny/loramesh-monitoring` for a watcher the host
can reach with a cable, and it is the one to use wherever a cable reaches: the
position is as the aircraft transmitted it, the identity is on every line, and
none of the compression a LoRa post has to pay for applies. A post on a fence no
cable reaches is a different exercise, in
[docs/drone-mesh-forwarding.md](drone-mesh-forwarding.md).

Four things about it are deliberate:

- **The three fast-changing things are paced apart.** A position is a tick, at
  `Q`; an operator's position is an event, sent when they move more than `M`
  metres; an identity is sent when it arrives or changes. Both of the slow ones
  are repeated every five minutes as well, because nothing else will ever say
  them again and a host that was restarting would otherwise hold an aircraft it
  cannot name.
- **An unknown value is an absent key**, never the standard's `-1000`. A height
  of -1000 stored as a number is an aircraft a kilometre underground.
- **The feed replaces the console blocks**, exactly as `loraMeshReportData`
  does for mesh traffic: several hundred bytes of prose per sighting is the port
  spent on lines no host will parse. `dl`, `dd` and `di` still print, and `di`
  says which of the two this board is doing.
- **The line does not name the board.** The host learns that once, by asking
  `ai` when it opens the port — the same as for `ble`.

## Tests

    ~/.platformio/penv/bin/pio test -e native

Nine cases over `src/droneId/droneIdFrames.cpp`, which is where an off-by-one
decodes a valid message as garbage or - worse - garbage as a position. They run
against real frames from the reference transmitter, frozen in
`test/test_droneid_frames/fixtures.h`: a legacy advertisement, a pack behind an
AD Flags structure (which the reference Android receiver's fixed offset misses),
a beacon, a NAN action frame, a truncated pack, cross-transport frames and
noise. The decoded values are checked against what was encoded, not against
"something came back".

## The decoding is not ours

`lib/opendroneid/` is [opendroneid-core-c](https://github.com/opendroneid/opendroneid-core-c),
the reference implementation, vendored unmodified at commit `6484f26`,
Apache-2.0. It owns every byte of the message format — the nibble order, the
two horizontal-speed scales, the half-metre altitudes, the accuracy
enumerations, the 2019 epoch — and it accumulates across frames, which is
exactly the shape a receiver needs.

It is vendored rather than named in `lib_deps` because **nothing in the
opendroneid organisation is packaged for PlatformIO or the Arduino Library
Manager** — no `library.json`, no `library.properties`, in any of its nine
repositories. A `lib_deps` git URL clones it and then fails: the headers in
`libopendroneid/` never reach the include path and `libmav2odid/` is compiled
too and wants MAVLink. See `lib/opendroneid/README.md`.

What this project adds is the part upstream does not do: finding the payload
inside each transport, bounding it before handing it over, keeping one record
per transmitter, and printing it.

**The one place we knowingly differ from upstream** is the four nautical mile
horizontal accuracy bound. `decodeHorizontalAccuracy()` returns 7808 m where
four times 1852 is 7408, which is what its own header comment says. It is
printed as a distance somebody may act on, so it is corrected in
`droneIdLabels.cpp`.

## Do not use the `(w)` menu on this board

The Wi-Fi half puts the radio in promiscuous mode and never associates with
anything. A station role pins the channel to its access point's, and the two
cannot both hold it — so connecting to a network takes the receiver away until
the board is rebooted. `taskWifi` is compiled into every image in this
repository but is never started here.

## Coverage and residual risk — read this before relying on it

*Written to be read on its own, by whoever has to sign off a deployment.*

**In one sentence: this detects drones that are broadcasting their legally
required identification, and it detects them well. It does not detect drones,
and it cannot be relied on to find one whose operator does not want to be
found.** Everything below is the detail behind that sentence.

### What it detects, reliably

An aircraft complying with ASTM F3411 or ASD-STAN prEN 4709-002 — which is what
every drone sold into the European or United States markets under the current
rules must do — broadcasts its identity, position, altitude, heading and speed,
and the position of the operator on the ground, in the clear, at least once a
second. This board receives that on **all four transports the standard defines**
(Bluetooth 4 legacy, Bluetooth 5 Long Range, Wi-Fi Beacon, Wi-Fi NAN) in the
2.4 GHz band, decodes it with the standard's own reference library, and logs it.

For a perimeter, that reliably covers: every lawful overflight, every hobbyist
who has not modified their aircraft, and every operator who does not know or does
not care that they are being recorded. On the available evidence that is most
incursions, and the **operator's position** is often the operationally useful
part — it is where the person is standing, not just where the aircraft is.

### What it does not detect

**1. A transmitter that has been switched off.** This is the important one and no
amount of receiver improves it. Remote ID is a broadcast an aircraft *chooses* to
make. Firmware modifications that suppress it are circulated freely, older
aircraft never had it, and an aircraft assembled from parts never had it either.
Somebody delivering contraband over a wall is, by definition, in the category
that has a reason to switch it off.

**2. Anything on 5 GHz.** Channel 149 is the second social channel, and Wi-Fi
Beacon on 5 GHz is a mandatory-alternative transport in Europe — an aircraft that
chose it is fully conformant and completely inaudible here, because the ESP32-S3
has a 2.4 GHz radio. This is a hardware limit, not a configuration. `di` prints
it on every invocation rather than leaving it to be discovered.

**3. DJI's proprietary DroneID.** DJI aircraft emit a telemetry broadcast of
DJI's own, unrelated to and predating ASTM F3411, which exists to feed DJI's own
detection product. It carries much the same information — aircraft position,
**the pilot's location**, the serial number.

It is not 802.11 at all. It shares the 2.4 GHz band (and 5.8 GHz) but has its own
physical layer: about 10 MHz occupied, 15.36 MHz with guard carriers, nine OFDM
symbols per burst with Zadoff-Chu pilot sequences in the fourth and sixth, QPSK
data carriers, a burst roughly every 600 ms, over a Turbo Product Code and a
scrambler.

The radio on this board is an **802.11 demodulator**. Promiscuous mode delivers
802.11 frames that would otherwise be dropped by address filtering; it does not
deliver raw radio. A DroneID burst carries no 802.11 preamble and no MAC header,
so the hardware never registers a packet at all — it passes through the antenna
and nothing happens. There is no build flag that changes this. Receiving it needs
a software radio of at least 15.36 MHz bandwidth, correlating against a generated
Zadoff-Chu sequence to find the burst.

Two things narrow this gap, and one widens it:

- A DJI aircraft sold under current EU or US rules **also transmits standard
  Remote ID, usually as a Wi-Fi beacon — and that one is decoded here.** So the
  uncovered set is older aircraft, grey imports without compliance firmware, and
  aircraft deliberately modified.
- That last category has usually silenced DroneID as well, so an SDR would not
  have caught them either. The gap is narrower in practice than in principle.
- **DJI has begun encrypting DroneID.** Earlier generations were not encrypted
  despite the manufacturer's claims — that is published, peer-reviewed work — but
  current packets are. Building an SDR receiver is therefore a diminishing asset.

**4. A nine-message Bluetooth 5 pack.** A transmitter filling an extended
advertisement past 229 bytes has it split across two radio reports that the
Bluetooth library does not reassemble, and both fragments are dropped. No
transmitter is known to send packs that large — one to eight messages is what is
actually observed — and `di` counts the occurrences as *split too long* so the
assumption is checkable rather than assumed.

### Nothing in a Remote ID broadcast is authenticated

The standard defines an Authentication message and `dd` reports that one was
offered and of what type, but verifying a signature needs a registry this board
cannot reach. **Every field is what a transmitter chose to say** — the serial
number, the operator's position, the aircraft's own position. A false identity is
as easy to broadcast as a true one.

Treat a sighting as *an aircraft's claim about itself, received at this place, at
this signal strength*. The claim may be false; the reception is a fact, and the
signal strength and transport are the parts no transmitter controls.

### What would close each gap

| Gap | Who it lets through | What closes it |
|---|---|---|
| Remote ID switched off | a deliberate incursion | RF-signature, radar or acoustic detection — a different class of system |
| 5 GHz | a conformant aircraft that chose channel 149 | a second receiver with a 5 GHz radio |
| DJI DroneID | older or modified DJI aircraft | a software radio, or a commercial counter-UAS product that already does it |
| 9-message BT5 pack | no known transmitter | a patched Bluetooth library; not currently worth it |

Several counter-UAS vendors already sell DJI-frame decoding as a product.
Where DJI coverage is a requirement, buying that capability is more sensible
than building it.

### What this system is, honestly

**One layer.** It is a complete, standards-correct receiver for the
identification drones are required to broadcast, it runs unattended on a
inexpensive board, it logs every compliant overflight with the operator's
position, and over a mesh it can do that from several points at once.

It is **not** a perimeter intrusion detection system, and it should not be
signed off as one. Deployed alongside detection that does not depend on the
target's cooperation, it answers "who was that, and where was the pilot
standing" — which is the question the other systems cannot answer.
