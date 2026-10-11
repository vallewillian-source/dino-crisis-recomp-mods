#!/usr/bin/env python3
"""filter_generated.py — Fase 2: remove segmentos substituídos do C gerado.

O PSXRecomp gera os shards como saída da recompilação (DO NOT EDIT). Este
script é o passo de build que produz, em <out>, uma cópia dos shards sem os
corpos das funções listadas em <replacements> (as substituições legíveis em
src_game/ fornecem as mesmas definições). Regenerar o jogo não perde o
mecanismo: o filtro roda de novo sobre a saída nova.

Modos:
  --mode filter   <gen> -> <out>: shards filtrados + decls.h + dispatch.c
  --mode extract  <gen> -> <out>: orig_extracted.c com os corpos ORIGINAIS
                  das funções substituídas (para o teste diferencial; compile
                  com -Dfunc_<PC>=orig_<PC> para renomear)

É idempotente e falha ALTO se uma função da lista não for encontrada no
shard (lista fora de sincronia com o C gerado = erro de build, nunca silêncio).
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path


def parse_replacements(path: Path) -> list[int]:
    pcs = []
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        pcs.append(int(line, 16) if line.lower().startswith("0x") else int(line))
    return pcs


def remove_function(text: str, func_name: str) -> tuple[str, bool]:
    """Remove `void func_name(CPUState* cpu) { ... }` (match de chaves)."""
    start = text.find(f"void {func_name}(CPUState* cpu)")
    if start < 0:
        return text, False
    brace = text.index("{", start)
    depth = 0
    i = brace
    while i < len(text):
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                # consome o newline final
                while end < len(text) and text[end] == "\n":
                    end += 1
                    break
                return text[:start] + text[end:], True
        i += 1
    raise SystemExit(f"filter_generated: chaves desbalanceadas em {func_name}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", choices=["filter", "extract"], required=True)
    ap.add_argument("--gen", required=True, help="dir generated/ (entrada)")
    ap.add_argument("--out", required=True, help="dir de saída")
    ap.add_argument("--replacements", required=True)
    args = ap.parse_args()

    gen = Path(args.gen)
    out = Path(args.out)
    pcs = parse_replacements(Path(args.replacements))
    names = [f"func_{pc:08X}" for pc in pcs]

    if args.mode == "extract":
        out.mkdir(parents=True, exist_ok=True)
        parts = [
            "/* orig_extracted.c — corpos ORIGINAIS gerados das funções",
            " * substituídas, extraídos para o teste diferencial",
            " * (psx-matched-test). NÃO ligar no jogo. Compilar com",
            " * -Dfunc_<PC>=orig_<PC>. */",
            '#include "SLUS_009.22_decls.h"',
            "",
        ]
        for name in names:
            for shard in sorted(gen.glob("SLUS_009.22_full_*.c")):
                text = shard.read_text()
                m = re.search(
                    r"void %s\(CPUState\* cpu\).*?\n\}\n" % name, text, re.S
                )
                if m:
                    parts.append(f"/* ==== original {name} ({shard.name}) ==== */")
                    parts.append(m.group(0))
                    break
            else:
                print(f"extract: {name} NÃO encontrado", file=sys.stderr)
                return 1
        (out / "orig_extracted.c").write_text("\n".join(parts))
        print(f"extract: {len(names)} originais -> {out/'orig_extracted.c'}")
        return 0

    # ---- mode filter ----
    out.mkdir(parents=True, exist_ok=True)
    removed = {n: False for n in names}
    for shard in sorted(gen.glob("SLUS_009.22_full_*.c")):
        text = shard.read_text()
        for name in names:
            if f"void {name}(CPUState* cpu)" in text:
                text, ok = remove_function(text, name)
                removed[name] = removed[name] or ok
        (out / shard.name).write_text(text)
    for aux in ("SLUS_009.22_decls.h", "SLUS_009.22_dispatch.c",
                "SLUS_009.22_full.ranges"):
        src = gen / aux
        if src.is_file():
            shutil.copy2(src, out / aux)
    missing = [n for n, ok in removed.items() if not ok]
    if missing:
        print(
            "filter_generated: substituições não encontradas no C gerado "
            f"(regenerar? lista desincronizada?): {missing}",
            file=sys.stderr,
        )
        return 1
    print(f"filter: {len(names)} segmentos removidos -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
