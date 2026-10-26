#!/usr/bin/env bash
# Copies m3-bottom (+ font, from the device's own PortMaster resources) and the companion's gba-launch to the RG DS.
# usage: DEVICE=192.168.0.105 ./deploy.sh      (password: ROCKNIX default "rocknix")
# The wrapper EmulationStation calls is /storage/roms/ports/tmc-rgds/gba-launch; WRAPPER=1 also installs this folder's
# gba-launch there (the old one is kept as gba-launch.bak-m3).
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEV=${DEVICE:-192.168.0.105}; PASS=${DEVICE_PASS:-rocknix}
APP=/storage/roms/ports/m3-companion
SSH="sshpass -p $PASS ssh -o ConnectTimeout=8 root@$DEV"
$SSH "for p in /proc/[0-9]*; do c=\$(tr '\\0' ' ' < \$p/cmdline 2>/dev/null); case \"\$c\" in \"$APP/m3-bottom\"*) kill \${p#/proc/};; esac; done 2>/dev/null; mkdir -p $APP; [ -f $APP/DejaVuSans.ttf ] || cp /storage/roms/ports/PortMaster/resources/DejaVuSans.ttf $APP/; sleep 1"
sshpass -p "$PASS" scp -q "$D/build/m3-bottom" root@$DEV:$APP/m3-bottom
$SSH "chmod +x $APP/m3-bottom"
sshpass -p "$PASS" scp -q "$D/gba-launch" root@$DEV:$APP/gba-launch
$SSH "chmod +x $APP/gba-launch"
if [ "${WRAPPER:-0}" = 1 ]; then
    $SSH "W=/storage/roms/ports/tmc-rgds/gba-launch; [ -e \$W.bak-m3 ] || cp \$W \$W.bak-m3; cp $APP/gba-launch \$W; chmod +x \$W"
fi
echo deployed
