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
`THR_LORA`, `THR_LORA_MESH`, the role constants and the parameters.

**The mesh owns parameters 104–113 (`DA`…`DJ`) on every board.** The code refers
to parameters only by name, so each config *could* pick its own slots — and that
is exactly the trap: the block would then collide with `PARAM_OUT2_COLOR1` on
the handrail, `PARAM_GATE1_IN` on the pixels board, and something else again on
the next one. 104–113 is the one range free in every config here, which is why
`MAX_PARAM` is 114 wherever the mesh is enabled. A board that sets a smaller
`MAX_PARAM` after including the header fails to compile rather than writing past
`parameters[]`.

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

`axC6` broadcasts parameters C through H; `ax42:C6` sends them to node 42 and
waits for an ACK through the escalation ladder (direct ×2, 2 hops ×2, 4 hops).
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

### Telemetry is the same block, sent periodically as DATA

There is no per-sensor frame type. A node broadcasts the parameter window
`DG`…`DG + DH` every `DF` seconds (0 = never), encoded exactly like an `ac`
copy — but as **DATA rather than CMD**, so a receiver *prints* the block instead
of applying it. A broadcast SET would have every neighbour overwrite its own `G`
with the tracker's latitude.

That is all a GPS tracker is: `taskGPS` writes the fix into `G`…`N` (latitude
and longitude are int32, each spread over two adjacent int16 slots via
`setParameterInt32`), and `DF60 DG6 DH8` puts those eight on the air. Any future
sensor joins the same way — write parameters, set the window. Adjacency is load
bearing: an int32 only survives the trip because both halves sit in the same
run of slots.

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
| `params` | a DATA or RESP block arrives | `src`, then one member per parameter *label* (`"G":-15616`), plus `lat`/`lon` when the block covers the fix and `hdop` when it covers `M` |
| `data` | a DATA body with an unknown opcode | `src opcode length` |
| `cmd` | a remote SET was applied here | `src status` |
| `noack` | the escalation ladder gave up | `dst counter` |
| `peers` | `ap` on a bridge | `count`, then an array of `address counter rssi snr age` |

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
without a reboot whenever one of them changes. The defaults are **869.525 MHz,
250 kHz, SF12**, and they are one decision rather than three: sub-band P
(869.4–869.65) is the only part of the band that allows 500 mW and a 10 % duty
cycle, the regulation lets it be used either as 25 kHz channels or as **one
channel for high speed data** — so 250 kHz is the only wideband shape allowed
there and 869.525 the only centre that fits it — and SF12 then buys back the
3 dB the wider bandwidth costs plus 9.5 dB more. Against 868.4/125/SF7 that is
**+17.5 dB**, roughly three times the range, for twelve times the airtime per
frame and an exchange that answers ten times slower. It is not a quiet channel:
LoRaWAN gateways send their RX2 downlinks there at 27 dBm. `DC18736 DD125 DE7`
goes back to the old settings — 868.4 sits in the gap between the mandatory
LoRaWAN channels at 868.3 and 868.5 and shares a channel with nobody.

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

**It is a budget, not a delay.** EN 300 220 defines the duty cycle as transmit
time within an observation window — one hour — so the governor is a token
bucket, not a gap between frames: `airtimeBudgetMillis` holds the transmit time
still available, `refillAirtimeBudget()` credits it back at 1/N of real time,
and `transmitFrame` spends it. At the default 10 % that is **360 s of airtime
per hour**, which the node may burst through — roughly 485 frames of 742 ms back
to back at SF12/250 kHz — before it has to wait. Enforcing a fixed post-transmission
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
