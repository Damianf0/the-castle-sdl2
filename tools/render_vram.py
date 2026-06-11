from PIL import Image
PAL=[(0,0,0),(0,0,0),(33,200,66),(94,220,120),(84,85,237),(125,118,252),(212,82,77),
(66,235,245),(252,85,84),(255,121,120),(212,193,84),(230,206,128),(33,176,59),
(201,91,186),(204,204,204),(255,255,255)]
v=open('vram.bin','rb').read()
NT=0x1800; PAT=0x0000; COL=0x2000
img=Image.new('RGB',(256,192)); px=img.load()
for r in range(24):
    third=r//8
    for c in range(32):
        tile=v[NT+r*32+c]
        pb=PAT+(third*256+tile)*8
        cb=COL+(third*256+tile)*8
        for row in range(8):
            pat=v[pb+row]; colb=v[cb+row]
            fg=colb>>4; bg=colb&0xF
            for b in range(8):
                ci=fg if (pat&(0x80>>b)) else bg
                px[c*8+b, r*8+row]=PAL[ci]
img.save('vram_render.png')
print('vram_render.png = sala 0x70 REAL del ROM (via openMSX)')
