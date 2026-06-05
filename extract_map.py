from PIL import Image
import sys
m=Image.open('../The Castle/Castle-SG-All.png').convert('RGB')
W,H=m.size
ROOMS_X, ROOMS_Y = 10, 10
TCOLS, TROWS = 28, 20           # tiles por sala
rw, rh = W/ROOMS_X, H/ROOMS_Y
cw, ch = rw/TCOLS, rh/TROWS

# clasificacion: 0=vacio 1=pared blanca 2=pared roja 3=plataforma cian 4=hazard/spike
def classify(cell):
    px=cell.load(); w,hh=cell.size
    nw=nr=nc=nk=tot=0
    for y in range(hh):
        for x in range(w):
            r,g,b=px[x,y]; tot+=1
            if r<50 and g<50 and b<50: nk+=1
            elif r>180 and g>180 and b>180: nw+=1           # blanco (ladrillo)
            elif r>150 and g<120 and b<120: nr+=1           # rojo
            elif b>160 and g>120: nc+=1                     # cian/celeste plataforma
    if nk > tot*0.75: return 0
    if nr > tot*0.12: return 2
    if nw > tot*0.18: return 1
    if nc > tot*0.18: return 3
    return 0

PAL={0:(8,8,14),1:(235,235,235),2:(220,60,60),3:(110,200,250),4:(255,255,0)}
def extract_room(rx,ry):
    grid=[]
    for tr in range(TROWS):
        row=[]
        for tc in range(TCOLS):
            x0=int(rx*rw+tc*cw); y0=int(ry*rh+tr*ch)
            x1=int(rx*rw+(tc+1)*cw); y1=int(ry*rh+(tr+1)*ch)
            row.append(classify(m.crop((x0,y0,x1,y1))))
        grid.append(row)
    return grid

def render(grid,scale=8):
    im=Image.new('RGB',(TCOLS*scale,TROWS*scale),(0,0,0));px=im.load()
    for tr,row in enumerate(grid):
        for tc,v in enumerate(row):
            c=PAL[v]
            for dy in range(scale):
                for dx in range(scale): px[tc*scale+dx,tr*scale+dy]=c
    return im

if __name__=='__main__':
    # verificar sala 0x00: original vs reclasificada
    for room in [0x00,0x70,0x80,0x01]:
        rx,ry=room&0xF, room>>4
        g=extract_room(rx,ry)
        orig=m.crop((int(rx*rw),int(ry*rh),int((rx+1)*rw),int((ry+1)*rh))).resize((TCOLS*8,TROWS*8),Image.NEAREST)
        rec=render(g,8)
        cmp=Image.new('RGB',(TCOLS*8*2+8,TROWS*8),(40,40,40))
        cmp.paste(orig,(0,0)); cmp.paste(rec,(TCOLS*8+8,0))
        cmp.save(f'extract_{room:02x}.png')
        nwall=sum(r.count(1)+r.count(2) for r in g)
        print(f'sala {room:#04x}: {nwall} paredes  -> extract_{room:02x}.png (izq orig, der reclasif)')
