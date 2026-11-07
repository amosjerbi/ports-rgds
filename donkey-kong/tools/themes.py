"""World themes -> out/themes.json: per world 8 BG palettes (class 0 sky, 1 decor, 2 structure, 3 dense, 7 HUD) + shared character palettes/classes (from 1-1)."""
import json
W={ # name, sky, decor, structure, dense, ink
 1:('Big-City',(196,222,250),(150,170,206),(206,126,100),(96,84,110),(30,26,48)),
 2:('Forest',(210,236,250),(120,190,90),(176,124,72),(48,100,52),(28,24,40)),
 3:('Ship',(214,238,242),(84,152,182),(198,136,84),(40,62,104),(14,20,38)),
 4:('Jungle',(222,240,190),(90,170,70),(196,154,92),(40,92,44),(24,32,24)),
 5:('Desert',(250,236,190),(236,200,120),(204,142,82),(112,72,42),(44,28,20)),
 6:('Airplane',(190,214,248),(232,242,252),(130,152,192),(62,82,122),(26,32,56)),
 7:('Iceberg',(212,238,252),(170,214,240),(120,170,214),(60,100,152),(24,36,66)),
 8:('Rocky-Valley',(168,212,248),(156,118,190),(214,150,84),(86,50,110),(28,16,44)),
 9:('Tower',(214,224,238),(170,182,204),(128,140,168),(60,68,92),(26,30,46)),
}
def mix(a,b,t): return tuple(a[i]*(1-t)+b[i]*t for i in range(3))
ov=json.load(open('data/ovr12.json')) if __import__('os').path.exists('data/ovr12.json') else json.load(open('out/ovr12.json'))
out=[]
for w,(name,sky,dec,st,dn,ink) in W.items():
    p0=[sky,mix(sky,dec,.5),dec,ink]
    p1=[mix(sky,dec,.35),mix(sky,dec,.7),dec,ink]
    p2=[mix(sky,st,.3),mix(sky,st,.65),st,ink]
    p3=[mix(st,dn,.3),mix(st,dn,.7),dn,ink]
    bg=[p0,p1,p2,p3,p1,p2,p3,ov['bg'][7]]
    if w==1: bg=ov['bg']       # Big-City: exactly the 1-1 / 1-2 palettes
    out.append({'world':w,'name':name,'bg':[[list(map(float,c)) for c in p] for p in bg],'obj':ov['obj']})
json.dump({'themes':out,'cls':ov['cls']},open('out/themes.json','w')); print('saved out/themes.json')
