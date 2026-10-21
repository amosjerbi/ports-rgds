#!/bin/bash
# Runs ON the device (copy it there and run with bash; do not put pkill -f patterns in an ssh command line).
# env: CORE (gambatte|sameboy), STATE (a gambatte state file to install as slot 7, optional), LOADCMD (UDP command, e.g.
#      "LOAD_STATE_SLOT 7" or "LOAD_STATE"), LS (seconds at which to send it), WAIT, SHOTS ("t1 t2"),
#      PKS ("t:addr:b,b;..." live memory writes), CPUAT, LOGLINES
B=/storage/roms/ports/frog/backup; SD=/storage/roms/savestates/gb; GB=/storage/roms/gb
CORE=${CORE:-gambatte}
[ "$CORE" = sameboy ] && EXTRA="-state_slot 0 -autosave 1"
mkdir -p $B
for f in frog-color.srm; do [ -e $B/$f.orig ] || cp -p $GB/$f $B/$f.orig; done
cp -p $SD/frog-color.state $B/frog-color.state.orig 2>/dev/null; cp -p $SD/frog-color.state.png $B/frog-color.state.png.orig 2>/dev/null
ls $SD | grep -v '\.auto' > /tmp/states.before 2>/dev/null
restore() {
  pkill -TERM retroarch; sleep 5; pkill -9 retroarch; pkill -9 frog-bottom; pkill -9 -f "[r]unemu.sh"; pkill -9 -f "[g]b-launch"
  cp -p $B/frog-color.srm.orig $GB/frog-color.srm
  [ -e $B/frog-color.state.orig ] && cp -p $B/frog-color.state.orig $SD/frog-color.state
  [ -e $B/frog-color.state.png.orig ] && cp -p $B/frog-color.state.png.orig $SD/frog-color.state.png
  for f in $(ls $SD 2>/dev/null); do grep -qx "$f" /tmp/states.before || rm -f "$SD/$f"; done
  sed -i 's|^network_cmd_enable = .*|network_cmd_enable = "false"|' /storage/.config/retroarch/retroarch.cfg
  export XDG_RUNTIME_DIR=/var/run/0-runtime-dir SWAYSOCK=/run/0-runtime-dir/sway-ipc.0.sock; swaymsg "output DSI-1 power off" >/dev/null 2>&1
  echo "restored: srm=$(md5sum $GB/frog-color.srm | cut -c1-8)/$(md5sum $B/frog-color.srm.orig | cut -c1-8) state0=$(md5sum $SD/frog-color.state 2>/dev/null | cut -c1-8)/$(md5sum $B/frog-color.state.orig 2>/dev/null | cut -c1-8)"
}
trap restore EXIT
export XDG_RUNTIME_DIR=/var/run/0-runtime-dir WAYLAND_DISPLAY=wayland-1 SWAYSOCK=/run/0-runtime-dir/sway-ipc.0.sock
ESVARS=$(tr '\0' '\n' < /proc/$(pidof emulationstation)/environ | grep -E '^[A-Za-z_0-9]+=[^ ]*$' | grep -vE '^(_|SHLVL|PWD|OLDPWD)=')
[ -n "$STATE" ] && cp "$STATE" $SD/frog-color.state7
pkill -9 retroarch; pkill -9 frog-bottom; sleep 1
if [ -n "$TOUCHSIM" ]; then
  rm -f /tmp/touchsim.node; (setsid nohup python3 /tmp/touchsim.py "$TOUCHSIM" $((WAIT+10)) > /tmp/fq_ts.log 2>&1 &)
  for i in 1 2 3 4 5 6 7 8 9 10; do [ -s /tmp/touchsim.node ] && break; sleep 0.5; done
  TOUCHDEV=$(cat /tmp/touchsim.node)
fi
cd /tmp
(systemd-inhibit --what=idle:sleep --why=frog-qa sleep $((WAIT+30)) >/dev/null 2>&1 &)
setsid nohup env -i $ESVARS FROG_SAVE="$FROG_SAVE" FROG_TEST_TAP="$TESTTAP" FROG_TOUCH_DEV="$TOUCHDEV" sh -c "/storage/roms/ports/frog/gb-launch /storage/roms/gb/frog-color.gb -Pgb --core=$CORE --emulator=retroarch --controllers=\"\" $EXTRA" > /tmp/frog-wrap.log 2>&1 < /dev/null &
udp() { python3 -c "
import socket,sys;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.sendto(sys.argv[1].encode(),('127.0.0.1',55355))" "$1"; }
cpu() { awk '{print $14+$15}' /proc/$1/stat 2>/dev/null; }
T=0
while [ $T -lt $WAIT ]; do
  sleep 1; T=$((T+1))
  for PK in $(echo "${PKS:-}" | tr ';' ' '); do
    PT=${PK%%:*}; REST=${PK#*:}; PA=${REST%%:*}; PB=${REST#*:}
    [ $T -eq "$PT" ] && { udp "WRITE_CORE_MEMORY $PA $(echo $PB | tr ',' ' ')"; echo "t=$T poke $PA <- $PB"; }
  done
  [ $T -eq ${LS:-0} ] && [ ${LS:-0} -gt 0 ] && { udp "${LOADCMD:-LOAD_STATE_SLOT 7}"; echo "t=$T ${LOADCMD:-LOAD_STATE_SLOT 7}"; }
  for S in $SHOTS; do [ $T -eq $S ] && { grim -o DSI-2 /tmp/fq_top_$S.png; grim -o DSI-1 /tmp/fq_bot_$S.png; echo "t=$T shot"; }; done
  if [ ${CPUAT:-0} -gt 0 ]; then
    [ $T -eq $CPUAT ] && { RA=$(pidof retroarch | awk '{print $1}'); FB=$(pidof frog-bottom); R0=$(cpu $RA); F0=$(cpu $FB); }
    [ $T -eq $((CPUAT+10)) ] && echo "t=$T CPU ticks in 10 s (100 = one core): retroarch=$(( $(cpu $RA) - R0 )) frog-bottom=$(( $(cpu $FB) - F0 ))"
  fi
  [ $((T%10)) -eq 0 ] && echo "t=$T retroarch=$(pidof retroarch | wc -w) frog-bottom=$(pidof frog-bottom | wc -w)"
done
echo "--- core in use: $(ps | grep '[r]etroarch -L' | grep -o '[a-z]*_libretro' | head -1)"
[ -n "$READS" ] && python3 - <<PYEOF
import socket
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.settimeout(2)
def rd(a,n):
    s.sendto(("READ_CORE_MEMORY %x %d"%(a,n)).encode(),("127.0.0.1",55355))
    try: return s.recvfrom(8192)[0].decode().strip()
    except Exception as e: return "ERR "+str(e)
print("live life/max  CBCC:", rd(0xCBCC,2))
print("live cell r1c0 CBEB:", rd(0xCBEB,1))
print("live HUD hearts 9C02:", rd(0x9C02,16))
PYEOF
echo "--- frog-bottom log"; grep -E "^\[frog\]|^\[lr" /tmp/frog-bottom.log | tail -${LOGLINES:-8}
