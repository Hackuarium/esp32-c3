# esp32-c3 — notes for Claude

Firmware for a family of ESP32 boards. One codebase, many `[env:…]` targets in
`platformio.ini`; each selects a `BOARD_TYPE` and pulls the matching
`include/config<Kind>.h`. Most files are compiled for every target, so guard
target-specific code with the build flags rather than assuming a target.

## Build and upload

PlatformIO is not on `PATH`. Use the full path:

    ~/.platformio/penv/bin/pio run -e <env>
    ~/.platformio/penv/bin/pio run -e <env> -t upload

`default_envs = loraMesh`, so a bare `pio run -t upload` targets a mesh node.

**Always build before claiming a firmware change works.** A build is ~12 s once
the toolchain is warm.

### Devices

| Env | Board | Address | What it is |
|---|---|---|---|
| `square` | `seeed_xiao_esp32s3` | **192.168.1.200** | The 16×16 energy wall (default env) |

The square is flashed **over the air** — it is on a wall. `upload_port` is set to
its IP in `platformio.ini`, and an IP upload port makes PlatformIO pick `espota`
automatically. ArduinoOTA has no password. To use USB instead, override:
`-t upload --upload-port /dev/cu.usbmodem…`.

### Toolchains are per-board and not all installed

`seeed_xiao_esp32s3` needs **espressif32 ≥ 6.x**; older installed platforms only
carry the C3 boards and fail with `Unknown board ID`. Install with
`pio platform install "espressif32@6.5.0"` (a few hundred MB). Envs that pin
`platform = espressif32` unversioned resolve to the newest installed version, so
installing a new platform changes what *every* unpinned env builds against.

If you only need a syntax check of shared code and the target toolchain is
missing, `-e lineC3` compiles the same sources for the C3.

**A green C3 build does not mean the S3 builds.** They can resolve to different
Arduino cores, and the cores differ in more than versions — Arduino-ESP32 ships
its own `esp_crt_bundle.h` (in `libraries/WiFiClientSecure/src/`) that shadows
the ESP-IDF one and renames the helper to `arduino_esp_crt_bundle_attach`. Code
that compiled fine against bare IDF failed on the S3 for exactly that reason.
Build the env you are actually shipping.

## The energy wall (`square`)

Renders the live house energy balance on a 16×16 NeoPixel matrix behind a
diffuser: four 5×5 squares — solar (yellow, top-left), battery (green for the
BYD, mint for the Marstek, top-right), grid (white, bottom-left), consumption
(red, bottom-right) — with blue dots marching along the links between them.

- `src/pixels/meteo/froniusDisplay.cpp` — the renderer. `paintSquare` lights the
  **16 perimeter LEDs** for the coarse unit and the **3×3 centre** for the
  remainder. 500 W and 50 W per LED for power, 1 kWh and 100 Wh for the battery.
  A power square signals "off the scale" by overflowing its ring and lighting
  completely; the battery instead gets an explicit `full` flag, because the
  ~18.4 kWh fleet holds more than the 16.9 kWh its LEDs cover — it lights up
  bright only within **1 % of the usable capacity** the backend reports (`bc`).
  The battery square also stacks **two sources in two greens**: the BYD's share
  of the level fills the ring first in pure green, the Marstek fleet's continues
  in turquoise, so the boundary shows which pack holds the charge. `paintFlux`
  lights every third of six LEDs and marches them; `getFluxSpeed` sets the rate
  from the power.
- `src/fronius.cpp` — fetches the data. The names are historical: it no longer
  talks to a Fronius inverter, it reads
  `https://solar.patiny.com/api/energy-flow/compact`, which already aggregates
  the inverter **and** the Marstek batteries and splits the balance into flows.
  Nothing is derived on the device.
- Override the URL with `-D ENERGY_FLOW_URL=…` and the cadence with
  `-D ENERGY_FLOW_INTERVAL_MS=…`.

The web dashboard renders the same wall LED-for-LED at
`https://solar.patiny.com` → Overview → *Energy wall*. **Keep the two in step**:
the scales live in `froniusDisplay.cpp` here and in
`frontend/src/pages/home/components/energyLed/ledScale.ts` in the
`lpatiny/solar.patiny.com` repo (usually cloned at `~/git/lpatiny/`).

## The decoration programs have a second implementation

`src/pixels/` is mirrored in TypeScript at `frontend/src/utils/pixels/` in the
`lpatiny/loramesh-monitoring` repo (usually cloned at `~/git/lpatiny/`), so a
scene for the Christmas decorations can be watched in a browser before it is
broadcast over the mesh — the decorations are outdoors and the channel allows
tens of seconds of transmission an hour, so getting it wrong costs two round
trips and a ladder. Five programs are ported: **rain (1), rgb (2), comet (3),
wave (4) and line (11)**, plus `getColor`, `ColorHSV`, `decreaseColor`,
`updateMapping`, the byte order `updateType` packs `CA` into, and the current
limiter of `TaskPixels`.

**Keep the two in step.** Changing any of those, or the meaning of a slot in
`include/configPixels.h`, silently makes that preview lie — it is a copy, not a
shared source. The port is faithful down to the parts that look like defects:
the 12-bit palette expanding by a shift so `0xf` is 240, `updateRGB` filling all
`MAX_LED` regardless of `BI`×`BJ` and so tripping the 10 A limiter on any
length, and `updateLine` addressing the buffer directly rather than through
`getLedIndex`.

**`CA` alone does not determine what a strip shows**, which is the one thing
that side cannot mirror from here. `updateType` makes `CA` the byte of the wire
each channel is packed into; the LED never reads it, and decodes the wire in the
order its own silicon expects. So the visible colour is the strip's own order
composed with the inverse of `CA` — and no parameter records what the LEDs are.
The fleet is genuinely mixed: `resetLine` writes `NEO_RGB` for the garlands,
`pr`/`ps` write `NEO_GRB` for the panels. The monitoring page therefore asks the
operator what the strip is, separately from `CA`, and previews the permutation
a mismatch produces.

## The two LoRa stacks

They share nothing but the SX1262 and the `(a)` serial menu, and a board builds
exactly one of them.

- **LoRaWAN** (`KIND_LORAWAN`, `[env:lorawan]`) — `src/taskLoraWanSend.cpp`, ABP
  against a real network server, RadioLib's `LoRaWANNode`. The session buffer is
  written to NVS after every uplink because the frame counter must survive a
  reboot or the network rejects the next uplink as a replay.
- **The private mesh** (`THR_LORA_MESH`, `[env:loraMesh]`, `[env:loraGPS]`) —
  `src/lora/`, no network server, one AES-128 group key.

### Adding the mesh to any board

Two lines. In the board's `config<Kind>.h`:

    #include "./configLoraMeshParams.h"

and `taskLoraMesh();` in its setup. That is all — the header brings
`THR_LORA`, `THR_LORA_MESH`, the role constants and the parameters. Defaults for
the block come from `loraMeshResetParameters()`, called from the board's own
`resetParameters()`, so a second board joining does not copy a list of settings
that then drifts from what every other node runs.

**`taskLoraMesh()` also writes those defaults when the block was never written**,
because a board that joins with two lines must not need a `ur` as well — on a
board that has a configuration of its own, a reset wipes it. An untouched NVS
key reads **0, not `ERROR_VALUE`**, so unset cannot be recognised slot by slot:
0 is a legitimate `DB` and a legitimate `DI`, and a node that came up on those
two would relay nothing it originates and never appear in a peer table. The
radio triple — `DC`, `DD`, `DE` — is what says the block is untouched, since no
carrier at no bandwidth and no spreading factor is not a choice anyone made.

A board that only *sometimes* carries a radio makes the include conditional
instead — the pixels config takes the mesh on `-D TASK_LORA_MESH`, which is what
separates `lineS3lora` from `lineS3`. That flag belongs in **`build_flags`, not
`build_src_flags`**: `lib/hack` allocates `parameters[MAX_PARAM]` and prints the
`(a)` menu, so a library that still saw 104 would have the mesh block written
past the end of the array.

On the XIAO ESP32S3 the radio is on the default SPI bus, whose **SCK is GPIO7 —
the D8 that `lineS3` drives its pixels from**, so a combined board moves the
strip (`lineS3lora` uses D0). GPIO8 and GPIO9 go the same way.

**The mesh owns parameters 104–113 (`DA`…`DJ`) on every board.** The code refers
to parameters only by name, so each config *could* pick its own slots — and that
is exactly the trap: the block would then collide with `PARAM_OUT2_COLOR1` on
the handrail, `PARAM_GATE1_IN` on the pixels board, and something else again on
the next one. 104–113 is the one range free in every config here, which is why
`MAX_PARAM` is 114 wherever the mesh is enabled. A board that sets a smaller
`MAX_PARAM` after including the header fails to compile rather than writing past
`parameters[]`.

The **GPS block does not travel as easily**: `configLoraMesh.h` puts the fix at
6–13, which on the pixels board is `PARAM_BLUE` through `PARAM_INT_TEMPERATURE_A`,
and nothing below 104 there offers eight adjacent free slots. A pixels tracker
would have to reserve its own range above the mesh block (114–121, `DK`…`DR`) —
and a bridge decorates `lat`/`lon` from *its own* `PARAM_GPS_LATITUDE`, so it
would then only decode fixes from boards numbered like itself.

`DJ` is spare on purpose. Parameters are persisted in NVS **under
their letter** (`NVS.setInt(numberToLabel(i), …)`), so renumbering one silently
hands a deployed node the value of something else — the block has to grow into
its spares, never shift. For the same reason raising `MAX_PARAM` is safe: it
only adds keys, and no existing value changes meaning.

### The mesh wire format (`src/lora/loraFrame.h`)

The full specification — every body layout, a worked hex example and a reference
decoder for a host reading a bridge's `raw` lines — is in
[docs/lora-mesh-frame.md](docs/lora-mesh-frame.md). The summary below is what
matters when changing the firmware.

    ctrl(1) src(1) dst(1) counter(3 or 4) | ciphertext | mic(4) | route(2h) | budget,hops(1)
    \___________ authenticated _________/   \_ encrypted _/       \________ mutable _______/

    ctrl, bit 7 to 0:     ver(1) type(3) cntsz(1) spare(3)
    trailer, bit 7 to 0:  budget(4) hops(4)

11 bytes of overhead, plus 2 per recorded hop. AES-128-CCM with a 4-byte tag
does confidentiality and authenticity in one pass; there is no second key and no
separate CMAC.

- **Everything mutable lives in the trailer, past the tag**, so the header is
  authenticated in full — nothing is masked out of the nonce, and the header as
  transmitted *is* the additional data, passed to mbedtls without a copy. A
  relay cannot instead sign its passage inside the ciphertext: it holds the
  group key and could re-encrypt, but the nonce comes from the origin's `src`
  and `counter`, which it must not change, and re-encrypting under a spent nonce
  is the one misuse CCM does not survive.
- **The trailer is read from the end**, which is what makes it self-describing:
  the last byte gives the hop count, the number of stored route entries follows
  from it (`min(hops, LORA_ROUTE_MAX)`), and everything before them is header,
  ciphertext and tag. No length field, no flag bit.
- **A route entry is `address(1) rssi(1)`** — the dBm at which *that* relay heard
  the frame, so one reception carries the margin of every hop it crossed. The
  last hop is deliberately absent: the receiver measures that one itself. Past
  `LORA_ROUTE_MAX` (4) the hops keep counting and stop being recorded, so
  `hops > 4` is how a truncated route announces itself.
- **The route is unauthenticated by construction** — metadata of the same
  standing as an RSSI reading, not evidence. Anyone replaying a captured frame
  can claim `budget 15, hops 0`. `LORA_TTL_MAX_ACCEPT`, not the budget, is what
  actually caps amplification.
- **The counter does two jobs**: it is the CCM nonce and the anti-replay
  sequence, so it must never go backwards. `mesh.counter` in NVS therefore holds
  a **reservation**, not the live value — a promise that nothing above it was
  ever used. The node claims `LORA_COUNTER_RESERVATION` (100) at a time and
  restarts at the bound, so a crash mid-block skips forward over the counters it
  may or may not have spent. That is the flash-wear knob: one NVS write per 100
  frames, paid for by burning 100 counters on every boot.
- `cntsz` flips permanently once the counter passes 2²⁴. The nonce is always
  built from the zero-extended 32-bit value, so the widening cannot collide.
- **The retry ladder is three attempts, one per rung** — direct, two relayed
  hops, then four (`LORA_LADDER_ATTEMPTS`). It holds the node's single
  confirmed-request slot for its whole length, so its duration is what every
  other addressed command waits for before being refused with `A confirmed
  request is already in flight`. The first two rungs used to be doubled, which
  ran 10.5 s for a short frame at SF9 and 18 s for a full one — long enough that
  a host polling on a ten second timer refused nearly everything else the mesh
  had to say. A node not heard from in the last half hour skips the direct rung
  and starts at two hops.
- **A relay verifies the MIC before forwarding**, so only authentic group
  traffic is ever amplified. It then dedups on `(src, counter)`, waits a random
  0…3× airtime, and cancels its copy if it hears two other nodes relay the same
  frame. Skipping any of the three turns a flood into an N² storm.
- **The budget counts up, not down**: a frame travels while `hops < budget`, and
  a `budget` of 0 means "do not relay" — that, not the broadcast address, is the
  direct-vs-flood switch. A relay also refuses anything whose *remaining* budget
  exceeds `LORA_TTL_MAX_ACCEPT`, which caps amplification whatever the sender
  claims. An ACK or a RESP is sent back with a budget of the `hops` the request
  actually took, which is a measurement rather than the guess the countdown gave.
- **A retry reuses the same counter.** Incrementing it would make the receiver
  execute the command twice, because it cannot tell a lost ACK from a second
  command.
- Broadcasts are never acknowledged — 255 nodes answering one frame is an ACK
  storm.

**A write with holes in it is one frame, not one per run.** `ax42:BB1,17,1,A1,2,3`
is the console syntax the decoration pages always used — a letter after a comma
starts a new slot instead of stepping to the next — and it now survives onto the
air. `LORA_CMD_SET_PARAMETER_RUNS` carries a sequence of `first(1) header(1)
values…`, where the header is the count with bit 7 set when the values need
int16, so each run picks its own width. A scene touching `BB`…`BO` with the two
geometry slots skipped is **21 body bytes in one frame**, against 34 for the
same command as text, 30 for carrying the gap values across to make one run, and
three acknowledged round trips for one frame per run. Runs are validated in full
before any of them is applied, so a truncated body cannot leave half a scene
behind a NACK.

**One parser reads that syntax, in `lib/hack/params.cpp`.**
`parseParameterAssignments` is what the serial console and the mesh both call —
the console applies the assignments where it stands, `processLoraMeshSetCommand`
groups them into runs and encodes them. Two parsers for one grammar is how a
command comes to mean different things depending on how it arrived.

**A node that predates the opcode NACKs with `LORA_REASON_UNKNOWN_COMMAND`**
rather than misreading it, so this is the one protocol change that needs the
*receiving* boards flashed. A single-run write still goes out in the old shape,
which every node understands.

`axC6` broadcasts parameters C through H; `ax42:C6` sends them to node 42 and
waits for an ACK through the escalation ladder (direct, 2 hops, 4 hops).
The first parameter index travels in the body, so the receiver knows exactly
which block it is being asked to overwrite. Values go out as int8 when they all
fit and int16 otherwise — the opcode says which.

**Reading is `ag`, and it reads a block**: `ag42:DA8` asks node 42 for `DA` to
`DH` and gets them in one RESP. The GET body is `opcode first count`, and
`handleCommand` has always passed that count to `sendParameterResponse` — it was
only `ax42:A`, which pins it at 1, that made a read one slot at a time. So a host
upgrading to `ag` needs new firmware **on the bridge alone**; the nodes it
questions already answer correctly. A count is capped by
`LORA_MAX_PARAMETERS_PER_FRAME` (20), which is exactly what fits: 3 echoed
counter bytes + 2 header + 20 int16 = 45 of the 48-byte body. `ag` cannot be
broadcast, for the same reason `ax42:A` cannot — every node answering at once is
a response storm.

### `ar` is the one CMD that is not a parameter

`ar42:pr1234` types `pr1234` on node 42's own console and brings back what it
printed. It is the only way to reach a **verb** over the air: SET and GET are
the whole vocabulary otherwise, so before it there was no wire format for a
reboot, a peer dump or a wifi scan, and a node nobody can plug a cable into
could only be reconfigured, never operated.

What it sends is the command text itself — printable ASCII, lowercase verb
first — and what executes it is `printResult`, the same dispatcher the serial
task uses. That is the point: there is no second menu to keep in step with the
first, so every command the board grows is remotely reachable the day it lands.

Three things about it are load bearing:

- **It is answered twice**, an ACK when it is queued and a RESP once it has run.
  The command runs *after* its receipt precisely so `ar42:ub` can work: a node
  told to reboot never gets to send a RESP, and the ACK is then the only
  evidence the frame arrived at all.
- **It runs from the task loop, never from the receive path.** A console command
  is a whole console command — some block for seconds, some transmit, and `ar`
  can be nested — so executing it where it arrives would re-enter the radio from
  inside its own callback. One job is queued at a time; a second is NACKed with
  `LORA_REASON_BUSY` rather than replacing the request the RESP is addressed to.
- **The reply is one frame, truncated at 43 bytes**, with a flags bit saying so.
  What a node prints is unbounded — `ai` alone overruns it — while that frame
  already costs ~350 ms of a 36 s hourly budget. Paging a console over a
  channel with a duty cycle is
  not a trade worth making, so `ar42:ai` returns the beginning of the answer and
  admits it. Anything longer belongs on the node's own port.

`ar` cannot be broadcast: the answer is addressed back to the caller, so a
broadcast run would have every node transmitting its own console at once.

### Telemetry is the same block, sent periodically as DATA

There is no per-sensor frame type. A node broadcasts the parameter window
`DG`…`DG + DH` every `DF` seconds (0 = never), encoded exactly like an `ac`
copy — but as **DATA rather than CMD**, so a receiver *prints* the block instead
of applying it. A broadcast SET would have every neighbour overwrite its own `G`
with the tracker's latitude.

That is all a GPS tracker is: `taskGPS` writes the fix into `G`…`N` (latitude
and longitude are int32, each spread over two adjacent int16 slots via
`setParameterInt32`), and `DF20 DG6 DH8` puts those eight on the air. Any future
sensor joins the same way — write parameters, set the window. Adjacency is load
bearing: an int32 only survives the trip because both halves sit in the same
run of slots.

**The Bluetooth observer is the first sensor that joined that way**, and it is
why the window is `PARAM_TELEMETRY_FIRST` and `PARAM_TELEMETRY_BLOCK_SIZE`
rather than a GPS constant: `taskBLE` writes a beacon's RSSI into `O`, the slot
straight after the fix quality, so a board built with `BLE_SCAN` broadcasts
`DG6 DH9` and one run carries both. See *The Bluetooth observer* below.

**The cadence is a parameter and nothing else.** `resetParameters` writes `DF60`
when the env defines `GPS_RX`, and that literal is the only place a default is
decided — there is no build flag, because a firmware carrying its own interval
would silently disagree with the node it was flashed onto, and the node is the
one holding the value. Change it with `gt` or `DF` on the running board.

A minute is not a preference but what the sub-band leaves: the telemetry frame
is 29 bytes, **226 ms** on the air at the SF9/125 kHz default, so 60 frames an
hour take **13.6 s of the 36 s** sub-band M allows at 1 %. That is already 38 %
of one node's airtime for its own position, and the relays, the HELLOs and every
other node's traffic share what is left of the channel. `gt20` would ask for
40.7 s — past the whole allowance — and the governor **drops** what it cannot
pay for rather than sending it late, so a faster cadence does not degrade
gracefully, it goes missing. `gt` faster than about 30 s belongs in sub-band P.

**`gt` now says so itself**, because on an endpoint nothing prints when a frame
is dropped — the automatic sends report to `loraMeshSilent()`, so an over-budget
cadence is invisible until a bridge is counting, which is the wrong place and
usually the wrong day to find out. It prints the cost as a percentage of the
duty cycle and, past 100 %, what fraction will go missing and the two ways out:

    gt10
    Tracker every 10 s
    Window: G + 9
    Airtime: 247% of the duty cycle
    Over budget - 60% of frames will be dropped
    Either gt25, or move to sub-band P with DC18781,250

The number comes from `loraMeshBroadcastBudgetPercent`, which rebuilds the frame
the broadcast would send and prices it through the same `airtimeMillis` and
`dutyCycleDivisor` the governor uses — so it follows the carrier, and moving to
sub-band P changes the verdict rather than needing the advice rewritten.

`gt` is that setting with the window attached: `gt30` broadcasts the fix every
30 s, `gt0` stops, `gt` alone reports. It writes `DG` and `DH` too, since the
interval is the only free choice — a `DH` one short sends half a longitude every
period with nothing looking broken, and the plain `DF30` spelling cannot notice.
The window it reports is whatever the node holds, so a board carrying an older
firmware's six-slot window says so.

The last three of the eight — satellites (`L`), HDOP × 100 (`M`) and the GGA
fix quality (`N`) — travel with the position because a coordinate alone cannot
be weighted centrally: nothing else distinguishes a 4-satellite 2D fix from a
12-satellite one. They are published whenever *either* the location or the
satellite count updates, so a receiver that is still searching still reports how
badly it is searching. The bridge decorates the block with a decimal `hdop`
alongside `lat`/`lon`, the only other place a raw parameter is given a meaning.

The one frame a node sends on its own without being asked to carry anything is
the HELLO, every `DI` seconds (0 = never, **10800 — three hours — by default**),
plus one as soon as the task starts so a node that has just booted does not stay
out of its neighbours' peer tables for a whole period. It proves a direct link
and nothing else, which is why it is never relayed and why its cadence is
measured in hours: a peer table costs airtime to maintain, and airtime is the
budget everything else competes for.

### The three roles, and the bridge (`DA`, `src/lora/loraBridge.h`)

`DA` is `0 = endpoint`, `1 = repeater`, `2 = bridge`. A bridge is an endpoint
whose **console is a data feed rather than a log**: it emits one JSON object per
line on Serial, so a host reads the port line by line and stores what parses.

Everything else is **quiet by default** — an endpoint has no reason to narrate
its own traffic to a port nobody reads, so the automatic sends report to
`loraMeshSilent()`, a `Print` that discards. The bridge still sees them, because
the JSON is emitted by the send path itself rather than written to that stream.
That split is the whole design: `output` is who asked, the JSON feed is what
happened.

| Event | Emitted when | Carries |
|---|---|---|
| `raw` | **any** packet is received, before the key is consulted | `length rssi snr frame` (whole packet as hex) |
| `reject` | the tag did not verify | `length` — pairs with the `raw` line above it |
| `tx` | any frame leaves | `type dst counter ack` |
| `rx` | an authentic frame is heard, **including ones not addressed here** | `type src dst counter budget hops rssi snr fresh route body` |
| `params` | a DATA or RESP block arrives | `src`, then one member per parameter *label* (`"G":-15616`), plus `lat`/`lon` when the block covers the fix, `hdop` when it covers `M` and `rssi` when it covers `O` |
| `data` | a DATA body with an unknown opcode | `src opcode length` |
| `cmd` | a remote SET was applied here | `src status` |
| `exec` | a remote `ar` command is about to run here | `src cmd` |
| `console` | an `ar` reply arrives | `src text truncated` — `text` is escaped, so a quote or a newline in a node's output cannot break the line |
| `noack` | the escalation ladder gave up | `dst counter` |
| `peers` | `ap` on a bridge | `count`, then an array of `address counter rssi snr age` |
| `ble` | every `T` seconds on a bridge built with `BLE_SCAN`, one line per device heard in that window | `addr rssi best adv type`, plus `phy` and `ext` under `CONFIG_BT_NIMBLE_EXT_ADV`, `name` when the device advertises one and `tx` when it publishes a TX Power |

Every packet therefore produces **two lines**: `raw` before anything is trusted,
then `rx` (or `reject`). A bridge with no key, or the wrong one, still logs every
`raw` line — a capture survives a node that cannot read what it heard. `rx`
carries `route` as an array of `{address, rssi}` and `body` as the decrypted
plaintext in hex, so a host can archive what it cannot yet interpret.

Parameter labels are uppercase and the fixed keys lowercase, so a flat object
never collides. A command answered over MQTT or the web page is **echoed on
Serial** (`loraBridgeCopy`), so the host sees exchanges it did not start.

A bridge does not relay — `isRepeater()` is still only role 1. Set `AI1` on the
nodes that should extend range and `AI2` on the one plugged into the machine.

Human lines and JSON lines can still interleave on a bridge: typing `ai` on its
serial port prints the human block. A host should keep the lines that parse and
drop the rest.

### Radio settings and the duty cycle

Carrier, bandwidth and spreading factor are all runtime parameters, re-applied
without a reboot whenever one of them changes. The defaults are **868.4 MHz,
125 kHz, SF9**, and they are one decision rather than three, taken from the
channel outwards: 868.4 falls in the gap between the mandatory LoRaWAN channels
at 868.3 and 868.5, so the mesh has it to itself, which also fixes the bandwidth
at the 125 kHz that fits between them. Sub-band M then allows 1 % and 14 dBm —
36 s of airtime an hour — and SF9 is what that budget can afford: 226 ms for a
29-byte frame against 1647 ms at SF12.

`DC18781 DD250 DE12` is the other end of the trade. Sub-band P (869.4–869.65) is
the only part of the band that allows 500 mW and a 10 % duty cycle, and the
regulation lets it be used either as 25 kHz channels or as **one channel for
high speed data** — so 250 kHz is the only wideband shape allowed there and
869.525 the only centre that fits it, and the 10 % is what makes SF12 payable.
That is **+12.5 dB** against the default — 8 dB of transmit power, 7.5 dB of
processing gain, less 3 dB for the wider channel — so roughly twice the range,
for 3.6× the airtime per frame and an exchange that answers that much slower. It
is not a quiet channel: LoRaWAN gateways send their RX2 downlinks there at
27 dBm.

**`DC` counts 25 kHz steps above 400 MHz**, so the default 868.4 is `18736`
and 869.525 is `18781`. The step is the raster of sub-band P and the
origin keeps the SX1262's whole 150–960 MHz range inside a signed int16, so the
carrier needs no unsigned accessor. It counted 0.1 MHz until 2026-08, so
`frequencyCode()` refuses a stored 1500…9600 — unambiguously an old value, since
no band this radio uses lands there in the new encoding — and falls back to the
default. Nothing is converted or written back: a node that was never reset would
otherwise read its 8684 as 617.1 MHz, pass the range guard, and disappear.

**The duty cycle follows the frequency** and is not a constant —
`dutyCycleDivisor()` derives it from the carrier, falling back to the strictest
value for anything unrecognised:

| Sub-band | Range | Duty cycle |
|---|---|---|
| K | 863 – 865 MHz | 0.1 % |
| L / M | 865 – 868.6 MHz | 1 % |
| N | 868.7 – 869.2 MHz | 0.1 % |
| P | 869.4 – 869.65 MHz | 10 % |
| Q | 869.7 – 870 MHz | 1 % |

**`ad` hands the whole window back.** The bucket is the node's own bookkeeping,
and a poller left on a short interval empties it in an evening — after which the
node transmits at the refill rate, one second of airtime per hundred at 1 %, and
every command queues behind the last. `loraMeshResetAirtimeBudget` forgets the
spending, which is what makes a bench session usable again; it changes nothing
about what EN 300 220 allows. Reachable over the air as `ar42:ad`, like any
other console verb.

**It is a budget, not a delay.** EN 300 220 defines the duty cycle as transmit
time within an observation window — one hour — so the governor is a token
bucket, not a gap between frames: `airtimeBudgetMillis` holds the transmit time
still available, `refillAirtimeBudget()` credits it back at 1/N of real time,
and `transmitFrame` spends it. At the default 1 % that is **36 s of airtime per
hour**, which the node may burst through — roughly 159 frames of 226 ms back to
back at SF9/125 kHz — before it has to wait, and a frame it cannot pay for is
dropped rather than delayed. Enforcing a fixed post-transmission
silence instead would be far stricter than the regulation and would make a
retry ladder unusable. `LORA_DUTY_CYCLE_WINDOW_MS` shortens the window if you
want the node more conservative. RadioLib does not enforce any of this outside
LoRaWAN.

**Transmit power follows the carrier too**, for the same reason. `maxTxPowerDbm()`
returns **14 dBm** (25 mW ERP) across 863–870, 22 dBm in sub-band P, which allows
500 mW — more than the SX1262 can produce, so there the radio's own ceiling is
what binds — and **10 dBm** in 433.05–434.79, which allows only 10 mW. It is
applied at `begin()` and re-applied whenever the frequency changes, since moving
the carrier can move the limit.

It is deliberately **not** a parameter: there is no legitimate reason to raise
it, and a parameter is one typo away from transmitting illegally. The old value
was a flat 22 dBm inherited from the beacon code, roughly six times over the
limit at 868 MHz.

## The Bluetooth observer (`b`, `src/taskBLE.cpp`)

`-D BLE_SCAN=1` in an env turns on `THR_BLE`, which starts `taskBLE` and the
`(b)` serial menu. Two envs carry it, and they are the two halves of one job:

- **`[env:loraBridge]`** — mesh + BLE, **no GPS**. What a bridge runs. It emits
  a `ble` JSON line per device per sweep, which is how a tag gets *identified*.
- **`[env:loraBeacon]`** — `loraGPS` + BLE. The tracker. It monitors the one tag
  it was told to and reports the RSSI beside its fix.

**Identification is the whole reason the bridge listens.** A VespaFinder tag
advertises an anonymous address among thirty other anonymous addresses, so the
operator holds the tag against the bridge, takes the address off the strongest
line, and hands it to the tracker with the remote console — no cable, no reflash:

    ar42:bs3c:1a:cc:36:ad:10     node 42 now monitors that tag
    ar42:bk                      calibrate it, tag held at 1 m from node 42
    ar42:bi                      what node 42 hears, and how far it thinks it is

That works because `bs` is an ordinary console verb and `ar` runs console verbs —
nothing had to be added to the wire format for it.

**The bridge has no GPS on purpose, and that is what keeps it off the air.**
`PARAM_TELEMETRY_FIRST` is only defined when `THR_GPS` is, so a BLE listener
without a receiver has no broadcast window at all: it reports down the serial
port it is already plugged into rather than spending a duty cycle to tell the
host something the host is holding the other end of.

**The feed is one line per device per sweep, never per advertisement.** A single
tag produces hundreds a minute and two dozen devices would saturate 115200 baud
and starve the mesh's own JSON — the host would be reading Bluetooth while the
packets it exists to record went unwritten. `T` sets the sweep (0 = off, 10 s by
default). `adv` and `best` reset at each sweep, so a line describes its window;
`best` is the strongest sample in it, which is the one to identify by, being the
least obstructed path and what a tag held against the bridge produces.

Each entry is copied out under the mutex and printed outside it: a sweep is
several kilobytes at 115200 baud, and holding the lock across that would stall
the scan callback for as long as the printing takes.

It never connects to anything. It scans continuously — window equal to
interval, `setMaxResults(0)` so NimBLE keeps no results of its own — and the
advertisement callback does both jobs at once: it keeps a 24-entry table of what
is around, so `bl` can list it and `bs3` can pick the third line without anyone
knowing an address in advance, and when the advertisement comes from the
selected device its RSSI goes into `O`.

    bl          list what is being heard, * marks the selected one
    bs3         monitor line 3 of that list
    bsaa:bb:…   monitor an address directly, whether or not it is in the list
    bk          calibrate: this signal is 1 m    bk500  …is 5 m
    bi          selection, signal, range model, scan state
    bc          clear the list      bz  stop monitoring

**What it was built for is ranging [VespaFinder](https://www.robor-nature.eu/en/solutions/asian-hornet/)
tags** — BLE transmitters glued to an Asian hornet so it can be followed back
to its nest. So `bl` and `bi` also print an estimated distance, from the
log-distance model `d = 10 ^ ((R − rssi) / S)` with `R` the RSSI at one metre
and `S` the path loss exponent × 10.

`R` is **left unset on purpose and is not broadcast**. No datasheet publishes
what these tags transmit, a VFT80 does not transmit like a VFT160, and the
reference includes the antenna and the body of the insect it is glued to — so
it is measured against the tag in hand with `bk`, never assumed. Until it is,
no distance is printed at all: an uncalibrated one is a number that reads like
a measurement while pointing at the wrong field. `bk500` calibrates at a paced
five metres, which is far easier to set up outdoors than exactly one, and
solves the same model backwards.

**Never calibrate closer than about a metre.** 2.4 GHz is a 12.5 cm wavelength,
so a tag held at 5 cm is inside the near field, where the log-distance model
this uses does not apply at all — a reference taken there extrapolates wrong at
every range that matters. One metre is eight wavelengths and is comfortably far
field, which is the whole reason `bk` means one metre.

Calibrate in the medium you will search. Against the −97 dBm floor, a 1 m
reference of −64 dBm gives ~45 m in the open (`S20`) but ~13 m through
vegetation (`S30`) — the exponent moves the working radius further than any
antenna does.

Both constants stay **off the wire** (17 and 18, after the telemetry block):
they are properties of the receiver, so a host holding them once derives the
distance from the dBm itself, and neither is worth 2 bytes in every frame.

Treat the number as *warmer or colder*, not as a measurement. RSSI through a
hedge reads like RSSI across twice the open field, and a hornet turns its own
body between the tag and the antenna several times a second — which is what the
median is for. What finds a nest is the reading that keeps falling as you walk.

Four things are load bearing:

- **The RSSI is written with `setParameter`, never `setAndSaveParameter`.** An
  advertisement arrives several times a second and NVS is good for about
  100 000 writes; the periodic broadcast reads the parameter array, not flash.
- **The selection lives in NVS under `ble.mac`, as an address.** Six bytes do
  not fit in an int16, so it cannot be a parameter — and a table index would
  point at a different device after every reboot, so `bs3` is resolved to an
  address at the moment it is typed and the index is never stored.
- **What is reported is the median of the last eleven samples**, via the
  `getMedianInt11` already in `lib/hack`. A motionless beacon swings ten dB
  between two advertisements, and the frame that carries the reading goes out
  once a minute — a single sample would be whichever one happened to land last.
  The ring is primed with the first sample seen, so the median is defined from
  the first advertisement rather than after eleven.
- **Extended advertising is on (`CONFIG_BT_NIMBLE_EXT_ADV`), and it is not
  free.** Without it NimBLE calls `ble_gap_disc` and hears only legacy PDUs on
  the 1M PHY, so a BLE 5 extended advertiser is invisible at any distance —
  which is exactly how a tag sitting 5 cm from the antenna came to be
  unfindable. With it NimBLE calls `ble_gap_ext_disc` with scan params for the
  1M **and** Coded PHYs, so it also reaches long-range advertisers, at about
  −104 dBm against −97. The cost is that the controller time-slices between the
  two PHYs, so a legacy 1M tag is dwelt on roughly half as long as before. The
  feed carries `phy` so that trade can be settled from data rather than
  argued: a device only ever heard on PHY 3 is one a legacy scan would lose
  entirely.
- **It also changed what "a busy place" means**, which is why the table is 256
  and not 48. At 48 one office produced 275 distinct addresses in six sweeps —
  every slot turning over every sweep, so a device could be heard and never
  survive to be reported. That does not lose the weakest signal, it loses an
  arbitrary one, which to somebody hunting a specific tag is indistinguishable
  from the tag not being there.
- **The sensitivity floor is a cliff, not a fade**, and it is worth knowing
  where it is: about **−97 dBm** on the 1M PHY, ~−104 on Coded. RSSI only
  exists for a packet that decoded, so there is no faint-but-present region
  below it. Measured across 443 devices the tail runs flat through the low 90s
  (28 devices at −92, 11 at −97) and then collapses to 4, 3, 1 at −98, −99,
  −100 — the datasheet number showing itself in the capture.
- **The TX Power AD field is shown but never used as the calibration.** It is
  the radiated power, not the RSSI at a metre, and converting between them means
  assuming a path loss the receiver is trying to measure. Many tags do not
  advertise it at all, so `bi` reports it as information about the model.
- **Silence is reported as `ERROR_VALUE`, not as the last value heard.** After
  `BLE_BEACON_TIMEOUT_MS` (30 s) the slot goes unset and the bridge omits `rssi`
  entirely. The measurement is proximity, so "I cannot hear it" is an answer;
  a stale −71 dBm is a lie, and −32768 arriving as a number is how an averaged
  track ends up hundreds of dB below anything real.

The scan is restarted from the task loop whenever `isScanning()` goes false: the
NimBLE host resets on its own errors and comes back idle, and a tracker nobody
can plug a cable into has to notice that itself.

## The beacon (`[env:bleBeacon]`, `src/taskBLEBeacon.cpp`)

The other half of the observer, on a **XIAO ESP32C3** that does nothing else:
`KIND_BLE_BEACON`, `include/configBleBeacon.h`, `src/mainBleBeacon.cpp`. It
exists because the thing being hunted cannot be borrowed — a VespaFinder tag is
glued to a hornet — so `bs`, `bk` and the range model had nothing to be
exercised against. It is a tag that can be told what to be, and it says on the
air what it is.

Three parameters, and the advertising set is rebuilt whenever one changes, since
a beacon at the far end of a field is not somewhere you go to reboot:

    A18     dBm, -27 to +18            A-12 emulates a weak tag
    B100    ms between advertisements  B20 is the shortest allowed
    C3      1 = 1M legacy, 3 = coded   the numbering of the feed's phy

**`bi` is the only verb, because the three settings are parameters.** A `bp` /
`bt` / `by` that only wrote one slot each would be a second spelling of `A`,
`B` and `C` — a grammar to keep in step with itself for nothing, since the task
compares the parameters against what it last applied and picks up a change
whoever made it. That also means an out-of-range value is caught on the way
*out* rather than on the way in: `C2` stores, and the next boot treats the
block as never written and puts the three defaults back, since the PHY slot is
the tell (below).

Defaults are **coded PHY at +18 dBm every 100 ms**, which is the loudest and
furthest this board can be — roughly 25 dB over a phone advertising on 1M, which
is the difference between across a room and across a field.

- **+18 dBm is where the chip stops, and it is also where the law does.**
  `esp_power_level_t` is a 3 dB ladder from −27 to +18, and EN 300 328 allows
  20 dBm EIRP at 2.4 GHz — the antenna is worth a couple of those. So a value
  between two rungs is rounded **down**, never up, and the ceiling is a clamp
  rather than an error. Unlike the mesh's transmit power this *is* a parameter:
  emulating a quiet tag is the point, and the top of the range is legal.
- **PHY 1 means legacy, not just 1M.** Legacy PDUs exist only on 1M, and being
  seen by a phone or any pre-BLE5 scanner is the whole reason to ask for that
  PHY — an *extended* advertisement on 1M is no more visible to them than a
  coded one. `C3` is ~7 dB further and invisible to anything not running an
  extended scan, which `bi` says rather than leaving it to be discovered.
- **The interval changes nothing about range and everything about being found.**
  A listener only hears the windows its own sweep covers, so a beacon somebody
  is walking towards should repeat several times a second. It also sets how fast
  the observer's reading follows: that reading is the median of eleven samples,
  which is ~1 s of them at `B100` and ~11 s at `B1000` — long enough that a
  slow beacon still reports where it used to be.
- **It publishes the TX Power AD field**, so the observer's `bi` reports what it
  radiates. That is honest here and only here: the field is radiated power, and
  the observer still refuses to use it as a calibration.
- **The PHY parameter is what says the block was never written.** An untouched
  NVS key reads 0, not `ERROR_VALUE`, and 0 dBm is a legitimate power — but 0 is
  not a PHY anyone chose. Same tell as the mesh's radio triple.

`processBleCommand` is defined by *either* `taskBLE.cpp` or `taskBLEBeacon.cpp`,
never both — `THR_BLE` and `THR_BLE_BEACON` are mutually exclusive, and the `(b)`
menu in `lib/hack` dispatches on either.

Pair it with a listener by holding it against the bridge and reading the address
off the strongest `ble` line, exactly like a real tag; `bi` prints the `bs…`
command to type on the other board.

## HTTP (`src/http.cpp`)

One shared 1000-byte `httpBuffer` and one shared connection for every fetch, so:

- **Payloads must stay well under 900 bytes.** The compact wall payload is ~110.
- The connection is **kept open between requests to the same URL**. Measured
  against the backend: a reused connection costs ~3 ms, a cold one ~13 ms, and
  the server's keep-alive window is 5 s — so polling *faster* than 5 s is cheaper
  per fetch than polling slower, which always reconnects. This is why the wall
  polls at 2.5 s. Changing the URL (the forecast is on another host) closes it.
- HTTPS validates against the bundled root store via `crt_bundle_attach`; no
  certificate is pinned.
- `fetch()` logs a line **only when it has to open a connection**, with the
  handshake time. On the serial console, silence means reuse is working.

## Things that have bitten before

- `httpBuffer[MAX_HTTP_BUFFER] = {0}` wrote one past the end of the array and
  cleared nothing, so a short response kept the tail of a longer previous one.
  Use `memset`.
- The response write cursor must be reset per request, not only on
  `HTTP_EVENT_ON_CONNECTED` — a **reused** connection never raises that event.
- A TLS handshake needs several KB more stack than plain HTTP. `TaskFetch` runs
  with 24576 bytes for that reason; a stack overflow here is a reboot loop.
- `TaskOTA` must stay at priority 3 — the comment in `taskOTA.cpp` says it
  crashes otherwise.

## Style

Follow the existing C++: 2-space indent, `lowerCamelCase` functions and
variables, `UPPER_SNAKE` macros, `/* */` block comments above a function
explaining *why*. Comment sparingly and only where the reason is non-obvious.
