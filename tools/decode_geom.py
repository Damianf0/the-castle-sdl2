#!/usr/bin/env python3
# Decoder FIEL de geometria de sala de The Castle (MSX), reconstruido del disasm.
# Cada sala: 3 punteros en 0x7CF2 + 42*row + 4*col, offsets +0/+4/+2.
#   ptr#1(+0): sub_6616, cursor row=0   -> borde superior
#   ptr#2(+4): sub_6616, cursor row=28  -> borde inferior
#   ptr#3(+2): sub_6671 = pasada1 sub_66A2 (col-major) + pasada2 sub_6766 (row-major)
#              + pasada3 sub_69AA (objetos), todas compartiendo cursor y puntero de stream.
# Decoders de byte:
#   sub_6616: count=(b&7)+1 (8->extiende), shape=b>>3
#   sub_6973: count=(b&0xF)+1 (0x10->extiende), shape=b>>4
# Avance de cursor:
#   sub_6A3A (row-major): si row==0 o 0x1C -> avanza col; sino row+=2, wrap 0x1C->row=2,col+=1
#   sub_6A63 (col-major): col+=1, wrap 0x14 -> col=0, row+=2
# Pintado (footprint de celdas ocupadas), shape 0 = AIRE (skip, no pinta):
#   sub_66B0 (row-major):  1..3 -> (r,c)+(r+1,c) ; 0xA..0xB -> bloque 2x2 ; resto !=0 -> (r,c)
#   sub_6774 (col-major):  !=0 -> (r,c)  (run horizontal)
import sys
from PIL import Image

ROM_ORG = 0x4000
TABLE   = 0x7CF2

def load_rom(path):
    with open(path, "rb") as f: return f.read()

class Decoder:
    def __init__(self, rom):
        self.rom = rom
        self.row = 0
        self.col = 0
        self.ptr = 0
        self.grid = {}       # (row,col) -> shape
        self.passgrid = {}   # (row,col) -> pass_id (1=top 2=bot 3=66A2 4=6766)
        self.cur_pass = 0
        self.objs = []       # objetos del pass sub_69AA
    def rb(self, addr):
        off = addr - ROM_ORG
        return self.rom[off] if 0 <= off < len(self.rom) else 0xFF
    def rw(self, addr):
        return self.rb(addr) | (self.rb(addr+1) << 8)
    def mark(self, r, c, s):
        if 0 <= c < 20 and 0 <= r < 30:
            self.grid[(r, c)] = s
            self.passgrid[(r, c)] = self.cur_pass
    # --- avanzadores ---
    def adv_row(self):       # sub_6A3A
        if self.row == 0 or self.row == 0x1C:
            return self.adv_col()
        self.row += 2
        if self.row == 0x1C:
            self.row = 2; self.col += 1
    def adv_col(self):       # sub_6A63
        self.col += 1
        if self.col == 0x14:
            self.col = 0; self.row += 2
    # --- lectura de count extendido sub_6998 ---
    def ext_count(self, count):
        guard = 0
        while guard < 64:
            guard += 1
            b = self.rb(self.ptr); self.ptr += 1
            count += b
            if b != 0xFF: break
        return min(count, 600)   # clamp por seguridad
    # --- pintado de un paso (devuelve nada, marca celdas) ---
    def paint_rowmajor(self, A):     # sub_66B0 footprint
        r, c = self.row, self.col
        if A == 0:
            pass                                   # aire
        elif 1 <= A <= 3:
            self.mark(r, c, A); self.mark(r+1, c, A)   # par vertical
        elif 0xA <= A <= 0xB:
            self.mark(r, c, A); self.mark(r+1, c, A)
            self.mark(r, c+1, A); self.mark(r+1, c+1, A)  # bloque 2x2
        else:
            self.mark(r, c, A)
        self.adv_row()
    def paint_colmajor(self, A):     # sub_6774 footprint
        if A != 0:
            self.mark(self.row, self.col, A)       # run horizontal
        self.adv_col()
    # --- decoder sub_6616 (ptr#1, ptr#2): byte>>3 / byte&7 ---
    def run_6616(self, start_row, start_col, start_ptr, max_iter=4000):
        self.row, self.col, self.ptr = start_row, start_col, start_ptr
        for _ in range(max_iter):
            byte = self.rb(self.ptr); self.ptr += 1
            count = (byte & 7) + 1
            if count == 8: count = self.ext_count(count)
            shape = byte >> 3
            if   shape == 7 and self.row == 0x1C: shape = 8
            elif shape == 8 and self.row == 0x00: shape = 7
            # dispatch sub_6664
            if shape == 0:
                for _ in range(count): self.paint_rowmajor(0)
            elif shape < 0x10:
                for _ in range(count): self.paint_colmajor(shape)
            else:
                a = shape & 0x0F
                for _ in range(count): self.paint_rowmajor(a)
            if self.row == 0x02 or self.row == 0x1E:
                return
    # --- decoder sub_6973 (ptr#3): byte>>4 / byte&0xF ---
    def read_6973(self):
        byte = self.rb(self.ptr); self.ptr += 1
        count = (byte & 0x0F) + 1
        if count == 0x10: count = self.ext_count(count)
        shape = byte >> 4
        return shape, count
    def run_66A2(self, max_iter=8000):   # pasada col-major-fill (avance row-major), term col==0x14
        for _ in range(max_iter):
            shape, count = self.read_6973()
            for _ in range(count): self.paint_rowmajor(shape)
            if self.col == 0x14: return
    def run_6766(self, max_iter=8000):   # pasada row-fill (avance col-major), term row==0x1C
        for _ in range(max_iter):
            shape, count = self.read_6973()
            for _ in range(count): self.paint_colmajor(shape)
            if self.row == 0x1C: return
    def run_objects(self, maxn=40):      # sub_69AA: stream de objetos 2 bytes
        for _ in range(maxn):
            b0 = self.rb(self.ptr)
            if b0 == 0: break
            b1 = self.rb(self.ptr+1); self.ptr += 2
            r = (b0 & 0x0F) * 2
            c = b1 & 0x1F
            t = (b0 >> 4) + (0x20 if (b1 & 0x40) else 0x30)
            self.objs.append((t, r, c))

def room_pointers(rom, room_x):
    row = room_x >> 4; col = room_x & 0x0F
    off = 42*row + 4*col
    base = TABLE + off
    d = Decoder(rom)
    return off, d.rw(base+0), d.rw(base+4), d.rw(base+2)

def decode_room(rom, room_x):
    off, p1, p2, p3 = room_pointers(rom, room_x)
    d = Decoder(rom)
    # ptr#1 borde superior
    d.cur_pass = 1
    if ROM_ORG <= p1 < ROM_ORG+len(rom): d.run_6616(0, 0, p1)
    # ptr#2 borde inferior
    d.cur_pass = 2
    if ROM_ORG <= p2 < ROM_ORG+len(rom): d.run_6616(0x1C, 0, p2)
    # ptr#3 cuerpo: 3 pasadas comparten cursor+ptr
    if ROM_ORG <= p3 < ROM_ORG+len(rom):
        d.row, d.col, d.ptr = 2, 0, p3
        d.cur_pass = 3
        d.run_66A2()
        d.row, d.col = 2, 0
        d.cur_pass = 4
        d.run_6766()
        d.run_objects()
    return d, (off, p1, p2, p3)

def diagnose(rom, room_x):
    d, (off,p1,p2,p3) = decode_room(rom, room_x)
    from collections import Counter
    sh = Counter(d.grid.values())
    pa = Counter(d.passgrid.values())
    pnames = {1:'top(6616)', 2:'bot(6616)', 3:'body-66A2', 4:'body-6766'}
    print(f"=== SALA {room_x:#04x}: {len(d.grid)} celdas ===")
    print("  por shape:", dict(sorted(sh.items())))
    print("  por pasada:", {pnames[k]:v for k,v in sorted(pa.items())})
    # render coloreado por PASADA
    PASSCOL = {1:(220,60,60), 2:(60,120,220), 3:(60,200,90), 4:(230,200,40)}
    scale=12
    img = Image.new("RGB",(20*scale,30*scale),(8,8,12)); px=img.load()
    for (r,c),pid in d.passgrid.items():
        col=PASSCOL.get(pid,(255,255,255))
        for dy in range(scale):
            for dx in range(scale):
                x,y=c*scale+dx,r*scale+dy
                if 0<=x<img.width and 0<=y<img.height: px[x,y]=col
    img.save(f"diag_pass_{room_x:02x}.png")

def color_for(shape):
    import colorsys
    if shape == 0: return (30,30,40)
    h = (shape * 0.15) % 1.0
    r,g,b = colorsys.hsv_to_rgb(h, 0.55, 0.95)
    return (int(r*255), int(g*255), int(b*255))

def render_png(grid, scale=12):
    img = Image.new("RGB", (20*scale, 30*scale), (8,8,12))
    px = img.load()
    for (r,c), s in grid.items():
        col = color_for(s)
        for dy in range(scale):
            for dx in range(scale):
                x, y = c*scale+dx, r*scale+dy
                if 0 <= x < img.width and 0 <= y < img.height: px[x,y] = col
    return img

def main():
    rom = load_rom(sys.argv[1] if len(sys.argv) > 1 else "the_castle.rom")
    print(f"ROM {len(rom)} bytes")
    if len(sys.argv) > 2 and sys.argv[2] == "diag":
        for room in [0x00, 0x70, 0x80]:
            diagnose(rom, room)
        print("Guardado diag_pass_*.png")
        return
    rooms = [0x00, 0x70, 0x01, 0x80]
    panels = []
    for room in rooms:
        d, (off,p1,p2,p3) = decode_room(rom, room)
        print(f"SALA {room:#04x} off={off} p1={p1:#06x} p2={p2:#06x} p3={p3:#06x} "
              f"celdas={len(d.grid)} objs={len(d.objs)}")
        if d.objs:
            print("   objs:", [(hex(t),r,c) for t,r,c in d.objs[:10]])
        img = render_png(d.grid)
        img.save(f"room_{room:02x}.png")
        panels.append(img)
    if panels:
        pw, ph = panels[0].size
        sheet = Image.new("RGB", (len(panels)*(pw+8), ph), (0,0,0))
        for i,im in enumerate(panels): sheet.paste(im, (i*(pw+8), 0))
        sheet.save("rooms_sheet.png")
        print("Guardado rooms_sheet.png + room_*.png")

if __name__ == "__main__":
    main()
