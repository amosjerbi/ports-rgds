#!/usr/bin/env bash
# Build calico (ARM7 + ARM9) without CMake into toolchain/dkp/calico.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/src/calico"
OUT="$HERE/dkp/calico"
BIN=/opt/devkitpro/devkitARM/bin
CC="$BIN/arm-none-eabi-gcc"
AR="$BIN/arm-none-eabi-ar"

common=(source/system/irq.c source/system/tick.c source/system/thread_cold.c
  source/system/thread_hot.32.c source/system/mutex.c source/system/mailbox.c
  source/system/dietprint.c source/system/newlib_syscalls.c)
# remaining common sources from CMakeLists (between newlib_syscalls.c and fugu)
mapfile -t extra < <(sed -n '/newlib_syscalls.c/,/fugu.32.c/p' "$SRC/CMakeLists.txt" \
  | grep -o 'source/[^ )]*' | grep -v newlib_syscalls)
nds=(source/nds/startup.crt0.c source/nds/utils.crt0.s source/nds/bios.s
  source/nds/bios.twl.s source/nds/irq_handler.32.s source/nds/tlnc.twl.c
  source/nds/pxi.c source/nds/smutex.32.c source/nds/keypad.c source/nds/pm.c
  source/nds/netbuf.c source/dev/wlan.c source/nds/gbacart.c source/nds/ntrcard.c
  source/nds/nitrorom.c)
mapfile -t arm9 < <(sed -n '/if(ARM9)/,/endif()/p' "$SRC/CMakeLists.txt" | grep -o 'source/[^ )]*' | grep -v dietprint)
mapfile -t arm7 < <(sed -n '/if(ARM7)/,/endif()/p' "$SRC/CMakeLists.txt" | grep -o 'source/[^ )]*')

build() { # name arch-flags... -- files...
  local name=$1; shift
  local flags=()
  while [[ $1 != -- ]]; do flags+=("$1"); shift; done; shift
  local obj="$HERE/build/calico_$name"
  rm -rf "$obj"; mkdir -p "$obj"
  for f in "$@"; do
    local base; base=$(basename "$f"); base="${base%.*}"
    local mode=()
    case "$f" in *.32.c) mode=(-marm);; *.16.c) mode=(-mthumb);; esac
    [[ $name == ds9 && $f == source/system/dietprint.c ]] && mode=(-mthumb)
    (cd "$SRC" && "$CC" "${flags[@]}" "${mode[@]}" -Iinclude -x "$( [[ $f == *.s ]] && echo assembler-with-cpp || echo c )" -c "$f" -o "$obj/$base.o") || { echo "FAILED: $name $f"; exit 1; }
  done
  "$AR" rcs "$OUT/lib/libcalico_$name.a" "$obj"/*.o
  echo "built libcalico_$name.a ($(ls "$obj" | wc -l | tr -d ' ') objects)"
}

mkdir -p "$OUT/lib" "$OUT/include" "$OUT/share"
cp -R "$SRC/include/." "$OUT/include/"
cp "$SRC"/share/*.ld "$OUT/lib/"
cp "$SRC"/share/*.specs "$SRC"/share/*.bmp "$OUT/share/"

base=(-ffunction-sections -fdata-sections -D__NDS__ -DNDEBUG -Wall)
build ds9 -march=armv5te -mtune=arm946e-s -O2 "${base[@]}" -DARM9 -- \
  "${common[@]}" "${extra[@]}" source/arm/arm-cache.32.s source/arm/arm-shims-mpu.32.c "${nds[@]}" "${arm9[@]}"
build ds7 -march=armv4t -mtune=arm7tdmi -mthumb -Os "${base[@]}" -DARM7 -- \
  "${common[@]}" "${extra[@]}" "${nds[@]}" "${arm7[@]}"
