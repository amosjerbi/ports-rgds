#!/usr/bin/env bash
# Copies hod-bottom (+ font, from the device's own PortMaster resources) to the RG DS.
# usage: DEVICE=192.168.0.105 ./deploy.sh      (password: ROCKNIX default "rocknix")
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEV=${DEVICE:-192.168.0.105}; PASS=${DEVICE_PASS:-rocknix}
SSH="sshpass -p $PASS ssh -o ConnectTimeout=8 root@$DEV"
$SSH 'for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in "/storage/roms/ports/hod-companion/hod-bottom"*) kill ${p#/proc/};; esac; done 2>/dev/null; mkdir -p /storage/roms/ports/hod-companion; [ -f /storage/roms/ports/hod-companion/DejaVuSans.ttf ] || cp /storage/roms/ports/PortMaster/resources/DejaVuSans.ttf /storage/roms/ports/hod-companion/; sleep 1'
sshpass -p "$PASS" scp -q "$D/build/hod-bottom" root@$DEV:/storage/roms/ports/hod-companion/hod-bottom
$SSH 'chmod +x /storage/roms/ports/hod-companion/hod-bottom'
sshpass -p "$PASS" scp -q "$D/gba-launch" root@$DEV:/storage/roms/ports/hod-companion/gba-launch
$SSH 'chmod +x /storage/roms/ports/hod-companion/gba-launch'
echo deployed
