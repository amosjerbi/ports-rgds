#!/usr/bin/env python3
"""Virtual multitouch screen (uinput) driven through /tmp/touch.fifo, for QA on the device.
Reports like the Goodix panel (ABS_MT_* 0..639 x 0..479). Writes its /dev/input node to /tmp/touchd.node.
Commands (one per line, several per line allowed, space separated):
  tap:X,Y            tap
  drag:X1,Y1:X2,Y2   swipe
  select             press the SELECT button
  w<seconds>         wait
  quit"""
import fcntl, glob, os, struct, sys, time

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045567
UI_DEV_CREATE, UI_DEV_DESTROY = 0x5501, 0x5502
EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
BTN_TOUCH = 0x14a
BTN_SELECT = 0x13a
ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID = 0, 1, 0x2f, 0x35, 0x36, 0x39

fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
for ev in (EV_SYN, EV_KEY, EV_ABS):
    fcntl.ioctl(fd, UI_SET_EVBIT, ev)
fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH)
fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_SELECT)
for a in (ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID):
    fcntl.ioctl(fd, UI_SET_ABSBIT, a)
absmax, absmin = [0] * 64, [0] * 64
absmax[ABS_X] = absmax[ABS_MT_POSITION_X] = 639
absmax[ABS_Y] = absmax[ABS_MT_POSITION_Y] = 479
absmax[ABS_MT_SLOT] = 9
absmax[ABS_MT_TRACKING_ID] = 65535
dev = struct.pack("80sHHHHi", b"aos-test-touch", 0x03, 0x1234, 0x5678, 1, 0) + struct.pack("64i", *absmax) + \
      struct.pack("64i", *absmin) + struct.pack("64i", *([0] * 64)) + struct.pack("64i", *([0] * 64))
os.write(fd, dev)
fcntl.ioctl(fd, UI_DEV_CREATE)
time.sleep(1.0)
node = ""
for d in glob.glob("/sys/class/input/event*/device/name"):
    if open(d).read().strip() == "aos-test-touch":
        node = "/dev/input/" + d.split("/")[4]
open("/tmp/touchd.node", "w").write(node)
print("touchd device:", node, flush=True)

def emit(t, c, v):
    os.write(fd, struct.pack("llHHi", int(time.time()), 0, t, c, v))

tid = 100
def down(x, y):
    global tid
    tid += 1
    emit(EV_ABS, ABS_MT_SLOT, 0); emit(EV_ABS, ABS_MT_TRACKING_ID, tid)
    emit(EV_ABS, ABS_MT_POSITION_X, int(x)); emit(EV_ABS, ABS_MT_POSITION_Y, int(y))
    emit(EV_KEY, BTN_TOUCH, 1); emit(EV_SYN, 0, 0)
def move(x, y):
    emit(EV_ABS, ABS_MT_POSITION_X, int(x)); emit(EV_ABS, ABS_MT_POSITION_Y, int(y)); emit(EV_SYN, 0, 0)
def up():
    emit(EV_ABS, ABS_MT_TRACKING_ID, -1); emit(EV_KEY, BTN_TOUCH, 0); emit(EV_SYN, 0, 0)

if os.path.exists("/tmp/touch.fifo"):
    os.remove("/tmp/touch.fifo")
os.mkfifo("/tmp/touch.fifo")
while True:
    with open("/tmp/touch.fifo") as f:
        for line in f:
            for tok in line.split():
                if tok == "quit":
                    fcntl.ioctl(fd, UI_DEV_DESTROY); sys.exit()
                if tok[0] == "w":
                    time.sleep(float(tok[1:])); continue
                if tok == "select":
                    emit(EV_KEY, BTN_SELECT, 1); emit(EV_SYN, 0, 0); time.sleep(0.12); emit(EV_KEY, BTN_SELECT, 0); emit(EV_SYN, 0, 0); time.sleep(0.25)
                elif tok.startswith("tap:"):
                    x, y = tok[4:].split(","); down(x, y); time.sleep(0.12); up(); time.sleep(0.25)
                elif tok.startswith("drag:"):
                    a, b = tok[5:].split(":"); x1, y1 = map(int, a.split(",")); x2, y2 = map(int, b.split(","))
                    down(x1, y1)
                    for i in range(1, 11):
                        time.sleep(0.03); move(x1 + (x2 - x1) * i / 10, y1 + (y2 - y1) * i / 10)
                    time.sleep(0.05); up(); time.sleep(0.25)
