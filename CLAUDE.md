# esp32-c3 — notes for Claude

Firmware for a family of ESP32 boards. One codebase, many `[env:…]` targets in
`platformio.ini`; each selects a `BOARD_TYPE` and pulls the matching
`include/config<Kind>.h`. Most files are compiled for every target, so guard
target-specific code with the build flags rather than assuming a target.

## Build and upload

PlatformIO is not on `PATH`. Use the full path:

    ~/.platformio/penv/bin/pio run -e <env>
    ~/.platformio/penv/bin/pio run -e <env> -t upload

`default_envs = droneTracker`, so a bare `pio run -t upload` targets the drone
watcher, which flashes over its built-in USB-JTAG.

**Always build before claiming a firmware change works.** A build is ~12 s once
the toolchain is warm.

The host tests cover the drone tracker's frame parsing and records, the test
transmitter's frames, and the mesh HELLO body with the location line a host
parses:

    ~/.platformio/penv/bin/pio test -e native

`[env:native]` empties the inherited `framework` and `lib_deps` - a host builds
neither Arduino nor two dozen board libraries - and `build_src_filter` picks out
the source files that need no Arduino. See § The drone watcher.

### Devices

| Env | Board | Address | What it is |
|---|---|---|---|
| `square` | `seeed_xiao_esp32s3` | **192.168.1.200** | The 16×16 energy wall |

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

**The three radio values it writes are the macros the accessors fall back to**,
`LORA_FREQUENCY_DEFAULT` / `LORA_BANDWIDTH_DEFAULT` /
`LORA_SPREADING_FACTOR_DEFAULT` in `configLoraMeshParams.h` — one declaration
each, because a reset writing its own literals would put a fresh board on a
different channel from the one an unwritten board falls back to, and the two
would never hear each other.

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
`MAX_PARAM` is at least 114 wherever the mesh is enabled. A board that sets a smaller
`MAX_PARAM` after including the header fails to compile rather than writing past
`parameters[]`.

The **GPS block does not travel as easily**: `configLoraMesh.h` puts the fix at
6–13, which on the pixels board is `PARAM_BLUE` through `PARAM_INT_TEMPERATURE_A`,
and nothing below 104 there offers eight adjacent free slots. The drone watcher
hit the same wall — 10–12 are its `K`, `L`, `M` — and reserves 114–121
(`DK`…`DR`) above the mesh block, declaring `MAX_PARAM` 122 before including
`configLoraMeshParams.h`. A pixels tracker would do the same. The one cost: a
bridge decorates `lat`/`lon` from *its own* `PARAM_GPS_LATITUDE`, so it only
decorates telemetry from boards numbered like itself. The HELLO carries degrees,
not slots, and is unaffected.

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

    from(1) seal(3|4) | E( ctrl(1) src(1) dst(1) counter(3|4) body route(2h) budget,hops(1) ) | mic(4)
    \__ clear, AAD __/     \_______________________ encrypted _______________________/

    ctrl, bit 7 to 0:     ver(1)=1 type(3) spare(4)
    trailer, bit 7 to 0:  budget(4) hops(4)
    a counter:            big-endian, 3 bytes below 2^23, 4 with bit 7 set above

15 bytes of overhead, plus 2 per recorded hop. AES-128-CCM with a 4-byte tag
does confidentiality and authenticity in one pass; there is no second key and no
separate CMAC. Only `from` and `seal` are readable without the key.

- **Every transmission is sealed afresh by whoever makes it** — the origin, each
  retry, each relay — under a nonce built from its own address (`from`) and a
  counter it has never used (`seal`). That is the whole reason a relay can
  record its passage and a retry can raise the budget: both change the
  plaintext, and encrypting two plaintexts under one nonce is the one misuse CCM
  does not survive. `sealAndTransmit()` in `taskLoraMesh.cpp` is the only place
  a frame is sealed, and a message's first transmission passes the counter it was
  just given while every later one passes `nextCounter()`. Passing anything else
  there reuses a nonce.
- **So a frame carries two numbers.** The seal only has to be unique. The
  counter names the message and stays the same across every copy and every
  retry, which is what duplicate suppression, anti-replay, relay cancellation and
  the ACK echo key on — never the seal, which differs in every copy.
- **Nothing on the air is unauthenticated.** The route and the budget are inside
  the ciphertext, so a replayed or rewritten frame fails its tag. A key holder
  can still claim anything, which is why `LORA_TTL_MAX_ACCEPT`, not the budget,
  caps amplification. What a listener without the key still learns is who
  transmitted each copy, how often, how long it was and how strong it arrived.
- **The plaintext is read from both ends**, which is what makes it
  self-describing: the counter's first byte gives the header size, the last byte
  gives the hop count, the number of stored route entries follows from it
  (`min(hops, LORA_ROUTE_MAX)`), and the body is whatever lies between. No length
  field, no flag bit.
- **A route entry is `address(1) rssi(1)`** — the dBm at which *that* relay heard
  the frame, so one reception carries the margin of every hop it crossed. The
  last hop is deliberately absent: the receiver measures that one itself, and
  `from` says who it was. Past `LORA_ROUTE_MAX` (4) the hops keep counting and
  stop being recorded, so `hops > 4` is how a truncated route announces itself —
  and `from` is then the only record of the relay that transmitted the copy
  heard.
- **One counter sequence feeds seals and messages, and it must never go
  backwards**: a seal used twice is a nonce used twice. `mesh.counter` in NVS
  therefore holds a **reservation**, not the live value — a promise that nothing
  above it was ever used. The node claims `LORA_COUNTER_RESERVATION` (100) at a
  time and restarts at the bound, so a crash mid-block skips forward over the
  counters it may or may not have spent. That is the flash-wear knob: one NVS
  write per 100 transmissions, paid for by burning 100 counters on every boot.
  A repeater now spends one per copy it relays, so a node's message counters
  are no longer consecutive.
- A counter widens permanently past 2²³. The nonce always holds the full 32-bit
  seal, and its first byte (`0x01`) differs from every nonce the previous
  version-0 envelope built, so neither the widening nor the upgrade can collide.
- **The codec needs neither Arduino nor a radio**, so `test/test_lora_frame`
  runs it on the host against frames sealed by Node's `aes-128-ccm` — the
  decoder the frame document gives a host. The host needs mbedtls
  (`brew install mbedtls`), found through pkg-config.
- **The retry ladder is three attempts, one per rung** — direct, two relayed
  hops, then three (`LORA_LADDER_ATTEMPTS`). The top rung is three because
  that is the most a relay accepts: it used to be four, which the first relay
  refused, so the last attempt was a second direct one. It holds the node's single
  confirmed-request slot for its whole length, so its duration is what every
  other addressed command waits for before being refused with `A confirmed
  request is already in flight`. The first two rungs used to be doubled, which
  ran 10.5 s for a short frame at SF9/125 kHz and 18 s for a full one — long
  enough that a host polling on a ten second timer refused nearly everything
  else the mesh had to say. A node not heard *directly* in the last half hour
  skips the direct rung and starts at two hops — one heard only through a relay
  is not a neighbour, however recently its frames arrived.
- **A relay verifies the MIC before forwarding**, so only authentic group
  traffic is ever amplified. It then dedups on `(src, counter)`, **waits for the
  frame's answer**, then a random 0…3× airtime, and cancels its copy if it
  hears that answer or two other nodes relay the same message. Skipping any of
  these turns a flood into an N² storm. The relay queue holds the decoded frame,
  not bytes: its passage is recorded with `loraFrameRecordRelay` and the copy is
  sealed only when it leaves, so a cancelled copy costs no counter.
- **A frame the bridge already has is not repeated** (`src/lora/loraRelayPolicy.h`,
  tested on the host by `test/test_lora_relay`). A bridge answers every HELLO
  and DATA meant for the host that could still travel with a **receipt** — an
  ordinary ACK to the source echoing its counter, budget = the hops the copy
  took — sent before it prints anything. A repeater waits
  `LORA_REPLY_TURNAROUND_MS` (100) plus the answer's airtime before its jitter,
  about 0.2 s at the defaults, and drops its copy when it hears the receipt, or
  the destination's ACK/RESP for a command. Only a repeater that hears no
  answer carries the frame on, which is the one whose copy is needed; a
  repeater out of the bridge's range relays as before, so nothing is lost. The
  bridge keeps a fifth of its airtime back and stops receipting below it
  (`LORA_RECEIPT_RESERVE_DIVISOR`): a receipt is ~93 ms, so a watcher
  reporting every 5 s costs the bridge ~19 % of its hour, against ~37 % of
  every repeater's that two relayed copies used to cost.
- **An answer retraces its request.** A repeater carries an ACK, NACK or RESP
  on only if it relayed the message it answers — replies already echo that
  counter, so this is a 16-entry memory on the node (`LoraRelayedMemory`), not a
  field on the wire. A reply no longer floods the mesh up to its budget.
- **The budget counts up, not down**: a frame travels while `hops < budget`, and
  a `budget` of 0 means "do not relay" — that, not the broadcast address, is the
  direct-vs-flood switch. A relay also refuses anything whose *remaining* budget
  exceeds `LORA_TTL_MAX_ACCEPT`, which caps amplification whatever the sender
  claims. An ACK or a RESP is sent back with a budget of the `hops` the request
  actually took, which is a measurement rather than the guess the countdown gave.
  The ladder's timeout pays for the relays' wait on the way out.
- **A retry keeps the message counter.** Incrementing it would make the
  receiver execute the command twice, because it cannot tell a lost ACK from a
  second command. It is sealed again under a new seal, since the budget it
  carries has changed.
- Broadcasts are never acknowledged by the nodes they reach — 255 nodes
  answering one frame is an ACK storm. The one answer to a broadcast is a
  bridge's receipt, and only for what is meant for the host.

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
waits for an ACK through the escalation ladder (direct, 2 hops, 3 hops).
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

A minute is what the sub-band leaves room to change: the telemetry frame is
33 bytes, **124 ms** on the air at the SF9/250 kHz default, so 60 frames an hour
take **7.4 s of the 360 s** sub-band P allows at 10 % — about 2 % of one node's
airtime for its own position, with the relays, the HELLOs and every other node's
traffic sharing the rest. `gt10` is 45 s, still an eighth of the budget, so on
the default carrier a tracker is paced by what is worth knowing rather than by
the regulation. On 868.4 MHz the same frame costs 247 ms against a 36 s
allowance, where a minute is **14.8 s — 41 %** and `gt20` asks for more than the
whole hour: the governor **drops** what it cannot pay for rather than sending it
late, so a faster cadence there does not degrade gracefully, it goes missing.

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
    Either gt25, or DC18781,250 if this node left sub-band P

The number comes from `loraMeshBroadcastBudgetPercent`, which rebuilds the frame
the broadcast would send and prices it through the same `airtimeMillis` and
`dutyCycleDivisor` the governor uses — so it follows the carrier, and moving to
sub-band P changes the verdict rather than needing the advice rewritten.

**A position is not broadcast when there is none.** `publishFix()` only writes a
location TinyGPSPlus calls valid, which is what keeps a half-parsed sentence out
of the parameters — and it is also why the last good fix stays in those slots for
ever once the receiver stops solving. A tracker carried indoors would otherwise
go on announcing the doorway it last saw the sky from, every `DF` seconds,
indistinguishably from a node standing there, and at `gt5` that is a quarter of
the duty cycle spent saying something untrue. So `loraMeshBroadcastParameters()`
asks `gpsHasCurrentFix()` first and holds the frame, printing once on each
transition:

    LoRa mesh started, address 1, counter 20600, key set
    No GPS fix, holding the broadcast

Three things about that test. **The age of the fix is what decides it**, not
`isValid()`, which answers for the last position ever seen and never lapses;
`GPS_FIX_MAX_AGE_MS` is 30 s, about thirty missed GGA sentences, while the
slowest cadence anyone sets is a minute — so a tracker standing still, which
re-solves every second, is never held. A GGA reporting quality 0 is taken as the
answer immediately rather than waiting the age out. **It applies only when the
window actually carries the coordinates** (`DG`/`DH` covering `G`…`J`): the
broadcast is generic, and a board sending a temperature has nothing to do with a
fix. And **nothing else is suppressed** — HELLO keeps its own interval, so a node
with no position still appears in every peer table.

The satellite count and the fix quality cannot be sent on their own to say
"still searching": they sit in the same run as the position, so the frame is all
of it or none of it.

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

The one frame a node sends on its own without being asked is the HELLO, every
`DI` seconds (0 = never, **10800 — three hours — by default**), plus one as soon
as the task starts so a node that has just booted does not stay out of its
neighbours' peer tables for a whole period. Its cadence is measured in hours
because a peer table costs airtime to maintain, and airtime is the budget
everything else competes for.

**A HELLO travels with the default budget `DB`**, like a report, so a post out
of the bridge's range still gets its position to the host — the bridge's
receipt keeps it from being repeated when the bridge heard it directly. It used
to be budget 0, which left a far post unplaced. A bridge's own HELLO stays at 0:
nobody receipts it, so every repeater around would carry it for nothing. A
repeated HELLO still proves only the link its last hop crossed, which is how
the peer table already files it (under `from`).

**It also says where the node is** (`src/lora/loraHello.h`), because a drone
watcher on a fence has no GPS and the host still has to place it — and to
rebuild the `TRACK` coordinates it relays, which are put back against the
position of the post that sent them:

    flags(1) [latitude(4) longitude(4)]
    flags: bit 0 a position follows, bit 1 it is a current GPS fix (clear:
           placed by hand), bit 2 built with THR_DRONE_ID, bit 3 relays
           (DA1), bits 4-7 zero
    latitude, longitude: int32 little-endian, degrees x 1e6, the scale of G..J

A fix is announced only while `gpsHasCurrentFix()` vouches for it, then a
position placed by hand with `al`, then none — the flags alone. **The body could
grow because nothing reads it**: an older receiver treats a 9-byte HELLO exactly
like an empty one, and a bridge of any version prints it as hex on its `rx`
line, so the host decodes it and no bridge had to be reflashed. An empty body is
what older firmware still sends. With a position the frame is 20 bytes, 93 ms
against 73 at the defaults.

`al46.5191,6.5668` — or `al46.5191, 6.5668`, as a map copies it — stores the
position in NVS (`mesh.lat`, `mesh.lon`) and broadcasts a HELLO at once, so a
bridge in range learns it now rather than in up to three hours; `al0` forgets
it, and `al` alone prints the line the host parses —
`Location: 46.519100,6.566800 (fixed)`, `(gps)` for a live fix, or
`Location: not set`. The line reports what a HELLO would carry right now, so a
current fix wins over the hand-placed position. An untouched NVS key reads 0, so
0,0 is "not set", and `al0,0` forgets rather than placing a node in the Gulf of
Guinea.

`ai` prints the same line after `Role:`, then `Drone watcher: yes` on a build
with `THR_DRONE_ID` and `Drone watcher: no` otherwise — bit 2 of the HELLO,
said the only way a bridge can say it at connect: its own HELLO never comes
back up its port, so without the line the host would know it about every node
but the one it is plugged into. After that the bridge's `tx` line carries the
body of every HELLO it sends, so a bridge with a GPS keeps the host up to date
with its fix — one it got after the `ai` it answered on connect included.

A node can also be asked where it stands: `ar<address>:al` brings back the same
line in one frame — 40 characters at most, inside the 43 bytes a console reply
holds.

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
| `tx` | any frame leaves | `type dst counter ack body` — `body` the plaintext in hex, as on `rx` |
| `rx` | an authentic frame is heard, **including ones not addressed here** | `type src dst from counter budget hops rssi snr fresh route body` |
| `params` | a DATA or RESP block arrives | `src`, then one member per parameter *label* (`"G":-15616`), plus `lat`/`lon` when the block covers the fix, `hdop` when it covers `M` and `rssi` when it covers `O` |
| `data` | a DATA body with an unknown opcode | `src opcode length` |
| `cmd` | a remote SET was applied here | `src status` |
| `exec` | a remote `ar` command is about to run here | `src cmd` |
| `console` | an `ar` reply arrives | `src text truncated` — `text` is escaped, so a quote or a newline in a node's output cannot break the line |
| `noack` | the escalation ladder gave up | `dst counter` |
| `peers` | `ap` on a bridge | `count`, then an array of `address counter rssi snr rssiAge age` — `rssi`, `snr` and `rssiAge` only for a node heard directly, `counter` only once one of its own messages arrived |
| `ble` | every `T` seconds on a bridge built with `BLE_SCAN`, one line per device heard in that window | `addr rssi best adv type`, plus `phy` and `ext` under `CONFIG_BT_NIMBLE_EXT_ADV`, `name` when the device advertises one and `tx` when it publishes a TX Power |
| `drone` | every `Q` seconds per transmitter, on a bridge built with `THR_DRONE_ID`, while an aircraft is being heard | `addr via uas status lat lon alt height heightRef speed vspeed heading hacc vacc rssi ch` — the unknown ones omitted rather than sent as the standard's -1000 |
| `pilot` | the operator's position arrives, or moves more than `M` metres | `addr via uas lat lon alt source category class rssi` |
| `ident` | an aircraft is first identified, or renames itself | `addr via uas idType uaType operator selfId version` |
| `lost` | a transmitter is dropped after `E` seconds of silence | `addr via uas seen silent messages best` |

Every packet therefore produces **two lines**: `raw` before anything is trusted,
then `rx` (or `reject`). A bridge with no key, or the wrong one, still logs every
`raw` line — a capture survives a node that cannot read what it heard, though
without the key `from` and `seal` are all such a line can say. `rx` carries
`route` as an array of `{address, rssi}` and `body` as the decrypted plaintext in
hex, so a host can archive what it cannot yet interpret, and `from`, the node
whose transmission was heard — the source when it came direct, otherwise the
last relay, even one past what the route records. That is the node the `rssi`
belongs to, and the one the peer table files it under: a `peers` entry carries a
margin only for a node heard directly, dated by that reception (`rssiAge`)
rather than by whatever last arrived from it through a relay (`age`).

Parameter labels are uppercase and the fixed keys lowercase, so a flat object
never collides. A command answered over MQTT or the web page is **echoed on
Serial** (`loraBridgeCopy`), so the host sees exchanges it did not start.

A bridge does not relay — `isRepeater()` is still only role 1. Set `DA1` on the
nodes that should extend range and `DA2` on the one plugged into the machine.

The four drone lines are the exception to "a bridge is an endpoint that only
relays what it hears": they are what *this* board heard on 2.4 GHz, and on a
bridge they **replace** the console blocks rather than joining them — the same
trade `loraMeshReportData` makes. `di` says which of the two a board is doing,
because a port that has gone quiet otherwise reads as a dead radio.

Human lines and JSON lines can still interleave on a bridge: typing `ai` on its
serial port prints the human block. A host should keep the lines that parse and
drop the rest. It sends `ai` itself on every connect, because three lines of
that block say what nothing else does about the bridge: `Address: N` (the
bridge's own address appears nowhere in the JSON), the `Location:` line, and
`Drone watcher: yes` or `no`.

### The repeater (`DA1`, `[env:loraGPS]`)

**A normal repeater is `loraGPS` set to `DA1`, with or without a GPS**, and its
HELLO says so with bit 3 — the host has no other way to tell a repeater from an
endpoint before it has carried anybody's frame. With a
receiver's TX on D7 — Seeed's L76K for the XIAO sits exactly there, at 9600
baud, which `detectGpsBaud` finds — every HELLO carries the current fix, flags
`0x03`. Without one the probe finds nothing and the HELLO carries the flags
alone, or a position placed with `al`. The HELLO is sealed like any other
frame, so the coordinates only ever travel encrypted.

On its console:

    an7                    its address
    ak<group key>          the one the bridge's ai prints
    DA1                    relay
    DI300                  a HELLO every five minutes

`DI` defaults to three hours, right for a node proving a link and wrong for one
whose position is the point: 300 s is twelve 93 ms frames an hour, 1.1 s of the
360 s sub-band P allows. The HELLO being the carrier has two consequences:

- **The boot HELLO leaves before any fix** — a cold start takes tens of seconds
  under open sky and never ends indoors — so the first one with coordinates is
  the next `DI` period. `ar7:ah` sends one on demand.
- **A HELLO is repeated only when the bridge did not hear it**, so a post out
  of the bridge's range is placed too, at the cost of the relays that carry
  it. `ar7:al` asks for the position at any time.

**A bridge can carry the GPS too**: `loraGPS` set to `DA2` is a bridge with a
receiver on D7 (and no Bluetooth scan, which only `loraBridge` builds). Its
HELLO never reaches its own port, so the host reads the same flags and fix off
the `tx` line of each HELLO it sends, and off `ai` on connect.

`gt` reports *Tracker off* on a repeater and that is fine: the telemetry window
would repeat the same fix as relayed DATA every `DF` seconds, which a node that
does not move has no use for.

| Node | Role | Image | Notes |
|---|---|---|---|
| 3 | bridge | `droneTracker` | USB serial `E8:06:90:A1:02:AC`, no GPS fitted yet |
| 5, 6 | repeater | `droneTracker` | the prison project's drone watchers; still on the old envelope |
| 7 | repeater | `droneTracker` | USB serial `68:EE:8F:62:F4:A8`, L76K GPS, `DI300` |
| 8 | repeater | `droneTracker` | USB serial `68:EE:8F:62:FA:B8`, L76K GPS, `DI300` |

### Radio settings and the duty cycle

Carrier, bandwidth and spreading factor are all runtime parameters, re-applied
without a reboot whenever one of them changes. The defaults are **869.525 MHz,
250 kHz, SF9**, and they are one decision rather than three, taken from the duty
cycle backwards: sub-band P (869.4–869.65) is the only part of the band that
allows 500 mW and 10 % — 360 s of airtime an hour, and the radio's own 22 dBm
instead of the 14 dBm the rest of the band permits. The regulation lets P be
used either as 25 kHz channels or as **one channel for high speed data**, and
869.4–869.65 is exactly 250 kHz, so the bandwidth follows the carrier and 869.525
is the only centre that fits. SF9 is then what the budget can afford without
spending it: 124 ms for a 33-byte frame, nearly three thousand an hour.

**Airtime is what this mesh runs out of first**, which is what chooses P: a
tracker reporting every 10 s spends 45 s of the hour, 12 % of the allowance here
and 247 % of the 1 % a quieter sub-band grants — it does not fit there at any
spreading factor. The price is company, since 869.525 is also every LoRaWAN
gateway's RX2 downlink, at 27 dBm.

`DC18736 DD125 DE9` is the other end of the trade: 868.4 falls in the gap between
the mandatory LoRaWAN channels at 868.3 and 868.5, so the mesh has that channel
to itself, at 125 kHz — the widest that fits between them. It gives up 8 dB of
transmit power and nine tenths of the airtime, and takes 3 dB of sensitivity back
from the narrower channel: **−5 dB net**, for the quiet. `DE12` on the default
carrier is the opposite move, **+7.5 dB** for 7.3× the airtime per frame, worth
making only for a link that will not otherwise close.

**`DC` counts 25 kHz steps above 400 MHz**, so the default 869.525 is `18781`
and 868.4 is `18736`. The step is the raster of sub-band P and the
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
node transmits at the refill rate, one second of airtime per ten at the default
10 %, and every command queues behind the last. `loraMeshResetAirtimeBudget` forgets the
spending, which is what makes a bench session usable again; it changes nothing
about what EN 300 220 allows. Reachable over the air as `ar42:ad`, like any
other console verb.

**It is a budget, not a delay.** EN 300 220 defines the duty cycle as transmit
time within an observation window — one hour — so the governor is a token
bucket, not a gap between frames: `airtimeBudgetMillis` holds the transmit time
still available, `refillAirtimeBudget()` credits it back at 1/N of real time,
and `transmitFrame` spends it. At the default 10 % that is **360 s of airtime per
hour**, which the node may burst through — roughly 2903 frames of 124 ms back to
back at SF9/250 kHz — before it has to wait, and a frame it cannot pay for is
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

## The drone watcher (`d`, `[env:droneTracker]`, `src/droneId/`)

`KIND_DRONE_TRACKER`, `include/configDroneTracker.h`, `src/mainDroneTracker.cpp`.
A XIAO ESP32S3 with the Wio-SX1262 that listens for drone Remote ID. It is a
mesh node too - `configDroneTracker.h` takes `configLoraMeshParams.h` the way
CLAUDE.md's two-line recipe says, so the block at 104 to 113 and `MAX_PARAM`
114 arrive with it, and the drone parameters sit between 0 and 16, well clear
(122 with the GPS block above the mesh one). Three
radios, of which only Bluetooth and Wi-Fi compete: LoRa is a separate chip at
868 MHz. The mesh earns its place through `ar`, which runs a console command on
another node - so `ar42:dl` lists what node 42 can see from where it stands,
and no new frame type was needed for it. The full
reference — the four transports, the console, the parameters — is in
[docs/drone-remote-id.md](docs/drone-remote-id.md); what follows is what matters
when changing the firmware.

**The decoding is not ours.** `lib/opendroneid/` is
[opendroneid-core-c](https://github.com/opendroneid/opendroneid-core-c) vendored
unmodified at commit `6484f26`, Apache-2.0, and it owns every byte of the
message format. It is vendored rather than named in `lib_deps` because **nothing
in the opendroneid organisation is packaged for PlatformIO or the Arduino
Library Manager** — no `library.json` or `library.properties` in any of its nine
repositories, so a `lib_deps` git URL clones it and then fails to build: the
headers in `libopendroneid/` never reach the include path, and `libmav2odid/` is
compiled too and wants MAVLink. `lib/tinyexpr` is here for the same reason.
Do not edit those files; what this project adds lives in `src/droneId/`.

Three `ODID_*` build flags configure it, and they are in **`build_flags`, not
`build_src_flags`** — the latter never reaches `lib/`. `ODID_AUTH_MAX_PAGES=9`
is a floor, not a preference: `checkPackContent()` refuses a whole message pack
carrying more authentication pages than that, and a pack holds at most nine
messages, so anything smaller would throw away an aircraft's position because of
signature pages this board does not read.

**Four transports, one message format.** Bluetooth 4 legacy advertising carries
exactly one 25-byte message, because the service data structure fills all 31
bytes a legacy payload has; Bluetooth 5 Long Range carries a pack of up to nine;
Wi-Fi puts a pack in a beacon's vendor element or in a NAN action frame. Both
Bluetooth methods arrive through one scan — `CONFIG_BT_NIMBLE_EXT_ADV` makes
NimBLE call `ble_gap_ext_disc()` with the same parameters for the uncoded and
the coded PHY, so there is nothing to configure for BT5.

**Both radios are one radio, so they take turns.** `A` seconds of Bluetooth then
`B` seconds of Wi-Fi (7 and 3), with the scan stopped and the receiver closed at
each handover — Espressif rates Wi-Fi promiscuous receive alongside Bluetooth as
supported but *unstable*, and with Wi-Fi idle the arbiter hands the radio to
Bluetooth, so an explicit slice is the only one that holds. Either at 0 gives
the whole radio to the other.

Four things are load bearing:

- **A scan started with duration 0 is not endless under extended advertising.**
  NimBLE turns 0 into `BLE_HS_FOREVER`, then divides by 10 for the extended
  API's 10 ms units, where the parameter is 16 bits — so it truncates to about
  524 seconds and the controller stops. `droneIdBleListen()` is called every
  loop and restarts it; a receiver that started once would go deaf after nine
  minutes with nothing looking wrong.
- **`setAdvertisedDeviceCallbacks(cb, true)` is what turns the duplicate filter
  off**, and without it an aircraft's identity would arrive once and its
  position never — one address reports once, and Remote ID sends five different
  message types from one address.
- **`decodeOpenDroneID()` takes no length.** For a pack it casts to a 228-byte
  struct and reads `3 + 25 * MsgPackSize` before any of its own checks run, so
  `droneIdDecode()` bounds the buffer first. A nine-message pack over Bluetooth
  5 is a separate case and is caught earlier: past 229 bytes the controller
  splits the report, NimBLE 1.4.3 does not reassemble, so neither fragment is a
  valid AD structure and the walk drops both — `droneIdBle.cpp` counts those
  itself, since they never reach the decoder to be counted as refused.
- **The frame parsing is testable, and tested.** `src/droneId/droneIdFrames.cpp`
  is the one file here with no `config.h` and no `THR_DRONE_ID` guard, so it
  builds on a host: locating the payload is where an off-by-one decodes garbage
  as a position, and `pio test -e native` runs it against real frames from the
  reference transmitter, frozen in `test/test_droneid_frames/fixtures.h`.
- **Neither radio callback prints.** The Wi-Fi one runs inside the driver task,
  where a hundred bytes at 115200 baud is nine milliseconds of lost frames; the
  Bluetooth one runs on the NimBLE host task with a 4 kB stack. Both filter,
  copy into `droneIdQueue` and return; `TaskDroneId` decodes and prints.

**One row is one transmitter, not one aircraft.** A drone on both radios uses a
different address on each, and the reference transmitter uses a different one
again for its BT4 and BT5 advertising sets, so one aircraft can occupy four
rows. They are not merged because merging means trusting the UAS ID, which is
the field a spoofer picks; `dl` marks the rows that agree on one instead.

Detecting a drone that is **not** cooperating - one with Remote ID switched off -
is evaluated in [docs/drone-rf-detection.md](docs/drone-rf-detection.md) and not
built. Three findings decide it: the ESP32's Wi-Fi radio cannot be made into a
spectrum sensor at all (no energy-detect API exists in any IDF version, and the
ROM symbol that looks like one returns a constant while the `libphy` one that
sounds like one *transmits a tone*); the SX1262 already here **is** a real
sub-GHz sensor and can decode ExpressLRS 900 at SF7-SF9; and the cheapest win of
all is software - DJI's own DroneID rides in a Wi-Fi beacon vendor element under
OUI `26 37 12`, carrying the serial number, the aircraft position and **the
pilot's location**, decodable by the beacon walk in `droneIdFrames.cpp` with one
more `memcmp`. Note its coordinates scale by 174533.0, not 1e7.

Getting what it hears into a database — `lpatiny/loramesh-monitoring`, a map and
an intrusion alert — is designed in
[docs/drone-mesh-forwarding.md](docs/drone-mesh-forwarding.md). Both paths are
built on the firmware side: the JSON feed on a bridge, and on every other board
`src/droneId/droneIdMesh.cpp`, which the mesh task calls each loop to broadcast
a summary every `K` seconds (default 5, `dm` prices it). The proximity test and
the urgent `DATA_ACK` path are not built, nor is the host's decoder.
Three things decide its shape. One drone's position at 1 Hz costs 135 % of
sub-band P's whole allowance, so raw forwarding over LoRa is impossible and what
travels is an 11-byte `TRACK` record four to a frame, plus a 6-byte `PILOT` for
the operator's position and a variable `IDENT` binding the handle to the UAS ID —
three new DATA opcodes in an envelope that is otherwise unchanged, encryption
included. **A board that is a bridge is also a drone watcher**, so a post the
host can reach with a cable emits its sightings as JSON on the same serial feed,
at 1 Hz and full precision, and the mesh is spent only on the fences no cable
reaches. And the firmware only ever *encodes* those records: the host already
stores every decrypted body as hex, so there is exactly one decoder and it is
the one that can be re-run over a capture.

**A watcher places itself with a GPS on D7, or is placed by hand** —
`al46.5191,6.5668` on the post, or `ar<address>:al46.5191,6.5668` from the
bridge — and its HELLO carries that position with the drone bit set (see
*Telemetry is the same block* above). The host needs it twice: to draw the
post, and as the reference its relayed `TRACK` coordinates are rebuilt
against. The bridge is the same image and reads a GPS the same way; its own
HELLO reaches the host on its `tx` line. So a post reports three positions:
its own, the aircraft's and the operator's.

The fix lives at 114–121 (`DK`…`DR`) on this board, not at `G`…`N`, and
`MAX_PARAM` is 122 — see *The GPS block does not travel as easily*. The baud
probe runs once at boot, so a receiver fitted later needs a reboot.

`taskWifi` is compiled into every image here but never started on this board,
and **the `(w)` menu must not be used on it** — associating with a network pins
the channel to the access point's and takes the receiver away until a reboot.

## The drone transmitter (`[env:droneTransmitter]`)

`KIND_DRONE_TRANSMITTER`, `include/configDroneTransmitter.h`,
`src/taskDroneTransmitter.cpp`. A bare XIAO ESP32S3 that pretends to be a drone,
so the watcher has a known aircraft to hear. Every second it announces a serial
number drawn at boot, flying a 1 km circle at 10 m/s around a fixed centre
while its operator walks a 100 m one at 1.4 m/s, on
Bluetooth 4 legacy (`A`), Bluetooth 5 Long Range (`B`) and a Wi-Fi beacon on
channel `C`. The ID changes at every boot, so after
a reflash the old aircraft lingers on the watchers for `E` (300) seconds. See [docs/drone-remote-id.md](docs/drone-remote-id.md).

- **The frames are built in `src/droneId/droneIdTransmit.cpp`**, which has no
  Arduino, and `test/test_droneid_transmit` reads them back through the
  receiver's own locators. Keep it that way: a transmitter tested only against
  itself proves nothing about the watcher.
- **Bluetooth 4 rotates its payload every 200 ms** by calling the NimBLE host's
  `ble_gap_ext_adv_set_data` directly. NimBLE-Arduino only sets data by
  reconfiguring the set, which the host refuses while it advertises. A refused
  update stops the set so the next turn starts it again, because the host
  forgets its sets when it resets.
- **It builds only its own four files** (`build_src_filter`), so `lib_deps` is
  NimBLE and ArduinoNvs alone. It is also the one env not on `[env]`'s list,
  which pins AnalogWrite 4.x and ArduinoNvs 2.8, versions the registry no
  longer serves. Every other env fails to install on a fresh checkout until
  that list is updated.

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
- A board that arrives running Meshtastic (TinyUSB, `303A:0059`) reaches the
  ROM when esptool connects, but that jump sets `RTC_CNTL_FORCE_DOWNLOAD_BOOT`,
  which survives esptool's RTS reset: the flash verifies, then the board sits
  at `waiting for download`. Replug it, or clear the flag and reset:
  `esptool.py --chip esp32s3 -p <port> --before no_reset --after hard_reset write_mem 0x6000812C 0 1`.
- **A port path names a USB socket, not a board.** macOS names the USB-JTAG
  port after its location (`/dev/cu.usbmodem8401` is hub 8, port 4), so a board
  swapped into the same socket gets the same path. A board once took an image
  meant for the one unplugged a minute earlier. Read the MAC first
  (`esptool.py -p <port> read_mac`, or the `SER=` of `pio device list`) and
  flash only when it is the board you mean.
- `upload_protocol = esp-builtin` programs whichever USB-JTAG OpenOCD finds
  first, and PlatformIO hands OpenOCD no `upload_flags`. With two boards
  plugged in, run OpenOCD yourself with `-c "adapter serial <USB serial>"`
  after `interface/esp_usb_jtag.cfg`; a serial that matches nothing fails
  rather than falling back to the other board.

## Style

Follow the existing C++: 2-space indent, `lowerCamelCase` functions and
variables, `UPPER_SNAKE` macros, `/* */` block comments above a function
explaining *why*. Comment sparingly and only where the reason is non-obvious.
