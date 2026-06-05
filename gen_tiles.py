from PIL import Image
import numpy as np
PAL=[(0,0,0),(0,0,0),(33,200,66),(94,220,120),(84,85,237),(125,118,252),(212,82,77),
(66,235,245),(252,85,84),(255,121,120),(212,193,84),(230,206,128),(33,176,59),
(201,91,186),(204,204,204),(255,255,255)]
palarr=np.array(PAL)
m=Image.open('../The Castle/Castle-SG-All.png').convert('RGB')
a=np.asarray(m).astype(int); H,W,_=a.shape
# cuantizar cada pixel al color MSX mas cercano
flat=a.reshape(-1,3)
d=((flat[:,None,:]-palarr[None,:,:])**2).sum(2)
idx=d.argmin(1).reshape(H,W).astype(np.uint8)   # indice de paleta por pixel

ROOMS=10; TCOLS,TROWS=28,20
rwf,rhf=W/ROOMS, H/ROOMS
tiles={}   # bytes(64 indices) -> tile_id
order=[]
roomtiles=np.zeros((100,TROWS,TCOLS),dtype=np.uint16)
for ry in range(ROOMS):
    for rx in range(ROOMS):
        ox=rx*224; oy=ry*160
        for tr in range(TROWS):
            for tc in range(TCOLS):
                y=oy+tr*8; x=ox+tc*8
                blk=idx[y:y+8, x:x+8]
                if blk.shape!=(8,8):
                    bb=np.zeros((8,8),np.uint8); bb[:blk.shape[0],:blk.shape[1]]=blk; blk=bb
                key=blk.tobytes()
                if key not in tiles:
                    tiles[key]=len(order); order.append(blk)
                roomtiles[ry*10+rx,tr,tc]=tiles[key]
print('tiles unicos:', len(order))

# verificar: reconstruir sala 0x00 desde tileset
def render_room(roomidx,scale=4):
    im=Image.new('RGB',(TCOLS*8*scale,TROWS*8*scale))
    px=im.load()
    for tr in range(TROWS):
        for tc in range(TCOLS):
            blk=order[roomtiles[roomidx,tr,tc]]
            for yy in range(8):
                for xx in range(8):
                    c=PAL[blk[yy,xx]]
                    for dy in range(scale):
                        for dx in range(scale):
                            px[(tc*8+xx)*scale+dx,(tr*8+yy)*scale+dy]=c
    return im
for room in [0x00,0x80,0x70,0x01]:
    rx,ry=room&0xF,room>>4
    rec=render_room(ry*10+rx)
    orig=m.crop((rx*224,ry*160,rx*224+224,ry*160+160)).resize(rec.size,Image.NEAREST)
    s=Image.new('RGB',(rec.width*2+8,rec.height),(40,40,40)); s.paste(orig,(0,0)); s.paste(rec,(rec.width+8,0))
    s.save(f'tiles_{room:02x}.png')
print('tiles_00.png tiles_80.png (orig | reconstruido desde tileset)')
np.save('roomtiles.npy', roomtiles)
import pickle
pickle.dump([b.tobytes() for b in order], open('tileset.pkl','wb'))

# === emitir C: TILESET (4bpp) + ROOM_TILES ===
N=len(order)
assert N<256, "necesita uint16"
with open('map_tiles.h','w') as f:
    f.write('#pragma once\n')
    f.write(f'#define TILE_COUNT {N}\n#define MT_COLS 28\n#define MT_ROWS 20\n')
    f.write('extern const unsigned char TILESET[TILE_COUNT][32];\n')
    f.write('extern const unsigned char ROOM_TILES[100][MT_ROWS][MT_COLS];\n')
with open('map_tiles.c','w') as f:
    f.write('/* Tileset REAL extraido del mapa Castle-SG-All.png (8x8, 4bpp, paleta MSX).\n')
    f.write(' * Reconstruye las salas pixel-identicas. Generado por gen_tiles.py. */\n')
    f.write('#include "map_tiles.h"\n\n')
    f.write('const unsigned char TILESET[TILE_COUNT][32] = {\n')
    for blk in order:
        bts=[]
        flat=blk.flatten()
        for k in range(0,64,2):
            bts.append((int(flat[k])<<4)|int(flat[k+1]))
        f.write('{'+','.join(str(b) for b in bts)+'},\n')
    f.write('};\n\n')
    f.write('const unsigned char ROOM_TILES[100][MT_ROWS][MT_COLS] = {\n')
    for ri in range(100):
        f.write('{')
        rows=['{'+','.join(str(int(roomtiles[ri,tr,tc])) for tc in range(TCOLS))+'}' for tr in range(TROWS)]
        f.write(','.join(rows)); f.write('},\n')
    f.write('};\n')
import os
print('map_tiles.c', os.path.getsize('map_tiles.c'),'bytes  | tiles', N)
