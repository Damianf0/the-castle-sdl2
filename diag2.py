import sys
from PIL import Image
import decode_geom as D
rom=D.load_rom("the_castle.rom")
for room,dom in [(0x70,1),(0x00,1),(0x80,2)]:
    d,_=D.decode_room(rom,room)
    sc=12
    def render(pred):
        im=Image.new("RGB",(20*sc,30*sc),(8,8,12));px=im.load()
        for (r,c),s in d.grid.items():
            if not pred(s):continue
            for dy in range(sc):
                for dx in range(sc):
                    x,y=c*sc+dx,r*sc+dy
                    if 0<=x<im.width and 0<=y<im.height:px[x,y]=(80,220,120)
        return im
    only_dom=render(lambda s:s==dom)
    only_oth=render(lambda s:s!=dom)
    mp=Image.open(f"map_room_{room:02x}.png").convert("RGB").resize((20*sc,18*sc),Image.NEAREST)
    W=20*sc
    sheet=Image.new("RGB",(W*3+16,30*sc),(30,30,30))
    sheet.paste(mp,(0,0)); sheet.paste(only_dom,(W+8,0)); sheet.paste(only_oth,(2*W+16,0))
    sheet.save(f"diag2_{room:02x}.png")
    print(f"sala {room:#04x}: diag2_{room:02x}.png  (izq=mapa, centro=solo shape{dom}, der=resto)")
