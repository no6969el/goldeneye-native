#!/usr/bin/env python3
"""Extract matched (vertices + display list) pairs for one level's rooms.

Feeds tools/room_render.cpp.

The other two extractors deliberately pull the halves apart — geometry for
geom_validate, display lists for dl_validate. Rendering needs them together,
because a room's display list indexes its own point table through segment 14
(SPSEGMENT_BG_VTX), the way src/game/bg.c:2688 sets it up.

Both halves are emitted in HOST order. The structures are known here — a Vtx is
six s16 plus four colour bytes, a Gfx is two u32 — so the swap is unambiguous.
That is exactly the loader-side conversion src/ultra/os_io.h argues for, and the
reason PI DMA does not blanket-swap.

USAGE
    python3 tools/extract_room_pair.py <decomp-root> <level-stem> <out.bin>
    ./build/room_render <out.bin> <room-index> <out.ppm>

FILE FORMAT (little-endian, host order)
    magic 'GERP' u32
    room_count u32
    per room:
        origin f32[3]
        vtx_count u32,  vtx bytes  (16 each)
        dl_bytes  u32,  dl  bytes  (8 each)
"""

import glob
import os
import re
import struct
import sys
import zlib

ROOM_RE = re.compile(
    r'\{\s*&(point_table_binary_\d+)\s*,\s*&(pri_mapping_binary_\d+)\s*,\s*[^,]+,\s*'
    r'(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\}')
ARR_RE = re.compile(r'u32 (\w+)\[\]\s*=\s*\{(.*?)\};', re.S)
HEX_RE = re.compile(r'0x([0-9A-Fa-f]+)')


def inflate(raw, align):
    if len(raw) < 3 or raw[:2] != b'\x11\x72':
        return None
    try:
        d = zlib.decompressobj(-15)
        out = d.decompress(raw[2:]) + d.flush()
    except zlib.error:
        return None
    # 1172 omits trailing zeros; pad back to whole records.
    if len(out) % align:
        out += b'\x00' * (align - len(out) % align)
    return out


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    repo, stem, outpath = sys.argv[1], sys.argv[2], sys.argv[3]

    matches = glob.glob(os.path.join(repo, f'assets/obseg/bg/{stem}*.c'))
    if not matches:
        print(f'no bg source matching {stem}', file=sys.stderr)
        return 2
    path = sorted(matches)[0]
    src = open(path, errors='ignore').read()
    arrays = {m.group(1): m.group(2) for m in ARR_RE.finditer(src)}

    def blob(name):
        body = arrays.get(name)
        if body is None:
            return None
        words = [int(x, 16) for x in HEX_RE.findall(body)]
        return b''.join(struct.pack('>I', w) for w in words)

    rooms = []
    for m in ROOM_RE.finditer(src):
        vt = inflate(blob(m.group(1)) or b'', 16)
        dl = inflate(blob(m.group(2)) or b'', 8)
        if not vt or not dl:
            continue

        # Vtx: 6 x s16 then 4 colour bytes.
        verts = bytearray()
        for i in range(len(vt) // 16):
            f = struct.unpack_from('>6h', vt, i * 16)
            verts += struct.pack('<6h', *f) + vt[i * 16 + 12: i * 16 + 16]

        # Gfx: two u32.
        cmds = bytearray()
        for i in range(len(dl) // 8):
            w0, w1 = struct.unpack_from('>II', dl, i * 8)
            cmds += struct.pack('<II', w0, w1)

        rooms.append({
            'origin': (float(m.group(3)), float(m.group(4)), float(m.group(5))),
            'verts': bytes(verts),
            'dl': bytes(cmds),
        })

    if not rooms:
        print('no rooms extracted', file=sys.stderr)
        return 1

    with open(outpath, 'wb') as f:
        f.write(b'GERP')
        f.write(struct.pack('<I', len(rooms)))
        for r in rooms:
            f.write(struct.pack('<3f', *r['origin']))
            f.write(struct.pack('<I', len(r['verts']) // 16))
            f.write(r['verts'])
            f.write(struct.pack('<I', len(r['dl'])))
            f.write(r['dl'])

    print(f'{os.path.basename(path)}: {len(rooms)} rooms')
    for i, r in enumerate(rooms[:12]):
        print(f'  room {i:3d}: {len(r["verts"])//16:4d} verts, '
              f'{len(r["dl"])//8:4d} cmds, origin=({r["origin"][0]:.0f},'
              f'{r["origin"][1]:.0f},{r["origin"][2]:.0f})')
    print(f'wrote {outpath}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
