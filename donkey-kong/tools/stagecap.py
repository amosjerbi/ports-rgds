"""stagecap.py rom -> out/stagecap.npz : per play stage BG tile map (32x32 ids), tile pixel shades (256x8x8, displayed), scroll, LCDC"""
import sys,json; sys.path.insert(0,'tools')
import numpy as np
from gsearch import *
rom=sys.argv[1]
r=open('backup/dk.original.gb','rb').read(); b=0x0c*0x4000+(0x4f97-0x4000)
T=[(r[b+i*2],r[b+i*2+1]) for i in range(331)]
s=parse(open('out/state3/orig.raw','rb').read()); res={}
for i,(o,a) in enumerate(T):
    if o!=1: continue
    p=boot(rom,s); m=p.memory
    m[0xc82b]=s['wram'][0x82b]|0x80; m[0xda40]=0x9c
    for f in range(4000):
        if f==5: v=max(i-1,0); m[0xc85c]=v&255; m[0xc85d]=v>>8
        if f>=20 and f%40==0: p.button('a',8)
        p.tick()
        if (m[0xc85c]|m[0xc85d]<<8)==i and f>40 and m[0xff8f]!=0: break
    for _ in range(150): p.tick()
    lcdc=m[0xff40]; base=0x9c00 if lcdc&8 else 0x9800
    tm=np.array([[m[base+y*32+x] for x in range(32)] for y in range(32)],np.uint8)
    bgp=m[0xff47]; sh=[(bgp>>(2*k))&3 for k in range(4)]
    tiles=np.zeros((256,8,8),np.uint8)
    for t in range(256):
        a=(0x8000+t*16) if lcdc&16 else (0x9000+((t^0x80)-0x80)*16)
        for y in range(8):
            lo,hi=m[a+2*y],m[a+2*y+1]
            for x in range(8): tiles[t,y,x]=sh[((lo>>(7-x))&1)|(((hi>>(7-x))&1)<<1)]
    res['tm%d'%i]=tm; res['tl%d'%i]=tiles; res['sc%d'%i]=np.array([m[0xff43],m[0xff42],lcdc,m[0xff4a],m[0xff4b]])
    print(i,m[0xff43],m[0xff42],hex(lcdc),flush=True)
np.savez_compressed('out/stagecap.npz',**res)
