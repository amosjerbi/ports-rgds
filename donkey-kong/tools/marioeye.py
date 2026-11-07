"""marioeye.py rom_nt -> data/mario_eye.json: Mario head tiles whose eye pixel uses the hat colour (idx 3); found by playing random inputs"""
import sys,json,random; sys.path.insert(0,'tools')
from gsearch import *
rom=sys.argv[1]; random.seed(3)
s=parse(open('out/state3/orig.raw','rb').read()); found={}
def raw(m,t): a=0x8000+t*16; return bytes(m[a+k] for k in range(16))
def px(rb,r,c): return ((rb[2*r]>>(7-c))&1)|(((rb[2*r+1]>>(7-c))&1)<<1)
for step in (16,19,22,42,60,110):
    p=boot(rom,s); m=p.memory
    m[0xc82b]=s['wram'][0x82b]|0x80; m[0xda40]=0x9c
    for f in range(4000):
        if f==5: v=step-1; m[0xc85c]=v&255; m[0xc85d]=v>>8
        if f>=20 and f%40==0: p.button('a',8)
        p.tick()
        if (m[0xc85c]|m[0xc85d]<<8)==step and f>40 and m[0xff8f]!=0: break
    for _ in range(100): p.tick()
    for it in range(220):
        b=random.choice(['left','right','up','down','a','b'])
        p.button_press(b)
        for _ in range(random.randint(2,6)): p.tick()
        p.button_release(b)
        sp=[(m[0xfe00+i*4],m[0xfe00+i*4+1],m[0xfe00+i*4+2],m[0xfe00+i*4+3]) for i in range(40)]
        sp=[q for q in sp if q[0] and q[2]<0x50 and m[4,0xd700+q[2]]==0]   # class 0 = Mario palette
        if not sp: continue
        y0=min(q[0] for q in sp); x0=min(q[1] for q in sp); G={}
        tall=m[0xff40]&4
        for (y,x,t,fl) in sp:
            for half in ((0,1) if tall else (0,)):
                tt=(t&0xfe)+half if tall else t; rb=raw(m,tt)
                for r in range(8):
                    for c in range(8):
                        rr=7-r if fl&0x40 else r; cc=7-c if fl&0x20 else c
                        yy=(y-y0)+r+half*8; xx=(x-x0)+c
                        G[(yy,xx)]=(px(rb,rr,cc),rb.hex(),rr,cc)
        for (yy,xx),(v,h,rr,cc) in G.items():
            if v!=3 or not (2<=yy<=6): continue
            l=G.get((yy,xx-1),(0,))[0]; rgt=G.get((yy,xx+1),(0,))[0]
            if {l,rgt}=={1,2}: found[(h,rr,cc)]=1
print(len(found),'eye candidates')
json.dump([list(k) for k in found],open('data/mario_eye.json','w'))
