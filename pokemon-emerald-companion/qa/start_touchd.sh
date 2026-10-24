#!/bin/sh
# on device: (re)start the virtual touch screen and print its node
for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in "python3 /tmp/touchd.py"*) kill ${p#/proc/};; esac; done 2>/dev/null
sleep 1
setsid nohup python3 /tmp/touchd.py >/tmp/touchd.log 2>&1 </dev/null &
sleep 2.5; cat /tmp/touchd.log
