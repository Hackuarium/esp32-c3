"""The DJI DroneID physical layer, as published by the reverse-engineering work.

Every constant here was read out of a reference implementation rather than
recalled, and the two independent projects agree on all of them:

  proto17/dji_droneid       matlab/updated_scripts/create_zc.m
                            matlab/updated_scripts/get_cyclic_prefix_lengths.m
                            matlab/updated_scripts/get_data_carrier_indices.m
  RUB-SysSec/DroneSecurity  src/helpers.py, src/zcsequence.py, src/Packet.py
                            (NDSS'23, "Drone Security and the Mysterious Case
                            of DJI's DroneID")

The one thing that matters for detection: symbols 4 and 6 (1-based) carry
Zadoff-Chu sequences with **roots 600 and 147**. DroneSecurity hard-codes a
check that raises "ZC Sequence not found. Expected: 600 and 147" - so those two
numbers are not a guess, they are an assertion in a working decoder.

A ZC sequence is the receiver's synchronisation preamble. It must be
demodulable before anything else can be, so it cannot be hidden by encrypting
the payload - which is what makes an O4 aircraft detectable even when it is not
readable.
"""

import numpy as np

NATURAL_RATE = 15_360_000  # Hz. DroneSecurity's default Fs, and LTE's rate
SUBCARRIER_HZ = 15_000  # DroneID's carrier spacing is LTE's
FFT_SIZE = NATURAL_RATE // SUBCARRIER_HZ  # 1024
DATA_CARRIERS = 600  # DC carrier is left empty, hence 600 and not 601
ZC_LENGTH = 601

# Cyclic prefixes, from get_cyclic_prefix_lengths.m:
#   long  = sample_rate / 192000       -> 80 at 15.36 Msps
#   short = 4.6875e-6 * sample_rate    -> 72
# The long prefix is on the FIRST and LAST symbol, not only the first.
CP_LONG = NATURAL_RATE // 192_000
CP_SHORT = round(4.6875e-6 * NATURAL_RATE)
CP_PATTERN = [CP_LONG] + [CP_SHORT] * 7 + [CP_LONG]  # 9 symbols
CP_PATTERN_LEGACY = [CP_LONG] + [CP_SHORT] * 6 + [CP_LONG]  # 8, Mavic 2 era

# Which symbols carry the ZC, 0-based, and with which root.
ZC_ROOTS = {3: 600, 5: 147}
ZC_ROOTS_LEGACY = {2: 600, 4: 147}

BURST_PERIOD_S = 0.600

# Centre frequencies proto17 observed DroneID bursts on. The aircraft hops, so
# a capture narrower than the whole hop set sees a fraction of the bursts.
KNOWN_CENTRES_HZ = [
    2_399_500_000, 2_414_500_000, 2_429_500_000, 2_444_500_000, 2_459_500_000,
    5_756_500_000, 5_776_500_000, 5_796_500_000,
]

# What DroneSecurity's own front end accepts, in seconds and Hz. Tighter than a
# guess would be, and derived from the symbol structure above.
BURST_LENGTH_S = (sum(CP_PATTERN) + FFT_SIZE * 9) / NATURAL_RATE  # 643.2 us
BURST_LENGTH_LEGACY_S = (sum(CP_PATTERN_LEGACY) + FFT_SIZE * 8) / NATURAL_RATE
OCCUPIED_HZ = (DATA_CARRIERS + 1) * SUBCARRIER_HZ  # 9.015 MHz
BANDWIDTH_GATE_HZ = (8e6, 11e6)  # packetizer.py's own test


def data_carrier_indices(fft_size=FFT_SIZE):
    """The FFT bins holding data, in fftshift (centre-zero) ordering.

    600 carriers straddling DC with DC itself skipped, which is why the ZC's
    own middle sample is thrown away rather than placed.
    """
    dc = fft_size // 2
    half = DATA_CARRIERS // 2
    return np.concatenate([
        np.arange(dc - half, dc),
        np.arange(dc + 1, dc + 1 + half),
    ])


def zadoff_chu(root, length=ZC_LENGTH):
    """exp(-j*pi*u*n*(n+1)/N) - identical in both reference implementations."""
    n = np.arange(length)
    return np.exp(-1j * np.pi * root * n * (n + 1) / length)


def zc_symbol(root, fft_size=FFT_SIZE):
    """One ZC OFDM symbol in the time domain, no cyclic prefix.

    The sequence is NOT transmitted as-is: it is mapped onto the data carriers
    and inverse-transformed. Correlating a raw ZC against the captured signal
    therefore finds nothing, which is the mistake this function exists to stop.
    """
    sequence = np.delete(zadoff_chu(root), ZC_LENGTH // 2)  # drop the DC one
    spectrum = np.zeros(fft_size, dtype=complex)
    spectrum[data_carrier_indices(fft_size)] = sequence
    return np.fft.ifft(np.fft.ifftshift(spectrum))


# The HackRF One has NO TCXO - the official docs call it "the internal crystal"
# and Great Scott Gadgets publishes no ppm figure. Community consensus is about
# +-20 ppm, which is +-49 kHz at 2.44 GHz and +-115 kHz at 5.76 GHz, i.e. up to
# 7.7 subcarriers. Nothing here may assume the capture is on frequency.
CFO_BINS = 8  # +-8 subcarriers = +-120 kHz, covers 20 ppm at 5.8 GHz


def zc_symbol_shifted(root, bins, rate=NATURAL_RATE):
    """A ZC reference moved `bins` subcarriers up in frequency.

    Needed because a Zadoff-Chu sequence has a time-frequency duality: shifting
    it in frequency shifts its correlation peak in TIME, by an amount that
    depends on the root. Measured on a synthetic burst, a 30 kHz offset leaves
    root 600's peak within 3 samples of truth while root 147's slides 501
    samples - so the fixed spacing between the two symbols appears to change,
    and a pairing test at the true spacing collapses from 0.995 to 0.006.

    Correlating against a bank of shifted references restores it, and the bin
    that wins is a measurement of the offset.
    """
    reference = zc_symbol(root)
    if bins == 0:
        return reference
    turn = 2j * np.pi * bins * SUBCARRIER_HZ / rate
    return reference * np.exp(turn * np.arange(len(reference)))


def burst_samples(rate=NATURAL_RATE, legacy=False):
    """Length of one burst in samples at `rate`."""
    pattern = CP_PATTERN_LEGACY if legacy else CP_PATTERN
    natural = sum(pattern) + FFT_SIZE * len(pattern)
    return int(round(natural * rate / NATURAL_RATE))


def synthesize_burst(seed=0, legacy=False):
    """A DroneID burst at the natural rate, for testing the detector.

    Real ZC symbols in their real positions; QPSK noise everywhere else, since
    the payload is encrypted on the aircraft we care about and the detector
    must not depend on it.
    """
    generator = np.random.default_rng(seed)
    pattern = CP_PATTERN_LEGACY if legacy else CP_PATTERN
    roots = ZC_ROOTS_LEGACY if legacy else ZC_ROOTS
    carriers = data_carrier_indices()

    pieces = []
    for index, cp in enumerate(pattern):
        if index in roots:
            symbol = zc_symbol(roots[index])
        else:
            bits = generator.integers(0, 4, DATA_CARRIERS)
            spectrum = np.zeros(FFT_SIZE, dtype=complex)
            spectrum[carriers] = np.exp(1j * (np.pi / 4 + bits * np.pi / 2))
            symbol = np.fft.ifft(np.fft.ifftshift(spectrum))
        pieces.append(np.concatenate([symbol[-cp:], symbol]))
    return np.concatenate(pieces)


def resample(samples, rate_in, rate_out):
    """Fourier resampling. Exact for a block, and needs no scipy.

    Capturing at NATURAL_RATE avoids this entirely, which is why that is what
    the README recommends.
    """
    if rate_in == rate_out:
        return samples
    n_in = len(samples)
    n_out = int(round(n_in * rate_out / rate_in))
    spectrum = np.fft.fftshift(np.fft.fft(samples))
    if n_out < n_in:
        start = (n_in - n_out) // 2
        spectrum = spectrum[start:start + n_out]
    else:
        pad = n_out - n_in
        spectrum = np.pad(spectrum, (pad // 2, pad - pad // 2))
    return np.fft.ifft(np.fft.ifftshift(spectrum)) * (n_out / n_in)
