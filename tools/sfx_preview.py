#!/usr/bin/env python3
"""Writes each synthesized sound effect (tools/assetgen/sfx.py) as a WAV in
build/sfx/, to listen to while tuning them: `make sfx-preview`. Stdlib only."""
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from assetgen.sfx import EFFECTS, RATE, pcm16  # noqa: E402
import assetgen.sfx as sfx  # noqa: E402

out = ROOT / "build" / "sfx"
out.mkdir(parents=True, exist_ok=True)
for index, (name, fn) in enumerate(EFFECTS):
    pcm = pcm16(getattr(sfx, fn)())
    path = out / f"{index}_{name.lower()}.wav"
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(v.to_bytes(2, "little", signed=True)
                               for v in pcm))
    print(f"wrote {path.relative_to(ROOT)} ({len(pcm) / RATE:.2f} s)")

# Ambience (ADR-031): each venue's loop mixed the way the module plays it
# (the murmur under the calls at their rows, pitches, and volumes), twice
# through its patterns, as build/sfx/ambience_<venue>.wav.
import json  # noqa: E402
from assetgen import ambience  # noqa: E402

MIX_RATE = 16000
ROW_SECONDS = 1 / (125 * 2 / 5 / 6)     # speed 6, tempo 125


def resample(pcm, rate, note):
    """The sample played at an IT note (C5 = its own rate), at MIX_RATE."""
    step = rate * 2 ** ((note - ambience.C5) / 12) / MIX_RATE
    out = []
    pos = 0.0
    while pos < len(pcm) - 1:
        out.append(pcm[int(pos)] / 32767)
        pos += step
    return out


venues = json.loads((ROOT / "assets" / "source" / "venues.json").read_text())
for i, venue in enumerate(venues["venues"]):
    kind = venue["ambience"]
    calls = ambience.KINDS[kind]
    murmur = resample(pcm16(ambience.murmur()), ambience.LOW_RATE, ambience.C5)
    voices = [resample(pcm16(synth()), rate, ambience.C5)
              for synth, rate, *_ in calls]
    rng = __import__("random").Random(100 + i)
    rows = []
    for p in range(ambience.PATTERNS):
        for row in range(ambience.ROWS):
            events = []
            for k, (_, rate, notes, chance, (low, high)) in enumerate(calls):
                if rng.random() < chance:
                    events.append((k, rng.choice(notes), rng.randint(low, high)))
            rows.append(events)
    length = int(len(rows) * 2 * ROW_SECONDS * MIX_RATE)
    mix = [murmur[n % len(murmur)] * ambience.MURMUR_VOLUME / 64
           for n in range(length)]
    for lap in range(2):
        for r, events in enumerate(rows):
            start = int((lap * len(rows) + r) * ROW_SECONDS * MIX_RATE)
            for k, note, volume in events:
                synth, rate, *_ = calls[k]
                sound = resample(pcm16(synth()), rate, note)
                for j, v in enumerate(sound):
                    if start + j < length:
                        mix[start + j] += v * volume / 64
    peak = max(abs(v) for v in mix) or 1
    gain = min(1.0, 0.9 / peak) * 150 / 255     # the module volume
    path = out / f"ambience_{venue['name'].lower()}.wav"
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(MIX_RATE)
        w.writeframes(b"".join(int(max(-32767, min(32767, v * gain * 32767)))
                               .to_bytes(2, "little", signed=True)
                               for v in mix))
    print(f"wrote {path.relative_to(ROOT)} ({length / MIX_RATE:.1f} s)")
