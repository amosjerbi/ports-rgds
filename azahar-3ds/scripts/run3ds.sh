#!/bin/sh
# Run a 3DS game with Azahar inside RG DS Play, with the launcher paused. Usage: run3ds.sh ROM [frames]
# The launcher is paused by binding a stub over the session script (init respawns it); everything is undone at exit.
ROM="$1"; FRAMES="${2:-0}"
AZ=/data/rgds/azahar
APP=/usr/share/rgds/releases/$(cat /usr/share/rgds/active)
STUB=/run/rgds/session-paused
printf '#!/bin/sh\nexec sleep 2147483647\n' > $STUB; chmod +x $STUB
mount --bind $STUB /usr/libexec/rgds/session
killall rgds-launcher 2>/dev/null; sleep 2
restore() { umount /usr/libexec/rgds/session 2>/dev/null; pkill -f "sleep 2147483647"; }
trap restore EXIT INT TERM
export HOME=$AZ/home XDG_RUNTIME_DIR=/run/rgds
export SDL_VIDEODRIVER=kmsdrm SDL_RENDER_DRIVER=opengles2
export MESA_SHADER_CACHE_DIR=/data/rgds/cache/mesa MESA_GLES_VERSION_OVERRIDE=3.2
export RGDS_PLAY_CORE_OPTIONS="citra_graphics_api=OpenGL;citra_resolution_factor=1;citra_layout_option=default;citra_use_hw_shader=enabled;citra_use_shader_jit=enabled;citra_use_cpu_jit=enabled;citra_analog_function=c_stick${EXTRA_OPTS:+;$EXTRA_OPTS}"
for e in /sys/class/input/event*; do
    case "$(readlink -f "$e/device")" in *fe5e0000.i2c*) export RGDS_TOUCH_DEV=/dev/input/${e##*/};; esac
done
[ "$FRAMES" -gt 0 ] && export RGDS_PLAY_QUIT_AT=$FRAMES RGDS_PLAY_SHOTS=$AZ/shots RGDS_PLAY_SHOT_EVERY=300
mkdir -p $AZ/home $AZ/shots $AZ/data /data/rgds/bios
# the main loop on CPU 3 at real-time priority (as the launcher runs games), helper threads on all four cores
export RGDS_PLAY_CPU=3
taskset -c 0-3 ${PLAY:-$AZ/rgds-play} --rom "$ROM" --data $AZ/data --fonts $APP/assets/fonts \
    --core $AZ/azahar_libretro.so --system 3ds --system-dir /data/rgds/bios --no-companion
echo "rgds-play exit $?"
