"""World themes for the stages that have no hand-made look: one BG palette (4 shades) + character palettes per world.
-> out/themes.json   (worlds 1..9; stage ranges are in color/hooks.s THTAB)"""
import json
# by displayed shade: 0 lightest .. 3 darkest
BG={
 1:('Big-City',    [(236,238,240),(214,140,110),(96,128,176),(32,36,56)]),
 2:('Forest',      [(233,232,200),(96,144,208),(110,174,16),(41,16,65)]),
 3:('Ship',        [(216,240,244),(120,184,207),(140,104,72),(32,40,60)]),
 4:('Jungle',      [(228,239,200),(143,207,90),(63,138,42),(27,42,20)]),
 5:('Desert',      [(246,236,200),(232,184,106),(176,116,58),(58,36,18)]),
 6:('Airplane',    [(232,242,251),(168,200,238),(108,136,180),(32,40,64)]),
 7:('Iceberg',     [(238,248,255),(180,220,244),(106,160,208),(28,44,76)]),
 8:('Rocky-Valley',[(240,228,208),(200,160,112),(138,94,58),(44,30,24)]),
 9:('Tower',       [(228,236,240),(160,184,200),(88,112,136),(28,36,48)]),
}
def shade_pal(by_idx,reg):
    out=[None]*4
    for i in range(4): out[(reg>>(2*i))&3]=by_idx[i]
    for s in range(4):
        if out[s] is None: out[s]=by_idx[0]
    return out
mario=[(0,0,0),(8,0,64),(240,190,150),(208,32,32)]
enemy=[(0,0,0),(40,16,24),(204,92,60),(250,230,200)]
hammer=[(0,0,0),(56,24,8),(120,72,32),(200,120,56)]
OBP0,OBP1=0x1e,0x9c
ov=json.load(open('data/ovr12.json'))
out=[]
for w in range(1,10):
    name,c=BG[w]
    objs=ov['obj']
    out.append({'world':w,'name':name,'bg':[[list(map(float,x)) for x in c]]*8,'obj':objs})
cls=ov['cls']
json.dump({'themes':out,'cls':cls},open('data/themes.json','w')); print('saved out/themes.json')
