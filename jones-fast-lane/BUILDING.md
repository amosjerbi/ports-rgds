# Building Jones in the Fast Lane for Nintendo DS

This project is a new SCI engine for the Nintendo DS. It contains **none of the
game's files**. You supply them from your own copy of *Jones in the Fast Lane*
(Sierra On-Line, 1991), and the build packs them into a `.nds` ROM for you.

> The ROM you build contains the game's copyrighted data. Keep it for your own
> use and don't share it. Share this source code instead, and let others build
> from their own copy.

There are two ROMs:

| ROM | Command | Game files needed |
|---|---|---|
| `build/jones.nds` (floppy, ~1.4 MB) | `make` | DOS floppy release |
| `build/jones-cdrom.nds` (voices, ~25 MB) | `make cd` | CD-ROM release **and** DOS floppy release |

The CD release has spoken dialogue but no on-screen text. The speech balloons
in `jones-cdrom.nds` are rebuilt from the floppy release's text, so the CD build
needs both copies.

---

## 1. What you need

- **macOS or Linux** with `make`, a host C compiler (`cc`), and `python3`
  (standard library only).
- **devkitARM** (the ARM compiler) from [devkitPro](https://devkitpro.org/wiki/Getting_Started).
  The build looks in `$DEVKITARM`, then in `/opt/devkitpro/devkitARM`. Tested
  with gcc 16.1.
- **ndstool**, which packs the `.nds` file. It comes with devkitPro's `nds-dev`
  package (`$DEVKITPRO/tools/bin/ndstool`). Without it, build it from
  [source](https://github.com/devkitPro/ndstool) (`./autogen.sh && ./configure && make`)
  and pass `NDSTOOL=/path/to/ndstool` to `make`.
- **The DS libraries come with this project** in `toolchain/dkp`, prebuilt for
  ARM so they work on any host OS:
  - libnds 2.0 (`84e6082`)
  - calico (`81b75e3`) with `toolchain/patches/calico-emulators.patch`, plus
    its `ds7_ragdoll` ARM7 binary. The patch keeps the ARM9 exception vectors
    high and routes IRQs through the BIOS, so emulators with an HLE BIOS
    (DraStic) don't crash on boot. It keeps the IRQ mask on the stack, not in
    CP15 c13 (DSperate doesn't emulate that register, so no thread ever woke
    from a vblank wait: no sound, and the game froze). It also lets NitroFS
    fall back to card reads when the loader leaves no ROM path.
  - libfat (`154f782`), with `toolchain/patches/libfat-calico.patch`, which
    connects it to calico's SD card driver

  Their licences are next to them. `toolchain/build_calico.sh` rebuilds calico
  from source (apply the patch to a calico checkout first). You can also use your devkitPro's own libraries instead; see
  [Using a stock devkitPro](#using-a-stock-devkitpro).
- **Your copy of the game** (see below).

## 2. Get the game files

Only a few files from each release are used. Copy them out of your install, or
off your disks or CD.

### Floppy (DOS) release: needed for both ROMs

| File | Size (bytes) | MD5 |
|---|---|---|
| `resource.map` | 1,800 | `65cbe19b36fffc71c8e7b2686bd49ad7` |
| `resource.001` | 313,476 | `56166eb58bc48f072cfd77e4aa1418d7` |
| `resource.002` | 719,747 | `e6ce044e9924c646ee3b66502c49fe5a` |

These came from interpreter version `1.000.060` (the `version` file in the game
folder). If you only have the original floppies, install the game first. The
installer combines the disks into these files.

### CD-ROM (Windows 3.x) release: needed for `make cd` only

| File | Size (bytes) | MD5 |
|---|---|---|
| `resource.map` | 4,878 | `459f5b04467bc2107aec02f5c4b71b37` |
| `resource.001` | 1,652,150 | `4da18a6dbc7bf313438af74c93d64396` |
| `audio001.map` | 5,340 | `b2b0591b47899bd80883bd8cf7bf7000` |
| `audio001.002` | 23,126,016 | `0e7d1d15dcbcabd9ccfaf9eca1792df4` |

All four are in the root of the CD.

To check your files: `md5 resource.*` (macOS) or `md5sum resource.*` (Linux). A
different release might still work, but it hasn't been tested. The CD subtitle
table in particular is tuned to these exact files.

## 3. Put the files where the build can find them

**Option A: the default folders.** Create these inside the project folder:

```
jones-fast-lane/
├── Jones-in-the-Fast-Lane_DOS_EN/   ← floppy: resource.map, resource.001, resource.002
└── Jones3x/CD/                      ← CD: resource.map, resource.001, audio001.map, audio001.002
```

**Option B: anywhere else.** Leave the files where they are and tell `make`:

```sh
make    GAMEDIR=~/games/jones-dos
make cd GAMEDIR=~/games/jones-dos CDDIR=~/games/jones-cd
```

Both default folders are listed in `.gitignore`, so the game files can't be
committed by mistake.

## 4. Build

```sh
make            # → build/jones.nds
make cd         # → build/jones-cdrom.nds
```

If a game file is missing, the build stops and names the file, for example:

```
error: missing .../Jones-in-the-Fast-Lane_DOS_EN/resource.map
  Copy resource.map, resource.001 and resource.002 from your floppy (DOS) copy ...
```

How the files are packed:

- **Floppy:** the three resource files are linked straight into the ARM9 binary
  (`src/nds/gamedata.s`).
- **CD:** only `resource.map` is linked in. `resource.001` and the speech files
  go into the ROM's NitroFS file system (`build/cdfs/`) and are read as needed.
  `build/subtitles.bin` is generated first: `tools/textdump.c` dumps the
  floppy's text, and `tools/mksubs.py` matches each line to its speech clip. It
  should report `474 subtitles`.

To start over: `make clean`.

## 5. Run it

- **Emulator:** open the `.nds` in melonDS (1.1 tested).
- **Hardware:** copy the `.nds` to your flashcart or handheld's SD card and
  launch it like any homebrew ROM.

Save games go to `/data/jones/jonesN.sav` on the SD card (L = save, R = restore).
In an emulator, enable DLDI/SD card emulation or saving won't work. The game
boots without an SD card; only saving is disabled.

### Anbernic RG DS (ROCKNIX)

Use **DSperate**. It's ROCKNIX's melonDS-based DS emulator, and it puts each DS
screen on its own physical panel:

1. Copy `jones-cdrom.nds` (or `jones.nds`) to `/storage/roms/nds`.
2. In EmulationStation, open the game's options and set the emulator to
   **dsperate**, then turn **auto save/load off**. With auto-resume on,
   DSperate restores the last snapshot instead of booting the game, and a
   snapshot taken with a different build of the ROM freezes it.

   Or set it over SSH, in `/storage/.config/system/configs/system.cfg`:

   ```
   nds["jones-cdrom.nds"].emulator=dsperate
   nds["jones-cdrom.nds"].core=dsperate-sa
   nds["jones-cdrom.nds"].autosave=0
   ```

Known limits on ROCKNIX:

- **Saving:** DSperate gives DS homebrew no SD card, so L/R saves don't work.
  Don't use DSperate's save states across ROM updates either.
- **RetroArch melonDS DS** draws both screens on one panel. With its JIT on,
  calico homebrew hangs, and with the JIT off it runs at about 18 fps.
- **DraStic** runs calico ROMs only with the patch above, and still can't read
  NitroFS, so the CD build doesn't start there.

---

## Using a stock devkitPro

The bundled libraries are the tested setup. To build against devkitPro's own
libnds and calico instead (`sudo dkp-pacman -S nds-dev`):

```sh
make DKP=/opt/devkitpro
```

These are the settings that most often need changing (all can be set on the
`make` command line):

| Variable | Default | Notes |
|---|---|---|
| `DKP` | `toolchain/dkp` | Where `libnds/` and `calico/` are |
| `DEVKITARM` | `$DEVKITARM`, then `/opt/devkitpro/devkitARM` | The compiler |
| `ARM7ELF` | `$(DKP)/calico/bin/ds7_ragdoll.elf` | The ARM7 binary. The port does its own audio mixing and doesn't need maxmod. If your install only has `ds7_maine.elf`, pass `ARM7ELF=$DEVKITPRO/calico/bin/ds7_maine.elf` |
| `NDSTOOL` | `toolchain/bin/ndstool`, then `$DEVKITPRO/tools/bin/ndstool`, then `ndstool` on your `PATH` | |
| `GAMEDIR` | `Jones-in-the-Fast-Lane_DOS_EN` | Floppy game files |
| `CDDIR` | `Jones3x/CD` | CD game files |

Troubleshooting:

- **`cannot find -lfat`:** your libnds install has no libfat. Copy
  `toolchain/dkp/libnds/lib/libfat.a` into `$(DKP)/libnds/lib`. To build it
  yourself, apply `toolchain/patches/libfat-calico.patch` to
  [devkitPro/libfat](https://github.com/devkitPro/libfat).
- **`multiple definition of nitrorom...`:** your calico already includes
  NitroFS support, so remove `src/nds/nitrorom_common.c` from `CDOBJS` in the
  Makefile. (The bundled calico lacks it, which is why the project carries its
  own copy.)

## Host tools (optional)

These run the same engine on your computer, which helps with debugging:

```sh
make scirun     # headless runner: build/mac/scirun <gamedir> -n <ticks> -f <framesdir> -e <events.txt>
make test       # decode every resource in GAMEDIR
make gfxtest    # render all pics/views to build/gfx/*.png (macOS: uses sips)
```
