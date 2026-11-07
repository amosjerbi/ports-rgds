#!/usr/bin/env python3
"""Run the ROM in PyBoy with SGB-detect forced, log every SGB packet send ($3872) and dump PAL_TRN / ATTR_TRN VRAM data.
usage: sgbdump.py rom outdir"""
import sys,os,json
from pyboy import PyBoy
rom,out=sys.argv[1],sys.argv[2]; os.makedirs(out,exist_ok=True)
d=bytearray(open(rom,'rb').read()); assert d[0x3724]==0xfe and d[0x3725]==0xff; d[0x3725]=0x00
open(out+'/forced.gb','wb').write(d)
p=PyBoy(out+'/forced.gb',window='null',sound_emulated=False); p.set_emulation_speed(0)
log=[]
def hk(ctx):
    a=p.register_file.A
    log.append((p.frame_count,a))
    if a in (0x10,0x11):
        v=bytes(p.memory[0x8800:0x9800]); open(out+'/trn_%02x.bin'%a,'wb').write(v)
p.hook_register(0,0x3872,hk,None)
for i in range(1200): p.tick()
print(log[:60]); print(len(log))
p.screen.image.save(out+'/screen.png')
