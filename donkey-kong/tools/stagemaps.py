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
# ---- Big-City (world 1): reuse the hand-made 1-1 / 1-2 look: tile -> palette learned from those two stages, nearest-tile fallback
o12=json.load(open('data/ovr12.json')); o19=json.load(open('data/ovr19.json'))
known={}    # tile graphics -> list of palettes
def feat(t): return np.array([t.mean()/3,(t==3).mean(),(t==0).mean(),(t==1).mean(),(t==2).mean()])
def learn(step,mp,rows):
    tm=d['tm%d'%step]; tl=d['tl%d'%step]
    for r in range(rows):
        for c in range(20):
            if r==0 and step==16: continue
            known.setdefault(tl[tm[r][c]].tobytes(),[]).append(mp[r][c])
learn(16,o12['map'],18); learn(19,o19['map'],32)
kt=[(np.frombuffer(k,np.uint8).reshape(8,8),max(set(v),key=v.count)) for k,v in known.items()]
kf=np.array([feat(t) for t,_ in kt]); kp=[p for _,p in kt]
cache={}
def pal_for(t):
    k=t.tobytes()
    if k in known: return max(set(known[k]),key=known[k].count)
    if k not in cache: cache[k]=kp[int(((kf-feat(t))**2).sum(1).argmin())]
    return cache[k]
def ambiguous(t):
    k=t.tobytes()
    if k in known:
        c=known[k]; top=max(set(c),key=c.count)
        return len(set(c))>1 and c.count(top)<0.9*len(c)
    return False
def resolve(tl,tm,rows):
    g=[[pal_for(tl[tm[r][c]]) for c in range(20)] for r in range(rows)]
    amb=[[ambiguous(tl[tm[r][c]]) for c in range(20)] for r in range(rows)]
    out_=[row[:] for row in g]
    for r in range(rows):
        for c in range(20):
            if not amb[r][c]: continue
            best=None
            for dist in range(1,12):          # nearest unambiguous cell: same row first (left, right), then column
                for rr,cc in ((r,c-dist),(r,c+dist),(r-dist,c),(r+dist,c)):
                    if 0<=rr<rows and 0<=cc<20 and not amb[rr][cc]: best=g[rr][cc]; break
                if best is not None: break
            if best is not None: out_[r][c]=best
    return out_
for i,v in out.items():
    if not (13<=i<39) or i in (16,19): continue
    tm=d['tm%d'%i]; tl=d['tl%d'%i]
    if v['kind']=='S':
        v['map']=resolve(tl,tm,18); v['map'][0]=[7]*20
    else: v['map']=resolve(tl,tm,32)
    print('world1 stage',i,v['kind'])
json.dump(out,open('out/stagemaps.json','w'))
from collections import Counter
print(Counter(v['kind'] for v in out.values()))
