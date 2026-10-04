#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d /opt/system/Tools/PortMaster ]; then
  controlfolder=/opt/system/Tools/PortMaster
elif [ -d /opt/tools/PortMaster ]; then
  controlfolder=/opt/tools/PortMaster
elif [ -d "$XDG_DATA_HOME/PortMaster" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder=/roms/ports/PortMaster
fi

source "$controlfolder/control.txt"
[ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && source "$controlfolder/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/ashorthike"
DATADIR="$GAMEDIR/gamedata"
cd "$GAMEDIR" || exit 1
exec > >(tee "$GAMEDIR/log.txt") 2>&1

if [ ! -f "$DATADIR/AShortHike.x86_64" ] || [ ! -f "$DATADIR/UnityPlayer.so" ] || [ ! -d "$DATADIR/AShortHike_Data" ]; then
  pm_message "Copy the Linux A Short Hike game files into ports/ashorthike/gamedata. The Switch NSP cannot be used here."
  exit 1
fi

graphics_mode=crusty_glx_core4es
if [ "${DEVICE_NAME:-}" = "Anbernic RG DS" ] && [ "${CFW_NAME:-}" = ROCKNIX ]; then
  if ! lsmod | grep -q '^panfrost '; then
    pm_message "A Short Hike needs the Panfrost GPU driver on RG DS. Select Panfrost in ROCKNIX and reboot."
    exit 1
  fi
  graphics_mode=system
  export MESA_GL_VERSION_OVERRIDE=3.2 MESA_GLSL_VERSION_OVERRIDE=330
fi

if [ -x "$GAMEDIR/box64/box64" ]; then
  BOX64="$GAMEDIR/box64/box64"
else
  BOX64=$(command -v box64)
fi
if [ -z "$BOX64" ]; then
  pm_message "Box64 is required. Place an ARM64 box64 binary at ports/ashorthike/box64/box64."
  exit 1
fi

weston_runtime=weston_pkg_0.2
weston_dir=/tmp/weston
if [ ! -f "$controlfolder/libs/${weston_runtime}.squashfs" ]; then
  "$controlfolder/harbourmaster" --quiet --no-check runtime_check "${weston_runtime}.squashfs"
fi
if [ ! -f "$controlfolder/libs/${weston_runtime}.squashfs" ]; then
  pm_message "Westonpack 0.2 runtime is missing. Update PortMaster and retry."
  exit 1
fi

$ESUDO mkdir -p "$weston_dir" "$GAMEDIR/conf/config" "$GAMEDIR/conf/local"
[ "$PM_CAN_MOUNT" != N ] && $ESUDO umount "$weston_dir" 2>/dev/null
$ESUDO mount "$controlfolder/libs/${weston_runtime}.squashfs" "$weston_dir" || exit 1

[ "$CFW_NAME" = ROCKNIX ] && export rocknix_mode=1 SDL_AUDIODRIVER=alsa
$ESUDO chmod a+x "$DATADIR/AShortHike.x86_64"
cd "$DATADIR" || exit 1
XDG_CONFIG_HOME="$GAMEDIR/conf/config" XDG_DATA_HOME="$GAMEDIR/conf/local" \
  "$weston_dir/westonwrap.sh" headless noop kiosk "$graphics_mode" \
  "$BOX64" ./AShortHike.x86_64 -screen-width 640 -screen-height 480

$ESUDO "$weston_dir/westonwrap.sh" cleanup
[ "$PM_CAN_MOUNT" != N ] && $ESUDO umount "$weston_dir"
pm_finish
