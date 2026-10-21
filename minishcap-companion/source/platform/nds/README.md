# The Minish Cap NDS (scaffold)

Early scaffold for a Nintendo DS target. It boots, finds a `.gba` on the SD/flashcart,
verifies the Minish Cap game code and exercises a demand-paged ROM reader. **The game
engine is linked (not yet run); video, audio and the sub-screen are stubs.** Design and open problems: [docs/NDS-PORT.md](../../docs/NDS-PORT.md).

## Build
The Makefile is self-contained (it does not use `ds_rules`). It needs devkitARM,
libnds 1.8.x + libfat (`NDS_PREFIX` with `include/` and `lib/`), `ndstool`, and an ARM7
core (`arm7/arm7.c` is a minimal one: build it with `-specs=ds_arm7.specs -lnds7`).
```sh
make -C platform/nds NDS_PREFIX=<prefix> NDSTOOL=<ndstool> ARM7ELF=<default.elf>
# -> platform/nds/tmc-nds.nds
```
Built successfully (178 KB .nds) against libnds v1.8.3 and libfat from source.
Not yet run on an emulator or hardware.

## Run
Preferred: put the ROM in Slot-2 (original cart or a GBA flashcart, DS/DS Lite only) - the
scaffold reports `Slot-2 ROM BZME`. Fallback (SD probe only):

Copy `tmc-nds.nds` to the flashcart and your own ROM to
`sd:/nds/The Minish Cap DS/` (any `*.gba` name; USA `BZME` or EU `BZMP`).
