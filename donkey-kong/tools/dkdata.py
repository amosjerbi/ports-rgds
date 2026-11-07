#!/usr/bin/env python3
"""Turn the game's own SGB packet list (bank $1E table) into compact CGB colour commands (own format).
Needs out/sgb/trn_10.bin + trn_11.bin (from sgbdump.py: PAL_TRN / ATTR_TRN RAM as the game sends them).
Record ops (bank $0D data):  00 end | 01 +32B: 4 palettes (BGR555 LE) | 02 +ptr16: 20x18 attr map (360 B) |
                             03 x1 y1 x2 y2 pal: fill rect | 04 n, n*(x,y,pal): single cells"""
import struct, sys, os
HERE=os.path.dirname(os.path.abspath(__file__)); ROOT=os.path.dirname(HERE)
def load(rom, dumpdir):
    r=open(rom,'rb').read()
    pal=open(dumpdir+'/trn_10.bin','rb').read(); atf=open(dumpdir+'/trn_11.bin','rb').read()
    P=[struct.unpack_from('<4H',pal,i*8) for i in range(512)]
    A=[]
    for n in range(45):
        b=atf[n*90:(n+1)*90]; cells=bytearray()
        for y in range(18):
            for x in range(20):
                cells.append((b[y*5+x//4]>>(6-2*(x%4)))&3)
        A.append(bytes(cells))
    B=0x1e*0x4000
    ptrs=[struct.unpack_from('<H',r,B+i*2)[0] for i in range(0x100)]
    n=(min(p for p in ptrs if 0x4000<=p<0x8000)-0x4000)//2
    pk=[]
    for i in range(n):
        o=B+ptrs[i]-0x4000; cnt=r[o]&7
        pk.append([r[o+16*k:o+16*k+16] for k in range(cnt)])
    return P,A,pk
def rects_for(x1,y1,x2,y2,ctrl,pin,pbd,pout):
    """ATTR_BLK data set -> list of fill rects (inclusive), drawn in order outside, inside, border."""
    R=[]
    if ctrl&4:
        pc=pout
        if y1>0: R.append((0,0,19,y1-1,pc))
        if y2<17: R.append((0,y2+1,19,17,pc))
        if x1>0: R.append((0,y1,x1-1,y2,pc))
        if x2<19: R.append((x2+1,y1,19,y2,pc))
    if ctrl&1 and x2>=x1 and y2>=y1: R.append((x1,y1,x2,y2,pin))
    if ctrl&2 or ctrl&1:       # border takes its own palette, or the inside palette when only bit0 is set
        pb=pbd if ctrl&2 else pin
        R+= [(x1,y1,x2,y1,pb),(x1,y2,x2,y2,pb),(x1,y1,x1,y2,pb),(x2,y1,x2,y2,pb)]
    return R
def build(rom,dumpdir):
    P,A,pk=load(rom,dumpdir)
    recs={}; used=[]
    def atf_ref(n):
        if n not in used: used.append(n)
        return n
    for i,pkts in enumerate(pk):
        h=pkts[0]; cmd=h[0]>>3; ops=[]
        if cmd==10:                       # PAL_SET
            ids=[h[1]|h[2]<<8,h[3]|h[4]<<8,h[5]|h[6]<<8,h[7]|h[8]<<8]
            ops.append(('pal',[P[j&0x1ff] for j in ids]))
            if h[9]&0x80: ops.append(('atf',atf_ref(h[9]&0x3f)))
        elif cmd==22:                     # ATTR_SET
            ops.append(('atf',atf_ref(h[1]&0x3f)))
        elif cmd==4:                      # ATTR_BLK
            cnt=h[1]; data=h[2:]+b''.join(p for p in pkts[1:])
            for s in range(cnt):
                ctrl,pals,x1,y1,x2,y2=data[s*6:s*6+6]
                for (a,b,c,d,pp) in rects_for(x1,y1,x2,y2,ctrl,pals&3,(pals>>2)&3,(pals>>4)&3): ops.append(('rect',a,b,c,d,pp))
        elif cmd==7:                      # ATTR_CHR
            ncell=h[3]|h[4]<<8; x=h[1]; y=h[2]; vert=h[5]&1; data=h[6:]+b''.join(p for p in pkts[1:]); cells=[]
            for k in range(ncell):
                pl=(data[k//4]>>(6-2*(k%4)))&3; cells.append((x,y,pl))
                if vert: y+=1
                else: x+=1
                if x>19: x=0; y+=1
                if y>17: y=0
            ops.append(('cells',cells))
        if ops: recs[i]=ops
    return P,A,pk,recs,used
SECTIONS=[(0x0d,0x6400,0x8000),(0x12,0x6300,0x8000),(0x19,0x6800,0x8000)]   # free ROM tails (verified 0xff): bank,start,end
def rec_bytes(ops,atfloc):
    b=bytearray()
    for op in ops:
        if op[0]=='pal':
            b+=bytes([1])
            for p in op[1]:
                for c in p: b+=struct.pack('<H',c)
        elif op[0]=='atf':
            bank,addr=atfloc.get(op[1],(0,0)); b+=bytes([2,bank])+struct.pack('<H',addr)
        elif op[0]=='rect': b+=bytes([3])+bytes(op[1:])
        elif op[0]=='cells':
            b+=bytes([4,len(op[1])])
            for c in op[1]: b+=bytes(c)
    b+=bytes([0]); return b
DYN_IDS=list(range(0x2c,0x40))+list(range(0x180,0x1a0))+[0xe4,0xe5,0xe6]   # palette ids the game can put into RAM-built PAL_SET packets
def layout(recs,used,A,P=None):
    """lay data out into the free ROM tails. returns {bank:(org,bytes)}, symbols{name:(bank,addr)}"""
    org={b:o for b,o,e in SECTIONS}; end={b:e for b,o,e in SECTIONS}
    buf={b:bytearray() for b in org}
    dpal_size=len(DYN_IDS)*10+2; atftab_size=45*3
    rsize=0x70*2+sum(len(rec_bytes(o,{})) for o in recs.values())+dpal_size+atftab_size
    buf[0x0d]+=bytes(rsize)
    atfloc={}; seen={}
    for n in range(45):                      # every attribute file (dynamic packets may pick any)
        key=A[n]
        if key in seen: atfloc[n]=seen[key]; continue
        for b in (0x12,0x19,0x0d):
            if org[b]+len(buf[b])+360<=end[b]: break
        else: raise SystemExit('no room for ATF %d'%n)
        addr=org[b]+len(buf[b]); buf[b]+=key; seen[key]=(b,addr); atfloc[n]=(b,addr)
    ptr={}; rb=bytearray(); base=org[0x0d]+0x70*2
    for i,ops in sorted(recs.items()): ptr[i]=base+len(rb); rb+=rec_bytes(ops,atfloc)
    tab=b''.join(struct.pack('<H',ptr.get(i,0)) for i in range(0x70))
    dp=bytearray()
    for i in DYN_IDS:
        dp+=struct.pack('<H',i)
        for c in P[i]: dp+=struct.pack('<H',c)
    dp+=b'\xff\xff'
    at=bytearray()
    for n in range(45):
        b,a=atfloc[n]; at+=bytes([b])+struct.pack('<H',a)
    dpal_addr=org[0x0d]+len(tab)+len(rb); atftab_addr=dpal_addr+len(dp)
    blob=tab+bytes(rb)+bytes(dp)+bytes(at); assert len(blob)==rsize,(len(blob),rsize)
    buf[0x0d][0:rsize]=blob
    return {b:(org[b],bytes(buf[b])) for b in org},{'PKTTAB':(0x0d,org[0x0d]),'DPAL':(0x0d,dpal_addr),'ATFTAB':(0x0d,atftab_addr)}
if __name__=='__main__':
    rom=sys.argv[1]; dd=sys.argv[2]
    P,A,pk,recs,used=build(rom,dd)
    print('records',len(recs),'atf used',sorted(used),len(used))
    for i,ops in sorted(recs.items()): print('%02x'%i,[(o[0],(o[1] if o[0]=='atf' else len(o[1]) if o[0] in('pal','cells') else o[1:])) for o in ops])
