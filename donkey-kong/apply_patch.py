#!/usr/bin/env python3
"""apply_patch.py <original.gb> <dk-color.ips> <out.gb>  -- applies the IPS to your own ROM (any IPS patcher, e.g. Flips, works too)"""
import sys, hashlib
sys.path.insert(0, __import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), 'tools'))
import mkips
ORIG = '6ed661bd1d6d8cdd48e1c10f8ca4e8dcba49128e'
PATCHED = '0ee193e89a27109eb3335177cc22f894d37747e8'
o, p, out = sys.argv[1:4]
rom = open(o, 'rb').read()
if hashlib.sha1(rom).hexdigest() != ORIG:
    sys.exit('not the expected ROM: need Donkey Kong (Japan, USA) (En) (SGB Enhanced), SHA-1 ' + ORIG)
res = mkips.apply(rom, open(p, 'rb').read())
open(out, 'wb').write(res)
print('wrote', out, hashlib.sha1(res).hexdigest(), '(expected %s)' % PATCHED)
