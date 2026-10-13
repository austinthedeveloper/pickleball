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
