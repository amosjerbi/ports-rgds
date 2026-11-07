"""paulscan.py rom -> out/pauline.json: per stage, OAM tile ids whose VRAM data equals Pauline's tiles of stage 24 (head / body / bubble)"""
import sys,json; sys.path.insert(0,'tools')
from gsearch import *
rom=sys.argv[1]
r=open('backup/dk.original.gb','rb').read(); b=0x0c*0x4000+(0x4f97-0x4000)
T=[(r[b+i*2],r[b+i*2+1]) for i in range(331)]
s=parse(open('out/state3/orig.raw','rb').read())
def boot_to(step):
    p=boot(rom,s); m=p.memory
    m[0xc82b]=s['wram'][0x82b]|0x80; m[0xda40]=0x9c
    for f in range(4000):
        if f==5: v=step-1; m[0xc85c]=v&255; m[0xc85d]=v>>8
        if f>=20 and f%40==0: p.button('a',8)
        p.tick()
        if (m[0xc85c]|m[0xc85d]<<8)==step and f>40 and m[0xff8f]!=0: break
    return p,m
def raw(m,t,half): a=0x8000+((t&0xfe)+half)*16; return bytes(m[a+k] for k in range(16))
p,m=boot_to(24)
ref={}   # (top,bottom) raw -> role
roles={0xa6:'head',0xa8:'head',0xb2:'head',0xb4:'head',0xaa:'body',0xac:'body',0xae:'body',0xb0:'body',0xb6:'body',0xb8:'body',0xba:'bubble',0xbc:'bubble'}
for f in range(600):
    p.tick()
    for i in range(40):
        y,x,t=m[0xfe00+i*4],m[0xfe00+i*4+1],m[0xfe00+i*4+2]
        if y and t in roles: ref[(raw(m,t,0),raw(m,t,1))]=roles[t]
print(len(ref),'reference tiles')
res={}
for i,(o,a) in enumerate(T):
    if o!=1: continue
    p,m=boot_to(i); found={}
    for f in range(600):
        p.tick()
        if f%2: continue
        for k in range(40):
            y,x,t=m[0xfe00+k*4],m[0xfe00+k*4+1],m[0xfe00+k*4+2]
            if y and y<160:
                key=(raw(m,t,0),raw(m,t,1))
                if key in ref: found[t]=ref[key]
    if found: res[i]={hex(t):v for t,v in found.items()}; print(i,res[i],flush=True)
json.dump(res,open('out/pauline.json','w'))
