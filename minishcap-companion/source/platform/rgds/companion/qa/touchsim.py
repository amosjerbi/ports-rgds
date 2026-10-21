#!/usr/bin/env python3
"""Virtual multitouch screen (uinput) for QA. Usage: touchsim.py "<seconds>,<x>,<y>;..." <total_seconds>
Creates a device that reports like the Goodix panel (ABS_MT_* 0..639 x 0..479, MT type B tracking ids),
writes its /dev/input node to /tmp/touchsim.node, then taps at the given times."""
import fcntl, os, struct, sys, time, glob

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045567
UI_DEV_CREATE, UI_DEV_DESTROY = 0x5501, 0x5502
EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
BTN_TOUCH = 0x14a
ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID = 0, 1, 0x2f, 0x35, 0x36, 0x39

fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
for ev in (EV_SYN, EV_KEY, EV_ABS):
    fcntl.ioctl(fd, UI_SET_EVBIT, ev)
fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH)
for a in (ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID):
    fcntl.ioctl(fd, UI_SET_ABSBIT, a)
absmax, absmin = [0] * 64, [0] * 64
absmax[ABS_X] = absmax[ABS_MT_POSITION_X] = 639
absmax[ABS_Y] = absmax[ABS_MT_POSITION_Y] = 479
absmax[ABS_MT_SLOT] = 9
absmax[ABS_MT_TRACKING_ID] = 65535
dev = struct.pack("80sHHHHi", b"tmc-test-touch", 0x03, 0x1234, 0x5678, 1, 0) + struct.pack("64i", *absmax) + \
      struct.pack("64i", *absmin) + struct.pack("64i", *([0] * 64)) + struct.pack("64i", *([0] * 64))
os.write(fd, dev)
fcntl.ioctl(fd, UI_DEV_CREATE)
time.sleep(1.0)
node = None
for d in glob.glob("/sys/class/input/event*/device/name"):
    if open(d).read().strip() == "tmc-test-touch":
        node = "/dev/input/" + d.split("/")[4]
open("/tmp/touchsim.node", "w").write(node or "")
print("touchsim device:", node, flush=True)

def emit(t, c, v):
    os.write(fd, struct.pack("llHHi", int(time.time()), 0, t, c, v))

taps = [tuple(float(p) for p in s.split(",")) for s in sys.argv[1].split(";") if s]
t0, tid = time.time(), 100
for when, x, y in sorted(taps):
    while time.time() - t0 < when:
        time.sleep(0.01)
    tid += 1
    emit(EV_ABS, ABS_MT_SLOT, 0); emit(EV_ABS, ABS_MT_TRACKING_ID, tid)
    emit(EV_ABS, ABS_MT_POSITION_X, int(x)); emit(EV_ABS, ABS_MT_POSITION_Y, int(y))
    emit(EV_KEY, BTN_TOUCH, 1); emit(EV_SYN, 0, 0)
    time.sleep(0.12)
    emit(EV_ABS, ABS_MT_TRACKING_ID, -1); emit(EV_KEY, BTN_TOUCH, 0); emit(EV_SYN, 0, 0)
    print(f"tap at t={when}s ({int(x)},{int(y)})", flush=True)
while time.time() - t0 < float(sys.argv[2]):
    time.sleep(0.2)
fcntl.ioctl(fd, UI_DEV_DESTROY)
