#!/bin/bash
rm -f /tmp/touchsim.node /tmp/to.log
(setsid nohup python3 /tmp/touchsim.py "6,296,240;9,368,240;12,440,240;15,512,240;18,368,312" 30 > /tmp/ts.log 2>&1 &)
for i in 1 2 3 4 5 6 7 8 9 10; do [ -s /tmp/touchsim.node ] && break; sleep 0.5; done
export SDL_VIDEODRIVER=dummy FROG_TOUCH_DEV=$(cat /tmp/touchsim.node)
timeout 26 /storage/roms/ports/frog/frog-bottom /storage/roms/gb/frog-color.gb > /tmp/to.log 2>&1
grep -E "touch|tap" /tmp/to.log; echo "--- simulator:"; cat /tmp/ts.log
pkill -f "[t]ouchsim.py"; rm -f /tmp/touchsim* /tmp/ts.log /tmp/to.log
