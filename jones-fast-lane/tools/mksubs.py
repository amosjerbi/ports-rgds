#!/usr/bin/env python3
"""Build subtitles for the CD release's speech from the floppy release's text.

The CD release dropped the on-screen dialogue text. Its speech clips are
numbered in runs, one run per text module of the floppy release, in the same
order as that module's spoken lines (pure format strings such as "%s$%d" are
not spoken). Usage:
    mksubs.py <textdump> <floppy gamedir> <audio001.map> <out.bin>
Output: u16 count, then count x (u16 clip, u16 offset), then NUL-terminated
strings (offsets from the start of the string area)."""
import re, struct, subprocess, sys

textdump, gamedir, amap, out = sys.argv[1:5]

# text module -> first clip of its run (checked against clip durations)
RUNS = {209: 10, 205: 50, 212: 70, 208: 100, 201: 160, 232: 230, 204: 300,
        211: 330, 207: 380, 206: 420, 215: 460, 203: 530, 210: 610}

m = open(amap, 'rb').read()
clips = set()
for i in range(0, len(m) - 9, 10):
    n = struct.unpack_from('<H', m, i)[0]
    if n != 0xffff:
        clips.add(n)

def spoken(t):
    if t in ('DUMMY', 'Filler', ''):
        return False
    letters = len(re.sub(r'[^A-Za-z]', '', re.sub(r'%[-0-9.]*[a-z]', '', t)))
    return letters >= 3 and not ('%' in t and letters <= 8)

def tidy(t):
    t = t.replace('\\n', ' ')
    t = re.sub(r'%[-0-9.]*[a-z]', '..', t)
    return re.sub(r'\s+', ' ', t).strip()

subs = {}
for mod, base in RUNS.items():
    lines = subprocess.run([textdump, gamedir, str(mod)], capture_output=True,
                           text=True, encoding='latin1').stdout.split('\n')[:-1]
    clip = base
    for t in lines:
        if not spoken(t):
            continue
        if clip in clips:
            subs[clip] = tidy(t)
        clip += 1

items = sorted(subs.items())
strings = b''
table = b''
for clip, t in items:
    table += struct.pack('<HH', clip, len(strings))
    strings += t.encode('latin1') + b'\0'
open(out, 'wb').write(struct.pack('<H', len(items)) + table + strings)
print(f'{out}: {len(items)} subtitles')
