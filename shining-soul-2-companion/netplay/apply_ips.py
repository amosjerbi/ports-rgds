#!/usr/bin/env python3
"""apply_ips.py ROM.gba|ROM.zip patch.ips out.gba  -- applies an IPS patch (no ROM data is shipped with the patch)."""
import sys, zipfile
src, ips, out = sys.argv[1:4]
if src.lower().endswith('.zip'):
    z = zipfile.ZipFile(src)
    name = [n for n in z.namelist() if n.lower().endswith('.gba')][0]
    rom = bytearray(z.read(name))
else:
    rom = bytearray(open(src, 'rb').read())
p = open(ips, 'rb').read()
assert p[:5] == b'PATCH'
i = 5
while p[i:i + 3] != b'EOF':
    off = int.from_bytes(p[i:i + 3], 'big'); n = int.from_bytes(p[i + 3:i + 5], 'big'); i += 5
    if n:
        rom[off:off + n] = p[i:i + n]; i += n
    else:
        n = int.from_bytes(p[i:i + 2], 'big'); rom[off:off + n] = bytes([p[i + 2]]) * n; i += 3
open(out, 'wb').write(rom)
print('wrote', out, len(rom))
