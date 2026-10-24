#!/bin/sh
# on device: launch Circle of the Moon through the GBA wrapper with EmulationStation's own environment (like picking it in the list)
export XDG_RUNTIME_DIR=/var/run/0-runtime-dir WAYLAND_DISPLAY=wayland-1 SWAYSOCK=/run/0-runtime-dir/sway-ipc.0.sock
ESVARS=$(tr '\0' '\n' < /proc/$(pidof emulationstation)/environ | grep -E '^[A-Za-z_0-9]+=[^ ]*$' | grep -vE '^(_|SHLVL|PWD|OLDPWD)=')
for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in "/storage/roms/ports/cima-companion/cima-bottom"*) kill ${p#/proc/};; esac; done 2>/dev/null
pkill -9 retroarch; pkill -9 -f "[r]unemu.sh"; sleep 2
cd /tmp
setsid nohup env -i $ESVARS sh -c '/storage/roms/ports/cima-companion/gba-launch "/storage/roms/gba/CIMA - The Enemy (USA).zip" -Pgba --core=mgba --emulator=retroarch --controllers="" -autosave 0 -state_slot 1' >/tmp/cima_wrap.log 2>&1 < /dev/null &
sleep ${WAIT:-20}
echo "retroarch=$(pidof retroarch | wc -w) cima-bottom=$(pidof cima-bottom | wc -w)"; cat /tmp/cima-bottom.log
