import numpy as np
tiles={}; order=[]
roomnt=np.zeros((100,24,32),dtype=np.uint16)
for hi in range(10):
    for lo in range(10):
        room=hi*16+lo; ridx=hi*10+lo
        v=open(f"tests/fixtures/vram/vram_{room:02X}.bin","rb").read()
        for r in range(24):
            third=r//8
            for c in range(32):
                t=v[0x1800+r*32+c]
                pat=v[(third*256+t)*8:(third*256+t)*8+8]
                col=v[0x2000+(third*256+t)*8:0x2000+(third*256+t)*8+8]
                key=bytes(pat)+bytes(col)
                if key not in tiles:
                    tiles[key]=len(order); order.append(key)
                roomnt[ridx,r,c]=tiles[key]
N=len(order)
print("tiles unicos reales:", N)
# emitir C
with open("map_real.h","w") as f:
    f.write("#pragma once\n")
    f.write(f"#define RT_COUNT {N}\n#define RT_COLS 32\n#define RT_ROWS 24\n")
    typ = "unsigned short" if N>255 else "unsigned char"
    f.write(f"extern const unsigned char RT_TILES[RT_COUNT][16];\n")  # 8 pat + 8 col
    f.write(f"extern const {typ} ROOM_NT[100][RT_ROWS][RT_COLS];\n")
with open("map_real.c","w") as f:
    f.write("/* Tiles REALES capturados de la VRAM del ROM (openMSX, force-call sub_64DD).\n")
    f.write(" * RT_TILES[i] = 8 bytes patron + 8 bytes color (SCREEN 2). Byte-exacto. */\n")
    f.write('#include "map_real.h"\n\n')
    f.write("const unsigned char RT_TILES[RT_COUNT][16] = {\n")
    for k in order:
        f.write("{"+",".join(str(b) for b in k)+"},\n")
    f.write("};\n\n")
    typ = "unsigned short" if N>255 else "unsigned char"
    f.write(f"const {typ} ROOM_NT[100][RT_ROWS][RT_COLS] = {{\n")
    for ri in range(100):
        f.write("{")
        f.write(",".join("{"+",".join(str(int(roomnt[ri,r,c])) for c in range(32))+"}" for r in range(24)))
        f.write("},\n")
    f.write("};\n")
import os
print("map_real.c", os.path.getsize("map_real.c"), "bytes")
