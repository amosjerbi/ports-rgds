# A Short Hike on PortMaster (experimental)

This package contains the PortMaster launcher and a helper script. It does **not** contain the game. You need a legally obtained Linux copy of A Short Hike that includes `game-root.dwarfs`; the Nintendo Switch NSP is not usable by this launcher.

## Extract your game files

Install the DwarFS tools. On macOS with Homebrew:

```sh
brew install dwarfs
```

From this folder, run the extraction helper with the path to your own archive:

```sh
./extract-gamedata.sh /path/to/files/game-root.dwarfs
```

The script extracts the archive into `ashorthike/gamedata/`. You can pass a second argument to choose another output directory:

```sh
./extract-gamedata.sh /path/to/game-root.dwarfs /path/to/ashorthike/gamedata
```

It checks for `AShortHike.x86_64`, `UnityPlayer.so`, and `AShortHike_Data/` before reporting success. The destination must be empty; this avoids mixing files from separate game builds. Keep the original archive and extracted game files private.

## Install on the device

Copy `A Short Hike.sh` and the `ashorthike` folder, including the generated `gamedata`, to `/roms/ports/` on the device. PortMaster installs the declared Westonpack runtime. The launcher needs ARM64 `box64` on the system `PATH` or at `ashorthike/box64/box64`.

On an Anbernic RG DS running ROCKNIX, select the **Panfrost** GPU driver in ROCKNIX's system settings and reboot before launching. The launcher checks that Panfrost is active. The first start may take about three minutes. This prototype has rendered the opening scene, but controls, frame rate, audio, saving, and later gameplay still need testing. Logs are written to `ashorthike/log.txt`.

The game is sold at [itch.io](https://adamgryu.itch.io/a-short-hike). This prototype is not an official PortMaster release.
