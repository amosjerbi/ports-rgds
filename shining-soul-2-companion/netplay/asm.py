import subprocess,struct,os,tempfile
def asm_thumb(src,base=None):
    d=tempfile.mkdtemp()
    open(d+'/a.s','w').write('.syntax unified\n.thumb\n.text\n'+src)
    subprocess.run(['clang','-target','thumbv4t-none-eabi','-c',d+'/a.s','-o',d+'/a.o'],check=True)
    e=open(d+'/a.o','rb').read()
    shoff=struct.unpack_from('<I',e,0x20)[0];shentsize,shnum,shstrndx=struct.unpack_from('<HHH',e,0x2e)
    secs=[struct.unpack_from('<IIIIIIIIII',e,shoff+i*shentsize) for i in range(shnum)]
    stro=secs[shstrndx][4]
    for s in secs:
        nm=e[stro+s[0]:e.index(b'\0',stro+s[0])]
        if nm==b'.text':return e[s[4]:s[4]+s[5]]
