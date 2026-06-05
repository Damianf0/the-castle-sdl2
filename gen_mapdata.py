from PIL import Image
import numpy as np
m=Image.open('../The Castle/Castle-SG-All.png').convert('RGB')
a=np.asarray(m); W,H=m.size
TCOLS,TROWS=28,20
rw,rh=W/10.0,H/10.0; cw,ch=rw/TCOLS,rh/TROWS
def classify(rx,ry,tc,tr):
    x0=int(rx*rw+tc*cw); x1=int(rx*rw+(tc+1)*cw)
    y0=int(ry*rh+tr*ch); y1=int(ry*rh+(tr+1)*ch)
    p=a[y0:y1,x0:x1].reshape(-1,3).astype(int)
    R,G,B=p[:,0],p[:,1],p[:,2]; n=len(p)
    black=((R<60)&(G<60)&(B<70)).sum()
    white=((R>170)&(G>170)&(B>170)).sum()
    blue =((B>150)&(R<140)&(G<140)).sum()
    red  =((R>150)&(G<110)&(B<110)).sum()
    cyan =((B>170)&(G>150)&(R<160)).sum()
    if black>n*0.62: return 0
    cats={1:white+blue,2:red,3:cyan}
    k=max(cats,key=cats.get)
    return k if cats[k]>n*0.12 else 0

with open('map_data.c','w') as f:
    f.write('/* Geometria de las 100 salas, DERIVADA del mapa real Castle-SG-All.png\n')
    f.write(' * (clasificacion por celda: 0=vacio 1=pared blanca 2=pared roja 3=plataforma cian)\n')
    f.write(' * Generado por gen_mapdata.py. Indexado [ry*10+rx][row][col], 28x20 tiles. */\n')
    f.write('#include "map_data.h"\n\n')
    f.write('const unsigned char MAP_GEOM[100][MAP_ROWS][MAP_COLS] = {\n')
    for ry in range(10):
        for rx in range(10):
            f.write(f'/* sala {ry:X}{rx:X} */ {{')
            rows=[]
            for tr in range(TROWS):
                cells=','.join(str(classify(rx,ry,tc,tr)) for tc in range(TCOLS))
                rows.append('{'+cells+'}')
            f.write(','.join(rows))
            f.write('},\n')
    f.write('};\n')
with open('map_data.h','w') as f:
    f.write('#pragma once\n#define MAP_COLS 28\n#define MAP_ROWS 20\n')
    f.write('extern const unsigned char MAP_GEOM[100][MAP_ROWS][MAP_COLS];\n')
print('map_data.c + map_data.h generados')
import os; print('map_data.c =', os.path.getsize('map_data.c'), 'bytes')
