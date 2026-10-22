#!/usr/bin/env bash
# Copies aos-bottom (+ font, from the device's own PortMaster resources) to the RG DS.
# usage: DEVICE=192.168.0.105 ./deploy.sh      (password: ROCKNIX default "rocknix")
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEV=${DEVICE:-192.168.0.105}; PASS=${DEVICE_PASS:-rocknix}
SSH="sshpass -p $PASS ssh -o ConnectTimeout=8 root@$DEV"
$SSH 'for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in "/storage/roms/ports/aos-companion/aos-bottom"*) kill ${p#/proc/};; esac; done 2>/dev/null; mkdir -p /storage/roms/ports/aos-companion; [ -f /storage/roms/ports/aos-companion/DejaVuSans.ttf ] || cp /storage/roms/ports/PortMaster/resources/DejaVuSans.ttf /storage/roms/ports/aos-companion/; sleep 1'
sshpass -p "$PASS" scp -q "$D/build/aos-bottom" root@$DEV:/storage/roms/ports/aos-companion/aos-bottom
$SSH 'chmod +x /storage/roms/ports/aos-companion/aos-bottom'
echo deployed
