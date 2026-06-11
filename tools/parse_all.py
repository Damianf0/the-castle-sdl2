import json
from collections import Counter
def slots(data, base, n, step=5):
    o=base-0xE380; out=[]
    for i in range(n):
        s=list(data[o+i*step:o+i*step+step])
        if any(s): out.append(s)
    return out
allrooms={}; cb=Counter(); cc=Counter(); co=Counter()
nbat=ncoll=nobj=0; rooms_with_enemy=0
for hi in range(10):
    for lo in range(10):
        room=hi*16+lo
        try: data=open(f"tests/fixtures/objs/objs_{room:02X}.bin","rb").read()
        except: continue
        bat=slots(data,0xE416,8); coll=slots(data,0xE386,16); obj=slots(data,0xE43E,16)
        # BAT/COLL: [param,type,row,col,flags]  OBJ: [type,row,col,p3,p4]
        enemies=[{"type":s[1],"row":s[2],"col":s[3],"flags":s[4]} for s in bat]
        colls  =[{"type":s[1],"row":s[2],"col":s[3]} for s in coll]
        objs   =[{"type":s[0],"row":s[1],"col":s[2],"p3":s[3],"p4":s[4]} for s in obj]
        allrooms[f"{room:02X}"]={"enemies":enemies,"coll":colls,"obj":objs}
        nbat+=len(enemies); ncoll+=len(colls); nobj+=len(objs)
        if enemies: rooms_with_enemy+=1
        for e in enemies: cb[e["type"]]+=1
        for c in colls: cc[c["type"]]+=1
        for o in objs: co[o["type"]]+=1
json.dump(allrooms, open("tests/fixtures/castle_objects.json","w"), indent=0)
print(f"=== CENSO REAL (del ROM via openMSX, 100 salas) ===")
print(f"Enemigos: {nbat} total en {rooms_with_enemy} salas. Tipos: "+", ".join(f"0x{t:02X}x{n}" for t,n in sorted(cb.items())))
print(f"Coleccionables: {ncoll}. Tipos: "+", ".join(f"0x{t:02X}x{n}" for t,n in sorted(cc.items())))
print(f"Estructural/rampas/ascensores: {nobj}. Tipos: "+", ".join(f"0x{t:02X}x{n}" for t,n in sorted(co.items())))
print("\nEjemplos:")
for r in ["00","01","70","80"]:
    d=allrooms[r]
    print(f"  sala {r}: {len(d['enemies'])} enem, {len(d['coll'])} coll, {len(d['obj'])} estruct")
