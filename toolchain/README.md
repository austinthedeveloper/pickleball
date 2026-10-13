# Toolchain

This project is pinned to **PVSnesLib 4.6.0**. The version is verified from
`pvsneslib/pvsneslib_version.txt` in the installed distribution.

Set `PVSNESLIB_HOME` to the directory that contains both `devkitsnes/` and
`pvsneslib/`. The default Makefile value supports the sibling layout used by
this repository:

```sh
export PVSNESLIB_HOME="$(cd ../pvsneslib && pwd)"
make check debug
```

Supported development host: macOS with POSIX `make`, Python 3, and the host C
compiler installed. PVSnesLib supplies `816-tcc`, WLA-DX, and the SNES runtime.

Build outputs are under `build/<configuration>/`. `make dist` produces the
release ROM, symbol/map files, SHA-256 checksum, and build report under `dist/`.

Useful targets: `debug`, `release`, `hardware-test`, `assets`, `check`, `clean`,
and `dist`. Open the resulting `.sfc` in a current SNES emulator. Mesen 2 is the
preferred fast/debug emulator; bsnes accuracy mode is the accuracy check.

