# vision-lab

Firmware for the Jarvis unit's dual-eye displays: two Waveshare 0.71" **GC9D01**
round panels (160x160) sharing one SPI bus with dual chip-select, on an
**ESP32-C6**.

## Layout

| Path | What it is |
|---|---|
| `platformio.ini` | pioarduino / Arduino framework, `esp32-c6-devkitc-1` |
| `include/board_pins.h` | **the only** place hardware pins are defined |
| `src/selftest.cpp` | dual-GC9D01 brightness self-test (white/black), `env:jarvis_eyes_c6` |
| `src/eyes.cpp` | animated eyes: iris texture, blinks, saccades, `env:jarvis_eyes_anim` |
| `.claude/hooks/bootstrap-pio.sh` | makes `pio run` work in a fresh cloud container |
| `include/board_pins.h` placeholder pins | raise a `#warning` until metered |
| `index.html` | unrelated page, pre-existing |

## Compile check

```bash
pio run
```

Both environments build:

| Env | Static RAM | Flash |
|---|---|---|
| `jarvis_eyes_c6` (self-test) | 4.8% (15,572 B) | 25.0% |
| `jarvis_eyes_anim` (animated) | 4.8% (15,604 B) | 25.6% |

The animated build also allocates **one** 160x160 RGB565 framebuffer on the heap
at runtime: 51,200 B. Two canvases (102 KB) would be a bad trade on a 320 KB
part for no benefit, so there is one buffer - draw the left eye, push it to the
left panel, redraw for the right eye, push it to the right panel. Both eyes
still update every frame. The allocation is checked and logged, because a silent
canvas failure is indistinguishable from a wiring fault.

A new cloud session runs `.claude/hooks/bootstrap-pio.sh` on start, so `pio run`
works without the manual steps at the bottom of this file.

Compiling costs nothing and needs no hardware, so it is the first check for
every change. Flashing is never automatic and never done without explicit
approval.

## Before flashing: three things to meter

`board_pins.h` emits a `#warning` while any pin in it is still unverified. These
were open at the end of the 2026-10-14 sessions:

1. **VCC must read ~3.3 V, not ~5 V.** The schematic puts `USB_5V` on header J4
   immediately beside GPIO4/GPIO5, and Waveshare's own UNO wiring table says
   `VCC -> 5V`, which is right for a 5 V Arduino and wrong for these 3.3 V
   panels. Meter this first; it decides whether the panels are still good parts.
2. **Nothing on CHIP_PU or BOOT0** (same J4 connector). A lead on CHIP_PU holds
   the chip in reset; one on BOOT0 forces download mode.
3. **RST1/RST2 and BL1/BL2 are individual flying leads** on the SH1.0-to-Dupont
   cable, not tied on the module. A floating RST means the controller never
   leaves reset; a floating BL means no light regardless of what is drawn.

## Why the self-test draws white and black, not colours

Colour tests through the tinted domes returned no signal in the earlier
sessions, and that non-signal was misread as faulty chip-select wiring. Full
white vs full black is the boldest difference a panel can produce. If the domes
do not visibly change, no eye code will show through them and the domes have to
come off or be swapped for clear ones.

## GC9D01 is not GC9A01

Different controllers. GC9A01 is 240x240 (1.28"); GC9D01 is 160x160 (0.71",
this part). A GC9A01 init table on a GC9D01 panel gives a wrong init sequence
and wrong framebuffer geometry, which presents as a lit panel showing a
detail-free glow. The earlier sketch (`gc9a01_dual_test`) used the GC9A01
driver, because **`Arduino_GC9D01` exists only on Arduino_GFX `master`, not in
the v1.5.0 release tag** - hence the commit-pinned `lib_deps`.

## Three Arduino_GFX traps

Each of these makes the board look dead. All three are avoided and labelled in
`src/main.cpp`:

1. Two SPI bus objects on one SPI port -> one bus object, shared.
2. Two display objects both owning the bus chip-select -> the bus is built with
   `GFX_NOT_DEFINED` for CS and each panel's CS is driven by hand, so every
   burst is wrapped: assert CS, draw, release.
3. Constructing displays at file scope, before `setup()` runs -> both are
   heap-allocated inside `setup()`.

Panels are also initialised one at a time with only that panel selected, or the
second `begin()` command stream lands on both controllers.

## Cloud-container build notes

A fresh cloud session needs four fixes before `pio run` works. The agent proxy
blocks `api.registry.nm1.platformio.org` (403 on CONNECT) while GitHub and PyPI
are allowed:

```bash
pip install platformio scons rich_click "click==8.1.8"
# tool-scons comes from the blocked registry: satisfy it from PyPI instead
D=~/.platformio/packages/tool-scons; mkdir -p "$D"
printf '{"name":"tool-scons","version":"4.41101.0","system":"all"}\n' > "$D/package.json"
printf '{"type":"tool","name":"tool-scons","version":"4.41101.0","spec":{"owner":"platformio","id":null,"name":"tool-scons","requirements":null}}' > "$D/.piopm"
printf 'import sys\nfrom SCons.Script import main\nif __name__ == "__main__":\n    main()\n' > "$D/scons.py"
pio run  # first run installs the RISC-V toolchain, then fails to find it
ln -sf ~/.platformio/packages/riscv32-esp-elf/bin/riscv32-esp-elf-* /usr/local/bin/
pio run
```

- `click` must be `<8.2`; esptool 5.0.0-dev1 calls the old
  `ParamType.get_metavar()` signature and the image step dies otherwise.
- The toolchain unpacks to `packages/riscv32-esp-elf/bin` but PlatformIO
  exports `packages/toolchain-riscv32-esp`, so the symlink is what makes
  `riscv32-esp-elf-g++` resolvable.
