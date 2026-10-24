#!/usr/bin/env bash
# Copies gsl-bottom (+ font, from the device's own PortMaster resources) to the RG DS.
# WRAPPER=1 also installs gba-launch as the wrapper EmulationStation calls (/storage/roms/ports/tmc-rgds/gba-launch; old one kept as .bak-gsl).
# usage: DEVICE=192.168.0.105 ./deploy.sh      (password: ROCKNIX default "rocknix")
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEV=${DEVICE:-192.168.0.105}; PASS=${DEVICE_PASS:-rocknix}
SSH="sshpass -p $PASS ssh -o ConnectTimeout=8 root@$DEV"
$SSH 'for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in "/storage/roms/ports/gsl-companion/gsl-bottom"*) kill ${p#/proc/};; esac; done 2>/dev/null; mkdir -p /storage/roms/ports/gsl-companion; [ -f /storage/roms/ports/gsl-companion/DejaVuSans.ttf ] || cp /storage/roms/ports/PortMaster/resources/DejaVuSans.ttf /storage/roms/ports/gsl-companion/; sleep 1'
sshpass -p "$PASS" scp -q "$D/build/gsl-bottom" root@$DEV:/storage/roms/ports/gsl-companion/gsl-bottom
$SSH 'chmod +x /storage/roms/ports/gsl-companion/gsl-bottom'
sshpass -p "$PASS" scp -q "$D/gba-launch" root@$DEV:/storage/roms/ports/gsl-companion/gba-launch
$SSH 'chmod +x /storage/roms/ports/gsl-companion/gba-launch'
if [ "${WRAPPER:-0}" = 1 ]; then
    $SSH 'W=/storage/roms/ports/tmc-rgds/gba-launch; [ -e $W.bak-gsl ] || cp $W $W.bak-gsl; cp /storage/roms/ports/gsl-companion/gba-launch $W; chmod +x $W'
fi
echo deployed
