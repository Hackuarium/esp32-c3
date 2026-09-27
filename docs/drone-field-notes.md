# Field notes — what we measured, and what to do next

Written 2026-09-16, at the end of the first hardware session. This is the record
of what was *measured* rather than reasoned, the decisions that came out of it,
and the state everything was left in. The companion documents hold the
analysis:

- [drone-remote-id.md](drone-remote-id.md) — the receiver, the console, the threat model
- [drone-rf-detection.md](drone-rf-detection.md) — detecting a drone that does not cooperate
- [drone-droneid-phy.md](drone-droneid-phy.md) — the DJI DroneID waveform, and the detector built on it
- [drone-mesh-forwarding.md](drone-mesh-forwarding.md) — carrying sightings over the mesh

## The headline

**The receiver works. The aircraft was silent.** A DJI Mini 4 Pro running a few
metres away produced nothing, on any transport, on any channel — and we proved
the silence was the drone's and not ours.

## What was measured

### The receiver hears the whole band

A sweep of all thirteen 2.4 GHz channels, five seconds each, with the drone
powered and motors running:

| ch | mgmt frames | ch | frames | ch | frames |
|---|---|---|---|---|---|
| 1 | 0\* | 6 | 242 | 11 | 378 |
| 2 | 370 | 7 | 251 | 12 | 495 |
| 3 | 100 | 8 | 97 | 13 | 143 |
| 4 | 13 | 9 | 67 | | |
| 5 | 57 | 10 | 81 | | |

\* the channel-1 window straddled the retune; not meaningful.

**2 908 management frames, 2 965 BLE advertisements, `Frames refused: 0`.**

That last number is the one that matters. "Refused" counts frames carrying the
ASTM identifier that then failed to decode. Zero refusals beside thousands of
frames heard means nothing resembling Remote ID crossed 2.4 GHz at all.

### And it can say what it heard instead

The `(dv)` survey, added during the session, over 50 seconds with the drone up:

```
Frames from a known drone vendor OUI: 0
Wi-Fi vendor 00:50:F2 x2807   Microsoft      (WPA/WPS, in every AP beacon)
Wi-Fi vendor 00:90:4C x1071   Epigram        (HT capability)
Wi-Fi vendor 00:10:18 x1071   Broadcom
Wi-Fi vendor 8C:FD:F0  x924   Qualcomm
Wi-Fi vendor 50:6F:9A  x605   Wi-Fi Alliance (P2P/WFD, not NAN Remote ID)
Wi-Fi vendor 00:0C:43  x200   Ralink
BLE service 0xFDF7, 0xFE98, 0xFD5A, 0xFEF3, 0xFCB2   consumer services
```

No `FA:0B:BC` (ASTM over Wi-Fi). No `0xFFFA` (ASTM over BLE). No `26:37:12`
(DJI's proprietary DroneID in a beacon). **Zero** frames from any of DJI's
fifteen registered OUIs — the aircraft emits no 802.11 at all, which is exactly
what OcuSync should look like.

Everything in that list is the building's own Wi-Fi and its occupants' phones.

## Why the Mini 4 Pro was silent — three gates, all shut

1. **DJI's own manual**: *"the Remote ID system is ONLY activated when using the
   Intelligent Flight Battery Plus… The aircraft using the Intelligent Flight
   Battery does not activate Remote ID system."* Standard battery keeps it under
   250 g, and DJI disables the broadcast by design.
2. **EASA class C0.** The Mini 4 Pro ships in Europe as MT4MFVD with a C0 mark,
   and Delegated Regulation (EU) 2019/945 requires Direct Remote ID only from
   **C1** upward (Part 2, point 12). In Switzerland there is no obligation.
3. **Flight state.** DJI broadcasts *"from takeoff to shutdown"*. Powered on with
   props turning on the ground is before takeoff. DJI's FAQ additionally
   conditions activation on the aircraft being *"within airspace of the United
   States"*, which if it holds in CE firmware means a Swiss unit may never
   broadcast at all.

**This is the most important finding of the session**, and it is a stronger
argument than anything previously written in the threat model: a sub-250 g DJI is
not a smuggler *defeating* Remote ID, it is the exempt configuration sold off the
shelf. No receiver at any price detects it, because there is no transmission.

## Corrections to earlier advice, recorded so they are not repeated

- **DJI's Remote ID is Wi-Fi Beacon, not Bluetooth** — the opendroneid device
  table records Mavic 3 and Mini 3 Pro as Beacon only (no BT4, BT5 or NAN). The
  Mini 4 Pro is *not* in that table, so this is inference from its sibling.
  Turning Bluetooth off during a test whose transport is unknown is still wrong.
- **Parking on channel 6 is unsafe.** The channel-6 "shall" in F3411 §5.4.8.7 is
  justified for NAN discovery; for Beacon, any channel is permitted at a faster
  5 Hz rate, and BlueMark's DroneScout manual states Beacon Remote ID "can be
  found on all Wi-Fi channels". Hop, or sweep.
- **RX5808 is not a Remote ID instrument.** It cannot tune DJI's 5.170–5.250 GHz
  band at all, and it detects energy, not identity. See below.

## The USB gotcha that cost an hour

**The ESP32-S3's USB-Serial-JTAG RX path latches up after a DTR/RTS-induced
reset, and only a physical unplug clears it.** Symptoms: boot banner and all
output arrive normally, and nothing typed is ever received.

It is not a firmware fault. Proven by: a heartbeat in `TaskSerial` showing
`alive ticks=342…2317, available=0, bytesSeen=0`; writing through `stty`/`printf`
instead of pyserial; and flashing `loraBridge` as a control, which failed
identically. A JTAG upload resets the chip differently and does not trigger it.

**If the console goes deaf, unplug the board. Reflashing will not fix it.**

## Firmware written this session

| what | where |
|---|---|
| commissioning counters — advertisements and frames heard | `di` |
| the `(dv)` survey of everything on the air that is not Remote ID | `droneIdSurvey.cpp` |
| defaults written on a reflashed board, keyed on the qualifier | `taskDroneId.cpp` |

The defaults bug is worth remembering: parameters are stored **under their
letter**, so a board reflashed from another board kind inherits values that mean
something else. `writeDefaultsWhenUntouched()` now tests
`getQualifier() != DRONE_QUALIFIER`, which is per-board-kind and exists for
exactly this.

## Hardware decisions reached

| part | verdict |
|---|---|
| **HackRF One** (owned) | **the instrument to use.** Specified 1 MHz–6 GHz — not coaxed there like an unlocked Pluto. Reference, calibration and identification |
| **XIAO ESP32-C5** | the only way to receive 5 GHz Remote ID. Needs ESP-IDF ≥ 5.5, so a toolchain jump from this repo's 4.4.6 |
| **RX5808 / RTC6715**, €7 | worth it as an **experiment**, not a product decision. Module spec is **5705–5945 MHz**, so it misses *both* 5170–5250 (where a Swiss drone is legally 9 dB louder) and the 5333–5613 Lowband. Gives energy without identity, and needs the SPI mod |
| ADALM-Pluto | ~€150–230 and needs the AD9363→AD9364 unlock, which leaves the guaranteed-performance envelope. No reason to buy it |
| RTL-SDR | **cannot reach 2.4 or 5.8 GHz** — the tuner stops near 1.75 GHz. Rules out every cheap RX-only SDR |
| nRF24L01+ | one bit at −64 dBm, permanently saturated by Wi-Fi. Do not build |

## The direction-finding node, if it gets built

**One receiver behind an SP4T switch, not four receivers.** Amplitude-comparison
DF works on the *ratio* between antennas; with four receivers every bearing
carries their mutual calibration error, and it drifts. One receiver compares
against itself. It is also 170 mA instead of 680 mA, and six pins instead of ten.

Two things are not optional: the four coax tails must be **cut to equal length**,
and the antennas must be **matched** — mismatch is a direct bearing error.

Those open questions are now answered in §6c of
[drone-rf-detection.md](drone-rf-detection.md) — the switch is a **pSemi
PE42442**, the receiver needs a **5.7–6.0 GHz filter** because the RTC6715's
image lands on Wi-Fi channel 6, an **LNA is worth 8× the range** where the
precision op-amp is worth 0.7°, and the node's **own SX1262 compresses the
front end asymmetrically**, which is a bearing error rather than a dropout.

One measurement still decides one-receiver versus four: the **RX5808's RSSI
settling time after an RF step**, which no datasheet gives.

### It cannot handle five drones

Four antennas give four numbers; each emitter costs two unknowns, power and
bearing. Two emitters is already ill-conditioned; **three or more is
underdetermined and no algorithm recovers it**. Worse, the failure is silent —
summing produces a confident bearing pointing at the power centroid, where
nothing is.

Frequency separation rescues it: the RX5808 has roughly 8–10 MHz of IF
bandwidth, so sweeping and running the comparison **per frequency peak** restores
the budget for drones on different channels. Cross-node consistency is the other
discriminator — real targets produce intersecting bearings, artefacts do not.

**Design rule: report "multiple emitters, bearing unreliable" rather than a
confident centroid.** A wrong bearing sends staff to the wrong fence.

## The 2.4 GHz problem, and the one discriminator that works

Energy detection in 2.4 GHz is hopeless: an AP at 20 dBm and 30 m reads −57 dBm
while a drone at 200 m reads −73 dBm, so the interferer is 16 dB *louder* than
the target. Bluetooth Classic hopping 1600 times a second looks exactly like a
hopping drone link.

What does work is **energy minus identified traffic**:

```
candidate = energy present
            AND no 802.11 frames explaining it
            AND not the BLE advertising channels (2402/2426/2480)
```

Two of those three inputs already run on the board and count. A prison has an
unfair advantage here: **you own most of the emitters**, so your own APs can be
whitelisted and the residual baselined over a week.

## The most promising lead: DJI DroneID

DJI's **AeroScope** — the drone detector sold to police from 2017, discontinued
2023 — worked by receiving **DroneID**: DJI's own broadcast carrying serial
number, aircraft position and **the pilot's location**. DJI claimed it was
encrypted; [it was not](https://arxiv.org/pdf/2207.10795), shown independently by
Bender and by RUB-SysSec at NDSS 2023.

Open-source decoders exist and run on a HackRF:
[proto17/dji_droneid](https://github.com/proto17/dji_droneid),
[RUB-SysSec/DroneSecurity](https://github.com/RUB-SysSec/DroneSecurity),
[anarkiwi/samples2djidroneid](https://github.com/anarkiwi/samples2djidroneid).

**The catch: O4 aircraft broadcast encrypted DroneID, and the Mini 4 Pro is O4.**
The payload is probably unreadable.

**The opening: detection is not decoding.** The burst has a fixed structure —
nine OFDM symbols, ~10 MHz wide, with Zadoff-Chu sequences at fixed positions,
repeating roughly every 600 ms. Those ZC sequences are the receiver's
synchronisation preamble and must survive encryption, because the receiver needs
them *before* it can decrypt. Correlating against them detects the burst without
reading it — and that works on a Mini 4 Pro **despite Remote ID being disabled,
despite the C0 exemption, and despite encryption.**

That is the non-cooperative DJI detector, and the radio for it is already owned.

A deployable form exists: the **AntSDR E200** runs DroneID detection on its own
CPU ([alphafox02/antsdr_dji_droneid](https://github.com/alphafox02/antsdr_dji_droneid)).

## What was settled after the hardware session

The three open research threads finished. Details are in
[drone-droneid-phy.md](drone-droneid-phy.md) (new) and in §6b/§6c of
[drone-rf-detection.md](drone-rf-detection.md); the decision-relevant parts:

**The DroneID detector exists and is tested.** The Zadoff-Chu parameters were
the blocking unknown and are now confirmed from two independent
implementations — **roots 600 and 147, on symbols 4 and 6**. `tools/hackrf/`
holds a working detector that finds a synthesised burst down to **0 dB SNR**
(gated) or **−10 dB** (matched filter), and fires on neither noise nor
Wi-Fi-shaped OFDM. `./droneid.py --selftest` proves it without hardware.

**And it nearly did not work.** The HackRF One has **no TCXO** — about ±20 ppm,
so ±49 kHz at 2.44 GHz. Because a Zadoff-Chu peak shifts in *time* when the
signal shifts in *frequency*, and by a different amount per root, the pairing
test collapsed from 0.995 to **0.007 at 15 kHz offset**: the tool would have
reported nothing, in the field, with a drone in the air. It now searches a
±120 kHz bank, and the winning bin measures the crystal error.

**Three things about the hardware plan changed.**

- **Sweep 5.1 GHz first.** OFCOM RIR1010-05 limits drones at 5 GHz to
  **5170–5250 MHz at 200 mW** against 25 mW at 5.8 — a 9 dB legal incentive to
  sit where an RX5808 cannot tune. This is a regulation, not an inference.
- **The AntSDR E200's cheap variant is useless here**: AD9363 tunes to 3.8 GHz
  only. 5.8 GHz needs the AD9361 version.
- **The threat model had the aircraft wrong.** The reporting names commercial
  DJI machines, not FPV builds. The real case for a 5.8 GHz sensor is the DJI
  **FCC-mode unlock**, which makes an ordinary Mavic a 1 W emitter.

**And the DF node has two new hard constraints**: the RTC6715's image lands on
Wi-Fi channel 6 (a filter is mandatory), and the node's own SX1262 at +22 dBm
compresses the receiver front end asymmetrically, which is a *bearing error*
rather than a dropout.

## Next session

1. `brew install hackrf`, then `hackrf_info`.
2. `pip3 install numpy && ./droneid.py --selftest` — two minutes, and it proves
   the analysis chain before any capture depends on it.
3. **Baseline sweep** with the aircraft off, then flying, forced to each band:
   `2g4`, `5g8`, **and `5g1`**. That settles which band a CE-spec Mini 4 Pro
   actually uses, whether it radiates in the RX5808's range at all, the site's
   background, and the SNR a cheap sensor would need.
4. **Capture IQ at 15.36 Msps** on whatever the sweep found (`-r`, never `-w`;
   add `-b 12000000`), and run `./droneid.py --deep`.
5. **Fly the aircraft.** Sources disagree on whether a grounded drone transmits
   DroneID at all, and the silent test above never left the ground — motors
   spinning is not the same experiment.
6. Use the band-forcing as a **control**: force 5.8, confirm detection; force
   2.4, confirm it moves.
7. Only then decide what to buy.

**A null result in step 4 is informative, not a failure.** It means O4 changed
the ZC roots — the published work is OcuSync 2.0, tested on a Mini 2. AntSDR
detects O4 aircraft, so *a* preamble survives; whether it is this one is the
thing the capture answers.

**Do not order the SP4T, the antennas or the filters until the HackRF says the
5.8 GHz path is real on this site.**

## Board state as left

Flashed with the survey build. Parameters `A2 B8 C0` — 2 s Bluetooth, 8 s Wi-Fi,
hopping 6/1/11 — and the survey enabled. Mesh address 3, role bridge, inherited
from its previous life as a `loraBridge`. `ur` would reset the drone parameters
but also the mesh block, so it was not run.
