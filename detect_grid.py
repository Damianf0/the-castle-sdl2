from PIL import Image
import numpy as np
m=Image.open('../The Castle/Castle-SG-All.png').convert('RGB')
a=np.asarray(m).astype(int)
H,W,_=a.shape
bright=a.sum(2)  # brillo por pixel
colprof=bright.mean(0)  # brillo medio por columna
rowprof=bright.mean(1)
# las paredes/bordes de sala son lineas brillantes continuas. Busco periodicidad.
def autoperiod(prof, pmin, pmax):
    prof=prof-prof.mean()
    best=(0,-1e18)
    for p in range(pmin,pmax):
        # correlacion con shift p
        c=np.dot(prof[:-p],prof[p:])/len(prof[:-p])
        if c>best[1]: best=(p,c)
    return best
print('W,H=',W,H)
print('periodo columna (room width?) en [120,260]:', autoperiod(colprof,120,260))
print('periodo fila (room height?) en [100,200]:', autoperiod(rowprof,100,200))
print('periodo tile col en [6,20]:', autoperiod(colprof,6,20))
print('periodo tile fila en [6,20]:', autoperiod(rowprof,6,20))
# guardar perfiles primeros 300
print('colprof[0:40]=', [int(x) for x in colprof[:40]])
