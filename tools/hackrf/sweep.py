#!/usr/bin/env python3
"""Spectrum survey with a HackRF: what is on the air, and what changed.

The measurement that settles arguments. Everything else in this project reasons
about what a drone *should* radiate; this records what it does.

    ./sweep.py baseline --band 2g4 -o baseline-2g4.npz     # drone off
    ./sweep.py baseline --band 5g8 -o baseline-5g8.npz
    ./sweep.py compare  --band 5g8 -b baseline-5g8.npz     # drone flying

`compare` prints the bins that rose above the baseline, which is the drone's
signature and nothing else. Run the baseline with the aircraft powered down and
everything else in the building exactly as it will be later - the point is to
subtract the site, so the site has to be the same.

Receive only: hackrf_sweep never keys the transmitter.

UNTESTED AGAINST HARDWARE - written before the HackRF tools were installed.
Check the first `raw` output against `hackrf_sweep` by eye before trusting a
number from it.
"""

import argparse
import subprocess
import sys

import numpy as np

# The two bands worth watching, and why these edges.
#
#   2g4  the whole ISM band. DJI OcuSync lives here, and so does everything
#        else, which is the problem.
#   5g8  starts at 5725 deliberately: EU Wi-Fi is allocated 5150-5350 and
#        5470-5725 only, so beginning above it removes the dominant interferer
#        outright. Under FCC this trick does not work - 5725-5850 is UNII-3.
BANDS = {
    "2g4": (2400, 2500),
    "5g8": (5725, 5945),
    "5g1": (5150, 5350),  # DJI's lower 5 GHz band, which the RX5808 cannot reach
    "5all": (5150, 5950),
}

# Ranges blanked before the statistics are taken, because a licensed service
# two orders of magnitude above a CE drone would set the threshold on its own.
# Both are Swiss allocations and both sit inside the 5g8 window:
#
#   5795-5815  road tolling, LSVA/RPLP (RIR1012-01 and RIR1012-06), 2 W e.i.r.p.
#   5855-5875  ITS non-safety (RIR0510-02), 33 dBm e.i.r.p. - 19 dB above a
#              CE-legal drone, and above the 5.725-5.850 the aircraft uses
#
# What is NOT notched, because it overlaps the drone band and has to be read
# rather than hidden: BFWA fixed links across 5725-5795 and 5815-5875, which
# are the likeliest persistent strong carrier on a given site.
NOTCHES = [(5795, 5815), (5855, 5875)]


def run_sweep(low_mhz, high_mhz, bin_hz, lna, vga, seconds):
    """One pass of hackrf_sweep, accumulated into a dict of bin -> powers.

    hackrf_sweep prints one line per segment:
        date, time, hz_low, hz_high, hz_bin_width, num_samples, dB, dB, ...
    where the dB values tile hz_low..hz_high. Segments arrive interleaved and
    out of order, so they are binned by frequency rather than by arrival.
    """
    command = [
        "hackrf_sweep",
        "-f", f"{low_mhz}:{high_mhz}",
        "-w", str(bin_hz),
        "-l", str(lna),
        "-g", str(vga),
    ]
    print(f"$ {' '.join(command)}", file=sys.stderr)

    bins = {}
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True
    )
    try:
        import time

        deadline = time.time() + seconds
        for line in process.stdout:
            if time.time() > deadline:
                break
            parts = [p.strip() for p in line.split(",")]
            if len(parts) < 7:
                continue
            try:
                hz_low = float(parts[2])
                width = float(parts[4])
                powers = [float(p) for p in parts[6:]]
            except ValueError:
                continue
            for index, power in enumerate(powers):
                centre = hz_low + width * (index + 0.5)
                bins.setdefault(round(centre / 1e6, 4), []).append(power)
    finally:
        process.terminate()
        process.wait(timeout=5)

    if not bins:
        sys.exit("no data from hackrf_sweep - is the HackRF connected?")

    frequencies = np.array(sorted(bins))
    # The maximum, not the mean. A drone burst is short and rare; averaging it
    # against the silence between bursts is how you lose it.
    peak = np.array([max(bins[f]) for f in frequencies])
    mean = np.array([sum(bins[f]) / len(bins[f]) for f in frequencies])
    return frequencies, peak, mean


def notch(frequencies, powers):
    """Blank the ranges that are known not to be drones."""
    masked = powers.copy()
    for low, high in NOTCHES:
        masked[(frequencies >= low) & (frequencies <= high)] = np.nan
    return masked


def noise_floor(powers):
    """Median absolute deviation, because the mean is dragged up by the very
    signals being looked for."""
    finite = powers[np.isfinite(powers)]
    median = np.median(finite)
    deviation = np.median(np.abs(finite - median))
    return median, 1.4826 * deviation


def report(frequencies, powers, label, sigma=6.0):
    median, spread = noise_floor(powers)
    threshold = median + sigma * spread
    print(f"\n=== {label} ===")
    print(f"noise floor {median:6.1f} dB, sigma {spread:4.1f} dB, "
          f"threshold {threshold:6.1f} dB")

    above = np.isfinite(powers) & (powers > threshold)
    if not above.any():
        print("nothing above the floor")
        return

    # Group adjacent bins into one emitter rather than printing every bin.
    groups, start = [], None
    for index, flag in enumerate(above):
        if flag and start is None:
            start = index
        elif not flag and start is not None:
            groups.append((start, index - 1))
            start = None
    if start is not None:
        groups.append((start, len(above) - 1))

    print(f"{'from MHz':>10} {'to MHz':>10} {'width':>8} {'peak dB':>9}")
    for first, last in groups:
        width = frequencies[last] - frequencies[first]
        print(f"{frequencies[first]:10.2f} {frequencies[last]:10.2f} "
              f"{width:7.1f}M {powers[first:last + 1].max():9.1f}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mode", choices=["baseline", "compare", "raw"])
    parser.add_argument("--band", default="2g4", choices=sorted(BANDS))
    parser.add_argument("-o", "--out", help="where to write a baseline")
    parser.add_argument("-b", "--baseline", help="baseline to compare against")
    parser.add_argument("--bin", type=int, default=1000000, help="bin width, Hz")
    parser.add_argument("--lna", type=int, default=32, help="LNA gain, 0-40 in 8 dB steps")
    parser.add_argument("--vga", type=int, default=20, help="VGA gain, 0-62 in 2 dB steps")
    parser.add_argument("--seconds", type=float, default=20.0)
    arguments = parser.parse_args()

    low, high = BANDS[arguments.band]
    frequencies, peak, mean = run_sweep(
        low, high, arguments.bin, arguments.lna, arguments.vga, arguments.seconds
    )

    if arguments.mode == "baseline":
        out = arguments.out or f"baseline-{arguments.band}.npz"
        np.savez(out, frequencies=frequencies, peak=peak, mean=mean,
                 band=arguments.band, lna=arguments.lna, vga=arguments.vga)
        print(f"wrote {out}: {len(frequencies)} bins, "
              f"{frequencies[0]:.1f}-{frequencies[-1]:.1f} MHz")
        report(frequencies, notch(frequencies, peak), "baseline, peak hold")
        return

    if arguments.mode == "raw":
        report(frequencies, notch(frequencies, peak), "peak hold")
        report(frequencies, notch(frequencies, mean), "mean")
        return

    if not arguments.baseline:
        sys.exit("compare needs -b baseline.npz")
    stored = np.load(arguments.baseline)
    if stored["frequencies"].shape != frequencies.shape:
        sys.exit("baseline has a different bin layout - re-run it with the same --bin")
    if int(stored["lna"]) != arguments.lna or int(stored["vga"]) != arguments.vga:
        sys.exit("baseline used different gains - a dB difference would be meaningless")

    # Peak-hold against peak-hold: a burst that appeared in neither is absent
    # from both, and one that appeared only now stands out by its full height.
    rise = peak - stored["peak"]
    report(frequencies, notch(frequencies, rise), "risen above baseline", sigma=5.0)
    print("\nAnything above is what changed since the baseline. If the only\n"
          "difference between the two runs was the aircraft, that is the aircraft.")


if __name__ == "__main__":
    main()
