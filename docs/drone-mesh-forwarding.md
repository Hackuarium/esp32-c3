# Forwarding drone Remote ID over the mesh — an evaluation

**The local feed is built; the LoRa records are not.** This is the design for a
perimeter watch — several
`[env:droneTracker]` posts reporting what they hear to one host running
`lpatiny/loramesh-monitoring` — and, more importantly, the arithmetic that
constrains it. Read the airtime section first: it decides everything else.

There are **two paths to the database and the mesh is only one of them**. A post
the host can reach with a cable is a post that needs no radio hop at all: the
board plugged into the serial port is itself a drone watcher, and what it hears
goes straight out as JSON at full precision and once a second. That path is
**built** — `src/droneId/droneIdFeed.cpp`, and *The bridge is a post too* below
describes it. The LoRa records are what a post on the far fence would send when
no cable reaches it, and for a site of one or two cabled watchers they are not
needed at all; they are kept here because the arithmetic that shapes them does
not change, and because the day a fence has no cable is the day it is read
again.

What has to arrive at the other end is two things, and the second is the one
that matters operationally: **the aircraft, and the person holding the remote
control**. Remote ID broadcasts both — the System message carries the operator's
position, which is where somebody is standing — so an intrusion is not a dot
over a wall, it is a dot over a wall and a dot in a car park.

## The one number that settles the shape

A drone transmits its position **at least once a second**, on every transport it
uses. Carrying one 25-byte Location message per mesh frame costs, at the default
SF9 / 250 kHz:

| | |
|---|---|
| frame | 6 header + 2 opcode + 26 payload + 4 tag + 1 trailer = **39 bytes** |
| airtime | **135 ms** |
| at 1 Hz | 485 s per hour |
| as a share of sub-band P's 10 % allowance (360 s/h) | **135 %** |

**One drone, forwarded raw, does not fit in the entire duty cycle** — before the
HELLOs, before any relaying, before a second drone, and before anything else the
mesh exists to carry. So "forward all the packets" is not a thing that can be
built. What can be built is a **normalized summary at a cadence the band
allows**, and the honest way to describe it is a report, not a forward.

## The three records

Four transports and six message types collapse into three fixed records, split
by how fast each one changes. Everything is quantized where the loss cannot
matter — the aircraft itself claims `<30 m` horizontal accuracy, so a coordinate
finer than a metre is carrying noise at 2 bytes apiece.

### Coordinates: 4 bytes, and a stale reference cannot corrupt them

A position travels as **two int16 in units of 1e-5 degrees**, modulo. The bridge
reconstructs the absolute value against the position it already holds for the
reporting node:

```
full = reference + wrapToSigned(sent - reference, 65536)   // units of 1e-5 deg
```

- **Resolution 1.11 m** in latitude, 0.77 m in longitude at 46.5° — finer than
  the aircraft's own claim, and finer than anything a map of a perimeter shows.
- **The window is ±0.327 deg**: ±36 km north-south, ±25 km east-west here. The
  reference only has to be right to within that, so a post surveyed to a few
  hundred metres — or entered by hand off a map — decodes **exactly** the same
  coordinates as one surveyed to a centimetre.

That last property is why this is preferred to the obvious encoding, an offset
in metres from the post. An offset is a byte cheaper and fails catastrophically:
a post whose stored position is wrong, or was moved, produces coordinates that
are confidently and silently displaced. Here a wrong reference either changes
nothing or moves the aircraft by a whole 73 km window, which is not a mistake
anybody acts on.

The reference lives in the tracker's **existing `G`…`J` slots** — the same four
int16 a GPS tracker writes its fix into, written by hand with
`setParameterInt32` semantics over `ar`. Nothing new, and a post that later
grows a receiver fills them itself.

### `TRACK` — 11 bytes, what changes

```
0   handle    u8   local to the sending node, bound by IDENT
1   flags     u8   2-0 status (0 undeclared, 1 ground, 2 airborne, 3 emergency,
                               4 Remote ID system failure)
                   3   height is above takeoff (else above ground)
                   4   position valid
                   5   conflict: another transmitter claims this UAS ID elsewhere
                   6   urgent: this node's own proximity test fired
                   7   first report of this handle
2   lat16     i16  1e-5 deg, modulo, against the node's reference
4   lon16     i16
6   height    u8   2 m steps, 0-508 m, 255 unknown
7   speed     u8   0.5 m/s steps, 255 unknown
8   heading   u8   2 deg steps, 255 unknown
9   rssi      i8   dBm as this node heard it, strongest transport
10  misc      u8   3-0 seconds since heard (15 = older), 7-4 transports bitmask
```

Frame: `opcode(1) count(1) records(n × 11)`. **Four records is 46 of the 48 the
body allows**, so one frame covers four aircraft.

**The vertical rate is not carried** — the host differences two heights. At a 5 s
cadence and 2 m steps that resolves 0.4 m/s, which is enough to see a descent
over a wall, and it saves a byte in every record of every frame.

**`rssi` and the transport bits are the whole point of forwarding at all.** The
aircraft's position it already broadcast to everyone; what only this node knows
is *how strongly, on which radio, from where it stands*. Four posts around a
perimeter give four margins on one handle, which is the thing one post cannot
produce — and it is also the only thing that contradicts a spoofed position: an
aircraft claiming to be 2 km away while four posts hear it at −60 dBm is lying,
and nothing else in Remote ID can say so.

### `PILOT` — 6 bytes, the remote control

```
0   handle    u8
1   flags     u8   1-0 operator location source (0 takeoff, 1 live GNSS,
                                                 2 fixed, 3 unknown)
                   2   moved since the last report
                   3   inside this node's proximity radius
                   7-4 EU category and class, the nibbles as on the wire
2   lat16     i16  same encoding as TRACK
4   lon16     i16
```

Sent on the first System message, **whenever the operator moves more than
`PILOT_MOVE_METRES` (25 by default)**, and otherwise every few minutes as a
keepalive. That cadence is the difference between this design and the obvious
one: an operator walking a fence line at 1.4 m/s crosses 25 m every 18 s, which
is a 19-byte frame — 26 ms at SF7 — and it is the single most useful thing the
mesh can carry. A pilot standing still costs nothing at all.

No operator altitude: the person is on the ground, at the post's own elevation
to within what matters.

### `IDENT` — variable, what does not change

```
0   opcode
1   handle    u8
2   types     u8   7-4 ID type, 3-0 UA type, the nibbles as on the wire
3   uasLen    u8   then uasLen bytes, trailing NULs trimmed
    opLen     u8   then opLen bytes of the Operator ID (the registration)
```

A serial number of 19 and a registration of 16 make a 40-byte body — 51 on air,
52 ms at SF7. It is sent when a handle is allocated, again when the Operator ID
arrives (it is a separate message and turns up later), then at a slow keepalive
while the handle is live.

**The handle is a local index, not an identity.** One byte, scoped to the
sending node, so the bridge keys on `(node, handle)` and resolves it through the
IDENT binding. A node reboot restarts the numbering, which is safe precisely
because every aircraft is then a first sighting and every handle is re-bound
before it is used.

A `TRACK` can still arrive before its `IDENT` — a broadcast is unacknowledged.
The host stores that point with a null UAS ID and back-fills it when the binding
lands, rather than dropping it. And because `ar` runs a console verb on any
node, the host can **ask**: a new `df<handle>` re-sends the IDENT for one
handle, which is a 52 ms answer instead of a five-minute wait.

### One handle is one aircraft, which is a deliberate change of mind

`droneIdTable` keeps one row per **transmitter** — four rows for a drone using
all four transports — and refuses to merge them, because merging means trusting
the UAS ID, the one field a spoofer picks. That is right for a console.

It is wrong for the air: it would quadruple the cost of saying one thing. So the
forwarder allocates a handle per **UAS ID**, carries the transports it was heard
on as a bitmask, and sets the `conflict` flag when two transmitters claiming one
UAS ID disagree on position by more than `CONFLICT_METRES`. The merge is
contained to the encoder, is announced in the record, and the evidence that
would contradict it travels with it.

## Compatibility: there is exactly one extension point, and this is it

**Nothing in the envelope changes.** A drone record rides a mesh frame like any
other body: AES-128-CCM under the group key, the header authenticated, the
counter as the nonce, the same relay rules, the same duty-cycle governor, the
same route trailer — see [docs/lora-mesh-frame.md](lora-mesh-frame.md).

What is new is three body layouts behind **three DATA opcodes**, and the format
already reserved the room: a DATA body is a SET body byte for byte, so opcodes
`0x01`…`0x05` are spoken for and everything above is free. Take `0x10` `TRACK`,
`0x11` `PILOT`, `0x12` `IDENT`.

Three consequences, and the third is the one worth planning around:

- **DATA, never CMD.** A receiver records these, it does not act on them. This
  is the same reason a tracker broadcasts its fix as DATA.
- **Broadcast and unacknowledged** for the periodic reports; 255 nodes
  acknowledging a sighting is the ACK storm the mesh already refuses. The one
  exception is the first urgent report of a handle, below.
- **A node that has never heard of these opcodes handles them correctly
  already.** `loraMeshReportData` prints `{"event":"data","src":…,"opcode":16,
  "length":45}` and moves on, and — this is the part that matters — the `rx`
  line of the same reception **already carries the whole decrypted body as
  hex**, which `recordEvent` already stores in `packets.body`.

So **the bridge needs no firmware change to carry these**. The records can be
decoded entirely in `loramesh-monitoring`, from a column it has been filling
since migration 001, and a capture taken before the decoder existed can be
replayed through it. Only the watching posts are reflashed.

That is also the rule to hold to afterwards: **the firmware encodes these
records and never decodes them.** A bridge that parsed the wire format would be
a second copy of the decoder, kept in step with the first by hand, and the thing
it would print is what the host is about to work out for itself from `body`. Its
own sightings need no wire format at all — they go out as JSON directly, below.

### Urgent is the node's word; intrusion is the host's

Two tests, deliberately not the same one:

- **The node** runs a crude one it can hold in two parameters — an aircraft
  within `ALERT_RADIUS` metres of the post and below `ALERT_CEILING` metres. It
  decides one thing: whether this report waits for the tick. An urgent report
  goes immediately, addressed to the bridge as `LORA_TYPE_DATA_ACK`, so it takes
  the escalation ladder and is retried if it is not heard.
- **The host** runs the real one — the perimeter polygon, the approach
  corridors, the time of day, the classes of aircraft that are expected. That
  belongs where it can be edited without a ladder, and a polygon is not
  something to carry in int16 slots.

One caution on the urgent path: a node holds **one confirmed request at a time**
and the ladder runs for seconds. So only the *first* urgent report of a handle
is acknowledged; everything after it rides the periodic broadcast. Alarming
twice about an aircraft that is already on the operator's screen is not worth
blocking every other command on that node.

## What it costs

Computed with `airtimeMillis()` from `src/taskLoraMesh.cpp`, the same function
the governor uses, at 250 kHz. Frame = 6 header + body + 4 tag + 1 trailer.

| Frame | body | on air | SF7 | SF8 | SF9 |
|---|---|---|---|---|---|
| GPS telemetry, for scale | 18 | 29 | 34 ms | 62 ms | 114 ms |
| `TRACK`, one aircraft | 13 | 24 | 31 ms | 57 ms | 103 ms |
| **`TRACK`, four aircraft** | **46** | **57** | **54 ms** | 98 ms | 175 ms |
| `PILOT`, one | 8 | 19 | 26 ms | 52 ms | 93 ms |
| `IDENT`, typical | 40 | 51 | 52 ms | 93 ms | 165 ms |

As a share of one node's **whole** 10 % allowance (360 s/h in sub-band P):

| cadence | 4 aircraft per frame, SF7 | SF9 | 1 aircraft, SF7 | SF9 |
|---|---|---|---|---|
| every 2 s | 27.0 % | 87.5 % | 15.5 % | 51.5 % |
| **every 5 s** | **10.8 %** | 35.0 % | 6.2 % | 20.6 % |
| every 10 s | 5.4 % | 17.5 % | 3.1 % | 10.3 % |
| every 30 s | 1.8 % | 5.8 % | 1.0 % | 3.4 % |

**Five seconds at SF7 is the working point**, and it costs a ninth of one post's
allowance whether it is watching one aircraft or four. The `PILOT` and `IDENT`
frames are events, not a rate, and in a quiet hour they add nothing.

### The spreading factor is the big lever, and it is a console command

`DE7` is four times the bit rate for one command and no code. The trade is
5 dB of sensitivity against SF9 — about 0.56× the range in free space — which
against a perimeter with line of sight along a wall, at the 22 dBm sub-band P
allows, is not a constraint: SF7 at 250 kHz still closes a kilometre in the
open. It would be the wrong choice for a post in a dip with a hill in the way,
and the answer there is a relay rather than a slower channel for everybody.

### What binds once the duty cycle stops binding

Each post has its own 360 s an hour, but **they all share one channel**, and
LoRa does not sense the carrier before transmitting. At 5 s and SF7, each post
spends 39 s of the hour:

| posts | on air | channel occupancy |
|---|---|---|
| 2 | 78 s/h | 2.2 % |
| 4 | 156 s/h | 4.3 % |
| 8 | 311 s/h | 8.6 % |

Eight posts still sit under about 9 %, where collisions stay rare.

**Do not add duplicate suppression at this scale.** It is the right answer when
the budget binds — a post that has just heard a stronger report of the same
handle stays quiet — but here it would throw away the one thing several posts
are for. Four margins on one aircraft are what give a bearing and what catch a
spoofed position; a post that stays quiet is indistinguishable from a post that
heard nothing. Keep it in reserve for the day a site runs sixteen posts.

Two smaller things for a fixed site:

- **Set the relay budget to 0 (`DB0`) on any post in direct range of the
  bridge.** A relayed frame is transmitted again at every hop, so a two-hop path
  triples the channel cost of the same information.
- **There is no clock on the mesh.** A record carries `age` in seconds, and the
  host timestamps it as the bridge's reception instant minus that age — exactly
  as `recordEvent` already timestamps a position. Nothing here needs the posts
  to agree on the time.

## Parameters on the watching post

`droneTracker` uses `A`…`F`, and the mesh block sits at 104–113, so the
forwarder takes free slots in between. `G`…`J` are the reference position; the
rest are new:

| | | Default |
|---|---|---|
| `G`…`J` | the post's surveyed position, lat and lon as int32 over two slots each | unset |
| `K` | seconds between `TRACK` frames, 0 = forwarding off | 0 |
| `L` | aircraft per frame, 1–4 | 4 |
| `M` | metres the operator must move before a `PILOT` frame | 25 |
| `N` | proximity radius in metres, 0 = nothing is urgent | 0 |
| `O` | proximity ceiling in metres above ground | 120 |
| `P` | bridge address for urgent reports, 255 = broadcast | 255 |
| `Q` | seconds between `drone` lines on this board's own serial feed, 0 = every frame | 1 |

`Q` is the only one of these a bridge uses, and a bridge uses **only** that one:
the radio costs of `K`…`P` are costs of the mesh, and a board reporting down a
cable pays none of them.

`K0` by default, so a board that is flashed and not yet configured says nothing
on the air — the same reasoning as `DF0`. And, like `gt`, the console verb that
sets the cadence should **price it first**: a forwarder that quietly loses 60 %
of its frames to the governor is a security system that is not reporting, which
is the worst possible way to find out about a duty cycle.

## The bridge is a post too, and then there is no mesh at all

**The board on the end of the serial cable should be a drone watcher itself.**
`[env:droneTracker]` already includes the mesh through `configLoraMeshParams.h`,
so a tracker with `DA2` *is* a bridge: one XIAO ESP32S3, three radios, watching
2.4 GHz for Remote ID and relaying the mesh's JSON on the same port. Nothing new
is needed to make that board exist. What is new is that it must **say what it
hears itself**, in the same feed, and never send it over LoRa to reach a host it
is already plugged into.

This is not a fallback. It is the better path wherever a cable reaches:

| | over the mesh | on the bridge itself |
|---|---|---|
| position | 1e-5 deg, quantized | as transmitted, 1e-7 deg |
| height | 2 m steps, 508 m ceiling | as transmitted, with the geodetic altitude |
| cadence | 5 s, and it costs a ninth of a duty cycle | **1 s, and it costs nothing** |
| identity | a handle, bound by a separate frame that can be lost | the UAS ID on every line |
| accuracies, self-ID, EU class, authentication pages | dropped — no room | all of it, if it is wanted |
| what can go wrong | airtime, collisions, an unbound handle | the cable |

So a site is normally **both**: the bridge watching the yard it stands in at
1 Hz, and two or three LoRa posts on the fences no cable reaches, at 5 s and
11 bytes. The mesh earns its place at the far end of the site and nowhere else.

### The JSON line is the interface; the LoRa record is a lossy transport of it

One shape, three events, emitted by `loraBridge`'s existing helpers. `via` says
which path it came by, and it is the only field that differs:

```json
{"event":"ident","addr":"1c:2f:33:44:55:66","via":"BT5","uas":"1581F5559000000ABCD","idType":1,"uaType":2,"operator":"FIN87astrdge12k8","version":2}
{"event":"pilot","addr":"1c:2f:33:44:55:66","via":"BT5","uas":"1581F5559000000ABCD","lat":46.5190000,"lon":6.6320000,"alt":480.0,"source":1,"category":1,"class":3,"rssi":-71}
{"event":"drone","addr":"1c:2f:33:44:55:66","via":"BT5","uas":"1581F5559000000ABCD","status":2,"lat":46.5197123,"lon":6.6323011,"alt":512.0,"height":120.0,"heightRef":1,"speed":12.50,"vspeed":1.50,"heading":203.0,"hacc":3,"vacc":2,"rssi":-71,"ch":6}
{"event":"lost","addr":"1c:2f:33:44:55:66","via":"BT5","uas":"1581F5559000000ABCD","seen":312,"silent":300,"messages":842,"best":-64}
```

- **No line names the board it came from**, exactly as a `ble` line names none:
  the host asks `ai` once when it opens the port and attributes everything on
  that port to that address. Two cabled watchers are two readers, and neither
  has to be told who it is.
- **A line is a complete statement.** The transmitter address, the transport it
  was heard on and the identity claimed on it are on every one of them, so
  nothing has to be held open across two lines to read either.
- **One row is one transmitter, as the console has it.** An aircraft on four
  transports produces four `drone` lines a second, with four margins — which is
  the honest thing to store and the only thing that answers whether Bluetooth or
  Wi-Fi is reaching further today. The host merges them by UAS ID for the map
  and keeps them apart in the track.
- **A local line carries the UAS ID and no handle.** The handle exists to save
  19 bytes on a radio; on a serial port it is a level of indirection that can
  only lose data, and a line that arrives before its binding is a line that
  cannot be interpreted. `via: "mesh"` records — which the host decodes from
  `rx.body`, not the bridge — are the only ones that carry a handle, and the
  host resolves it before storing.
- **The feed is paced and carries no age**: a line is written as the frame is
  received, so the host timestamps it on arrival, the way it already timestamps
  a position. One line per transmitter per `PARAM_DRONE_FEED_SECONDS` — `Q`, one
  second, which is the aircraft's own rate on the slowest transport. `Q0` is
  *not* "every frame": an untouched slot reads 0, and an unpaced feed is several
  lines a second per transport, so 0 means the default.
- **The feed replaces the console block rather than joining it.** That is the
  convention `loraMeshReportData` already set — a bridge emits JSON, a node
  prints prose — and `di` says which of the two this board is, so a silent port
  is not read as a dead radio. `dl` and `dd` still print.
- **A `pilot` or `ident` line is emitted on change, not on a tick** — the same
  rule as over the air, for the same reason: the operator is usually standing
  still and the serial number never changes.
- Nothing about the existing feed moves. These are three new `event` values
  beside `raw`, `rx`, `params` and `ble`, and a host that does not know them
  drops them exactly as it drops a human line.

### A watcher with no radio at all

The same feed makes a LoRa-less variant possible — a bare ESP32S3 on a USB port
watching one building — but it is not free: `bridgeReader` learns the node's own
address by writing `ai` and parsing `Address: N`, and a board with no mesh has
none to answer with. Either it keeps the mesh block and answers like any node
(one line in a config, and it can then also *hear* the fence posts), or the host
grows a per-port identity in `settings`. The first is cheaper and is what to
build; the second is the one to reach for when the hardware is genuinely not a
mesh board.

## The other end: `loramesh-monitoring`

Everything below already has a precedent in that repository — the Bluetooth
feed is the same shape, one layer lower.

**Ingest, from two sources into one record.** `parseBridgeLine` gains the three
`event` values, and that is the whole of the local path — the line already says
everything a row needs. The mesh path is a `droneRecords.ts` beside `gpsFix.ts`,
reading `rx` lines whose type is `DATA` and whose body starts `10`/`11`/`12`,
and it is the only place a wire record is ever parsed. Both produce the same
internal shape, and it is worth keeping them apart in exactly one respect: a
mesh record is stored with `via = 'mesh'`, because its position is quantized to
a metre and its cadence is five seconds, and a track drawn from the two mixed
together should be able to say which points were which.

Reconstructing a mesh coordinate needs the reporting node's position, which
`nodes.latitude` already holds; a record from a node with no known position is
stored with its raw int16 pair and no coordinate, and resolved later rather than
guessed at.

**Store** — one migration, five tables, following the `ble_devices` split
between a state row and the event that folded into it:

```sql
drone_aircraft   -- one row per UAS ID: identity, operator id, last position,
                 -- transports ever heard, sightings, conflicts
drone_handles    -- (node, handle) -> uas_id, bound_at: what IDENT said.
                 -- Mesh only; a local line names the aircraft outright
drone_points     -- append-only track: at, node, via, packet_id (mesh only),
                 -- uas_id (null until bound), position, height, speed,
                 -- heading, status, rssi, transports
drone_pilots     -- the operator's position over time, with its source
drone_alerts     -- opened_at, closed_at, uas_id, zone, acknowledged_by
```

These are **not** in `packets` and do not replace it: a mesh-borne record
arrived in a frame that is already a packet row with its own margin, route and
counter, and `drone_points.packet_id` points back at it. A local sighting has no
packet at all, which is why that column is nullable and why `via` is not
decoration — it is the difference between a row that has a frame behind it and
one that does not.

**Push** — a `droneFeed.ts` and `/v1/drones/stream`, copied from `bleFeed.ts`
and `openEventStream`. This one is genuinely real-time in a way the map's
positions are not: somebody is watching it to decide whether to send a person
outside.

**Show** — on the existing map: the aircraft with its heading, a dashed line to
the pilot marker, the track fading with age, and the posts that heard it drawn
with their margins. An unbound handle is drawn as an unidentified aircraft
rather than hidden. An open alert is a banner that stays until it is
acknowledged, because an alert that scrolls away is an alert nobody saw.

**The pilot marker is the one to make loud.** An aircraft over a wall is a
report; a person standing in a lane holding a controller is somewhere to send
somebody, and it is the only marker on that map that corresponds to a human
being who can be met. It deserves its own symbol, the line back to the aircraft
it belongs to, and a bearing and distance from the nearest gate.

**The map must not silently filter these.** `isUsableFix` refuses a fix solved
from fewer than six satellites, and the map filters again at nine — both are
about a node's own GPS and neither applies here: a Remote ID position comes from
the aircraft's receiver and carries no satellite count at all. Applying that
rule to a drone point would drop every intrusion.

## The honest caveats, which matter more than any of the above

**Remote ID is compliance, not detection.** It is a broadcast a lawful aircraft
chooses to make. Somebody flying contraband over a wall can switch it off, fly
an aircraft that never had it, or transmit a false identity — nothing in the
standard is authenticated in a way a receiver can check, which is stated in
[docs/drone-remote-id.md](drone-remote-id.md) and is not a limitation of this
implementation. For a prison, that is the population that matters most, and it
has to be written into the threat model in these words rather than discovered
during a review.

What this does catch is the careless and the compliant, every legitimate
overflight, and the operator's position when it is broadcast. It is **one
layer**. Three gaps:

- **5 GHz is not heard at all.** A Wi-Fi Beacon on channel 149 is fully
  conformant and inaudible to a 2.4 GHz radio. That needs a second receiver.
- **DJI's proprietary DroneID is not heard**, and DJI is much of the market. The
  standards beacon modern DJI aircraft also transmit *is* decoded; an older or
  modified one sending only the proprietary burst is invisible. It is the
  cheapest gap to close — it rides a Wi-Fi beacon vendor element under OUI
  `26 37 12`, decodable by the existing beacon walk with one more `memcmp`, and
  it carries the pilot's position too. See
  [docs/drone-rf-detection.md](drone-rf-detection.md).
- **A non-cooperative aircraft is not detected by any of this**, by
  construction. That is radar, RF signature or acoustics.

### And the question worth asking first

A prison perimeter has mains power and, usually, a cable duct. LoRa exists to
avoid infrastructure — battery life and kilometres of range. **Every post a
cable or a local Wi-Fi link reaches should use it**: it carries the full
identity, the accuracies and the self-ID at 1 Hz, with none of this arithmetic
and nothing to lose to a collision. That is the same conclusion as *The bridge
is a post too*, arrived at from the other end, and it decides the shape of a
deployment: count the posts a cable reaches before counting the ones it does
not, and spend the mesh only on the remainder.

## The order it would be built in

1. ~~**The JSON lines on a `droneTracker` running `DA2`**, from its own
   sightings.~~ **Done** — `drone`, `pilot`, `ident` and `lost`, paced by `Q`,
   with the pilot line on movement past `M`. It is the whole feature for a site
   with one watcher, it touches no wire format, and it is what the two ends
   agree about.
2. **The tables and the ingest in `loramesh-monitoring`**, fed by those lines.
   At this point a drone shows up on the map, over a cable, with no LoRa
   anywhere.
3. The map layer and the alert, including the host-side perimeter.
4. `droneIdMesh.cpp` on a remote post: handle allocation, the three encoders,
   the cadence priced through the governor. Nothing in `droneIdTable` or the
   decoding changes.
5. The host's decoder for `rx.body`, which folds a mesh-borne record into the
   record that already exists from step 2 — and which can be tested against a
   capture, since the bodies were stored all along.
6. The node's proximity test and the urgent `DATA_ACK` path — last, because it
   is the only part that touches the acknowledged-request slot every other
   command shares.

## Open questions

1. **Is 5 s per aircraft what the operator needs from a LoRa post?** It is what
   the band affords comfortably at SF7. 2 s is possible at 27 % of one post's
   allowance; 1 Hz is not, at any spreading factor, and that answer does not
   change with a better encoding. A cabled post has none of this ceiling, so the
   question is really about which fences are cabled.
2. **How much of the duty cycle may the drone feed have?** A ninth is my
   assumption, not a measurement of what else the mesh carries.
3. **Should a post with nothing in the sky say so?** Silence and a dead post
   look alike, and for a security system that distinction is the whole point. A
   `TRACK` frame with `count 0` is 19 bytes and would make it explicit — or the
   HELLO interval comes down from three hours, which costs the same and says
   less.
4. **Where does the perimeter polygon live?** The host, in this design. If an
   alert must survive the bridge being down, it has to be on the post, and then
   the post needs a siren or a relay output rather than a map.
5. **How many of the posts can be cabled?** It decides how much of this is ever
   written: a site whose watchers all reach the host needs the JSON feed and the
   tables, and none of the wire format.
6. **How long is a track kept?** A cabled watcher at 1 Hz writes 3600 rows an
   hour per aircraft. That is nothing for a day and something for a year, and
   the answer — thin the points past a week, keep every alert for ever — is a
   decision about evidence rather than about disk.
