#!/usr/bin/env python3
"""Consolida o dump.log do hook PSX_TEX_DUMP_DIR em inventários.

Entrada: assets/textures/runtime-dumps/dump.log (frame x y w h texhash por linha)
Saída:   assets/textures/inventory.tsv  (texhash, w, h, x, y, first, last, count)
         resumo por dimensão impresso no stdout.

Uso: python3 tools/texture_inventory.py [dir-runtime-dumps]
"""
import collections
import glob
import json
import os
import sys

D = sys.argv[1] if len(sys.argv) > 1 else "assets/textures/runtime-dumps"
log = os.path.join(D, "dump.log")

agg = {}
order = 0
for line in open(log):
    fr, x, y, w, h, th = line.split()
    fr, x, y, w, h = map(int, (fr, x, y, w, h))
    a = agg.get(th)
    if a is None:
        agg[th] = a = {"w": w, "h": h, "x": x, "y": y,
                       "first": fr, "last": fr, "count": 0, "ord": order}
        order += 1
    a["last"] = max(a["last"], fr)
    a["count"] += 1

out = os.path.join(os.path.dirname(D), "inventory.tsv")
with open(out, "w") as f:
    f.write("texhash\tw\th\tx\ty\tfirst_frame\tlast_frame\tuploads\tfile\n")
    for th, a in sorted(agg.items(), key=lambda kv: kv[1]["ord"]):
        png = f"{th}_{a['w']}x{a['h']}.png"
        f.write(f"{th}\t{a['w']}\t{a['h']}\t{a['x']}\t{a['y']}\t"
                f"{a['first']}\t{a['last']}\t{a['count']}\t{png}\n")

sizes = collections.Counter((a["w"], a["h"]) for a in agg.values())
print(f"uploads: {sum(a['count'] for a in agg.values())} | "
      f"texturas únicas: {len(agg)} | dimensões distintas: {len(sizes)}")
print("top 10 dimensões por área:")
for (w, h), n in sorted(sizes.items(), key=lambda kv: -(kv[0][0] * kv[0][1]))[:10]:
    print(f"  {w}x{h}: {n} texturas")
print("inventário:", out)

# saneia: PNG presente para cada linha
missing = 0
for th, a in agg.items():
    if not glob.glob(os.path.join(D, f"{th}_{a['w']}x{a['h']}.png")):
        missing += 1
print("linhas sem PNG no dump:", missing)
