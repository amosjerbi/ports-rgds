import sys,struct;sys.path.insert(0,'.')
from asm import *;from patchrom import make
NET=0x0203fe00
BLOB=0x646000      # poll replacement (0xc54c)
BLOB2=0x646200     # send hook (0x80368a0)
def copy20(): return ''.join(f'ldr r0,[r1,#{o}]\nstr r0,[r2,#{o}]\n' for o in range(0,20,4))
def poll():
    return asm_thumb(f'''
push {{r4,r5,r6,r7,lr}}
ldr r6,=0x{NET:08x}
movs r0,#0x20
ldrb r0,[r6,r0]
cmp r0,#0
bne 1f
movs r0,#0
pop {{r4,r5,r6,r7,pc}}
1: ldr r4,=0x03000ff8
ldr r5,=0x0203ffd8
ldrb r5,[r5]
lsrs r5,r5,#4
movs r0,#3
ands r5,r0
movs r0,#0x14
muls r0,r5,r0
adds r2,r4,r0
movs r1,#8
adds r1,r1,r6
{copy20()}
movs r0,#1
strb r0,[r2,#1]
movs r0,#0x21
ldrb r7,[r6,r0]
movs r0,#0x22
ldrb r2,[r6,r0]
cmp r7,r2
beq 3f
subs r3,r2,r7
lsls r3,r3,#24
lsrs r3,r3,#24
cmp r3,#3
bls 2f
adds r7,#1
2: movs r0,#7
ands r0,r7
movs r3,#20
muls r3,r0,r3
adds r3,#0x24
adds r3,r3,r6
adds r1,r3,#0
movs r2,#0xc4
adds r2,r2,r6
{copy20()}
adds r7,#1
movs r0,#0x21
strb r7,[r6,r0]
3: movs r0,#1
eors r0,r5
movs r3,#0x14
muls r3,r0,r3
adds r2,r4,r3
movs r1,#0xc4
adds r1,r1,r6
{copy20()}
movs r0,#1
strb r0,[r2,#1]
ldr r1,=0x02030790
movs r0,#3
strb r0,[r1,#3]
movs r0,#1
strb r0,[r1,#2]
ldr r0,=0x8383
pop {{r4,r5,r6,r7,pc}}
.ltorg
''')
def sendhook():
    return asm_thumb(f'''
push {{lr}}
ldr r1,=0x03000fd0
ldr r2,=0x{NET+8:08x}
{copy20()}
ldrb r0,[r1]
cmp r0,#0x30
bhs 1f
ldr r3,=0x04000130
ldrh r0,[r3]
mvns r0,r0
lsls r0,r0,#22
lsrs r0,r0,#22
strh r0,[r2,#2]
1: ldr r3,=0x{NET+4:08x}
ldr r0,[r3]
adds r0,#1
str r0,[r3]
ldr r0,=0x4e345346
subs r3,#4
str r0,[r3]
movs r0,#0
str r0,[r1,#0]
str r0,[r1,#4]
str r0,[r1,#8]
str r0,[r1,#12]
str r0,[r1,#16]
pop {{pc}}
.ltorg
''')
def tramp(addr): return asm_thumb(f'ldr r3,=0x{addr+0x08000000+1:x}\nbx r3\n.ltorg\n')
def patches(D=2):
    p={BLOB:poll(),BLOB2:sendhook(),0xc54c:tramp(BLOB),0x368a0:tramp(BLOB2),0x36a68:struct.pack('<I',0x0203ffd8)}
    assert len(p[BLOB])<0x200,len(p[BLOB])
    p[0x3730e]=struct.pack('<H',0x1E00|(D<<6)|(1<<3)|0)
    p[0x3776e]=struct.pack('<H',0x1E00|(D<<6)|(3<<3)|2)
    p[0x36fe4]=struct.pack('<H',0x2100|(16-D))
    return p
if __name__=='__main__':
    D=int(sys.argv[1]) if len(sys.argv)>1 else 2
    out=sys.argv[2] if len(sys.argv)>2 else '../fs_net.gba'
    p=patches(D);print('poll',len(p[BLOB]),'send',len(p[BLOB2]))
    make(out,p)
