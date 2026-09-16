# Detecting a drone from its own emissions — an evaluation

**Nothing here is implemented.** This answers one question: can this board detect
a drone that is *not* cooperating — one whose Remote ID is off or absent — by
listening for its control link or video downlink instead?

Short answer: **the Wi-Fi radio cannot be made into a spectrum sensor, but the
SX1262 already on the board is one in the sub-GHz band, and the single most
valuable thing on this page is a software change that costs no hardware at all.**

Passive receive only. Transmitting anything that interferes — jamming, spoofing,
protocol takeover — is illegal in essentially every European jurisdiction, and
the fact that some of these links are technically hijackable does not change
that.

## 1. No, the Wi-Fi radio cannot be hacked into a spectrum analyser

This was checked properly rather than assumed: the whole public ESP-IDF 4.4.6
surface (71 functions), `esp_private/wifi.h` (37), `phy.h` (14), then the same
headers on `release/v5.5` and `master`, then disassembly of the promising
symbols in `libpp.a` and `libphy.a`.

**There is no call, supported or otherwise, that returns channel energy without
a demodulated 802.11 frame.** The reason is architectural, not a missing API:

- `WIFI_PROMIS_FILTER_MASK_FCSFAIL` delivers frames that fully demodulated and
  then failed CRC. `WIFI_PKT_MISC` delivers a PPDU whose L-SIG decoded. Both are
  100 % 802.11-gated; a non-802.11 emitter produces neither.
- **CSI needs a received frame by definition.** As a tripwire it also fails on
  physics: a drone crossing a 100 m link at 30 m altitude sits in about the
  288th Fresnel zone and perturbs amplitude by ~8×10⁻⁴ — between 7× and 30×
  below one quantisation step. The most sensitive published outdoor comparable
  needed three *phase-coherent* transmit chains sharing one oscillator; three
  independent ESP32s cannot substitute for that.
- Two ROM symbols look like an answer and are not. `wDev_GetNoiseFloor` is a
  cached, ÷4-averaged, 1 dB-quantised minimum-tracker that **returns a constant
  on repeated calls** — designed to reject exactly the bursty signals wanted
  here. And `meas_tone_pwr_db`, the most promising-sounding name in `libphy`,
  **keys the transmitter and measures its own power amplifier.** Anyone
  experimenting with it would be transmitting.
- **No raw IQ in any mode, on any ESP32 variant.** No register path, no DMA path.
- The Bluetooth radio adds nothing: LE-only, `HCI_LE_Test_End` returns a packet
  count and no signal level, `HCI_Read_RSSI` needs a connection handle.

Above all of it sits a limit no software crosses: **the ESP32-S3 tunes
2412–2484 MHz only.** Everything at 5.1, 5.8 and 900 MHz is outside the silicon.

Stop looking at the Wi-Fi radio for this.

## 2. The free win: DJI's own DroneID is in a Wi-Fi beacon

This is the highest-value finding on the page and it needs no new hardware.

Separately from ASTM Remote ID, **Wi-Fi-link DJI aircraft embed a proprietary
telemetry record in a vendor element of their own beacons** — the same DroneID
record that also rides OcuSync, in a form an ordinary 802.11 sniffer can read.
Inside element `0xDD`:

| offset | bytes | meaning |
|---|---|---|
| 0–2 | `26 37 12` | DJI's self-assigned OUI — **not** an IEEE assignment (U/L bit set) |
| 3–5 | `58 62 13` | fixed magic |
| 6 | `10` / `11` | subcommand: flight telemetry / flight purpose |
| 7… | record | serial, position, velocity, **pilot position**, home point |

It carries the **aircraft serial number, its position, and the operator's
location** — the same things Remote ID gives, from an aircraft that never opted
in. Sources are Kismet's `dot11_ie_221_dji_droneid.h` and `anarkiwi/samples2djidroneid`,
both tracing to Department 13's Aeroscope analysis.

Four things to get right, each of which silently corrupts the output:

- **Coordinates scale by `/174533.0`, not `/1e7`.** The field is radians × 1e7.
  Reusing the ASTM scale puts every aircraft in the wrong hemisphere.
- **Branch on the version byte.** The v1 record has `pitch, roll, yaw`, no GPS
  time and *no pilot coordinates at all*; parsing it as v2 yields garbage that
  passes every range check.
- **Take the length from the 802.11 element length byte.** The "91 bytes" quoted
  around the internet counts different things in different sources.
- Home lat/lon order is genuinely disputed between the three implementations.
  Decode best-effort, label it, sanity-gate it.

**Fit here is good.** The record is 87–147 bytes against a `DRONE_MAX_PAYLOAD` of
229, and the parse point is inside the existing walk in
`droneIdFindBeaconPayload()` — a second `memcmp` in the same loop, a new
`DRONE_SOURCE_WIFI_DJI`, and a mapping into `ODID_UAS_Data` (serial →
`BasicID.UASID`, drone position → `Location`, pilot position →
`System.Operator*`) so `dl` and `dd` work unchanged. The source label must make
clear it is a reverse-engineered proprietary decode, not a standards message.

**Who it catches:** the Wi-Fi-link generation — Mavic Pro, Mavic Air, Spark and
similar. Newer DJI aircraft moved the control link to OcuSync, where the same
record still travels but needs an SDR. So this narrows the DJI gap; it does not
close it.

## 3. The SX1262 you already have is a real sub-GHz sensor

`getRSSI(false)` is `GetRssiInst` (opcode 0x15): a wideband power meter over the
configured channel filter, 0 to −127.5 dBm in 0.5 dB steps, settling in
30–54 µs at wide bandwidth. `scanChannel()` adds LoRa CAD — about 10 dB more
sensitivity and, decisively, *selectivity*: a chirp correlator that fires on a
LoRa preamble in 4.5 symbol times. Both are in the vendored RadioLib 7.4.0.

A sweep of 863–870 MHz at 500 kHz resolution around 200 times a second is
plausible *(derived from settling times, not measured)*, at zero airtime cost —
receiving has no duty cycle.

What that reaches:

| link | band | verdict |
|---|---|---|
| **ExpressLRS 900**, SF7/SF8/SF9 rates (100/50/25 Hz) | 863–927 | **decodable** — implicit header, CRC off, sync word 0x12 |
| ExpressLRS 900, SF6 rates (200 Hz, 100 Hz 8ch) | 863–927 | detectable, **not** decodable — Semtech changed SF6 between generations |
| FrSky R9 | 868/915 | detectable, not decodable (SF6) |
| TBS Crossfire | 860–928 | energy only — 2-FSK, no published sync word |

ELRS 900 is on the air 75–93 % of the time, so it is not a needle in a haystack:
a receiver parked on one channel sees packets every ~130–400 ms at 100 Hz.

**The cost to the mesh is real and must be paid deliberately.** A scan aborts a
reception wherever it lands, so a once-per-second sweep loses ~12 % of mesh
frames at SF9/250 kHz. Gating each scan on `PREAMBLE_DETECTED` / `HEADER_VALID`
brings that back to ~2–3 % and costs about five lines. Do not build the scanner
without it.

**Above 960 MHz the SX1262 is finished, absolutely** — datasheet Table 3-7, and
the Wio-SX1262 module's matching narrows it further to 862–930 MHz.

## 4. 2.4 and 5.8 GHz need new hardware, and most options are traps

| | what it gives | false positives at a prison |
|---|---|---|
| **SX1280** as an ELRS preamble detector, ~$10, RadioLib already vendored | protocol-level detection of any ELRS 2.4 link, −104…−115 dBm | **low** — chirp correlation rejects Wi-Fi/BLE/ovens structurally |
| **RX5808 / RTC6715** 5.8 GHz sweeper, ~$7 | RSSI vs frequency over the analogue FPV band and DJI's 5.8 downlink | low — a carrier in a near-empty band is specific |
| **SDR + Linux host**, ~$200–400 | the only thing that sees OcuSync properly | low, with real classification — but a different product |
| nRF24L01+ RPD, ~$2 | one bit, "above −64 dBm ±5 dB" | **very high**, and no level to gate on |
| AD8317 log detector | one scalar, ~−50 dBm floor, ~15 m | total — no frequency axis |
| RTL-SDR | nothing: tuning stops near 1.76 GHz | n/a |

**The false-positive argument is the whole argument.** A prison's 2.4 GHz band
contains staff Wi-Fi beaconing every 102.4 ms on channels 1/6/11, every phone's
Bluetooth, and a kitchen microwave drifting across the band. Bluetooth Classic
hops 1600 times a second — which is *precisely* what an occupancy scanner
mistakes for a hopping drone link. **There is no threshold that admits a 25 mW
handset at 200 m and rejects an access point at 50 m, because the access point is
louder.** Any energy detector deployed as an alarm here alarms continuously.

That asymmetry is why the SX1280 ranks first: it looks for a *waveform*, not a
level. The fact it rests on is specific and was re-derived from the ELRS source —
the FHSS builder pins slot 0 of every 80-hop block to channel 40 = **2440.4 MHz**
and the shuffle cannot touch that slot, so **every ELRS 2.4 link visits
2440.4 MHz once per block regardless of binding phrase**, every 160 ms to 3.2 s.
Three practical caveats: `setDioIrqParams()` is not public in RadioLib 7.4.0
(use `startReceive(timeout, irqFlags, irqMask, len)` and `getIrqFlags()`); the
SX1280 datasheet §16.1 forbids continuous RX in congested traffic, so use RX
single with re-arming; and ELRS derives IQ inversion from a UID bit, so both
polarities must be tried — a gated detection takes about a minute, not seconds.

## 5. The question to answer before spending anything

All of §4 may aim at the wrong aircraft, and the public record says so.

- US Bureau of Prisons: **479 drone incidents in 2024, against 23 in 2018**.
- HMPPS: **1,712 incidents in England and Wales in the year to March 2025**, up
  43 %, roughly five a day.
- June 2026: a federal indictment covering **ten prisons and at least 38 drops**,
  the largest such case, with coverage describing **off-the-shelf consumer
  drones — "no exotic technology"**.

That points at DJI-class machines, not ExpressLRS hobby builds. **Against a stock
DJI, the SX1280 detector fires never.** The FAA's own figure — DJI at 96.4 % of
Remote-ID-detected platforms — is measured with exactly the instrument a
switched-off transmitter defeats, so it is suggestive rather than decisive; and
the reporting that matters most, describing 25 lb payloads at 75 mph, describes
aircraft no DJI consumer model is.

**So the first thing to do costs nothing: ask whoever holds the local incident
reports what airframes have actually been recovered.** That single answer moves
this from a $10 experiment to a $400 product decision.

## 6. Recommendation, in order

1. **Add the DJI beacon decode** (§2). Free, no hardware, and it yields the
   pilot's location from aircraft that never opted into Remote ID.
2. **Get the local recovered-airframe data** (§5) before buying anything.
3. **If custom FPV builds appear in that data**: an SX1280 as an ELRS detector,
   and an RX5808 for the 5.8 GHz video downlink — which is the emitter that
   radiates *continuously* over the wall, where the 2.4 GHz control link is
   sparse.
4. **If the answer is DJI**: budget for an SDR and a small Linux host. No $10
   part demodulates a 10–60 MHz OFDM waveform, and anything sold as if it does
   is a log detector with a comparator.
5. **Do not build** an nRF24 scanner, a broadband log detector, or buy an
   RTL-SDR for this.

## 7. Two non-technical items for the deployment file

- **Passive receive is generally lawful; transmitting is not.** Jamming,
  spoofing and protocol takeover are illegal in essentially every European
  jurisdiction regardless of who operates the site.
- **A perimeter RF sensor is surveillance infrastructure sited next to a public
  road.** Whatever it records about passers-by — and an 802.11 sniffer records a
  great deal — needs a stated retention and access policy before it is switched
  on.
