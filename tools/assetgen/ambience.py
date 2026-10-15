"""Venue ambience (ADR-031): a quiet loop per venue as a SNESMOD module.

Each module is a soft crowd murmur (a looping sample) with nature calls
dropped at random rows: birds at the PARK, gulls and lapping water by the
LAKESIDE, crickets at SUNSET; jungle birds and a far-off rumble with the
DINOSAURs, blips and sweeps in SPACE, bubbles and a whale UNDERSEA
(ADR-046). SNESMOD loops the order list, so the
ambience plays until the next venue loads. Synthesized with the standard
library and fixed seeds, like the effects (sfx.py).
"""
import math
import random

from .sfx import it_module, pcm16

LOW_RATE = 8000                 # the murmur and the water: nothing high
HIGH_RATE = 16000               # calls with content up to ~5 kHz
ROWS = 64                       # speed 6, tempo 125: 8.3 rows/s, 7.7 s
PATTERNS = 3                    # three variations, played in a loop
C5 = 60                         # IT note that plays a sample at its rate
MURMUR_VOLUME = 26              # of 64: under everything else


def murmur():
    """1 s of crowd murmur, seamless when looped: band-limited noise with a
    slow swell, cross-faded so the end runs into the start."""
    rng = random.Random(11)
    length = LOW_RATE
    fade = LOW_RATE // 8
    raw = []
    low = band = 0.0
    for n in range(length + fade):
        white = rng.random() * 2 - 1
        low += 0.08 * (white - low)              # keep voices, drop hiss
        band += 0.3 * (low - band)
        swell = 0.8 + 0.2 * math.sin(2 * math.pi * 0.9 * n / LOW_RATE)
        raw.append((low - 0.6 * band) * swell)
    out = raw[:length]
    for i in range(fade):                        # tail into the head
        w = i / fade
        out[i] = out[i] * w + raw[length + i] * (1 - w)
    return out


def chirp():
    """Two quick rising whistles."""
    out = []
    for n in range(int(0.2 * HIGH_RATE)):
        t = n / HIGH_RATE
        note = t if t < 0.08 else t - 0.11
        if note < 0 or note > 0.08:
            out.append(0.0)
            continue
        freq = 2600 + 1600 * note / 0.08
        env = math.sin(math.pi * note / 0.08)
        out.append(math.sin(2 * math.pi * freq * note) * env *
                   (0.7 + 0.3 * math.sin(2 * math.pi * 60 * t)))
    return out


def gull():
    """A gull's falling "kee-ow"."""
    rng = random.Random(12)
    out = []
    phase = 0.0
    length = 0.5
    for n in range(int(length * HIGH_RATE)):
        t = n / HIGH_RATE
        freq = 1300 - 500 * (t / length) ** 0.7
        phase += 2 * math.pi * freq / HIGH_RATE
        env = min(1.0, t / 0.03) * (1 - t / length) ** 1.5
        voice = math.sin(phase) + 0.5 * math.sin(2 * phase) + \
            0.25 * math.sin(3 * phase)
        out.append((voice + 0.15 * (rng.random() * 2 - 1)) * env)
    return out


def water():
    """A small wave lapping: filtered noise that swells and falls away."""
    rng = random.Random(13)
    out = []
    low = 0.0
    length = 0.7
    for n in range(int(length * LOW_RATE)):
        t = n / LOW_RATE
        low += 0.15 * ((rng.random() * 2 - 1) - low)
        env = math.sin(math.pi * min(1.0, t / length)) ** 2
        out.append(low * env)
    return out


def cricket():
    """A cricket's chirp: three quick pulses of a 4.5 kHz tone."""
    out = []
    for n in range(int(0.22 * HIGH_RATE)):
        t = n / HIGH_RATE
        pulse = (t % 0.05) < 0.025 and t < 0.15
        env = math.sin(math.pi * (t % 0.05) / 0.025) if pulse else 0.0
        out.append(math.sin(2 * math.pi * 4500 * t) * env)
    return out


def trill():
    """A jungle bird: a fast warbling trill that falls away."""
    out = []
    phase = 0.0
    length = 0.35
    for n in range(int(length * HIGH_RATE)):
        t = n / HIGH_RATE
        freq = 2100 + 450 * math.sin(2 * math.pi * 22 * t) - 600 * t
        phase += 2 * math.pi * freq / HIGH_RATE
        env = min(1.0, t / 0.02) * (1 - t / length) ** 1.2
        out.append(math.sin(phase) * env)
    return out


def rumble():
    """Something big, far away: a low growl that swells and fades."""
    rng = random.Random(14)
    out = []
    phase = 0.0
    low = 0.0
    length = 0.7
    for n in range(int(length * LOW_RATE)):
        t = n / LOW_RATE
        freq = 110 - 30 * t / length
        phase += 2 * math.pi * freq / LOW_RATE
        low += 0.2 * ((rng.random() * 2 - 1) - low)
        env = math.sin(math.pi * t / length) ** 1.5
        out.append((0.7 * math.sin(phase) * (0.6 + 0.4 * low) + 0.3 * low) *
                   env)
    return out


def blip():
    """A console's two-note blip."""
    out = []
    for n in range(int(0.14 * HIGH_RATE)):
        t = n / HIGH_RATE
        freq = 1320 if t < 0.06 else 1760
        tone = 1.0 if math.sin(2 * math.pi * freq * t) >= 0 else -1.0
        env = 1.0 if t % 0.07 < 0.05 else 0.0
        out.append(0.5 * tone * env)
    return out


def sweep():
    """A sci-fi sweep: a rising tone with a wobble."""
    out = []
    phase = 0.0
    length = 0.4
    for n in range(int(length * HIGH_RATE)):
        t = n / HIGH_RATE
        freq = 500 + 900 * t / length + 60 * math.sin(2 * math.pi * 9 * t)
        phase += 2 * math.pi * freq / HIGH_RATE
        env = math.sin(math.pi * t / length)
        out.append(math.sin(phase) * env)
    return out


def bubble():
    """One bubble: a quick rising blip."""
    out = []
    phase = 0.0
    length = 0.07
    for n in range(int(length * HIGH_RATE)):
        t = n / HIGH_RATE
        freq = 350 + 900 * (t / length) ** 2
        phase += 2 * math.pi * freq / HIGH_RATE
        env = (1 - t / length) ** 0.5
        out.append(math.sin(phase) * env)
    return out


def whale():
    """A whale far off: a slow rising and falling moan."""
    out = []
    phase = 0.0
    length = 1.0
    for n in range(int(length * LOW_RATE)):
        t = n / LOW_RATE
        freq = 190 + 80 * math.sin(math.pi * t / length)
        phase += 2 * math.pi * freq / LOW_RATE
        env = math.sin(math.pi * t / length) ** 2
        out.append((math.sin(phase) + 0.3 * math.sin(2 * phase)) * env)
    return out


# Kind: the calls, each (synth, rate, notes to vary the pitch, chance per
# row, volume range of 64).
KINDS = {
    "birds": [(chirp, HIGH_RATE, (C5 - 2, C5, C5 + 3, C5 + 5), 0.10,
               (14, 34))],
    "gulls": [(gull, HIGH_RATE, (C5 - 1, C5, C5 + 2), 0.035, (16, 30)),
              (water, LOW_RATE, (C5 - 2, C5), 0.06, (14, 26))],
    "crickets": [(cricket, HIGH_RATE, (C5, C5 + 1), 0.16, (10, 22))],
    "jungle": [(trill, HIGH_RATE, (C5 - 3, C5, C5 + 2), 0.07, (12, 28)),
               (rumble, LOW_RATE, (C5 - 2, C5), 0.015, (18, 30))],
    "space": [(blip, HIGH_RATE, (C5 - 5, C5, C5 + 4), 0.05, (8, 18)),
              (sweep, HIGH_RATE, (C5 - 2, C5), 0.025, (10, 20))],
    "bubbles": [(bubble, HIGH_RATE, (C5 - 4, C5, C5 + 3, C5 + 7), 0.14,
                 (10, 24)),
                (whale, LOW_RATE, (C5 - 1, C5), 0.012, (16, 28))],
}


def ambience_module(kind, seed):
    """The IT module for a kind of ambience."""
    if kind not in KINDS:
        raise ValueError(f"ambience kind {kind!r}")
    samples = [("MURMUR", pcm16(murmur()), LOW_RATE, True)]
    for synth, rate, *_ in KINDS[kind]:
        samples.append((synth.__name__.upper(), pcm16(synth()), rate, False))
    rng = random.Random(seed)
    patterns = []
    for p in range(PATTERNS):
        events = {}
        if p == 0:                     # the murmur starts with the loop
            events[0] = [(0, C5, 1, MURMUR_VOLUME)]
        for row in range(ROWS):
            for k, (_, _, notes, chance, (low, high)) in \
                    enumerate(KINDS[kind]):
                if rng.random() < chance:
                    events.setdefault(row, []).append(
                        (1 + k, rng.choice(notes), 2 + k,
                         rng.randint(low, high)))
        patterns.append((ROWS, events))
    return it_module(f"AMBIENCE {kind.upper()}", samples, patterns,
                     list(range(PATTERNS)))
