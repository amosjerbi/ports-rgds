import struct
def make(out,patches):
    d=bytearray(open('fs.gba','rb').read())
    for off,b in patches.items():d[off:off+len(b)]=b
    open(out,'wb').write(d)
def retconst(val):
    # thumb: ldr r0,[pc,#0] ; bx lr ; .word val   (function must be 4-aligned? ldr uses aligned pc)
    return bytes.fromhex('0148 7047 0000'.replace(' ',''))+struct.pack('<I',val)
