#!/usr/bin/env python3
"""Small SM83 assembler for this project's source tree.
Syntax: lowercase mnemonics, `label:`, `.local` labels, `name = expr`, db/dw/ds,
`.bank N` (N=0 -> $0000, else $4000), `.org ADDR`, `.include file`.
Output is a flat ROM image (bank N, address A -> N*0x4000 + A - 0x4000, bank 0 -> A)."""
import re, os, sys
R8=['b','c','d','e','h','l','(hl)','a']; R16=['bc','de','hl','sp']; R16S=['bc','de','hl','af']
CC=['nz','z','nc','c']; ALU=['add a,','adc a,','sub ','sbc a,','and ','xor ','or ','cp ']
def _text(op):
    """canonical text of opcode `op` with placeholders N / NN / REL; returns (text,kind,len)"""
    if op==0xCB: return None
    lo=op&15; hi=op>>4; y=(op>>3)&7; z=op&7
    if op<0x40:
        if op==0: return 'nop','none',1
        if op==0x10: return 'stop','none',1   # one byte; the following padding byte is written explicitly (db $00)
        if op==0x08: return 'ld (NN),sp','n16',3
        if op==0x18: return 'jr REL','rel',2
        if op in(0x20,0x28,0x30,0x38): return 'jr %s,REL'%CC[y-4],'rel',2
        if lo==1: return 'ld %s,NN'%R16[hi],'n16',3
        if lo==9: return 'add hl,'+R16[hi],'none',1
        if lo==2: return 'ld (%s),a'%['bc','de','hl+','hl-'][hi],'none',1
        if lo==0xA: return 'ld a,(%s)'%['bc','de','hl+','hl-'][hi],'none',1
        if lo==3: return 'inc '+R16[hi],'none',1
        if lo==0xB: return 'dec '+R16[hi],'none',1
        if lo in(4,0xC): return 'inc '+R8[y],'none',1
        if lo in(5,0xD): return 'dec '+R8[y],'none',1
        if lo in(6,0xE): return 'ld %s,N'%R8[y],'n8',2
        return ['rlca','rrca','rla','rra','daa','cpl','scf','ccf'][y],'none',1
    if op<0x80:
        if op==0x76: return 'halt','none',1
        return 'ld %s,%s'%(R8[y],R8[z]),'none',1
    if op<0xC0: return ALU[y]+R8[z],'none',1
    fixed={0xC0:'ret nz',0xC8:'ret z',0xD0:'ret nc',0xD8:'ret c',0xC9:'ret',0xD9:'reti',0xE9:'jp hl',0xF3:'di',0xFB:'ei',0xF9:'ld sp,hl',
           0xE2:'ld ($ff00+c),a',0xF2:'ld a,($ff00+c)'}
    if op in fixed: return fixed[op],'none',1
    if z==1: return 'pop '+R16S[(op>>4)&3],'none',1
    if z==5 and op&8==0: return 'push '+R16S[(op>>4)&3],'none',1
    if z==7: return 'rst $%02x'%(y*8),'none',1
    if op in(0xC2,0xCA,0xD2,0xDA): return 'jp %s,NN'%CC[y],'n16',3
    if op==0xC3: return 'jp NN','n16',3
    if op in(0xC4,0xCC,0xD4,0xDC): return 'call %s,NN'%CC[y],'n16',3
    if op==0xCD: return 'call NN','n16',3
    if op==0xE0: return 'ldh (N),a','n8',2
    if op==0xF0: return 'ldh a,(N)','n8',2
    if op==0xEA: return 'ld (NN),a','n16',3
    if op==0xFA: return 'ld a,(NN)','n16',3
    if op==0xE8: return 'add sp,N','n8',2
    if op==0xF8: return 'ld hl,sp+N','n8',2
    if z==6: return ALU[y]+'N','n8',2
    return None
EXACT={}; PAT={}
def _init():
    for op in range(256):
        t=_text(op)
        if not t: continue
        text,kind,ln=t
        if kind=='none': EXACT[text]=(bytes([op]),ln); continue
        pat=re.escape(text)
        for ph in ('REL','NN','N'):
            if ph in text: pat=pat.replace(ph,'(?P<v>.+)',1); break
        if kind=='rst': pass
        PAT.setdefault(text.split(' ')[0],[]).append((re.compile(pat),bytes([op]),kind))
    for x in range(256):
        names=['rlc','rrc','rl','rr','sla','sra','swap','srl']
        t=names[x>>3]+' '+R8[x&7] if x<0x40 else ['','bit','res','set'][x>>6]+' %d,%s'%((x>>3)&7,R8[x&7])
        EXACT[t]=(bytes([0xCB,x]),2)
    for p in PAT.values(): p.sort(key=lambda e:-len(e[0].pattern))
_init()
class Asm:
    def __init__(s): s.out={}; s.sym={}; s.fix=[]; s.pc=0; s.base=0; s.scope=''; s.bankno=0; s.files=[]
    def set_bank(s,n):
        s.bankno=n; s.pc=0 if n==0 else 0x4000; s.base=0 if n==0 else n*0x4000-0x4000
    def q(s,e): return re.sub(r'(?<![\w$])\.([A-Za-z_]\w*)',lambda m:s.scope+'.'+m.group(1),e)
    def label(s,n):
        if n.startswith('.'): n=s.scope+n
        else: s.scope=n
        assert n not in s.sym,('dup label',n); s.sym[n]=s.pc
    def emit(s,bs):
        for b in bs:
            o=s.pc+s.base
            assert o not in s.out,('overlap at %x (bank %d pc %04x)'%(o,s.bankno,s.pc))
            s.out[o]=b; s.pc+=1
    def fixup(s,e,rel,bits):
        s.fix.append((s.pc,s.pc+s.base,s.q(e),rel,bits))
    def include(s,path):
        for l in open(path): s.line(l.rstrip('\n'),path)
    def line(s,l,src=''):
        m=re.search(r';@([0-9a-f]{4})\s*$',l)
        if m and re.sub(r';.*$','',l).strip():
            assert s.pc==int(m.group(1),16),('address drift: expected %s got %04x (%s)'%(m.group(1),s.pc,l.strip()))
        l=l.split(';')[0].strip()
        if not l: return
        if l.startswith('.push'):
            s.stk=getattr(s,'stk',[]); s.stk.append(s.pc); s.pc=int(s.ev(l.split()[1])); return
        if l=='.pop':
            s.pc=s.stk.pop(); return
        if l.startswith('.bank'): s.set_bank(int(l.split()[1],0)); return
        if l.startswith('.org'): s.pc=int(l.split()[1].replace('$','0x'),0); return
        if l.startswith('.include'):
            f=l.split(None,1)[1].strip().strip('"'); s.include(os.path.join(os.path.dirname(src) or '.',f)); return
        m=re.match(r'^([A-Za-z_.][\w.]*)\s*=\s*(.+)$',l)
        if m: s.sym[m.group(1)]=s.ev(m.group(2)); return
        m=re.match(r'^([A-Za-z_.][\w.]*):\s*(.*)$',l)
        if m: s.label(m.group(1)); l=m.group(2).strip()
        if not l: return
        if l.startswith('db '):
            for x in re.split(r',\s*',l[3:].strip()): s.fixup(x.strip(),False,8); s.emit(b'\0')
            return
        if l.startswith('dw '):
            for x in re.split(r',\s*',l[3:].strip()): s.fixup(x.strip(),False,16); s.emit(b'\0\0')
            return
        if l.startswith('ds '):
            n,_,v=l[3:].partition(','); s.emit(bytes([int(v.strip().replace('$','0x'),0) if v else 0])*int(n.strip().replace('$','0x'),0)); return
        mn,_,ops=re.sub(r'\s+',' ',l).partition(' ')
        text=(mn+' '+ops.replace(' ','')).strip()
        text={'ldh ($ff00+c),a':'ld ($ff00+c),a','ldh a,($ff00+c)':'ld a,($ff00+c)'}.get(text,text)
        if text in EXACT:
            b,ln=EXACT[text]
            s.emit(b); return
        m=re.fullmatch(r'rst (\S+)',text)
        if m:
            v=s.ev(m.group(1)); s.emit(bytes([0xC7|v])); return
        for rx,pre,kind in PAT.get(mn,()):
            m=rx.fullmatch(text)
            if not m: continue
            v=m.group('v')
            if kind=='rst':
                s.emit(pre); return
            s.emit(pre)
            if kind=='rel': s.fixup(v,True,8); s.emit(b'\0')
            elif kind=='n8': s.fixup(v,False,8); s.emit(b'\0')
            else: s.fixup(v,False,16); s.emit(b'\0\0')
            return
        raise ValueError('cannot assemble: %r (%s)'%(l,src))
    def ev(s,e):
        e=e.strip()
        def rep(m):
            n=m.group(0)
            if n in s.sym: return str(s.sym[n])
            raise KeyError(n)
        e2=re.sub(r'\$([0-9a-fA-F]+)',lambda m:str(int(m.group(1),16)),e)
        e2=re.sub(r'[A-Za-z_.][A-Za-z_0-9.]*',rep,e2)
        return eval(e2,{'__builtins__':{}})
    def link(s):
        for pc,o,e,rel,bits in s.fix:
            v=s.ev(e)
            if rel:
                d=v-(pc+1); assert -128<=d<=127,(e,d); s.out[o]=d&0xff
            elif bits==8:
                assert -128<=v<=255,('8-bit out of range',e,v) ; s.out[o]=v&0xff
            else:
                s.out[o]=v&0xff; s.out[o+1]=(v>>8)&0xff
        return s.out
    def rom(s,size=0x80000,fill=0xff):
        r=bytearray([fill])*size
        for o,b in s.out.items(): r[o]=b
        return bytes(r)
if __name__=='__main__':
    a=Asm(); a.set_bank(0); a.org=None
    a.pc=0x100
    for l in ['start: ld a,($ff00+c)','ldh ($47),a','ld (hl+),a','ld hl,$1234','jr nz,start','call start','bit 7,a','ld a,(start)','add sp,-2','ld a,5','rst $30','stop','ld a,b','ld hl,sp+3']: a.line(l)
    o=a.link(); print(' '.join('%02x'%o[k] for k in sorted(o)))
