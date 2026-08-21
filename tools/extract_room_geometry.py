#!/usr/bin/env python3
"""Extract room geometry (origins + vertices) from the GoldenEye decomp.

Feeds tools/geom_validate.cpp.

The oracle here is structural rather than bit-exact. Each level's
`room_data_table` (assets/obseg/bg/*.c) declares, per room:

    { &point_table_binary_N, &pri_mapping_binary_N, &sec..., originX, originY, originZ }

The point table holds room-LOCAL vertices, centred near zero; the origin places
that room in the level. So two things must hold on real data:

  * local vertices are centred near the origin of their own space, with an
    extent that looks like a room rather than a level;
  * origin + local assembles into one coherent level bounding box.

Both fail loudly if the vertex stride, endianness, or matrix convention is
wrong — a transposed translate, for instance, scatters rooms instead of
assembling them.

USAGE
    python3 tools/extract_room_geometry.py <decomp-root> <out.bin>
    ./build/geom_validate <out.bin>

FILE FORMAT (little-endian)
    magic 'GERG' u32
    level_count  u32
    per level:
        name_len u32, name bytes
        room_count u32
        per room:
            origin f32[3]
            vtx_count u32
            vtx: s16 x,y,z, s16 flag, s16 s,t, u8 r,g,b,a   (16 bytes, host order)
"""

import glob
import os
import re
import struct
import sys
import zlib

ROOM_RE = re.compile(
    r'\{\s*&(point_table_binary_\d+)\s*,\s*&\w+\s*,\s*[^,]+,\s*'
    r'(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\}')
ARRAY_RE = re.compile(r'u32 (point_table_binary_\d+)\[\]\s*=\s*\{(.*?)\};', re.S)
HEX_RE = re.compile(r'0x([0-9A-Fa-f]+)')


def inflate_1172(raw):
    if len(raw) < 3 or raw[:2] != b'\x11\x72':
        return None
    try:
        d = zlib.decompressobj(-15)
        out = d.decompress(raw[2:]) + d.flush()
    except zlib.error:
        return None
    # Same trailing-zero truncation as display lists; a Vtx is 16 bytes.
    if len(out) % 16:
        out += b'\x00' * (16 - len(out) % 16)
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    repo, outpath = sys.argv[1], sys.argv[2]

    files = sorted(glob.glob(os.path.join(repo, 'assets/obseg/bg/*.c')))
    if not files:
        print('no bg sources found', file=sys.stderr)
        return 2

    levels = []
    for path in files:
        src = open(path, errors='ignore').read()
        arrays = {m.group(1): m.group(2) for m in ARRAY_RE.finditer(src)}
        rooms = []
        for m in ROOM_RE.finditer(src):
            name = m.group(1)
            body = arrays.get(name)
            if body is None:
                continue
            words = [int(x, 16) for x in HEX_RE.findall(body)]
            raw = b''.join(struct.pack('>I', w) for w in words)
            data = inflate_1172(raw)
            if not data:
                continue
            # Re-pack each Vtx from big-endian to host order. The structure is
            # known here (s16 x6 then 4 bytes of colour), so the swap is
            # unambiguous — exactly the loader-side swap os_io.h argues for.
            n = len(data) // 16
            verts = bytearray()
            for i in range(n):
                x, y, z, flag, s, t = struct.unpack_from('>6h', data, i * 16)
                rgba = data[i * 16 + 12: i * 16 + 16]
                verts += struct.pack('<6h', x, y, z, flag, s, t) + rgba
            rooms.append({
                'origin': (float(m.group(2)), float(m.group(3)), float(m.group(4))),
                'n': n,
                'verts': bytes(verts),
            })
        if rooms:
            levels.append({'name': os.path.basename(path)[:-2], 'rooms': rooms})

    with open(outpath, 'wb') as f:
        f.write(b'GERG')
        f.write(struct.pack('<I', len(levels)))
        for lv in levels:
            nm = lv['name'].encode()
            f.write(struct.pack('<I', len(nm)))
            f.write(nm)
            f.write(struct.pack('<I', len(lv['rooms'])))
            for r in lv['rooms']:
                f.write(struct.pack('<3f', *r['origin']))
                f.write(struct.pack('<I', r['n']))
                f.write(r['verts'])

    total_rooms = sum(len(l['rooms']) for l in levels)
    total_verts = sum(r['n'] for l in levels for r in l['rooms'])
    print(f'levels: {len(levels)}  rooms: {total_rooms}  vertices: {total_verts}')
    print(f'wrote {outpath}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
