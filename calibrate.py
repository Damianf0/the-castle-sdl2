from PIL import Image, ImageDraw
import json
PAL=[(0,0,0),(0,0,0),(33,200,66),(94,220,120),(84,85,237),(125,118,252),(212,82,77),
(66,235,245),(252,85,84),(255,121,120),(212,193,84),(230,206,128),(33,176,59),
(201,91,186),(204,204,204),(255,255,255)]
def render(fn):
    v=open(fn,'rb').read(); img=Image.new('RGB',(256,192)); px=img.load()
    for r in range(24):
        third=r//8
        for c in range(32):
            t=v[0x1800+r*32+c]; pb=(third*256+t)*8; cb=0x2000+(third*256+t)*8
            for row in range(8):
                pat=v[pb+row]; colb=v[cb+row]; fg=colb>>4; bg=colb&0xF
                for b in range(8): px[c*8+b,r*8+row]=PAL[fg if (pat&(0x80>>b)) else bg]
    return img
obj=json.load(open("castle_objects.json"))
# probar mapeo DIRECTO: screen tile (row, col)
for room in ["70","33","01"]:
    img=render(f"vram_{room}.bin").resize((512,384),Image.NEAREST); d=ImageDraw.Draw(img)
    data=obj[room]
    for e in data["enemies"]:
        x,y=e["col"]*16, (e["row"]+3)*16; d.rectangle((x,y,x+15,y+15),outline=(255,0,0),width=2)
    for c in data["coll"]:
        x,y=c["col"]*16, (c["row"]+3)*16; d.ellipse((x,y,x+15,y+15),outline=(0,255,0),width=2)
    img.save(f"calib_{room}.png")
print("calib_70/33/01.png: rojo=enemigos, verde=coleccionables, mapeo (row,col) directo x2")
