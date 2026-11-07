#!/usr/bin/env python3
"""Build the CGB-colour version of Donkey Kong (GB) from your own ROM + write the IPS.
usage: dkbuild.py <original.gb> <out.gb> [out.ips]      (needs out/sgb/trn_*.bin, see tools/sgbdump.py)"""
import sys, os, hashlib
HERE=os.path.dirname(os.path.abspath(__file__)); ROOT=os.path.dirname(HERE)
sys.path.insert(0,HERE)
import gbasm, dkdata, mkips
ORIG_SHA1='6ed661bd1d6d8cdd48e1c10f8ca4e8dcba49128e'
def build(orig,out,dump=os.path.join(ROOT,'out','sgb')):
    rom=bytearray(open(orig,'rb').read())
    assert hashlib.sha1(rom).hexdigest()==ORIG_SHA1,'not the expected ROM (Donkey Kong (Japan, USA) (En) (SGB Enhanced))'
    P,A,pk,recs,used=dkdata.build(orig,dump)
    sec,sym=dkdata.layout(recs,used,A,P)
    assert sym['PKTTAB']==(0x0d,0x6400)
    import json, struct as _st
    ov=json.load(open(os.path.join(ROOT,'data','ovr12.json')))
    def c15(c): r,g,bl=[min(31,max(0,int(round(v*31/255)))) for v in c]; return r|g<<5|bl<<10
    blob=bytearray()
    for pal in ov['bg']:
        for c in pal: blob+=_st.pack('<H',c15(c))
    for pal in ov['obj']:
        for c in pal: blob+=_st.pack('<H',c15(c))
    for row in ov['map']: blob+=bytes(row)
    blob+=bytes(ov['cls']); assert len(blob)==64+64+360+256
    o_org,o_data=sec[0x0d]; ovrd=o_org+len(o_data); sec[0x0d]=(o_org,o_data+bytes(blob)); assert ovrd+len(blob)<=0x7c00
    ov2=json.load(open(os.path.join(ROOT,'data','ovr19.json'))); blob2=bytearray()
    for row in ov2['map']: blob2+=bytes(row)
    blob2+=bytes(ov2['cls']); assert len(blob2)==640+256 and 0x7002+len(blob2)<=0x8000
    ov3=json.load(open(os.path.join(ROOT,'data','ovr42.json'))); blob3=bytearray()
    for pal in ov3['bg']:
        for c in pal: blob3+=_st.pack('<H',c15(c))
    for pal in ov3['obj']:
        for c in pal: blob3+=_st.pack('<H',c15(c))
    for row in ov3['map']: blob3+=bytes(row)
    blob3+=bytes(ov3['cls']); assert len(blob3)==128+360+256
    ovdc=0x7002+len(blob2); assert ovdc+len(blob3)<=0x8000
    th=json.load(open(os.path.join(ROOT,'data','themes.json'))); blob4=bytearray()
    for t in th['themes']:
        for pal in t['bg']:
            for c in pal: blob4+=_st.pack('<H',c15(c))
        for pal in t['obj']:
            for c in pal: blob4+=_st.pack('<H',c15(c))
    assert len(blob4)==9*128
    ovdt=ovdc+len(blob3); blob4+=bytes(th['cls']); ovdg=ovdt+9*128; assert ovdt+len(blob4)<=0x8000
    assert 0x0c not in sec; sec[0x0c]=(0x7002,bytes(blob2)+bytes(blob3)+bytes(blob4))
    a=gbasm.Asm(); a.sym['DPAL']=sym['DPAL'][1]; a.sym['ATFTAB']=sym['ATFTAB'][1]; a.sym['OVRD']=ovrd; a.sym['OVRD2']=0x7002; a.sym['OVRDC']=0x7002+896; a.sym['OVRDT']=0x7002+896+744; a.sym['OVRDG']=0x7002+896+744+9*128
    a.include(os.path.join(ROOT,'color','hooks.s')); code=a.link(); S=a.sym
    json.dump({k:v for k,v in S.items() if isinstance(v,int)},open(os.path.join(ROOT,'out','syms.json'),'w'))
    lo,hi=min(code),max(k for k in code if k<0x4000)
    for k,v in code.items():
        assert rom[k]==0xff,('hook code over used bytes',hex(k)); rom[k]=v
    print('hook code $%04x-$%04x (%d bytes of 1780 free)'%(lo,hi,hi-lo+1))
    for bank,(org,data) in sec.items():
        o=bank*0x4000+org-0x4000
        assert all(x==0xff for x in rom[o:o+len(data)]),('data over used bytes',bank)
        rom[o:o+len(data)]=data
        print('data bank $%02x $%04x-$%04x (%d bytes)'%(bank,org,org+len(data)-1,len(data)))
    def patch(addr,expect,new):
        e=bytes.fromhex(expect)
        assert bytes(rom[addr:addr+len(e)])==e,(hex(addr),bytes(rom[addr:addr+len(e)]).hex())
        rom[addr:addr+len(new)]=new
    jp=lambda t:bytes([0xc3,t&0xff,t>>8]); call=lambda t:bytes([0xcd,t&0xff,t>>8])
    patch(0x0040,'c37002',jp(S['W40']))                      # interrupt vectors -> SVBK-safe wrappers
    patch(0x0048,'c3ad02',jp(S['W48']))
    patch(0x0050,'c3c402',jp(S['W50']))
    patch(0x0162,'cd1837',call(S['H_INIT']))                 # SGB detection -> our start-up
    patch(0x0176,'cda401',call(S['H_MAIN']))                 # state machine -> + per-frame colour work
    if os.environ.get('DMA'): patch(0x0281,'cd80ff',call(S['H_DMA']))                  # VBlank OAM DMA -> colour the sprites first
    patch(0x0284,'cde216',call(S['H_VB2']))                  # VBlank VRAM queue -> + palette / attribute upload
    patch(0x3894,'fa2bc8',jp(S['H_SGB2']))                    # RAM-built packet sender (palette sets for gameplay / title shimmer)
    patch(0x3872,'47fa2bc8cb7fc8',jp(S['H_SGB'])+bytes(4))   # SGB packet sender -> queue packet for the colour engine
    rom[0x143]=0x80                                          # CGB-compatible
    c=0
    for i in range(0x134,0x14d): c=(c-rom[i]-1)&0xff
    rom[0x14d]=c
    g=(sum(rom)-rom[0x14e]-rom[0x14f])&0xffff; rom[0x14e]=g>>8; rom[0x14f]=g&0xff
    open(out,'wb').write(rom)
    print('built',out,hashlib.sha1(rom).hexdigest())
    return bytes(rom)
if __name__=='__main__':
    o=sys.argv[1]; out=sys.argv[2]
    rom=build(o,out)
    if len(sys.argv)>3:
        ips=mkips.make(open(o,'rb').read(),rom); open(sys.argv[3],'wb').write(ips)
        assert mkips.apply(open(o,'rb').read(),ips)==rom
        print('ips',sys.argv[3],len(ips),'bytes (apply verified)')
