from PIL import Image
import numpy as np
m=Image.open('../The Castle/Castle-SG-All.png').convert('RGB')
a=np.asarray(m)
W,H=m.size
TCOLS,TROWS=28,20
rw,rh=W/10.0,H/10.0
cw,ch=rw/TCOLS,rh/TROWS

def cell_pixels(rx,ry,tc,tr):
    x0=int(rx*rw+tc*cw); x1=int(rx*rw+(tc+1)*cw)
    y0=int(ry*rh+tr*ch); y1=int(ry*rh+(tr+1)*ch)
    return a[y0:y1, x0:x1].reshape(-1,3)

def avg_color(rx,ry,tc,tr):
    p=cell_pixels(rx,ry,tc,tr); return tuple(int(x) for x in p.mean(0))

# 0=vacio 1=blanca 2=roja 3=cian/plataforma 4=amarillo
def classify(rx,ry,tc,tr):
    p=cell_pixels(rx,ry,tc,tr).astype(int)
    R,G,B=p[:,0],p[:,1],p[:,2]
    n=len(p)
    black=((R<60)&(G<60)&(B<70)).sum()
    white=((R>170)&(G>170)&(B>170)).sum()
    blue =((B>150)&(R<140)&(G<140)).sum()   # mortero azul (cuenta como ladrillo)
    red  =((R>150)&(G<110)&(B<110)).sum()
    cyan =((B>170)&(G>150)&(R<160)).sum()
    yel  =((R>180)&(G>180)&(B<120)).sum()
    if black > n*0.62: return 0
    cats={1:white+blue, 2:red, 3:cyan, 4:yel}
    k=max(cats,key=cats.get)
    return k if cats[k]>n*0.12 else 0

PAL={0:(8,8,14),1:(240,240,240),2:(214,82,77),3:(110,200,250),4:(230,206,80)}
def render_avg(rx,ry,scale=8):
    im=Image.new('RGB',(TCOLS*scale,TROWS*scale));px=im.load()
    for tr in range(TROWS):
        for tc in range(TCOLS):
            c=avg_color(rx,ry,tc,tr)
            for dy in range(scale):
                for dx in range(scale): px[tc*scale+dx,tr*scale+dy]=c
    return im
def render_cls(rx,ry,scale=8):
    im=Image.new('RGB',(TCOLS*scale,TROWS*scale));px=im.load()
    for tr in range(TROWS):
        for tc in range(TCOLS):
            c=PAL[classify(rx,ry,tc,tr)]
            for dy in range(scale):
                for dx in range(scale): px[tc*scale+dx,tr*scale+dy]=c
    return im

for room in [0x00,0x70,0x80]:
    rx,ry=room&0xF,room>>4
    orig=m.crop((int(rx*rw),int(ry*rh),int((rx+1)*rw),int((ry+1)*rh))).resize((TCOLS*8,TROWS*8),Image.NEAREST)
    av=render_avg(rx,ry); cl=render_cls(rx,ry)
    sheet=Image.new('RGB',(TCOLS*8*3+16,TROWS*8),(40,40,40))
    sheet.paste(orig,(0,0)); sheet.paste(av,(TCOLS*8+8,0)); sheet.paste(cl,(2*TCOLS*8+16,0))
    sheet.save(f'ex2_{room:02x}.png')
    print(f'sala {room:#04x}: ex2_{room:02x}.png (orig | promedio | clasificado)')

# === reconstruccion del mapa completo (las 100 salas clasificadas) ===
if __name__=='__main__' or True:
    full=Image.new('RGB',(10*TCOLS*4, 10*TROWS*4),(0,0,0))
    grids={}
    for ry in range(10):
        for rx in range(10):
            g=[[classify(rx,ry,tc,tr) for tc in range(TCOLS)] for tr in range(TROWS)]
            grids[(rx,ry)]=g
            tile=render_cls(rx,ry,4)
            full.paste(tile,(rx*TCOLS*4, ry*TROWS*4))
    full.save('full_reconstruct.png')
    # original reducido al mismo tamaño
    m.resize(full.size,Image.NEAREST).save('full_orig.png')
    print('full_reconstruct.png vs full_orig.png', full.size)
