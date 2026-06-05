from PIL import Image, ImageDraw
rom=open("the_castle.rom","rb").read()
# patrones de 8 bytes (solo bitmap, sin color) en zona 0x7B00-0x7E00 (file 0x3B00-0x3E00)
def tile8(foff,scale=10,ink=(220,220,255),bg=(20,20,40)):
    im=Image.new("RGB",(8*scale,8*scale),bg);px=im.load()
    for r in range(8):
        pat=rom[foff+r]
        for b in range(8):
            if pat&(0x80>>b):
                for dy in range(scale):
                    for dx in range(scale): px[b*scale+dx,r*scale+dy]=ink
    return im
start=0x3BC0; end=0x3D00; per_row=12; scale=8; ts=8*scale
sheet=Image.new("RGB",(per_row*(ts+44)+8, ((end-start)//8//per_row+1)*(ts+8)+8),(40,40,40))
d=ImageDraw.Draw(sheet)
for i,foff in enumerate(range(start,end,8)):
    cx=(i%per_row)*(ts+44)+40; cy=(i//per_row)*(ts+8)+4
    d.text((cx-38,cy+ts//2-4), f"{0x4000+foff:04X}", fill=(255,255,0))
    sheet.paste(tile8(foff,scale),(cx,cy))
sheet.save("frag.png"); print("frag.png", sheet.size)
