import json
from collections import defaultdict
obj=json.load(open("castle_objects.json"))
bytype=defaultdict(list)
for room,d in obj.items():
    for o in d["obj"]:
        bytype[o["type"]].append((room,o["row"],o["col"],o["p3"],o["p4"]))
print("=== Tipos estructurales (tabla OBJ) — muestras con [row,col,p3,p4] ===")
for t in sorted(bytype):
    items=bytype[t]
    p3s=set(i[3] for i in items); p4s=set(i[4] for i in items)
    print(f"\n0x{t:02X}  ({len(items)} ocurrencias)  p3 vals={sorted(p3s)}  p4 vals={sorted(p4s)}")
    # muestra hasta 4, agrupando por sala
    bysala=defaultdict(list)
    for r,row,col,p3,p4 in items: bysala[r].append((row,col,p3,p4))
    shown=0
    for sala in sorted(bysala):
        seq=bysala[sala]
        print(f"    sala {sala}: "+" ".join(f"({row},{col},p{p3},{p4})" for row,col,p3,p4 in seq))
        shown+=1
        if shown>=3: break
