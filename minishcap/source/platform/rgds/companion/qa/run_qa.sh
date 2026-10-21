#!/bin/bash
# usage: DEVICE=<ip> [TMC_DIR=/storage/roms/ports/tmc-rgds] [TAPS="ms,x,y;.."] [TOUCHSIM="s,x,y;.."] [SHOTS="s1 s2"] [LS=sec] [QUIT=sec] [CPUAT=sec] run_qa.sh <outdir> <seconds>
# Launches Minish Cap through the GBA wrapper with EmulationStation's own environment. See remote_run.sh.
OUT=$1; SECS=${2:-60}
D=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
DEVICE=${DEVICE:?set DEVICE=<ip of the RG DS>}   # e.g. DEVICE=192.168.1.50
PASS=${DEVICE_PASS:-rocknix}                      # ROCKNIX default root password
SSH="sshpass -p $PASS ssh -o ConnectTimeout=10 root@$DEVICE"
sshpass -p "$PASS" scp -q "$D/touchsim.py" "$D/remote_run.sh" root@$DEVICE:/tmp/
$SSH 'pkill -9 retroarch; pkill -9 tmc-bottom; pkill -9 tmc-rgds; pkill -9 -f "[r]unemu.sh"; pkill -9 -f "[g]ba-launch.sh"; pkill -9 -f "[t]ouchsim.py"; sleep 2; rm -f /var/log/exec.log /tmp/tmc-bottom.log /tmp/qa_*.png /tmp/touchsim.node; echo "start: retroarch=$(pidof retroarch | wc -w) bottom=$(pidof tmc-bottom | wc -w)"'
$SSH "SECS=$SECS LS='${LS:-0}' QUIT='${QUIT:-0}' SHOTS='${SHOTS:-}' TAPS='${TAPS:-}' TOUCHSIM='${TOUCHSIM:-}' CPUAT='${CPUAT:-0}' LOGLINES='${LOGLINES:-14}' STATUSEVERY='${STATUSEVERY:-10}' bash /tmp/remote_run.sh"
for f in $($SSH 'ls /tmp/qa_*.png 2>/dev/null'); do sshpass -p "$PASS" scp -q root@$DEVICE:$f "$OUT/$(basename $f | sed s/qa_//)"; done
ls "$OUT" | tr '\n' ' '; echo
