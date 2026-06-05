from PIL import Image, ImageDraw
PAL=[(0,0,0),(0,0,0),(33,200,66),(94,220,120),(84,85,237),(125,118,252),(212,82,77),
(66,235,245),(252,85,84),(255,121,120),(212,193,84),(230,206,128),(33,176,59),
(201,91,186),(204,204,204),(255,255,255)]
rom=open("the_castle.rom","rb").read()
def tile_img(off,scale=4):
    im=Image.new("RGB",(8*scale,8*scale),(20,20,20));px=im.load()
    for r in range(8):
        pat=rom[off+r*2]; colb=rom[off+r*2+1]
        ink=colb>>4; bg=colb&0xF
        for b in range(8):
            c=PAL[ink] if (pat&(0x80>>b)) else (PAL[bg] if bg else (15,15,22))
            for dy in range(scale):
                for dx in range(scale):
                    px[b*scale+dx, r*scale+dy]=c
    return im
# region 0x8000-0x9C00 (file offsets 0x4000-0x5C00)
start=0x4000; end=0x5C00; per_row=16; scale=4; ts=8*scale
cols=per_row; rows=(end-start)//16//per_row+1
sheet=Image.new("RGB",(cols*(ts+18)+60, rows*(ts+4)+4),(40,40,40))
d=ImageDraw.Draw(sheet)
i=0
for off in range(start,end,16):
    cx=(i%per_row)*(ts+18)+56; cy=(i//per_row)*(ts+4)+2
    if i%per_row==0:
        d.text((2,cy+ts//2-4), f"{0x4000+(off-start):04X}", fill=(255,255,0))
    sheet.paste(tile_img(off,scale),(cx,cy))
    i+=1
sheet.save("tilesheet.png")
print("tilesheet.png:", sheet.size, "tiles desde ROM 0x8000")
