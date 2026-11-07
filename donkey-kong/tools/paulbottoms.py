"""bottom halves of Pauline's head sprites (stage 24) -> data/pauline_tiles.json (patched in the ROM: dress pixels idx2 -> idx3)"""
import sys,json; sys.path.insert(0,'tools')
from gsearch import *
s=parse(open('out/state3/orig.raw','rb').read()); p=boot(sys.argv[1],s); m=p.memory
m[0xc82b]=s['wram'][0x82b]|0x80; m[0xda40]=0x9c; step=24
for f in range(4000):
    if f==5: v=step-1; m[0xc85c]=v&255; m[0xc85d]=v>>8
    if f>=20 and f%40==0: p.button('a',8)
    p.tick()
    if (m[0xc85c]|m[0xc85d]<<8)==step and f>40 and m[0xff8f]!=0: break
out=set()
for f in range(600):
    p.tick()
    for i in range(40):
        t=m[0xfe00+i*4+2]
        if m[0xfe00+i*4] and t in (0xa6,0xa8,0xb2,0xb4):
            a=0x8000+(t|1)*16; out.add(bytes(m[a+k] for k in range(16)).hex())
json.dump(sorted(out),open('data/pauline_tiles.json','w')); print(len(out),'bottom tiles')
