#!/usr/bin/env python3
"""make: mkips.py make <orig> <new> <out.ips> | apply: mkips.py apply <orig> <ips> <out>"""
import sys
def make(o,n):
    out=bytearray(b'PATCH'); i=0; L=len(n)
    while i<L:
        if o[i]==n[i]: i+=1; continue
        j=i
        while j<L and j-i<0xFFFF and (o[j]!=n[j] or (j+1<L and o[j+1]!=n[j+1]) or (j+2<L and o[j+2]!=n[j+2])): j+=1
        if i==0x454f46: i-=1  # avoid offset == 'EOF'
        out+=i.to_bytes(3,'big')+(j-i).to_bytes(2,'big')+n[i:j]; i=j
    return bytes(out+b'EOF')
def apply(o,p):
    d=bytearray(o); i=5
    while p[i:i+3]!=b'EOF':
        a=int.from_bytes(p[i:i+3],'big'); l=int.from_bytes(p[i+3:i+5],'big'); i+=5
        if l==0: r=int.from_bytes(p[i:i+2],'big'); d[a:a+r]=bytes([p[i+2]])*r; i+=3
        else: d[a:a+l]=p[i:i+l]; i+=l
    return bytes(d)
if __name__=='__main__':
    c,a,b,d=sys.argv[1:5]
    if c=='make': open(d,'wb').write(make(open(a,'rb').read(),open(b,'rb').read()))
    else: open(d,'wb').write(apply(open(a,'rb').read(),open(b,'rb').read()))
