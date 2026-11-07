"""stagemaps.py -> out/stagemaps.json: per play stage attribute map (palette class per BG cell) from the captured tile graphics.
kind S = static screen (20x18), V = vertical scroller (20x32 BG rows); other stages keep the flat world theme."""
import numpy as np, json
d=np.load('out/stagecap.npz')
def cls_of(t):
    ink=(t==3).mean(); mean=t.mean()
    if mean<0.35: return 0
    if mean<1.3 and ink<0.2: return 1
    if mean>=2.2: return 3
    return 2
out={}
for k in d.files:
    if not k.startswith('tm'): continue
    i=int(k[2:]); tm=d[k]; tl=d['tl%d'%i]; sx,sy=int(d['sc%d'%i][0]),int(d['sc%d'%i][1])
    tc=[cls_of(tl[t]) for t in range(256)]
    if sx==0 and sy==0:
        m=[[tc[tm[r][c]] for c in range(20)] for r in range(18)]; m[0]=[7]*20
        out[i]={'kind':'S','map':m}
    elif sx==0:
        m=[[tc[tm[r][c]] for c in range(20)] for r in range(32)]
        out[i]={'kind':'V','map':m}
json.dump(out,open('out/stagemaps.json','w'))
from collections import Counter
print(Counter(v['kind'] for v in out.values()))
