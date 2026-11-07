<p align="center">
  <img src="screens/Title%20screen.png" alt="Zelda: Ocarina of Time 3D on the RG DS: top screen on the top panel, touch screen on the bottom panel" width="320">
</p>

# 3DS on the RG DS (rgds-core): Azahar inside RG DS Play

Nintendo 3DS games on the Anbernic RG DS running **rgds-core**. A self-built [Azahar](https://github.com/azahar-emu/azahar)
libretro core runs inside a patched copy of the OS's own player, `rgds-play`. The 3DS top screen goes on the top
panel and the touch screen on the bottom panel, with working touch.

No ROMs, keys, or saves are included. Bring your own dumped `.3ds`/`.cci` game.

## Status
* Tested with Zelda: Ocarina of Time 3D (USA): about 95% speed on the title screen, 80–95% in gameplay (1× resolution).
* Sound is time-stretched with SoundTouch when a game dips below full speed. Pitch stays the same and the sound
  buffer doesn't run dry, so there is no crackle.
* The Home button (or holding Select+Start) quits back to the launcher.
* `run3ds.sh` sets two player options:
  * `RGDS_PLAY_TOP_DISPLAY=1`: when the player runs on its own, SDL's display 0 is DSI-1, the **lower** panel. Without
    this the game lands on the bottom panel and the touch screen on the top.
  * `RGDS_PLAY_SWAP_AB=1`: A and B trade places for the game. Remove the line for the original mapping.
* **One core per game.** `run3ds.sh` loads `$CORE` if a launcher sets it, otherwise `azahar_libretro.so`. Each launcher
  pins its game to its own build in `/data/rgds/azahar/cores/`, so an Azahar change made for one game leaves the
  others alone. Ocarina of Time 3D stays on the plain build (`azahar-oot.so`). A Link Between Worlds uses the patched
  one (`azahar-albw.so`, the `bin/azahar_libretro.so` here).
* The launcher can't list `.3ds` files: its extensions are compiled in, and the release is SHA-256-checked at every
  launch. So each game gets a **Ports** entry instead. Ports only appear with *Settings → Library → Show all systems* on.

## What changed and why
| Piece | Change |
|---|---|
| Azahar 2126.2 | Built as a libretro core for aarch64 with `-DENABLE_OPENGL=ON` (upstream turns it off on aarch64 Linux). `USING_GLES` is added for the libretro target, which upstream only sets for Android. `azahar/azahar-no-geometry-shaders.patch`: the GPU has no geometry shaders, so the per-fragment-lighting fix-up stage is skipped and the vertex shader feeds the fragment shader directly. Without it, games with per-fragment lighting (A Link Between Worlds) fail to link shaders and run out of memory. `azahar/azahar-gles-buffer-storage.patch`: upstream maps its stream buffers persistently only with desktop GL's ARB_buffer_storage, so on GLES every draw mapped and unmapped them. It now uses EXT_buffer_storage (one mapping for the buffer's life, a fence wait when the ring wraps), which is about 10% faster in A Link Between Worlds. The Mali-G52 (Panfrost) offers GLES 3.1, so the game runs with `MESA_GLES_VERSION_OVERRIDE=3.2`. |
| rgds-play | For system `3ds`, it splits Azahar's stacked frame across both panels and maps touches on the lower panel to libretro POINTER input. **Zero-copy** frames: the core renders into a GL texture shared with both panel contexts, blitted after a fence, instead of `glReadPixels`. This took the title screen from 63% to about 95%. **Sound**: 1024-sample periods, a 120 ms queue, and SoundTouch tempo that follows the measured game speed. |
| Port launcher | A port only gets the top panel's DRM lease and runs in its own namespace and cgroup. The `.sh` hands the game to `run3ds.sh` outside the port session, which pauses the launcher, plays, then restores it. |

## Install (on the device, as root over SSH)
```sh
mkdir -p /data/rgds/azahar /data/roms/3ds
# from this folder on your computer (dropbear has no sftp, so pipe through cat):
ssh root@rgds.local 'cat > /data/rgds/azahar/azahar_libretro.so' < bin/azahar_libretro.so
ssh root@rgds.local 'mkdir -p /data/rgds/azahar/cores; cp /data/rgds/azahar/azahar_libretro.so /data/rgds/azahar/cores/azahar-oot.so; cp /data/rgds/azahar/azahar_libretro.so /data/rgds/azahar/cores/azahar-albw.so'
ssh root@rgds.local 'cat > /data/rgds/azahar/rgds-play'          < bin/rgds-play
ssh root@rgds.local 'cat > /data/rgds/azahar/run3ds.sh'          < scripts/run3ds.sh
ssh root@rgds.local 'cat > "/data/portmaster/ports/Zelda Ocarina of Time 3D.sh"' < "port/Zelda Ocarina of Time 3D.sh"
ssh root@rgds.local 'chmod +x /data/rgds/azahar/* /data/portmaster/ports/*.sh'
```
Put your ROM in `/data/roms/3ds/`. For another game, copy the port script and change its `ROM=` line.

Saves live under `/data/rgds/azahar/data/Azahar/sdmc/Nintendo 3DS/.../title/<id>/`. You can drop in a save from Citra
or Azahar on a computer.

Run directly: `/data/rgds/azahar/run3ds.sh /data/roms/3ds/game.3ds`. Benchmark: `scripts/bench.sh NAME FRAMES` (prints
`speed N fps`; `RGDS_PLAY_DEBUG=1` adds sound tempo lines).

## Building
Both builds run in a Debian trixie **arm64** container (docker/colima on an Apple Silicon Mac works).

* **Core**: `azahar/build.sh`, run in the container with a volume at `/work` (put both `azahar/*.patch` files in `/work` too). It downloads the 2126.2 unified source,
  applies the GLES change, builds `citra_libretro`, and writes `/work/out/azahar_libretro.so`.
* **Player**: start from the source the OS ships, the release's `emulators/rgds-play/source.tar.gz` (folders
  `rgds/` and `mgba/`). Add `src/launcher/system_icons.hpp` from
  [rgds-core](https://github.com/kleinrein/rgds-core), which the tarball leaves out. Apply
  `rgds-play/rgds-play-3ds.patch` with `patch -p1` from the folder holding `rgds/`. Add `soundtouch/` (`include/` and
  `source/SoundTouch/` from Azahar's `externals/soundtouch`). Then:
  ```sh
  tar cf - rgds mgba soundtouch | docker run -i --rm --platform linux/arm64 debian:trixie bash -c "$(cat rgds-play/build_play.sh)"
  ```
  The player must link `-static-libstdc++ -static-libgcc` and export only `rgds_*`. Exporting libstdc++'s symbols
  (`-rdynamic`) makes Azahar crash at boot.

## Licences
Azahar is GPL-2.0-or-later. RG DS Play is GPL-3.0-or-later. SoundTouch is LGPL-2.1. The binaries in `bin/` are built
from those upstream sources plus the patch and build scripts in this folder.
