ram=open('ram.bin','rb').read()   # 0xE000..0xF000
def slot(addr): return addr-0xE000
def dump(name, base, nslots, step=5):
    print(f"\n=== {name} (0x{base:04X}) ===")
    o=slot(base)
    for i in range(nslots):
        s=ram[o+i*step : o+i*step+step]
        if any(s):
            print(f"  slot{i}: "+" ".join(f"{b:02X}" for b in s))
# Layout segun handoff: slot = [param, type, col, row, flags]
dump("BAT/enemigos", 0xE416, 8)
dump("COLL/coleccionables", 0xE386, 16)
dump("OBJ/estructurales-salidas", 0xE43E, 16)
dump("ExitDoor", 0xE3D6, 8, 4)
