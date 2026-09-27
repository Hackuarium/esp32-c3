# Air traffic

Gliders, helicopters, light aircraft and paragliders broadcast where they are
around 868 MHz, so that they can see each other. `[env:airTraffic]` listens to
that with the Wio-SX1262 and prints every frame it hears as a JSON line on its
USB port. It transmits nothing.

    ~/.platformio/penv/bin/pio run -e airTraffic -t upload

## What is on the air

| System | Who carries it | Frequency | Modulation | Sync word | When |
|---|---|---|---|---|---|
| FLARM | most gliders, many helicopters and light aircraft | 868.2 / 868.4 | 2-GFSK, 100 kchip/s Manchester, ±50 kHz | `F5 31 FA B6` | two slots per UTC second |
| ADS-L M-band | EASA's open standard; newer FLARM units can transmit it | 868.2 / 868.4 | the same as FLARM | `72 4B` after a preamble ending `F5` | once a second, CSMA, alternating channel |
| OGN tracker | open-source trackers, SoftRF | 868.2 / 868.4 | the same as FLARM | `0A F3 65 6C` | the same two slots as FLARM |
| FANET | paragliders, hang gliders (Skytraxx, Naviter…) | 868.2 | LoRa SF7, 250 kHz, CR 4/5 | `0xF1` | whenever the channel is free |
| ADS-L O-band | almost nothing yet | 869.525 | 2-GFSK 38.4 kbit/s, ±10 kHz | `2D D4` | CSMA |

Sources: the EASA *ADS-L 4 SRD-860* specification (Issue 1, ED Decision
2022/024/R) for everything ADS-L; SoftRF's protocol descriptors
(`lyusupov/SoftRF`, `software/firmware/source/SoftRF/src/protocol/radio/`) for
FLARM's and OGN's sync words,
slots and CRCs; SoftRF's `FANET.h` for FANET. SoftRF is GPL-3: it was read for
these facts and nothing was copied from it.

The sync word column is the decoded bytes; on the air they are Manchester coded,
IEEE convention (a 1 is the chips `01`), so `F5` is sent as `55 99`.

## Can it hear everything at once?

No. An SX1262 demodulates one modulation, on one frequency, and matches one
sync word at a time. Here that is six settings:

- M-band 868.2 and 868.4 — FLARM **and** ADS-L together, because both begin
  with `F5`: the radio matches `55 99` and the task reads what follows.
- OGN 868.2 and 868.4 — its own sync word.
- FANET — LoRa, a different modem.
- O-band — a third frequency and a third bit rate.

What makes one radio worth having is that in Europe the M-band is a timetable.
Every second, counted from the UTC second:

| Window | FLARM | OGN |
|---|---|---|
| 400–800 ms (slot 0) | 868.2 | 868.4 |
| 800–1200 ms (slot 1) | 868.4 | 868.2 |
| 200–400 ms | — | — |

(the European case of the OGN tracker's frequency plan: channel = slot XOR
protocol; elsewhere it hops pseudo-randomly). With the GPS time the radio
follows FLARM through both slots, and FANET gets the 200 ms nobody slotted uses.
When OGN is also asked for it gets one second in four, and the O-band takes one
gap in four from FANET the same way — `airSchedule.h`.

| Receiver | What it hears |
|---|---|
| One SX1262, no GPS | every setting in turn, a second each (`B`): a share of everything |
| One SX1262 + GPS | FLARM in full if the timetable holds, ADS-L while on the M-band, FANET ~20 % of the time, OGN one second in four |
| Two SX1262 on one board | one follows FLARM, the other OGN and takes the gap for FANET: nearly everything |
| Three SX1262 | the above, plus FANET all the time |
| An RTL-SDR | 2.4 MHz of spectrum at once, so all of it — why OGN ground stations use one |

The schedule is written per radio so a second one can be added.

## The feed

One line per frame, on the USB port:

    {"event":"air","proto":"flarm","mhz":868.200,"rssi":-91,"ms":517,"ok":true,"errors":0,"hex":"…"}

| Member | Meaning |
|---|---|
| `proto` | `flarm`, `adsl`, `ogn`, `fanet` |
| `mhz` | the setting it was heard on — 869.525 is an O-band ADS-L frame |
| `rssi` | dBm, as the SX1262 measured the packet |
| `ms` | where in the UTC second it landed; absent without a GPS time |
| `ok` | the frame's own check: FLARM's CRC-16, ADS-L's CRC-24, FANET's LoRa CRC. Absent where this side checks nothing yet (OGN) |
| `errors` | Manchester chip pairs that were neither `01` nor `10` |
| `hex` | the frame after its sync word, decoded from Manchester |

What `hex` holds per protocol: FLARM the 24-byte payload and its CRC; ADS-L the
length byte and that many bytes, CRC last; OGN 20 bytes and 6 of parity; FANET
the LoRa payload.

**Nothing here knows what the bytes mean.** The firmware does framing and
integrity; the host decodes, keeps the hex, and can decode a capture again when
its decoder improves — the same division the mesh makes with `rx` bodies. A
capture the sync word matched but that is neither protocol (noise) is dropped
and counted.

## Console

    ti     protocols, clock, what was heard, slowest retune
    tc     clear the counts

| Parameter | Default | Meaning |
|---|---|---|
| `A` | 7 | bits: 1 FLARM+ADS-L, 2 OGN, 4 FANET, 8 O-band |
| `B` | 1000 | ms per setting when there is no GPS time |
| `C` | 0 | ms the GPS sentences arrive after their second |
| `F` | — | frames reported in the last minute, written by the task |

The GPS fix sits at `G`…`N`, as on a mesh node.

## The clock

The timetable needs the start of the UTC second. `gpsClock.h` gives it two
ways:

- **A PPS line**, `-D GPS_PPS=<pin>`: exact. The sentences only say which
  second the pulse started.
- **The sentences alone**: dated by when the first sentence of a second
  arrived, which is late by the receiver's output delay plus up to 20 ms of the
  GPS task's polling. `C` subtracts the delay. It is not known in advance, so it
  is measured: FLARM frames should land between 400 and 1200 ms, and the `ms`
  of the first capture says how far they are shifted.

## Not done yet

- **Decoding, on the host.** ADS-L is fully specified (XXTEA with a zero key,
  6 rounds; header; iConspicuity payload) in the EASA document. FANET's format
  is public (`3s1d/fanet-stm32`, `protocol.txt`). FLARM's is FLARM's *FAMP
  Public Protocol* (FTD-116), free for non-commercial, receive-only use; its
  download page did not serve the PDF to a script, so it was not read.
- **OGN's LDPC parity check**, which is what would give OGN frames an `ok`.
- **Real frames as fixtures.** The host tests build frames by encoding and
  check them against outside references (the Mode S parity every ADS-B tutorial
  decodes, the CRC-16/CCITT-FALSE check value, SoftRF's sync words). The first
  field capture should be frozen into `test/test_air_frames`.
- **Confirming the timetable** from that capture, and setting `C`.
- **A reader on the host.** `loramesh-monitoring` reads one serial port today;
  this board is a second one.
- **Privacy.** FLARM has stealth and no-tracking flags, ADS-L a privacy mode
  with a random address, and OGN a device database where owners opt out. The
  host decoder must honour them before anything reaches a map.
- **A second radio** on the same board, which the schedule is shaped for.

## Why it is not a mesh node

The same XIAO ESP32S3 with the same Wio-SX1262 — `lora/loraPins.h` is shared —
but the radio is retuned several times a second, which leaves nothing to hold
the mesh's channel with. A post that has to report over the mesh needs a second
SX1262, and then summaries rather than frames: one aircraft at 1 Hz does not fit
sub-band P's duty cycle any more than a drone does (see
`drone-mesh-forwarding.md`).
