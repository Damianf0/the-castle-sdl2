from PIL import Image
PAL=[(0,0,0),(0,0,0),(33,200,66),(94,220,120),(84,85,237),(125,118,252),(212,82,77),
(66,235,245),(252,85,84),(255,121,120),(212,193,84),(230,206,128),(33,176,59),
(201,91,186),(204,204,204),(255,255,255)]
def render(fn):
    v=open(fn,'rb').read()
    img=Image.new('RGB',(256,192)); px=img.load()
    for r in range(24):
        third=r//8
        for c in range(32):
            t=v[0x1800+r*32+c]; pb=(third*256+t)*8; cb=0x2000+(third*256+t)*8
            for row in range(8):
                pat=v[pb+row]; colb=v[cb+row]; fg=colb>>4; bg=colb&0xF
                for b in range(8):
                    px[c*8+b,r*8+row]=PAL[fg if (pat&(0x80>>b)) else bg]
    return img
ims=[]
for room in ['00','33','80','70']:
    real=render(f'vram_{room}.bin').resize((256,192),Image.NEAREST)
    mp=Image.open(f'map_room_{int(room,16):02x}.png').convert('RGB').resize((256,183),Image.NEAREST)
    p=Image.new('RGB',(256,192+183+4),(0,0,0)); p.paste(real,(0,0)); p.paste(mp,(0,196))
    ims.append(p)
sheet=Image.new('RGB',(4*264,379),(30,30,30))
for i,p in enumerate(ims): sheet.paste(p,(i*264,0))
sheet.save('caps_vs_map.png'); print('caps_vs_map.png: ARRIBA = VRAM real (force-call), ABAJO = mapa. salas 00/33/80/70')
