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
  `src/lora/`, no network server, one AES-128 group key. Gated on the
  `THR_LORA_MESH` flag rather than a board kind, so **any** board joins the mesh
  by defining it in its `config<Kind>.h` and calling `taskLoraMesh()` from its
  setup; it also needs `PARAM_LORA_ROLE`, `PARAM_LORA_TTL`,
  `PARAM_LORA_SPREADING_FACTOR` and `PARAM_LORA_INTERVAL_SECONDS`.

### The mesh wire format (`src/lora/loraFrame.h`)

    ctrl(1) src(1) dst(1) counter(3 or 4) | ciphertext | mic(4)
    \___________ authenticated _________/   \_ encrypted _/

    ctrl, bit 7 to 0:  ver(1) type(3) cntsz(1) ttl(3)

10 bytes of overhead. AES-128-CCM with a 4-byte tag does confidentiality and
authenticity in one pass; there is no second key and no separate CMAC.

- **Only the three ttl bits are mutable.** They are masked out of both the nonce
  and the additional data, which is what lets a relay decrement the countdown
  without invalidating the tag — and why escalating a retry from ttl 0 to ttl 2
  rewrites one byte instead of re-encrypting.
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
- `ttl == 0` means "do not relay" — that, not the broadcast address, is the
  direct-vs-flood switch. A relay also refuses anything arriving with
  `ttl > LORA_TTL_MAX_ACCEPT`, which caps amplification whatever the sender
  claims.
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

### Telemetry is the same block, sent periodically as DATA

There is no per-sensor frame type. A node broadcasts the parameter window
`AE`…`AE + AF` every `S` seconds (0 = never), encoded exactly like an `ac`
copy — but as **DATA rather than CMD**, so a receiver *prints* the block instead
of applying it. A broadcast SET would have every neighbour overwrite its own `G`
with the tracker's latitude.

That is all a GPS tracker is: `taskGPS` writes the fix into `G`…`L` (latitude
and longitude are int32, each spread over two adjacent int16 slots via
`setParameterInt32`), and `S60 AE6 AF6` puts those six on the air. Any future
sensor joins the same way — write parameters, set the window. Adjacency is load
bearing: an int32 only survives the trip because both halves sit in the same
run of slots.

### Radio settings and the duty cycle

Carrier, bandwidth and spreading factor are all runtime parameters, re-applied
without a reboot whenever one of them changes. The defaults are **868.4 MHz,
125 kHz, SF7** — 868.4 is in EN 300 220 sub-band M (1 %) and lands in the gap
between the mandatory LoRaWAN channels at 868.3 and 868.5, so the mesh does not
share a channel with every LoRaWAN device around.

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
and `transmitFrame` spends it. At 1 % that is **36 s of airtime per hour**,
which the node may burst through — roughly 580 frames of 62 ms back to back at
SF7/125 kHz — before it has to wait. Enforcing a fixed post-transmission
silence instead would be far stricter than the regulation and would make a
retry ladder unusable. `LORA_DUTY_CYCLE_WINDOW_MS` shortens the window if you
want the node more conservative. RadioLib does not enforce any of this outside
LoRaWAN.

**Power is still `LORA_TX_POWER 22` (158 mW), over the 25 mW ERP the band
allows.** It was inherited from the old beacon code and is deliberately left
alone — lowering it to 14 dBm costs real range, so it is a decision, not an
oversight. Fix it before anything ships.

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
