#!/usr/bin/env python3
"""Builds the Shining Soul II (USA, AU2E) wifi-netplay patch.

  python3 build_net.py "Shining Soul II (USA).gba|.zip" [out.gba] [out.ips]

The patch is my own code only: it replaces the link library's poll (0x8001804) with a routine that exchanges the 24-byte
link records through a small EWRAM block (the bridge, ss2-link.py, carries them over wifi), and points the link
library's "which player am I" reads (SIOCNT bits 4-5) at a byte the bridge sets. Needs clang (thumbv4t) to assemble.
"""
import struct, sys, zipfile
from asm import asm_thumb

NET = 0x0203FE00      # network block: magic, out_seq, out packet, peer flag, inbox head/tail, 8 inbox slots, current packet
ROLE = 0x0203FFD8     # byte: (player id << 4), set by the bridge
MAGIC = 0x4E325353    # 'SS2N'
BLOB = 0xFFF400       # free padding at the end of the 16 MB ROM
POLL = 0x1804         # link poll: r0 = send record, r1 = receive records (4 x 0x18)
SIOLITS = [0x1E90, 0x1F00, 0x1F58, 0x1FB0, 0x1FD0, 0x2024, 0x2114, 0x2134]   # literal-pool words holding 0x04000128 (SIOCNT) in the id reads


def copy24(src, dst):
    return ''.join(f'ldr r0,[{src},#{o}]\nstr r0,[{dst},#{o}]\n' for o in range(0, 24, 4))


def poll():
    return asm_thumb(f'''
push {{r4,r5,r6,r7,lr}}
adds r7,r0,#0
adds r6,r1,#0
ldr r5,=0x{NET:08x}
ldr r0,=0x{MAGIC:08x}
str r0,[r5]
movs r0,#0x20
ldrb r0,[r5,r0]
cmp r0,#0
bne 1f
movs r0,#0x80
pop {{r4,r5,r6,r7,pc}}
1: adds r1,r7,#0
movs r2,#8
adds r2,r2,r5
{copy24('r1', 'r2')}
ldr r3,[r5,#4]
adds r3,#1
str r3,[r5,#4]
ldr r4,=0x{ROLE:08x}
ldrb r4,[r4]
lsrs r4,r4,#4
movs r0,#3
ands r4,r0
movs r3,#24
muls r3,r4,r3
adds r2,r6,r3
adds r1,r7,#0
{copy24('r1', 'r2')}
movs r0,#0x21
ldrb r1,[r5,r0]
movs r0,#0x22
ldrb r2,[r5,r0]
cmp r1,r2
beq 3f
subs r3,r2,r1
lsls r3,r3,#24
lsrs r3,r3,#24
cmp r3,#3
bls 2f
adds r1,#1
2: movs r0,#7
ands r0,r1
movs r3,#24
muls r3,r0,r3
adds r3,#0x24
adds r3,r3,r5
movs r2,#0xe4
adds r2,r2,r5
{copy24('r3', 'r2')}
adds r1,#1
movs r0,#0x21
strb r1,[r5,r0]
3: movs r0,#1
eors r0,r4
movs r3,#24
muls r3,r0,r3
adds r2,r6,r3
movs r1,#0xe4
adds r1,r1,r5
{copy24('r1', 'r2')}
ldr r0,=0x8303
cmp r4,#0
bne 4f
movs r1,#0x80
orrs r0,r1
4: pop {{r4,r5,r6,r7,pc}}
.ltorg
''')


def tramp(addr):
    return asm_thumb(f'ldr r3,=0x{addr + 0x08000000 + 1:x}\nbx r3\n.ltorg\n')


def patches():
    p = {BLOB: poll(), POLL: tramp(BLOB)}
    assert len(p[BLOB]) < 0x49C
    for o in SIOLITS:
        p[o] = struct.pack('<I', ROLE)
    return p


def ips(p):
    out = bytearray(b'PATCH')
    for off, b in sorted(p.items()):
        out += off.to_bytes(3, 'big') + len(b).to_bytes(2, 'big') + b
    return bytes(out + b'EOF')


if __name__ == '__main__':
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else 'Shining Soul II Netplay (USA).gba'
    out_ips = sys.argv[3] if len(sys.argv) > 3 else 'shiningsoul2_netplay.ips'
    p = patches()
    open(out_ips, 'wb').write(ips(p))
    print('wrote', out_ips, len(ips(p)), 'bytes')
    if src.lower().endswith('.zip'):
        z = zipfile.ZipFile(src)
        rom = bytearray(z.read([n for n in z.namelist() if n.lower().endswith('.gba')][0]))
    else:
        rom = bytearray(open(src, 'rb').read())
    assert rom[0xA0:0xB0] == b'SHININGSOUL2AU2E', 'not Shining Soul II (USA)'
    for off, b in p.items():
        rom[off:off + len(b)] = b
    open(out, 'wb').write(rom)
    print('wrote', out, len(rom), 'bytes')
