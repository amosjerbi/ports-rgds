#!/bin/bash
# Runs ON the device. Args via env: SECS LS QUIT SHOTS TAPS TOUCHSIM CPUAT
export XDG_RUNTIME_DIR=/var/run/0-runtime-dir WAYLAND_DISPLAY=wayland-1 SWAYSOCK=/run/0-runtime-dir/sway-ipc.0.sock
# the exact environment EmulationStation gives the commands it launches
ESVARS=$(tr '\0' '\n' < /proc/$(pidof emulationstation)/environ | grep -E '^[A-Za-z_0-9]+=[^ ]*$' | grep -vE '^(_|SHLVL|PWD|OLDPWD)=')
if [ -n "$TOUCHSIM" ]; then
  (setsid nohup python3 /tmp/touchsim.py "$TOUCHSIM" $((SECS+10)) > /tmp/qa_ts.log 2>&1 &)
  for i in 1 2 3 4 5 6 7 8 9 10; do [ -s /tmp/touchsim.node ] && break; sleep 0.5; done
  TOUCHDEV=$(cat /tmp/touchsim.node); echo "touchsim node: $TOUCHDEV"
fi
cd /tmp
(systemd-inhibit --what=idle:sleep --why=tmc-qa sleep $((SECS+30)) >/dev/null 2>&1 &)
setsid nohup env -i $ESVARS RGDS_TEST_TAPS="$TAPS" RGDS_TOUCH_DEV="$TOUCHDEV" sh -c "/storage/.config/tmc-rgds/gba-launch.sh /storage/roms/gba/Legend\ of\ Zelda,\ The\ -\ The\ Minish\ Cap\ \(USA\).gba -Pgba --core=mgba --emulator=retroarch --controllers=\"\"" >/tmp/qa_wrap.log 2>&1 < /dev/null &
udp() { python3 -c "
import socket,sys;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.sendto(sys.argv[1].encode(),('127.0.0.1',55355))" "$1"; }
cpu() { awk '{print $14+$15}' /proc/$1/stat 2>/dev/null; }
T=0
while [ $T -lt $SECS ]; do
  sleep 1; T=$((T+1))
  [ $T -eq "${LS:-0}" ] && [ "${LS:-0}" -gt 0 ] && { udp LOAD_STATE; echo "t=$T LOAD_STATE sent"; }
  [ $T -eq "${QUIT:-0}" ] && [ "${QUIT:-0}" -gt 0 ] && { pkill -TERM retroarch; echo "t=$T retroarch terminated"; }
  for S in $SHOTS; do [ $T -eq $S ] && { grim -o DSI-2 /tmp/qa_top_$S.png; grim -o DSI-1 /tmp/qa_bot_$S.png; echo "t=$T shot"; }; done
  if [ "${CPUAT:-0}" -gt 0 ]; then
    [ $T -eq $CPUAT ] && { RA=$(pidof retroarch | awk '{print $1}'); BT=$(pidof tmc-bottom); R0=$(cpu $RA); B0=$(cpu $BT); }
    [ $T -eq $((CPUAT+10)) ] && echo "t=$T CPU ticks in 10s (100 = one full core): retroarch=$(( $(cpu $RA) - R0 )) tmc-bottom=$(( $(cpu $BT) - B0 ))"
  fi
  [ $((T%${STATUSEVERY:-10})) -eq 0 ] && echo "t=$T retroarch=$(pidof retroarch | wc -w) bottom=$(pidof tmc-bottom | wc -w) wrapper=$(pgrep -f '[g]ba-launch' | wc -l) bottomRSS=$(awk '/VmRSS/{print $2}' /proc/$(pidof tmc-bottom)/status 2>/dev/null)kB"
done
echo "--- touchsim"; tail -6 /tmp/qa_ts.log 2>/dev/null
echo "--- bottom log"; grep -E "^\[(bottom|rgds)\]" /tmp/tmc-bottom.log | tail -${LOGLINES:-14}
