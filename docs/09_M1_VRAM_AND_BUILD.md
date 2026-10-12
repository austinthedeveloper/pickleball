# M1 VRAM and Build Record

PVSnesLib APIs express the addresses below in VRAM words; byte ranges are shown
to make overlap checks explicit. Static assets are uploaded once during forced
blank initialization.

| Region | Word address | Reserved byte range | Actual bytes |
|---|---:|---:|---:|
| BG1 court map | `0x0000` | `0x0000–0x07ff` | 2,048 |
| BG3 HUD map | `0x0400` | `0x0800–0x0fff` | 2,048 |
| BG1 court tiles | `0x1000` | `0x2000–0x5fff` | 352 |
| BG3 HUD font | `0x3000` | `0x6000–0x6fff` | 1,536 |
| OBJ player/shadow | `0x4000` | `0x8000–0xbfff` | 2,048 |

Palette data uses 32 bytes in BG palette 1 for the court, 8 bytes in BG palette
0 for the HUD, and 32 bytes in OBJ palette 0. The regions do not overlap and
leave `0x1000–0x1fff`, `0x7000–0x7fff`, and `0xc000–0xffff` free, plus unused
space inside each reserved budget.

`assets/generated/sizes.json` records the exact byte count and SHA-256 of every
generated asset on each build. The checked-in budget and address manifest is
`assets/manifest/m1-assets.json`.

The verified release ROM is 262,144 bytes (LoROM, slow ROM, NTSC, no SRAM). Its
internal checksum and complement sum to `0xffff`; `make dist` records the full
ROM SHA-256 in the distribution report.
