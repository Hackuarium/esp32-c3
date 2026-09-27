#!/usr/bin/env python3
"""Find DJI DroneID bursts in an IQ capture, without decoding them.

The point of this tool is the one thing Remote ID cannot do: detect a DJI
aircraft that is not broadcasting. A Mini 4 Pro on its standard battery is
sub-250 g, EASA class C0, and DJI disables Remote ID in firmware - so there is
nothing for a Remote ID receiver to hear. The aircraft still transmits DroneID,
DJI's own telemetry, while flying.

On O4 aircraft that payload is encrypted and this tool does not try to read it.
It detects the *waveform*. The synchronisation preamble has to survive
encryption, because the receiver needs it before it can decrypt anything - so
correlating against the preamble finds an aircraft that cannot be decoded.

Four stages, cheapest first, each usable on its own:

    1  bursts      short strong events against a robust noise floor
    2  shape       duration and occupied bandwidth
    3  OFDM        cyclic-prefix autocorrelation - needs no DJI knowledge
    4  Zadoff-Chu  the DJI-specific confirmation, roots 600 and 147

Capture (receive only; -r is read). Use the natural rate, so no resampling:

    hackrf_transfer -r hover.iq -f 2444500000 -s 15360000 -n 160000000 -l 24 -g 20
    ./droneid.py hover.iq -s 15360000

Check the whole chain with no hardware and no aircraft:

    ./droneid.py --selftest
"""

import argparse
import sys

import numpy as np

from droneid_phy import (BANDWIDTH_GATE_HZ, BURST_LENGTH_LEGACY_S,
                         BURST_LENGTH_S, BURST_PERIOD_S, CFO_BINS, CP_PATTERN,
                         CP_SHORT, FFT_SIZE, KNOWN_CENTRES_HZ, NATURAL_RATE,
                         ZC_ROOTS, burst_samples, resample, synthesize_burst,
                         zc_symbol, zc_symbol_shifted)


def load_iq(path, limit=None):
    """hackrf_transfer writes 8-bit SIGNED interleaved I/Q (int8_t, verified
    against hackrf_transfer.c). Reading it as unsigned puts a large DC term at
    0 Hz - if stage 1 reports one enormous bin at the centre, that is this."""
    raw = np.fromfile(path, dtype=np.int8, count=-1 if limit is None else limit * 2)
    if raw.size < 2:
        sys.exit(f"{path}: no samples")
    if raw.size % 2:
        raw = raw[:-1]
    return (raw[0::2].astype(np.float32) + 1j * raw[1::2].astype(np.float32)) / 128.0


def robust_floor(values):
    """Median absolute deviation. The mean is dragged up by the signal."""
    median = np.median(values)
    return median, 1.4826 * np.median(np.abs(values - median)) + 1e-12


def find_bursts(samples, rate, sigma=8.0):
    """Stage 1. The envelope is smoothed over about one OFDM symbol so the
    gaps between symbols do not split one burst into nine detections."""
    window = max(1, int(FFT_SIZE * rate / NATURAL_RATE))
    envelope = np.convolve(np.abs(samples) ** 2,
                           np.ones(window, dtype=np.float32) / window, mode="same")
    median, spread = robust_floor(envelope)
    above = envelope > median + sigma * spread

    bursts, start = [], None
    for index, flag in enumerate(above):
        if flag and start is None:
            start = index
        elif not flag and start is not None:
            if index - start >= window:
                bursts.append((start, index))
            start = None
    if start is not None and len(above) - start >= window:
        bursts.append((start, len(above)))

    snr = 10 * np.log10(envelope.max() / median) if median > 0 else float("nan")
    return bursts, snr


PSD_BINS = 256


def welch_psd(samples, bins=PSD_BINS):
    """Averaged periodogram, centre-zero. Averaging matters: a single FFT of a
    noisy block is itself so noisy that the 99 % span wanders."""
    segments = len(samples) // bins
    if segments == 0:
        samples = np.pad(samples, (0, bins - len(samples)))
        segments = 1
    block = samples[:segments * bins].reshape(segments, bins) * np.hanning(bins)
    return (np.abs(np.fft.fftshift(np.fft.fft(block, axis=1), axes=1)) ** 2).mean(axis=0)


def occupied_bandwidth(block, rate, noise_psd=None, decibels=6.0):
    """The signal's width, as the span `decibels` down from the in-band peak.

    Two things this does NOT do, both learned from the self test:

    The noise floor is subtracted first. Noise is white across the whole
    window, so without subtraction this measures the CAPTURE bandwidth as SNR
    falls - 15 MHz for a 9 MHz burst at 0 dB, which rejects a real detection.

    And the criterion is an x-dB width, not a 99 %-of-power width. A
    cumulative-power span is dominated by its tails, so residual noise in the
    skirts still inflated it to 11-13 MHz after subtraction. DroneID's
    spectrum is flat across its 600 carriers, so an x-dB width sits on a
    near-vertical edge and barely moves with SNR.
    """
    psd = welch_psd(block)
    if noise_psd is not None:
        psd = np.clip(psd - noise_psd, 0.0, None)
    smoothed = np.convolve(psd, np.ones(5) / 5, mode="same")
    if smoothed.max() <= 0:
        return 0.0
    above = np.flatnonzero(smoothed >= smoothed.max() * 10 ** (-decibels / 10))
    if above.size < 2:
        return 0.0
    return (above[-1] - above[0]) * rate / len(psd)


def noise_reference(samples, bursts, rate):
    """The PSD of everything that is not a burst. Returns None when the
    capture is too full of bursts to find a quiet stretch."""
    mask = np.ones(len(samples), dtype=bool)
    guard = int(FFT_SIZE * rate / NATURAL_RATE)
    for start, end in bursts:
        mask[max(0, start - guard):min(len(samples), end + guard)] = False
    quiet = samples[mask]
    return welch_psd(quiet) if len(quiet) >= PSD_BINS * 4 else None


def cp_autocorrelation(block, rate):
    """Stage 3: is it OFDM, with DJI's symbol timing?

    An OFDM symbol ends with a copy of its own head, so correlating the signal
    against itself delayed by one FFT length gives a peak of width CP. This
    uses no knowledge of the payload at all, which is exactly why it still
    works when the payload is encrypted.

    Wi-Fi is OFDM too, but its symbol is 4 us against DroneID's 66.7 us, so the
    lag at which the peak appears separates them by a factor of seventeen.
    """
    lag = int(FFT_SIZE * rate / NATURAL_RATE)
    cp = int(CP_SHORT * rate / NATURAL_RATE)
    if len(block) < lag + cp * 2:
        return 0.0
    head, tail = block[:-lag], block[lag:]
    n = min(len(head), len(tail))
    kernel = np.ones(cp, dtype=np.float32)
    numerator = np.abs(np.convolve(head[:n] * np.conj(tail[:n]), kernel, mode="valid"))
    energy = np.convolve(np.abs(head[:n]) ** 2 + np.abs(tail[:n]) ** 2, kernel, mode="valid")
    return float((2 * numerator / (energy + 1e-12)).max())


def matched_filter(samples, reference):
    """NORMALISED cross-correlation magnitude, 0 to 1, at every lag.

    The normalisation is the whole point and is not a refinement. A bare
    correlation scales with the input amplitude, so measuring its peak against
    a noise floor rewards loud signals rather than matching ones: a strong
    Wi-Fi burst then scores higher than a weak but genuine DroneID burst, and
    the self test duly produced 54 false pairings from Wi-Fi. Dividing by the
    energy actually inside each window removes amplitude from the answer.
    """
    length = len(reference)
    size = 1 << int(np.ceil(np.log2(len(samples) + length)))
    correlation = np.abs(np.fft.ifft(
        np.fft.fft(samples, size) * np.conj(np.fft.fft(reference, size))
    ))[:len(samples)]

    cumulative = np.concatenate([[0.0], np.cumsum(np.abs(samples) ** 2)])
    window = cumulative[length:] - cumulative[:-length]
    window = np.concatenate([window, np.full(len(samples) - len(window), np.inf)])
    return correlation / (np.sqrt(window) * np.linalg.norm(reference) + 1e-12)


def zc_correlate(block, rate):
    """Stage 4: roots 600 and 147, on symbols 4 and 6.

    The sequence is mapped onto the OFDM carriers and inverse-transformed, so
    the reference is a full symbol rather than the raw sequence - correlating
    against the raw sequence finds nothing.
    """
    natural = resample(block, rate, NATURAL_RATE) if rate != NATURAL_RATE else block
    return {root: max(float(matched_filter(natural, zc_symbol_shifted(root, k)).max())
                      for k in range(-CFO_BINS, CFO_BINS + 1))
            for root in sorted(set(ZC_ROOTS.values()))}


# Measured, not chosen. Pure noise peaks at rho = 0.133 over a chunk and the
# paired lag sits at 0.031; a real burst reaches 0.22 / 0.16 at -15 dB SNR.
# 12 chunks (3.3 s) of noise produced zero primaries above 0.18.
RHO_PRIMARY = 0.18
RHO_PAIRED = 0.10
RHO_MATCH = 0.30   # calling a single burst DJI, where the burst is already located
CHUNK = 1 << 22


def deep_scan(samples, rate, primary=RHO_PRIMARY, paired=RHO_PAIRED, quiet=False):
    """Matched-filter search with no energy gate. Much more sensitive.

    Stages 1-3 only examine bursts that energy detection already found, so the
    whole chain is only as sensitive as its threshold detector - it dies at
    about -5 dB SNR. Correlating against the ZC symbol directly recovers the
    symbol's own processing gain and still finds every burst at -10 dB, with
    the peak on the exact sample (marginal at -15 dB: 2 of 5). That is about
    10 dB, roughly 3x the range in free space, and it is what matters for a
    drone at the far end of a perimeter.

    The confirmation is the pair: root 600 is on symbol 4 and root 147 on
    symbol 6, so a real burst puts peaks at BOTH roots exactly CP+FFT+CP+FFT
    samples apart. Noise reaches the paired lag with rho 0.031 against a real
    burst's 0.16, so demanding the spacing costs nothing and removes the
    accidental matches.
    """
    if rate != NATURAL_RATE:
        samples = resample(samples, rate, NATURAL_RATE)
    spacing = CP_PATTERN[4] + FFT_SIZE + CP_PATTERN[5] + FFT_SIZE
    primary_reference = zc_symbol(600)
    bank = [(k, zc_symbol_shifted(600, k), zc_symbol_shifted(147, k))
            for k in range(-CFO_BINS, CFO_BINS + 1)]

    hits = []
    for base in range(0, len(samples), CHUNK):
        block = samples[base:base + CHUNK + spacing + FFT_SIZE * 2]
        if len(block) < spacing + FFT_SIZE * 2:
            break
        # Root 600 at zero offset is the cheap primary: measured, its peak
        # magnitude barely moves with carrier error (0.59-0.68 at 0 dB SNR from
        # 0 to 115 kHz), so one correlation finds every candidate whatever the
        # crystal is doing. The bank then runs only on the few windows it found.
        score = matched_filter(block, primary_reference)
        limit = min(CHUNK, len(block) - spacing)
        candidates = np.flatnonzero(score[:limit] > primary)

        last = -FFT_SIZE
        for index in candidates:
            if index - last < FFT_SIZE:
                continue
            best = confirm_pair(block, int(index), spacing, bank, paired)
            if best is not None:
                last = index
                hits.append((base + int(index), *best))

    if not quiet:
        print(f"--- deep scan: matched filter over a +-{CFO_BINS * 15} kHz bank, "
              f"{len(hits)} paired ZC hit(s) ---")
        if hits:
            print(f"{'t (s)':>10} {'root 600':>9} {'root 147':>9} {'offset kHz':>11}")
            for at, first, second, offset in hits[:40]:
                print(f"{at/NATURAL_RATE:10.4f} {first:9.3f} {second:9.3f} {offset:11.0f}")
            gaps = np.diff([h[0] for h in hits]) / NATURAL_RATE
            if len(gaps):
                print(f"median gap {np.median(gaps)*1000:.1f} ms")
            offsets = [h[3] for h in hits]
            print(f"carrier offset {np.median(offsets):+.0f} kHz "
                  f"(the HackRF's crystal, not the drone)")
        else:
            print("no burst carrying both Zadoff-Chu roots at the right spacing")
    return hits


def confirm_pair(block, index, spacing, bank, paired):
    """Is there a root-147 symbol exactly `spacing` after this root-600 peak?

    Searched over the frequency-shift bank, because the apparent spacing is
    only correct at the bin that matches the capture's carrier error. Returns
    (rho600, rho147, offset_kHz) for the best bin, or None.
    """
    start = max(0, index - FFT_SIZE)
    window = block[start:index + spacing + FFT_SIZE * 2]
    if len(window) < spacing + FFT_SIZE:
        return None

    best = None
    for bins, reference600, reference147 in bank:
        first = matched_filter(window, reference600)
        peak = int(np.argmax(first))
        if peak + spacing >= len(window):
            continue
        second = matched_filter(window, reference147)[peak + spacing]
        if second > paired and (best is None or second > best[1]):
            best = (float(first[peak]), float(second), bins * 15.0)
    return best


def periodicity(bursts, rate):
    """A burst every ~600 ms with a ~0.1 % duty cycle is itself a strong
    discriminator - nothing else in the band repeats on that cadence."""
    if len(bursts) < 3:
        return None
    starts = np.array([b[0] for b in bursts], dtype=np.float64) / rate
    gaps = np.diff(starts)
    return float(np.median(gaps)), float(np.std(gaps))


def analyse(samples, rate, sigma, quiet=False):
    """The four stages. Returns the candidate bursts."""
    expected = burst_samples(rate)
    say = (lambda *a: None) if quiet else print

    say(f"{len(samples):,} samples, {len(samples)/rate:.2f} s at {rate/1e6:.3f} Msps")
    say(f"a burst should be ~{expected:,} samples ({BURST_LENGTH_S*1e6:.0f} us, "
        f"{BURST_LENGTH_LEGACY_S*1e6:.0f} us for 8-symbol drones) "
        f"and repeat every ~{BURST_PERIOD_S*1000:.0f} ms\n")

    bursts, snr = find_bursts(samples, rate, sigma)
    say("--- stage 1: bursts ---")
    say(f"peak/floor {snr:.1f} dB, {len(bursts)} events above threshold")
    if not bursts:
        say("\nnothing to analyse. Either nothing was transmitting, the gain is too\n"
            "low, or the capture is off frequency - check with sweep.py first.")
        return []

    low_hz, high_hz = BANDWIDTH_GATE_HZ
    noise_psd = noise_reference(samples, bursts, rate)
    candidates = []
    say("\n--- stages 2 and 3: shape and OFDM ---")
    if noise_psd is None:
        say("(no quiet stretch found; bandwidth is not noise-corrected)")
    say(f"{'#':>3} {'t (s)':>8} {'len us':>8} {'BW MHz':>7} {'CP corr':>8}  verdict")
    for index, (start, end) in enumerate(bursts[:40]):
        block = samples[start:end]
        bandwidth = occupied_bandwidth(block, rate, noise_psd)
        correlation = cp_autocorrelation(block, rate)

        reasons = []
        if not 0.4 * expected <= len(block) <= 3.0 * expected:
            reasons.append("length")
        if not low_hz <= bandwidth <= high_hz:
            reasons.append("bandwidth")
        if correlation <= 0.5:
            reasons.append("not OFDM")
        if not reasons:
            candidates.append((start, end))
        say(f"{index:>3} {start/rate:8.3f} {len(block)/rate*1e6:8.0f} "
            f"{bandwidth/1e6:7.2f} {correlation:8.2f}  "
            f"{','.join(reasons) if reasons else 'CANDIDATE'}")

    say(f"\n{len(candidates)} candidate burst(s)")

    result = periodicity(candidates or bursts, rate)
    if result:
        median, spread = result
        say(f"\n--- periodicity ---\nmedian gap {median*1000:.1f} ms, "
            f"spread {spread*1000:.1f} ms")
        say(f"*** consistent with DroneID's ~{BURST_PERIOD_S*1000:.0f} ms cadence ***"
            if abs(median - BURST_PERIOD_S) < 0.1
            else "not the DroneID cadence - something else that bursts")

    if candidates:
        say("\n--- stage 4: Zadoff-Chu ---")
        start, end = candidates[0]
        say("normalised match, 0 to 1. Noise reaches 0.13; a real burst 0.7 at 0 dB")
        for root, score in zc_correlate(samples[start:end], rate).items():
            say(f"root {root:>3}: {score:6.3f}"
                f"{'   <-- DJI' if score > RHO_MATCH else ''}")
    return candidates


# A stock HackRF's crystal is ~20 ppm, so 2.44 GHz lands up to 49 kHz out. The
# self test runs at that, not at a token few hundred hertz: a detector checked
# only on-frequency passes and then finds nothing in the field.
SELFTEST_CFO_HZ = 48_800


def _stream(kind, snr_db, seed=7, bursts=5, cfo_hz=SELFTEST_CFO_HZ):
    """A test capture at the natural rate: `bursts` events on a noise floor,
    offset by the carrier error a real HackRF actually has."""
    generator = np.random.default_rng(seed)
    gap = int(BURST_PERIOD_S * NATURAL_RATE)
    stream = (generator.normal(size=gap * bursts)
              + 1j * generator.normal(size=gap * bursts)) / np.sqrt(2)
    if kind == "droneid":
        payload = synthesize_burst(seed=1)
    elif kind == "wifi":  # 802.11a/n at 20 MHz: 3.2 us symbol against DJI's 66.7
        symbols = []
        for _ in range(200):
            spectrum = np.zeros(64, dtype=complex)
            spectrum[4:60] = np.exp(1j * generator.integers(0, 4, 56) * np.pi / 2)
            time = np.fft.ifft(np.fft.ifftshift(spectrum))
            symbols.append(np.concatenate([time[-16:], time]))
        payload = resample(np.concatenate(symbols), 20e6, NATURAL_RATE)
    else:
        payload = None

    if payload is not None:
        payload = payload / np.sqrt(np.mean(np.abs(payload) ** 2)) * 10 ** (snr_db / 20)
        for index in range(bursts):
            at = index * gap + gap // 3
            stream[at:at + len(payload)] += payload
    return stream * np.exp(2j * np.pi * cfo_hz * np.arange(len(stream)) / NATURAL_RATE)


def selftest():
    """Run the whole chain against synthesised signals. No hardware, no drone.

    This checks the code against the published waveform. It cannot check that
    the published waveform is what a Mini 4 Pro emits - only a capture does
    that.
    """
    failures = []

    for snr_db in (20.0, 0.0):
        print(f"\n########## gated chain, burst at {snr_db:+.0f} dB SNR ##########")
        stream = _stream("droneid", snr_db)
        candidates = analyse(stream, NATURAL_RATE, sigma=8.0)
        if not candidates:
            failures.append(f"gated @ {snr_db:+.0f} dB: no candidate burst")
            continue
        scores = zc_correlate(stream[candidates[0][0]:candidates[0][1]], NATURAL_RATE)
        for root in (600, 147):
            if scores.get(root, 0) < RHO_MATCH:
                failures.append(f"gated @ {snr_db:+.0f} dB: root {root} "
                                f"only {scores.get(root, 0):.3f}")

    print(f"\n########## deep scan, no energy gate, "
          f"{SELFTEST_CFO_HZ/1000:+.1f} kHz carrier offset ##########")
    for snr_db, required in ((0.0, 5), (-10.0, 5), (-15.0, 0)):
        hits = deep_scan(_stream("droneid", snr_db), NATURAL_RATE, quiet=True)
        estimate = f", offset read back {np.median([h[3] for h in hits]):+.0f} kHz" if hits else ""
        print(f"{snr_db:+6.0f} dB SNR: {len(hits)} of 5 bursts"
              + (f", best rho {max(h[1] for h in hits):.3f}" if hits else "") + estimate
              + ("" if required else "   (marginal by design, not required)"))
        if len(hits) < required:
            failures.append(f"deep @ {snr_db:+.0f} dB: only {len(hits)} of 5 bursts")

    print("\n########## negative controls ##########")
    for kind, label in (("noise", "noise only"), ("wifi", "Wi-Fi-like OFDM @ +10 dB")):
        stream = _stream(kind, 10.0)
        gated = analyse(stream, NATURAL_RATE, sigma=8.0, quiet=True)
        deep = deep_scan(stream, NATURAL_RATE, quiet=True)
        print(f"{label:26} gated={len(gated)} candidates, deep={len(deep)} hits")
        if gated or deep:
            failures.append(f"{label}: false positive")

    print("\n==========================================")
    if failures:
        print("SELF TEST FAILED")
        for line in failures:
            print(" ", line)
        return 1
    print("SELF TEST PASSED")
    print(f"  gated chain fires to 0 dB SNR, deep scan to -10 dB")
    print(f"  both at a {SELFTEST_CFO_HZ/1000:+.1f} kHz carrier offset, which is what a stock HackRF has")
    print("  neither fires on noise or on Wi-Fi-shaped OFDM")
    print("This validates the code against the published waveform, not the")
    print("waveform against a real aircraft. Only a capture does that.")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("capture", nargs="?", help="IQ file from hackrf_transfer -r")
    parser.add_argument("-s", "--rate", type=float, help="sample rate, Hz")
    parser.add_argument("--sigma", type=float, default=8.0, help="burst threshold")
    parser.add_argument("--max-seconds", type=float, default=10.0)
    parser.add_argument("--deep", action="store_true",
                        help="matched filter, no energy gate: ~10 dB more sensitive")
    parser.add_argument("--rho", type=float, default=RHO_PRIMARY,
                        help=f"deep-scan match threshold, 0-1 (default {RHO_PRIMARY})")
    parser.add_argument("--selftest", action="store_true",
                        help="check the detector against a synthetic burst")
    arguments = parser.parse_args()

    if arguments.selftest:
        return selftest()
    if not arguments.capture or not arguments.rate:
        parser.error("give a capture and -s RATE, or --selftest")

    print("known DroneID centres, MHz: "
          + ", ".join(f"{f/1e6:.1f}" for f in KNOWN_CENTRES_HZ))
    print("(the aircraft hops; a capture narrower than the set sees a fraction "
          "of the bursts)\n")
    samples = load_iq(arguments.capture, int(arguments.rate * arguments.max_seconds))

    if arguments.deep:
        deep_scan(samples, arguments.rate, arguments.rho)
        print("\nA paired hit is a burst carrying both Zadoff-Chu roots at exactly\n"
              "the published spacing. Nothing else in the band does that.")
        return 0

    analyse(samples, arguments.rate, arguments.sigma)
    print("\nStages 1-3 say 'an OFDM burst of about the right shape and cadence'.\n"
          "Stage 4 is what makes it DJI. If nothing was found, try --deep, which\n"
          "drops the energy gate and reaches about 10 dB deeper.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
