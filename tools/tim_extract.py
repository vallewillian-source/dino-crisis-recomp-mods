#!/usr/bin/env python3
"""TIM (PS1 texture) extractor + inventory generator for Dino Crisis.

Scans a binary blob (the boot EXE, a disc file, or a whole raw disc track)
for TIM images (magic 0x00000010), validates the structure, decodes every
image (4bpp/8bpp with CLUT, 16bpp direct) and writes:

  assets/textures/original/<id>_<where>_<offset>.png
  assets/textures/inventory.json   (full machine-readable inventory)
  assets/textures/inventory.tsv    (flat table for docs/TEXTURES.md)

Usage:
  python3 tools/tim_extract.py <binary> [--label <name>] [--min-dim 8]
      [--out-dir assets/textures] [--bin2352]

  --bin2352  treats the file as a raw MODE2/2352 disc track: every 2352-byte
             sector has a 24-byte header, so the scan re-slices the stream
             into 2048-byte user-data sectors first (avoids false positives
             from the sync/subheader bytes, and de-skews file offsets).

Only structurally valid TIMs are accepted: magic, plausible block lengths,
dimensions within PS1 VRAM limits, and pixel-block length consistent with
w/h/bpp. False positives from code words are rejected by that gate.
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    sys.exit("Pillow necessário: pip install Pillow")

TIM_MAGIC = 0x00000010
PMODE_4BPP, PMODE_8BPP, PMODE_16BPP, PMODE_24BPP = 0, 1, 2, 3


def bgr555_to_rgb888(word: int) -> tuple[int, int, int]:
    r = word & 0x1F
    g = (word >> 5) & 0x1F
    b = (word >> 10) & 0x1F
    # PS1 5-bit -> 8-bit (beetle/duckstation style expansion)
    return (r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2)


def parse_tim(data: bytes, off: int) -> dict | None:
    """Parse and fully validate one TIM at off. Returns metadata or None."""
    if off + 12 > len(data):
        return None
    magic, flag = struct.unpack_from("<II", data, off)
    if magic != TIM_MAGIC:
        return None
    pmode = flag & 0x07
    has_clut = bool(flag & 0x08)
    if pmode > PMODE_24BPP:
        return None
    if has_clut and pmode > PMODE_8BPP:
        return None  # CLUT only valid for 4/8bpp
    pos = off + 8
    n_cluts = 1
    clut = None
    if has_clut:
        if pos + 12 > len(data):
            return None
        (blen,) = struct.unpack_from("<I", data, pos)
        if blen < 12 or blen % 4 or pos + blen > len(data):
            return None
        _dx, _dy, cw, ch = struct.unpack_from("<HHHH", data, pos + 8)
        expected = 12 + cw * ch * 2
        if blen != expected:
            return None
        if pmode == PMODE_4BPP and cw != 16:
            return None
        if pmode == PMODE_8BPP and cw != 256:
            return None
        n_cluts = ch
        clut_words = struct.unpack_from(f"<{cw * ch}H", data, pos + 16)
        clut = [bgr555_to_rgb888(w) for w in clut_words[:cw]]
        pos += blen
    if pos + 12 > len(data):
        return None
    (blen,) = struct.unpack_from("<I", data, pos)
    if blen < 12 or blen % 2 or pos + blen > len(data):
        return None
    _dx, _dy, w, h = struct.unpack_from("<HHHH", data, pos + 8)
    if w == 0 or h == 0 or w > 1024 or h > 512:
        return None
    px = w * {PMODE_4BPP: 4, PMODE_8BPP: 2, PMODE_16BPP: 1, PMODE_24BPP: 0.6667}[pmode]
    if pmode == PMODE_24BPP:
        if blen != 12 + w * h * 3 // 2:
            return None
    else:
        if blen != 12 + w * h * 2:
            return None
    pixels_words = struct.unpack_from(f"<{w * h}H", data, pos + 12)
    info = {
        "offset": off,
        "pmode": pmode,
        "bpp": {0: 4, 1: 8, 2: 16, 3: 24}[pmode],
        "has_clut": has_clut,
        "n_cluts": n_cluts,
        "clut_colors": len(clut) if clut else 0,
        "width": w * {0: 4, 1: 2, 2: 1}.get(pmode, 1),
        "height": h,
        "size_bytes": blen + (0 if not has_clut else struct.unpack_from('<I', data, off + 8)[0]) + 8,
    }
    info["_clut"] = clut
    info["_pixels"] = pixels_words
    return info


def decode_to_image(info: dict, clut_index: int = 0) -> Image.Image:
    w_words, h = info["_pixels"], info["height"]
    pmode = info["pmode"]
    clut = info["_clut"]
    if pmode == PMODE_4BPP:
        clut = clut[clut_index * 16:(clut_index + 1) * 16]
        img = Image.new("P", (info["width"], h))
        pal = []
        for c in clut:
            pal.extend(c)
        img.putpalette(pal + [0] * (768 - len(pal)))
        idx = bytearray(info["width"] * h)
        for row in range(h):
            base = row * w_words
            for col in range(w_words):
                word = info["_pixels"][base + col]
                idx[row * info["width"] + col * 4 + 0] = word & 0xF
                idx[row * info["width"] + col * 4 + 1] = (word >> 4) & 0xF
                idx[row * info["width"] + col * 4 + 2] = (word >> 8) & 0xF
                idx[row * info["width"] + col * 4 + 3] = (word >> 12) & 0xF
        img.putdata(idx)
        return img
    if pmode == PMODE_8BPP:
        clut = clut[clut_index * 256:(clut_index + 1) * 256]
        img = Image.new("P", (info["width"], h))
        pal = []
        for c in clut:
            pal.extend(c)
        img.putpalette(pal + [0] * (768 - len(pal)))
        idx = bytearray(info["width"] * h)
        for row in range(h):
            base = row * w_words
            for col in range(w_words):
                word = info["_pixels"][base + col]
                idx[row * info["width"] + col * 2 + 0] = word & 0xFF
                idx[row * info["width"] + col * 2 + 1] = (word >> 8) & 0xFF
        img.putdata(idx)
        return img
    if pmode == PMODE_16BPP:
        img = Image.new("RGB", (info["width"], h))
        rgb = bytearray()
        for word in w_words:
            rgb.extend(bgr555_to_rgb888(word))
        img.putdata(list(zip(rgb[0::3], rgb[1::3], rgb[2::3])))
        return img
    raise ValueError("24bpp não suportado")


def iter_sector_user_data(path: str):
    """Yield (file_offset, user_data_2048) for a MODE2/2352 raw track."""
    sector = 0
    with open(path, "rb") as f:
        while True:
            f.seek(sector * 2352 + 24)
            chunk = f.read(2048)
            if not chunk:
                return
            yield sector * 2352, chunk
            sector += 1


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("--label", default=None)
    ap.add_argument("--min-dim", type=int, default=8)
    ap.add_argument("--out-dir", default="assets/textures")
    ap.add_argument("--bin2352", action="store_true",
                    help="input is a raw MODE2/2352 track (scan user sectors)")
    ap.add_argument("--save-png", action="store_true", default=True)
    args = ap.parse_args()

    label = args.label or os.path.splitext(os.path.basename(args.binary))[0]
    os.makedirs(os.path.join(args.out_dir, "original"), exist_ok=True)
    inv_path = os.path.join(args.out_dir, "inventory.json")

    inv = {"sources": {}, "items": []}
    if os.path.exists(inv_path):
        inv = json.load(open(inv_path))

    found: list[dict] = []
    if args.bin2352:
        n_sectors = 0
        for foff, chunk in iter_sector_user_data(args.binary):
            n_sectors += 1
            if len(chunk) >= 12:
                for i in range(0, len(chunk) - 8, 4):
                    if chunk[i] == 0x10 and chunk[i + 1] == 0 and chunk[i + 2] == 0 and chunk[i + 3] == 0:
                        info = parse_tim(chunk, i)
                        if info:
                            info["offset"] = foff + i - 24 + 24 - 24  # sector-relative
                            info["offset"] = foff - 24 + i  # raw file offset
                            found.append(info)
        print(f"{label}: {n_sectors} setores varridos")
    else:
        data = open(args.binary, "rb").read()
        i = 0
        magic = struct.pack("<I", TIM_MAGIC)
        while True:
            i = data.find(magic, i)
            if i < 0:
                break
            info = parse_tim(data, i)
            if info:
                found.append(info)
                i += info["size_bytes"]
            else:
                i += 4
        print(f"{label}: {len(data)} bytes varridos")

    kept = 0
    items = inv["items"]
    known_offsets = {(it["source"], it["offset"]) for it in items}
    for n, info in enumerate(found):
        if info["width"] < args.min_dim or info["height"] < args.min_dim:
            continue
        src_key = label
        if (src_key, info["offset"]) in known_offsets:
            continue
        item = {k: v for k, v in info.items() if not k.startswith("_")}
        item["source"] = src_key
        aid = f"{len(items):04d}"
        item["id"] = aid
        png_rel = f"original/{aid}_{src_key}_{info['offset']:08X}.png"
        png_path = os.path.join(args.out_dir, png_rel)
        try:
            decode_to_image(info).save(png_path)
        except Exception as e:  # noqa: BLE001
            item["png"] = None
            item["error"] = str(e)
        else:
            item["png"] = png_rel
        items.append(item)
        kept += 1
    inv["sources"][label] = {"path": args.binary, "tim_count": len(found), "kept": kept}
    json.dump(inv, open(inv_path, "w"), indent=1)
    print(f"{label}: {len(found)} TIMs válidos, {kept} novos salvos (total {len(items)})")


if __name__ == "__main__":
    main()
