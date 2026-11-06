#!/bin/bash
# Zelda: A Link Between Worlds with Azahar on both screens (touch on the lower one); Home returns to the launcher.
# A port gets only the top panel, so this hands the game to /data/rgds/azahar/run3ds.sh, which runs outside the
# port session (init's mount namespace and the top cgroup): it pauses the launcher, plays, and brings it back.
ROM="/data/roms/3ds/Legend of Zelda, The - A Link Between Worlds (USA) (En,Fr,Es).3ds"
python3 - "$ROM" <<'PY'
import ctypes, os, sys
rom = sys.argv[1]
libc = ctypes.CDLL(None, use_errno=True)
# Before this script ends (the port runner then ends everything in the port's cgroup): out of the port's cgroup
# leaf (mounted only while ports run) and into init's mount namespace, where run3ds.sh pauses the launcher.
try:
    with open("/run/rgds-cgroup/cgroup.procs", "w") as procs: procs.write(str(os.getpid()))
except OSError:
    pass
fd = os.open("/proc/1/ns/mnt", os.O_RDONLY)
if libc.setns(fd, 0x00020000) != 0: sys.exit("setns failed")  # CLONE_NEWNS
os.close(fd)
if os.fork(): os._exit(0)
os.setsid()
os.chdir("/")
if os.fork(): os._exit(0)
log = os.open("/data/rgds/azahar/port-launch.log", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
os.dup2(log, 1); os.dup2(log, 2)
null = os.open("/dev/null", os.O_RDONLY); os.dup2(null, 0)
# a clean environment (no port lease shim); a moment for the port session to close first
os.execve("/bin/sh", ["sh", "-c", 'sleep 3; exec /data/rgds/azahar/run3ds.sh "$0"', rom],
          {"PATH": "/usr/sbin:/usr/bin:/sbin:/bin"})
PY
