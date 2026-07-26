# esp32-c3 — notes for Claude

Firmware for a family of ESP32 boards. One codebase, many `[env:…]` targets in
`platformio.ini`; each selects a `BOARD_TYPE` and pulls the matching
`include/config<Kind>.h`. Most files are compiled for every target, so guard
target-specific code with the build flags rather than assuming a target.

## Build and upload

PlatformIO is not on `PATH`. Use the full path:

    ~/.platformio/penv/bin/pio run -e <env>
    ~/.platformio/penv/bin/pio run -e <env> -t upload

`default_envs = square`, so a bare `pio run -t upload` targets the wall panel.

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
