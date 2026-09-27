# DJI DroneID — the waveform, and detecting it without decoding it

This is the answer to the problem the [field notes](drone-field-notes.md) end
on: a DJI Mini 4 Pro flying a few metres from a working Remote ID receiver
produces **nothing** — no Bluetooth, no Wi-Fi, no ASTM frame on any channel —
because sub-250 g aircraft are exempt and DJI disables the broadcast in
firmware. No Remote ID receiver at any price detects that aircraft, because
there is no transmission to detect.

But the aircraft is not silent. It transmits **DroneID**, DJI's own telemetry,
continuously while flying. That is what AeroScope received, and it is the only
thing a non-cooperative detector can work with.

The tools are in [`tools/hackrf/`](../tools/hackrf/).

## Why this works even though O4 is encrypted

DJI claimed DroneID was encrypted; [it was
not](https://arxiv.org/pdf/2207.10795), shown independently by Bender and by
RUB-SysSec at NDSS'23. DJI then actually encrypted it on O4 aircraft, and the
Mini 4 Pro is O4 — so the payload is probably unreadable.

**Detection is not decoding.** Every OFDM receiver needs to find the burst,
correct its timing and estimate the channel *before* it can decrypt anything,
and it does that with a synchronisation preamble. A preamble that was encrypted
would be a preamble the drone's own controller could not use. So the preamble
survives, and correlating against it answers "a DJI aircraft is transmitting
here" without reading one bit of payload.

This is not a theory. **AntSDR's `dji_receiver` already ships O4 encrypted
detection**, reporting a hash ID, frequency and RSSI but no position — exactly
the capability described here, and exactly its limit.

## The waveform

Every constant below was read out of a reference implementation, not recalled.
Two independent projects agree on all of them:

| | |
|---|---|
| [proto17/dji_droneid](https://github.com/proto17/dji_droneid) | `matlab/updated_scripts/create_zc.m`, `get_cyclic_prefix_lengths.m`, `get_data_carrier_indices.m` |
| [RUB-SysSec/DroneSecurity](https://github.com/RUB-SysSec/DroneSecurity) | `src/helpers.py`, `src/zcsequence.py`, `src/Packet.py` (NDSS'23) |
| [luyii-code-1/dji-ocusync-droneid-research](https://github.com/luyii-code-1/dji-ocusync-droneid-research) | `droneid_hackrf_scanner.py` — a published **HackRF** scanner, worth comparing against |

One caution when reading DroneSecurity: its `zcsequence_f` is the DFT of a
zero-padded ZC, **not** the carrier mapping, and is not what to port. What it
uses for root identification (`Packet.find_zc_seq`) is proto17's model, and that
is the one implemented here.

| property | value |
|---|---|
| structure | LTE-like OFDM, 15 kHz carrier spacing |
| natural rate | 15.36 MHz → FFT size 1024 |
| carriers | 600 data carriers, DC left empty |
| occupied bandwidth | 9.015 MHz (DroneSecurity gates on 8–11 MHz) |
| symbols | **9**; some older aircraft send 8, skipping the first |
| cyclic prefix | 80 on the **first and last** symbol, 72 on the rest |
| burst length | **643.2 µs** (571.9 µs for the 8-symbol variant) |
| burst interval | ~600 ms |
| **Zadoff-Chu** | **root 600 on symbol 4, root 147 on symbol 6** (1-based) |
| ZC length | 601, with the middle sample dropped — it would land on DC |

The roots are not an inference. DroneSecurity hard-codes a check that raises
`"ZC Sequence not found. Expected: 600 and 147"`.

Centre frequencies proto17 observed: **2399.5, 2414.5, 2429.5, 2444.5,
2459.5 MHz** and **5756.5, 5776.5, 5796.5 MHz**. The aircraft hops, so a
capture narrower than the set sees a fraction of the bursts.

### The trap that makes a naive correlator find nothing

The ZC sequence is **not transmitted as-is**. It is mapped onto the 600 data
carriers and inverse-transformed, so the reference to correlate against is a
full OFDM symbol. Correlating against the raw sequence silently returns
nothing — which reads exactly like "no drone present".

`droneid_phy.zc_symbol()` builds it correctly.

## The detector, and what it actually achieves

`droneid.py` has two paths. All numbers below are **measured** by
`./droneid.py --selftest`, against a burst synthesised from the parameters
above, in noise, at a +48.8 kHz carrier offset (see below for why that
number, and not a token one).

### The gated chain — four stages

| stage | test | measured |
|---|---|---|
| 1 bursts | envelope against a median-absolute-deviation floor | — |
| 2 shape | duration, and −6 dB bandwidth after noise subtraction | reads **8.94–9.12 MHz** against a true 9.015 |
| 3 OFDM | cyclic-prefix autocorrelation at lag = FFT size | 0.99 clean, 0.57 at 0 dB |
| 4 Zadoff-Chu | normalised match against both roots | **0.70 at 0 dB SNR**, noise 0.13 |

Stage 3 deserves note: an OFDM symbol ends with a copy of its own head, so the
signal correlates with itself at a lag of one FFT length. This uses **no DJI
knowledge whatsoever**, which is why it still works on an encrypted payload.
Wi-Fi is OFDM too, but its symbol is 4 µs against DroneID's 66.7 µs — a factor
of seventeen in the lag, which separates them cleanly.

**The chain works down to 0 dB SNR and fails at −5 dB.** The limit is stage 1:
energy detection.

### The deep scan — matched filter, no energy gate

Dropping the energy gate and correlating the whole capture against the ZC
symbol recovers the symbol's own processing gain:

    ./droneid.py hover.iq -s 15360000 --deep

| SNR | bursts found (of 5) |
|---|---|
| 0 dB | 5 |
| −10 dB | 5 |
| −15 dB | 1 — marginal, and not required to pass |

**About 10 dB better than the gated chain, roughly 3× the range in free
space.** For a perimeter, that is the difference between seeing an aircraft at
the fence and seeing it at the tree line.

The confirmation is the **pair**: root 600 on symbol 4 and root 147 on symbol
6 means a genuine burst puts peaks at both roots exactly `CP+FFT+CP+FFT` =
2192 samples apart. Noise at one specified lag is a far tighter distribution
than noise at the best of many, so demanding the spacing costs a real burst
nothing and removes the accidental matches — measured, it survives both
negative controls with no hits at all.

### The carrier offset, which nearly made all of this useless

**The HackRF One has no TCXO.** The official documentation calls it "the
internal crystal" — a plain 25 MHz part — and Great Scott Gadgets publishes no
ppm figure at all. Community consensus is about **±20 ppm**, which is:

| band | offset | in subcarriers |
|---|---|---|
| 2.44 GHz | **±49 kHz** | 3.3 |
| 5.76 GHz | **±115 kHz** | 7.7 |

The 0.1–0.5 ppm figures quoted for the HackRF are the *aftermarket TCXO
modules*, not the stock board.

A Zadoff-Chu sequence has a time–frequency duality: **shifting it in frequency
shifts its correlation peak in time**, by an amount that depends on the root.
Measured on a synthetic burst:

| carrier offset | root 600 peak moves | root 147 peak moves | pairing score |
|---|---|---|---|
| 0 | 0 | 0 | **0.995** |
| 15 kHz | +2 | **−250** | **0.007** |
| 30 kHz | +3 | **−501** | 0.006 |
| 48.8 kHz | +6 | +273 | 0.006 |

So the two symbols' apparent spacing changes, the fixed 2192-sample pairing
test fails, and **the deep scan silently finds nothing** — on a real capture,
with a real drone in the air. A self test at a token 300 Hz offset passes
happily and tells you none of this.

**The fix is a bank of frequency-shifted references**, ±8 subcarriers in 15 kHz
steps. At the bin matching the capture's error, both roots land at their true
positions and the exact spacing holds again. Two things fall out of it:

- The self test now runs at **+48.8 kHz** by default and still finds 5 of 5
  bursts at −10 dB.
- **The winning bin measures the crystal error.** The test reads +48.8 kHz back
  as +45 kHz — within half a bin. In the field that number is the HackRF's
  offset, not the drone's, and it is worth writing down.

Two useful asymmetries, both measured:

- **Root 600 at bin 0 barely cares about the offset** — its peak magnitude runs
  0.59–0.68 at 0 dB SNR from 0 to 115 kHz. So it stays the cheap primary over
  the whole capture, and the expensive bank runs only on the windows it found.
- **Cyclic-prefix autocorrelation is completely CFO-invariant** — 0.99 at every
  offset tested. Stage 3 needs no correction at all. (Its *phase* would be a
  CFO estimator, but only unambiguously within ±7.5 kHz, so it is not used for
  that.)

### False alarms

Both paths were run against two negative controls and fired on neither:

- **pure noise** — 12 chunks (3.3 s), zero detections, peak match 0.133
- **Wi-Fi-shaped OFDM** at +10 dB — zero detections

### The normalisation is load-bearing

The first version scored the correlation peak against a noise floor, which
**rewards loud signals rather than matching ones** — a strong Wi-Fi burst
outscored a weak genuine one, and the self test produced 54 false pairings.
Dividing by the energy inside each correlation window removes amplitude from
the answer entirely: Wi-Fi and noise both score exactly 0.133 whatever their
level. This is why proto17 bothered to write a normalised cross-correlation
and complained about its cost.

## Capture settings

Capture at the **natural rate**, so nothing is resampled:

```sh
hackrf_transfer -r hover.iq -f 2444500000 -s 15360000 -n 160000000 -l 24 -g 20
./droneid.py hover.iq -s 15360000 --deep
```

`hackrf_transfer` takes the sample rate in Hz, so 15360000 is directly settable
(it resolves exactly on the Si5351 fractional divider).

**Traps in the capture command**, all traced to `hackrf_transfer.c`:

- **`-r` writes SIGNED 8-bit; `-w` writes UNSIGNED.** The WAV path XORs every
  byte with `0x80`. A `-w` file read as `int8` is garbage that looks exactly
  like a dead band. Use `-r`.
- **`-b` is worth setting.** At 15.36 Msps the default baseband filter is
  10 MHz — just narrower than the signal's 9.015 MHz plus skirts. `-b 12000000`
  is a legal setting on the MAX2837 and gives the burst room.
- **`-C ppm` is one-sided.** It applies `(1000000 − ppm)/1000000`, so it can
  only correct a crystal that is fast. The reference bank handles either sign
  and needs no calibration, which is why it is the mechanism used here.
- **Do not require an empty DC bin.** The HackRF is zero-IF, so its own LO
  leakage lands exactly on DroneID's DC subcarrier; and the carrier is not null
  on air either — proto17 records it "sitting around 45 degrees". Exclude the
  centre bin from any occupancy test rather than testing it for silence.

## Two unresolved conflicts that affect the test itself

### Does a grounded drone transmit?

The two best sources say opposite things, and this is exactly the case we tested
in the field:

- **alphafox02/antsdr_dji_droneid**: *"DJI drones only broadcast DroneID when
  motors are spinning. Power-on alone only activates the OcuSync control link."*
- **luyii-code-1/dji-ocusync-droneid-research**: for O2/O3, *"the aircraft
  broadcasts the plaintext DroneID payload after power-on, including operation
  without a connected remote controller."*

Our silent Mini 4 Pro had **motors running but never took off**. If the first
source is right for O4, that is still not enough. **Fly it.** A hover at a few
metres removes the ambiguity, and it costs nothing to do.

### The channel raster is disputed

proto17 observed a **15 MHz** grid — 2399.5, 2414.5, 2429.5, 2444.5,
2459.5 MHz. But the AntSDR firmware's own frequency-shift bank is **±10 and
±20 MHz** around 2434.5, which is a **10 MHz** grid: 2414.5, 2424.5, 2434.5,
2444.5, 2454.5. Only 2414.5 and 2444.5 are common to both.

A shipping commercial receiver and the open-source tools disagree about where
to look. **2444.5 MHz is on both rasters**, which is why it is the capture
centre in the examples — and it is a further argument for running `sweep.py`
first rather than trusting either list.

## What is still unverified

1. **That O4 keeps these ZC roots.** The published work is OcuSync 2.0, tested
   on a Mini 2 and Mavic Air 2. AntSDR detects O4, which means *a* preamble
   survives; whether it is this one is not established here. **A capture
   settles it** — and a null result on a flying Mini 4 Pro means the roots
   changed, not that the method is wrong.
2. **Which band a CE-spec Mini 4 Pro actually uses.** In Switzerland, OFCOM
   RIR1010-05 limits UAS at 5 GHz to **5170–5250 MHz at 200 mW**, against
   14 dBm at 5.8 GHz — a 9 dB legal incentive to sit in the lower band. DJI's
   O4 Air Unit specs list both 5.170–5.250 and 5.725–5.850 GHz and **no 2.4 GHz
   at all**. Sweep first.
3. **Whether the Mini 4 Pro is O4-encrypted at all.** AntSDR's table names
   Mini 5 for O4-encrypted and lists Mavic 3 as decodable. If the Mini 4 Pro
   falls on the decodable side, the payload is readable and this becomes a
   full identification rather than a detection.

## Files

| file | what |
|---|---|
| `tools/hackrf/droneid_phy.py` | the waveform: constants, ZC construction, burst synthesis |
| `tools/hackrf/droneid.py` | the detector, the deep scan, and `--selftest` |
| `tools/hackrf/sweep.py` | spectrum survey, baseline and compare |
