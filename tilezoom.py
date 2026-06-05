from PIL import Image, ImageDraw
PAL=[(0,0,0),(0,0,0),(33,200,66),(94,220,120),(84,85,237),(125,118,252),(212,82,77),
(66,235,245),(252,85,84),(255,121,120),(212,193,84),(230,206,128),(33,176,59),
(201,91,186),(204,204,204),(255,255,255)]
rom=open("the_castle.rom","rb").read()
def tile_img(foff,scale=10):
    im=Image.new("RGB",(8*scale,8*scale),(20,20,20));px=im.load()
    for r in range(8):
        pat=rom[foff+r*2]; colb=rom[foff+r*2+1]; ink=colb>>4; bg=colb&0xF
        for b in range(8):
            c=PAL[ink] if (pat&(0x80>>b)) else (PAL[bg] if bg else (15,15,22))
            for dy in range(scale):
                for dx in range(scale): px[b*scale+dx,r*scale+dy]=c
    return im
# zona file 0x4900-0x4C00 (ROM 0x8900-0x8C00), scale grande
start=0x4900; end=0x4C00; per_row=8; scale=10; ts=8*scale
n=(end-start)//16
cols=per_row; rows=(n+per_row-1)//per_row
sheet=Image.new("RGB",(cols*(ts+50)+8, rows*(ts+8)+8),(40,40,40))
d=ImageDraw.Draw(sheet)
for i,foff in enumerate(range(start,end,16)):
    cx=(i%per_row)*(ts+50)+44; cy=(i//per_row)*(ts+8)+4
    d.text((cx-42,cy+ts//2-4), f"{0x4000+foff:04X}", fill=(255,255,0))
    sheet.paste(tile_img(foff,scale),(cx,cy))
sheet.save("tilezoom.png"); print("tilezoom.png", sheet.size)
