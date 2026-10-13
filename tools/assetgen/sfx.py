"""Sound effects (ADR-029): synthesized samples in an Impulse Tracker file.

PVSnesLib's smconv turns the IT file into a SNESMOD soundbank; each sample
is one effect (spcLoadEffect / spcEffect). Everything is synthesized here
with the standard library, deterministically (fixed seeds), so the build
needs no recorded audio and no third-party sounds.
"""
import math
import random
import struct

RATE = 16000                    # Hz; played back with SNESMOD pitch 4
# (name, synth function name): the order is the effect index (sfx.h).
EFFECTS = (
    ("HIT", "hit"),             # paddle meets ball: a hollow "pock"
    ("BOUNCE", "bounce"),       # ball on the court: a lower "tok"
    ("NET", "net"),             # ball into the net: thud and rattle
    ("WHISTLE", "whistle"),     # a fault: the referee's pea whistle
    ("APPLAUSE", "applause"),   # a point: the crowd claps
    ("BLIP", "blip"),           # menu cursor
    ("CONFIRM", "confirm"),     # menu choice
    ("BACK", "back"),           # menu back or cancel
)
PEAK = 0.85


def samples(seconds):
    return range(int(seconds * RATE))


def decay(t, tau):
    return math.exp(-t / tau)


def hit():
    rng = random.Random(1)
    out = []
    for n in samples(0.09):
        t = n / RATE
        body = (0.6 * math.sin(2 * math.pi * 1050 * t) +
                0.3 * math.sin(2 * math.pi * 1700 * t + 0.3) +
                0.2 * math.sin(2 * math.pi * 2600 * t + 1.1))
        click = (rng.random() * 2 - 1) * decay(t, 0.0015)
        out.append(body * decay(t, 0.018) + 0.5 * click)
    return out


def bounce():
    rng = random.Random(2)
    out = []
    for n in samples(0.07):
        t = n / RATE
        body = math.sin(2 * math.pi * 380 * t) + \
            0.4 * math.sin(2 * math.pi * 760 * t)
        grit = (rng.random() * 2 - 1) * decay(t, 0.002)
        out.append(body * decay(t, 0.02) + 0.35 * grit)
    return out


def net():
    rng = random.Random(3)
    out = []
    for n in samples(0.22):
        t = n / RATE
        thump = math.sin(2 * math.pi * 140 * t) * decay(t, 0.05)
        rattle = (rng.random() * 2 - 1) * \
            abs(math.sin(2 * math.pi * 35 * t)) * decay(t, 0.08)
        out.append(thump + 0.4 * rattle)
    return out


def whistle():
    rng = random.Random(4)
    out = []
    phase = 0.0
    length = 0.42
    for n in samples(length):
        t = n / RATE
        # The pea rattling in the whistle: a fast wobble in pitch.
        freq = 2900 + 80 * math.sin(2 * math.pi * 28 * t)
        phase += 2 * math.pi * freq / RATE
        env = min(1.0, t / 0.015) * min(1.0, (length - t) / 0.05)
        breath = (rng.random() * 2 - 1) * 0.1
        out.append((math.sin(phase) + breath) * env)
    return out


def applause():
    rng = random.Random(5)
    length = 1.4
    out = [0.0] * len(samples(length))
    # Claps arrive at random, quickly at first, then thinning out.
    t = 0.0
    while t < length:
        rate = 140 if t < 0.8 else 140 * max(0.05, (length - t) / 0.6)
        t += rng.expovariate(rate)
        start = int(t * RATE)
        gain = 0.25 + 0.2 * rng.random()
        width = int(0.012 * RATE)
        for k in range(width):
            if start + k >= len(out):
                break
            out[start + k] += (rng.random() * 2 - 1) * gain * \
                decay(k / RATE, 0.003)
    # A soft murmur under the claps, and a fade at the end.
    previous = 0.0
    for n in range(len(out)):
        t = n / RATE
        previous = 0.97 * previous + 0.03 * (rng.random() * 2 - 1)
        fade = min(1.0, (length - t) / 0.5)
        out[n] = (out[n] + 0.6 * previous) * fade
    return out


def tone(freqs, seconds, tau, harmonics=(1.0, 0.0, 0.25)):
    out = []
    span = len(samples(seconds))
    for n in range(span):
        t = n / RATE
        step = min(len(freqs) - 1, n * len(freqs) // span)
        freq = freqs[step]
        local = t - step * seconds / len(freqs)
        value = sum(a * math.sin(2 * math.pi * freq * (i + 1) * t)
                    for i, a in enumerate(harmonics) if a)
        out.append(value * decay(local, tau) * min(1.0, t / 0.002))
    return out


def blip():
    return tone([1320], 0.04, 0.015)


def confirm():
    return tone([880, 1320], 0.14, 0.05)


def back():
    return tone([660, 440], 0.1, 0.04)


def pcm16(values):
    """Normalized to PEAK, as signed 16-bit."""
    top = max(abs(v) for v in values) or 1.0
    return [max(-32767, min(32767, round(v / top * PEAK * 32767)))
            for v in values]


def pack_pattern(rows, events):
    """IT pattern data. events: {row: [(channel, note, instrument,
    volume)]}, channel 0-based, instrument 1-based, volume 0-64."""
    data = bytearray()
    for row in range(rows):
        for channel, note, instrument, volume in events.get(row, ()):
            data.append((channel + 1) | 0x80)
            data.append(0x07)           # note, instrument, volume follow
            data += bytes([note, instrument, volume])
        data.append(0)                  # end of row
    return struct.pack("<HH4x", len(data), rows) + bytes(data)


def it_module(title, samples, patterns, orders, speed=6, tempo=125):
    """A minimal Impulse Tracker module: one 16-bit sample and one
    instrument per sample, and the given patterns and order list.
    samples: (name, pcm, rate, loop) tuples; loop plays the whole sample
    again and again."""
    count = len(samples)
    order_bytes = bytes(list(orders) + [255])
    header = bytearray(0xC0)
    header[0:4] = b"IMPM"
    name = title.encode()[:25]
    header[4:4 + len(name)] = name
    struct.pack_into("<HHHH", header, 0x20, len(order_bytes), count, count,
                     len(patterns))
    # Cwt/v, Cmwt, flags (stereo, instruments, linear slides), special.
    struct.pack_into("<HHHH", header, 0x28, 0x0214, 0x0214, 0x0D, 0)
    header[0x30:0x36] = bytes([128, 48, speed, tempo, 128, 0])
    for ch in range(64):
        header[0x40 + ch] = 32          # centre
        header[0x80 + ch] = 64
    offsets_size = 4 * (count + count + len(patterns))
    cursor = 0xC0 + len(order_bytes) + offsets_size

    instruments = []
    for i in range(count):
        ins = bytearray(554)
        ins[0:4] = b"IMPI"
        ins[0x18] = 128                 # global volume
        ins[0x19] = 32 | 128            # default pan off
        label = samples[i][0].encode()[:25]
        ins[0x20:0x20 + len(label)] = label
        for note in range(120):         # every note plays this sample
            ins[0x40 + note * 2] = note
            ins[0x40 + note * 2 + 1] = i + 1
        instruments.append(bytes(ins))
    ins_offsets = []
    for ins in instruments:
        ins_offsets.append(cursor)
        cursor += len(ins)

    sample_headers_at = cursor
    cursor += 0x50 * count
    packed = [pack_pattern(rows, events) for rows, events in patterns]
    pattern_offsets = []
    for pat in packed:
        pattern_offsets.append(cursor)
        cursor += len(pat)

    sample_headers = []
    data = []
    for name, pcm, rate, loop in samples:
        smp = bytearray(0x50)
        smp[0:4] = b"IMPS"
        smp[0x11] = 64                  # global volume
        smp[0x12] = 0x03 | (0x10 if loop else 0)    # sample, 16-bit, loop
        smp[0x13] = 64                  # volume
        label = name.encode()[:25]
        smp[0x14:0x14 + len(label)] = label
        smp[0x2E] = 0x01                # signed
        smp[0x2F] = 32
        struct.pack_into("<IIII", smp, 0x30, len(pcm), 0,
                         len(pcm) if loop else 0, rate)
        struct.pack_into("<I", smp, 0x48, cursor)
        raw = struct.pack(f"<{len(pcm)}h", *pcm)
        data.append(raw)
        cursor += len(raw)
        sample_headers.append(bytes(smp))

    smp_offsets = [sample_headers_at + 0x50 * i for i in range(count)]
    out = bytearray(header)
    out += order_bytes
    out += struct.pack(f"<{count}I", *ins_offsets)
    out += struct.pack(f"<{count}I", *smp_offsets)
    out += struct.pack(f"<{len(patterns)}I", *pattern_offsets)
    for ins in instruments:
        out += ins
    for smp in sample_headers:
        out += smp
    for pat in packed:
        out += pat
    for raw in data:
        out += raw
    return bytes(out)


def it_file(effects):
    """The effects module: one sample per effect and one empty pattern
    (smconv needs a song)."""
    return it_module("PICKLEBALL SFX",
                     [(name, pcm, RATE, False) for name, pcm in effects],
                     [(64, {})], [0])


def sfx_assets():
    """sfx.it for smconv, and sfx.h with the effect indices."""
    effects = [(name, pcm16(globals()[fn]())) for name, fn in EFFECTS]
    lines = ["/* Generated by tools/generate_assets.py (tools/assetgen/sfx.py); "
             "do not edit. */",
             "#ifndef SFX_H", "#define SFX_H", "",
             "/* Effect indices in the soundbank (ADR-029), 16 kHz samples: "
             "play at pitch 4. */"]
    for i, (name, _) in enumerate(EFFECTS):
        lines.append(f"#define SFX_{name} {i}")
    lines += [f"#define SFX_COUNT {len(EFFECTS)}", "#define SFX_PITCH 4", "",
              "#endif", ""]
    seconds = sum(len(pcm) for _, pcm in effects) / RATE
    return it_file(effects), "\n".join(lines), seconds
