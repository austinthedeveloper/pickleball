# Technical Sources

Checked: 2026-10-11

These are implementation references, not dependencies to copy blindly. Pin the actual toolchain used by the repository.

## Toolchain and APIs

- [PVSnesLib repository](https://github.com/alekmaul/pvsneslib) — compiler/library scope, examples, installation, real-SNES target.
- [PVSnesLib generated documentation](https://alekmaul.github.io/pvsneslib/) — current API index.
- [PVSnesLib input API](https://alekmaul.github.io/pvsneslib/input_8h.html) — pad indices, automatic reads, `detectMPlay5`, and port-I/O caution.
- [PVSnesLib Multiplay5 example](https://github.com/alekmaul/pvsneslib/tree/master/snes-examples/input/multiplay5) — starting point for accessory detection and pad mapping.
- [PVSnesLib sprite guide](https://github.com/alekmaul/pvsneslib/wiki/Sprites) — OAM calls, transparency, palette selection, sprite visibility/size.

## SNES graphics and hardware behavior

- [SNESdev: Sprites](https://snes.nesdev.org/wiki/Sprites) — 128 OAM entries, 32 objects and 34 object slivers per scanline, offscreen-X erratum.
- [SNESdev: PPU for NES developers](https://snes.nesdev.org/wiki/SNES_PPU_for_NES_developers) — tilemaps, sprite size pairs, background overview.
- [SNESdev: MMIO registers](https://snes.nesdev.org/wiki/MMIO_registers) — automatic joypad reads and Multitap-related I/O behavior.
- [Fullsnes](https://problemkaputt.de/fullsnes.htm) — low-level CPU, memory, PPU, DMA, controller, and cartridge reference.

## Project policy derived from the sources

- Plan to 32 objects and 34 eight-pixel object slivers per scanline, not merely 128 total objects.
- Use one of the PPU’s global small/large object-size pairs; this plan chooses 8×8 and 32×32.
- Object graphics are 4bpp and use one of eight object palettes; palette color 0 is transparent.
- Avoid hiding objects at the SNES offscreen-X value implicated in the range/time-over erratum; use the library hide path.
- Treat Multitap support as a port-I/O concern owned by the input library/wrapper.

If a linked page moves, update this file and preserve the relevant fact in an ADR or code comment with the replacement reference.
