#!/bin/bash
# Build the patched rgds-play inside a Debian trixie arm64 container. Source arrives as a tar on stdin.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq && apt-get install -y -qq build-essential cmake ninja-build libsdl2-dev >/dev/null
# the device SDL has no X11: keep X11 headers (Font, None) out of SDL_syswm.h
for f in $(grep -rl "define SDL_VIDEO_DRIVER_X11 1" /usr/include); do sed -i "s/^#define SDL_VIDEO_DRIVER_X11 1/\/* no X11 *\//" $f; done
rm -rf /play && mkdir -p /play && cd /play && tar xf -
OPTS="LIBMGBA_ONLY=OFF BUILD_STATIC=ON BUILD_SHARED=OFF CMAKE_BUILD_TYPE=Release BUILD_LTO=OFF BUILD_SDL=OFF BUILD_QT=OFF
BUILD_LIBRETRO=OFF BUILD_GL=OFF BUILD_GLES2=OFF BUILD_GLES3=OFF BUILD_PERF=OFF BUILD_TEST=OFF BUILD_SUITE=OFF BUILD_CINEMA=OFF
BUILD_ROM_TEST=OFF BUILD_UPDATER=OFF BUILD_HEADLESS=OFF BUILD_PYTHON=OFF USE_EPOXY=OFF ENABLE_DEBUGGERS=OFF ENABLE_GDB_STUB=OFF
USE_EDITLINE=OFF USE_SQLITE3=OFF USE_ELF=OFF USE_LZMA=ON USE_ZLIB=ON USE_PNG=OFF USE_LIBZIP=OFF USE_MINIZIP=OFF USE_FFMPEG=OFF
USE_LUA=OFF USE_JSON_C=OFF USE_DISCORD_RPC=OFF USE_FREETYPE=OFF M_CORE_GB=ON M_CORE_GBA=ON ENABLE_SCRIPTING=OFF DISABLE_DEPS=OFF
CMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON CMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON PKG_CONFIG_EXECUTABLE=/usr/bin/false"
ARGS=(); for o in $OPTS; do ARGS+=("-D$o"); done
cmake -S mgba -B out/mgba -G Ninja "${ARGS[@]}" -DCMAKE_C_FLAGS=-mcpu=cortex-a55 >/dev/null
cmake --build out/mgba --target mgba -j"$(nproc)" >/dev/null
grep -q zlibstatic out/mgba/build.ninja && cmake --build out/mgba --target zlibstatic -j"$(nproc)" >/dev/null
DEFINES=$(grep -m1 '^  DEFINES = ' out/mgba/build.ninja | cut -d= -f2-)
INC="-Imgba/include -Iout/mgba/include -Irgds/play/src -Irgds/src"
[ -f out/mgba/zlib/libz.a ] && INC="$INC -Iout/mgba/zlib -Imgba/src/third-party/zlib"
CF="-mcpu=cortex-a55 -O2 -DSOUNDTOUCH_INTEGER_SAMPLES -DST_NO_EXCEPTION_HANDLING"
INC="$INC -Isoundtouch/include"
for c in AAFilter FIFOSampleBuffer FIRFilter InterpolateCubic InterpolateLinear InterpolateShannon RateTransposer SoundTouch TDStretch PeakFinder cpu_detect_x86; do
  g++ $CF -std=c++17 -w -Isoundtouch/include -c soundtouch/source/SoundTouch/$c.cpp -o out/st_$c.o; ST="${ST:-} out/st_$c.o"
done
OBJS=""
for c in core/mgba_split.c core/mgba_gb.c; do
  o=out/$(basename $c .c).o; gcc $CF -std=gnu11 -Wall -Wno-unused-parameter $DEFINES $INC -c rgds/play/src/$c -o $o; OBJS="$OBJS $o"
done
SRCS=""
for s in core/mgba_core.cpp core/libretro_core.cpp companion/profile.cpp companion/map_capture.cpp companion/companion.cpp \
  companion/party.cpp companion/party_gb.cpp companion/view.cpp companion/automap.cpp companion/mosaic.cpp companion/grid.cpp \
  companion/trace.cpp companion/board.cpp companion/battle_gb.cpp companion/dock.cpp ui/canvas.cpp ui/screens.cpp ui/frame.cpp \
  ui/colour.cpp ui/png.cpp ui/look.cpp ui/kit.cpp ui/words.cpp app/storage.cpp app/session.cpp app/cheats.cpp frontend/main.cpp; do
  SRCS="$SRCS rgds/play/src/$s"; done
ZLIB=""; [ -f out/mgba/zlib/libz.a ] && ZLIB=out/mgba/zlib/libz.a
g++ $CF -std=c++17 -Wall -Wextra $DEFINES $INC -static-libstdc++ -static-libgcc -Wl,--export-dynamic-symbol=rgds_* $SRCS $OBJS $ST out/mgba/libmgba.a $ZLIB -lSDL2 -lpthread -lm -ldl \
    -Wl,--strip-all -o out/rgds-play 2>&1 | grep -E "error|warning: unused" | head -30 || true
ls -la out/rgds-play
