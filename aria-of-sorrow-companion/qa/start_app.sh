#!/bin/sh
# on device: (re)start aos-bottom on the bottom panel. If /tmp/touchd.node exists the virtual touch screen is used.
# usage: start_app.sh [dir]   (default /storage/roms/ports/aos-companion)
D=${1:-/storage/roms/ports/aos-companion}
for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in "$D/aos-bottom"*) kill ${p#/proc/};; esac; done 2>/dev/null
sleep 1
. /etc/profile
export XDG_RUNTIME_DIR=/var/run/0-runtime-dir WAYLAND_DISPLAY=wayland-1 SWAYSOCK=/run/0-runtime-dir/sway-ipc.0.sock
[ -s /tmp/touchd.node ] && export RGDS_TOUCH_DEV=$(cat /tmp/touchd.node) AOS_PAD_DEV=$(cat /tmp/touchd.node)
swaymsg "focus output DSI-2" >/dev/null 2>&1
SDL_VIDEODRIVER=wayland setsid nohup $D/aos-bottom > /tmp/aos-bottom.log 2>&1 < /dev/null &
sleep ${WAIT:-5}; cat /tmp/aos-bottom.log
