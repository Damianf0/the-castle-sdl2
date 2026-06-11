import decode_geom as D
rom=D.load_rom("the_castle.rom")
# offsets en el dump (base 0xE380): COLL 0xE386, BAT 0xE416, OBJ 0xE43E
TABLES=[("COLL",0xE386,16),("BAT",0xE416,8),("OBJ",0xE43E,16)]
def real_slots(data, base, n, step=5):
    o=base-0xE380; out=[]
    for i in range(n):
        s=data[o+i*step:o+i*step+step]
        if any(s): out.append(tuple(s))
    return out
for room in [0x00,0x01,0x05,0x10,0x33,0x70,0x80,0x99]:
    try: data=open(f"tests/fixtures/objs/objs_{room:02X}.bin","rb").read()
    except: continue
    print(f"\n========= SALA {room:#04x} =========")
    print("  REAL (slot = b0 b1 b2 b3 b4):")
    for nm,base,n in TABLES:
        sl=real_slots(data,base,n)
        if sl: print(f"    {nm}: "+" | ".join(" ".join(f"{b:02X}" for b in s) for s in sl))
    d,_=D.decode_room(rom,room)
    print("  MI DECODE (type,row,col):", [(hex(t),r,c) for t,r,c in d.objs])
