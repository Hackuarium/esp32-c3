# opendroneid

The reference decoder for ASTM F3411 / ASD-STAN prEN 4709-002 drone Remote ID,
vendored unmodified from
[opendroneid/opendroneid-core-c](https://github.com/opendroneid/opendroneid-core-c)
at commit `6484f26545d4f012682524e2d843fab0fbdc0b34`, Apache-2.0.

Three files of `libopendroneid/`: `opendroneid.c` and `opendroneid.h`, which
turn the 25-byte messages into fields, and `odid_wifi.h`, which is the 802.11
frame layout the Wi-Fi transport wraps them in.

It is here rather than in `lib_deps` because upstream is a CMake project with
its sources in `libopendroneid/` and no `library.json`, so PlatformIO's
dependency finder cannot build it from the git URL. Vendoring is what
`lib/tinyexpr` already does for the same reason.

**Do not edit these files.** Anything this project needs that upstream does not
do belongs in `src/droneId/`. To update, copy the three files again from a newer
commit and change the revision above; the only thing this project relies on that
is not upstream API is the `ODID_*` build flags below.

`wifi.c` is deliberately not vendored. Its receive helper clears the
accumulating `ODID_UAS_Data` on every frame, which is the opposite of what a
receiver aggregating one aircraft's messages needs, and the rest of it is the
transmit path plus `printf`, `time.h` and `byteswap.h`. `src/droneId/droneIdWifi.cpp`
matches the frames against the struct definitions in `odid_wifi.h` and hands the
payload to `decodeOpenDroneID()`, which is the part that decodes.

## Build flags

`[env:droneTracker]` sets three, and they are load bearing:

- `ODID_DISABLE_PRINTF` — drops the `printXxx_data()` dumps. They format with
  `printf` to stdout, which on this board goes nowhere useful, and the console
  output lives in `src/droneId/droneIdReport.cpp` instead.
- `ODID_AUTH_MAX_PAGES=9` — the authentication pages one aircraft may hold.
  It cannot go below 9: `checkPackContent()` refuses a whole message pack that
  carries more auth pages than this, and a pack holds at most 9 messages, so
  anything smaller would let an aircraft's position be thrown away because of
  signature pages this board does not read. 16, the upstream default, would
  cost 287 bytes per aircraft for pages that cannot arrive in one pack.
- `ODID_BASIC_ID_MAX_MESSAGES=2` — upstream's default, stated rather than
  inherited because it is the reason a Japanese aircraft's serial number and
  session id can both be kept.
