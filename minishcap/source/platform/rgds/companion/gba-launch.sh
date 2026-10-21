#!/bin/bash
# GBA-system launch wrapper. EmulationStation passes: ROM -Pgba --core=.. --emulator=.. --controllers=..
#
#  * Minish Cap with the mGBA core (the default): runs real mGBA in RetroArch on the top panel and starts
#    tmc-bottom, which reads the game memory over RetroArch network commands and draws the second screen
#    (map / quest / items) on the bottom panel. It exits with the emulator.
#  * Minish Cap with the "minishcap-rgds" core: the native dual-screen build (tmc-rgds, no emulator).
#  * Anything else: the stock launcher, unchanged.
# Create /storage/.config/tmc-rgds/no-companion to run plain mGBA for Minish Cap.
ROM="$1"
D=/storage/.config/tmc-rgds
CODE=$(dd if="$ROM" bs=1 skip=172 count=4 2>/dev/null)
MINISH=0; { [ "$CODE" = "BZME" ] || [ "$CODE" = "BZMP" ]; } && MINISH=1
case " $* " in
  *"--core=minishcap-rgds "*)
    if [ "$MINISH" = 1 ]; then
        . /etc/profile
        export SDL_VIDEODRIVER=wayland
        cd $D || exit 1
        exec ./tmc-rgds "$ROM"
    fi ;;
  *"--core=mgba "*)
    # The companion's RAM map is the USA one (BZME); other regions get plain mGBA.
    if [ "$CODE" = "BZME" ] && [ ! -e $D/no-companion ]; then
        . /etc/profile
        # RetroArch's network command port (memory reads, state load) is only open during this session.
        RACFG=/storage/.config/retroarch/retroarch.cfg
        sed -i 's|^network_cmd_enable = .*|network_cmd_enable = "true"|' $RACFG
        SDL_VIDEODRIVER=wayland $D/tmc-bottom "$ROM" > /tmp/tmc-bottom.log 2>&1 &
        CP=$!
        /usr/bin/runemu.sh "$@"
        RC=$?
        kill $CP 2>/dev/null; wait $CP 2>/dev/null
        sed -i 's|^network_cmd_enable = .*|network_cmd_enable = "false"|' $RACFG
        # EmulationStation keeps the lower panel off; put it back the way it was.
        SWAYSOCK=${SWAYSOCK:-/run/0-runtime-dir/sway-ipc.0.sock} swaymsg "output DSI-1 power off" >/dev/null 2>&1
        exit $RC
    fi ;;
esac
exec /usr/bin/runemu.sh "$@"
