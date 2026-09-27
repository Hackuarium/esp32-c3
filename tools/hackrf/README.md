# HackRF tools

Two scripts for the next session. Both are **receive only** — `hackrf_sweep`
has no transmit path, and `hackrf_transfer -r` is the read direction.

    brew install hackrf
    hackrf_info          # confirm the device is seen
    pip3 install numpy   # the only dependency; no scipy

The waveform these decode, with sources and the measured detector performance,
is in [docs/drone-droneid-phy.md](../../docs/drone-droneid-phy.md).

## Check the code before trusting it

```sh
./droneid.py --selftest
```

Runs the whole chain against a DroneID burst synthesised from the published
parameters, in noise, at a **+48.8 kHz carrier offset** — what a stock HackRF's
crystal actually does — then against pure noise and against Wi-Fi-shaped OFDM to
check it stays quiet. Takes about ninety seconds and needs no hardware and no
aircraft.

It validates the **code against the published waveform**. Only a capture
validates the waveform against a real aircraft.

## 1. Which band does the aircraft actually use?

Everything in `docs/drone-rf-detection.md` reasons about what a drone *should*
radiate. This records what it does, and it is the measurement the RX5808
decision hangs on.

```sh
# aircraft powered DOWN, building exactly as it will be later
./sweep.py baseline --band 2g4 -o baseline-2g4.npz
./sweep.py baseline --band 5g8 -o baseline-5g8.npz

# aircraft flying, forced to each band in turn from DJI Fly
./sweep.py compare --band 5g8 -b baseline-5g8.npz
./sweep.py compare --band 2g4 -b baseline-2g4.npz
```

Forcing the band makes this a controlled experiment: the band you forced should
light up and the other should not, which validates the instrument at the same
time as it measures the aircraft.

**Sweep `5g1` too.** In Switzerland OFCOM RIR1010-05 limits drones at 5 GHz to
**5170–5250 MHz at 200 mW**, against 25 mW at 5.8 GHz — a 9 dB legal incentive
to sit in the band an RX5808 cannot tune at all. If the aircraft lives there,
the cheap-sensor plan is dead and it is better to know before ordering parts.

Bands: `2g4` (2400–2500), `5g8` (5725–5945), `5g1` (5150–5350), `5all`.

### What else is in the 5.8 GHz window

It is not empty, and two of these are far louder than any CE-legal drone:

| what | where | level |
|---|---|---|
| BFWA fixed links | 5725–5795, 5815–5875 | licensed; the likeliest persistent carrier |
| road tolling (LSVA/RPLP) | 5795–5815 | 2 W e.i.r.p. — **notched out by default** |
| ITS, non-safety | 5855–5875 | **33 dBm e.i.r.p.** — 19 dB above a CE drone |
| amateur, secondary | 5650–5850 | plus amateur-satellite downlink 5830–5850 |

5725–5875 is also an **ISM band** in Switzerland with **radiolocation primary**,
so it is quiet of Wi-Fi, not quiet.

## 2. Is that emission DroneID?

```sh
hackrf_transfer -r hover.iq -f 2444500000 -s 15360000 -b 12000000 \
                -n 160000000 -l 24 -g 20
./droneid.py hover.iq -s 15360000 --deep
```

Set `-f` to whatever `sweep.py compare` found; 2444.5 MHz is the one centre both
published channel rasters agree on. **Capture at 15360000** — the waveform's own
rate, so nothing is resampled — and **use `-r`, never `-w`**: the WAV path
writes *unsigned* bytes, which read as `int8` look exactly like a dead band.

**Fly the aircraft, do not just spin the motors.** Sources disagree on whether a
grounded drone transmits DroneID at all, and our silent field test never left
the ground.

Two paths:

- **default** — energy detection, then shape, OFDM structure and Zadoff-Chu.
  Prints its working at every stage. Good to **0 dB SNR**.
- **`--deep`** — matched filter with no energy gate, requiring both ZC roots at
  their exact published spacing, searched over a ±120 kHz frequency bank. Good
  to **−10 dB**, about 3× the range. Slower, and it also prints the HackRF's own
  crystal error, which is worth writing down.

Run `--deep` whenever the default finds nothing.

## Gain, and how to tell it is wrong

Start `-l 24 -g 20`.

- **clipping** — stage 1 reports a huge peak/floor and every burst has the
  wrong bandwidth. Reduce `-l` first, in 8 dB steps.
- **nothing at all** — raise `-l` to 32 or 40. If `sweep.py` shows a flat floor
  with no building Wi-Fi in it, the antenna is wrong for the band.

Do **not** reach for `hackrf_transfer -C` to correct the crystal: it only
corrects one direction, and `--deep` already searches both.

**The stock telescopic antenna is useless at 2.4 and 5.8 GHz.** Use an FPV
patch or a Wi-Fi dipole with an SMA connector.

## Caveat

Neither script has been run against a radio — there was no HackRF on this
machine when they were written. The self test exercises every stage of
`droneid.py` end to end, so the signal processing is checked; what is unchecked
is `hackrf_sweep`'s output format in `sweep.py`. **Compare `sweep.py raw`
against `hackrf_sweep` by eye once** before trusting a number from it.
